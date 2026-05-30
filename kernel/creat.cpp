/**
 * creat.cpp - File creation
 * Create new file or truncate existing one
 * Layer 4 (Member C)
 */
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include "filesys.h"

int creat(unsigned int uid, const char *filename, unsigned short mode)
{
    inode *ino = nullptr;

    /* ---------- 1. Check if file already exists ---------- */
    unsigned int di = namei(filename);

    /* namei returns directory-entry index on success,
     * (unsigned int)-1 or 0 on failure (known bug #1).
     * Defensive check: treat out-of-range as "not found". */
    if (di < DIRNUM && g_dir.direct[di].d_ino != DIEMPTY) {
        /* File exists → truncate */
        ino = iget(g_dir.direct[di].d_ino);
        if (!ino) {
            g_vfs_errno = E_VFS_IO;
            return -1;
        }

        /* Check write permission on the existing file */
        if (!access(uid, ino, WRITE_MODE)) {
            g_vfs_errno = E_VFS_NOPERM;
            iput(ino);
            return -1;
        }

        /* Free all data blocks allocated to this inode */
        for (int i = 0; i < NADDR; i++) {
            if (ino->di_addr[i] != 0) {
                bfree(ino->di_addr[i]);
                ino->di_addr[i] = 0;
            }
        }
        ino->di_size = 0;
        ino->i_flag |= IUPDATE;

        /* Reset f_off for any other open instances of this file */
        for (int i = 0; i < SYSOPENFILE; i++) {
            if (g_sys_ofile[i].f_inode == ino) {
                g_sys_ofile[i].f_off = 0;
            }
        }
    } else {
        /* File does not exist → create new */

        /* Find an empty directory-entry slot */
        unsigned short slot = iname(filename);
        if (slot >= DIRNUM) {
            g_vfs_errno = E_VFS_NOSPC;
            return -1;
        }

        /* Allocate a new disk inode */
        ino = ialloc();
        if (!ino) {
            /* ialloc already set g_vfs_errno */
            return -1;
        }

        /* Populate inode fields */
        ino->di_mode  = DIFILE | (mode & DEFAULTMODE);
        ino->di_uid   = g_user[uid].u_uid;
        ino->di_gid   = g_user[uid].u_gid;
        ino->di_size  = 0;
        ino->i_flag  |= IUPDATE;

        /* Link directory entry to the new inode */
        g_dir.direct[slot].d_ino = ino->i_ino;
        g_dir.size++;
    }

    /* ---------- 2. Allocate system open-file-table slot ---------- */
    int sys_no = -1;
    for (int i = 1; i < SYSOPENFILE; i++) {
        if (g_sys_ofile[i].f_count == 0) {
            sys_no = i;
            break;
        }
    }
    if (sys_no == -1) {
        g_vfs_errno = E_VFS_NFILE;
        iput(ino);
        return -1;
    }

    /* ---------- 3. Allocate user open-file-table slot ---------- */
    int user_fd = -1;
    for (int j = 0; j < NOFILE; j++) {
        if (g_user[uid].u_ofile[j] == SYSOPENFILE + 1) {
            user_fd = j;
            break;
        }
    }
    if (user_fd == -1) {
        g_vfs_errno = E_VFS_NFILE;
        iput(ino);
        return -1;
    }

    /* ---------- 4. Link everything together ---------- */
    g_sys_ofile[sys_no].f_flag  = FWRITE;
    g_sys_ofile[sys_no].f_count = 1;
    g_sys_ofile[sys_no].f_inode = ino;
    g_sys_ofile[sys_no].f_off   = 0;

    g_user[uid].u_ofile[user_fd] = sys_no;

    return user_fd;
}
