/**
 * open.cpp - File open
 * Open existing file with FREAD / FWRITE / FAPPEND mode
 * Layer 4 (Member C)
 */
#include <cstdio>
#include "filesys.h"

unsigned short aopen(unsigned int uid, const char *filename, unsigned short openmode)
{
    /* ---------- 1. Look up the file ---------- */
    unsigned int di = namei(filename);

    /* namei returns (unsigned int)-1 or 0 when not found (known bugs #1, #3).
     * We check defensively: out-of-range index OR d_ino == 0 → not found. */
    if (di >= DIRNUM || g_dir.direct[di].d_ino == DIEMPTY) {
        g_vfs_errno = E_VFS_NOENT;
        return (unsigned short)-1;
    }

    /* ---------- 2. Get inode and check permissions ---------- */
    inode *ino = iget(g_dir.direct[di].d_ino);
    if (!ino) {
        g_vfs_errno = E_VFS_IO;
        return (unsigned short)-1;
    }

    /* Map openmode flags to access() mode */
    if (openmode & FREAD) {
        if (!access(uid, ino, READ_MODE)) {
            g_vfs_errno = E_VFS_NOPERM;
            iput(ino);
            return (unsigned short)-1;
        }
    }
    if (openmode & (FWRITE | FAPPEND)) {
        if (!access(uid, ino, WRITE_MODE)) {
            g_vfs_errno = E_VFS_NOPERM;
            iput(ino);
            return (unsigned short)-1;
        }
    }

    /* ---------- 3. Allocate system open-file-table slot ---------- */
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
        return (unsigned short)-1;
    }

    /* ---------- 4. Allocate user open-file-table slot ---------- */
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
        return (unsigned short)-1;
    }

    /* ---------- 5. Populate system file table entry ---------- */
    g_sys_ofile[sys_no].f_flag  = openmode;
    g_sys_ofile[sys_no].f_count = 1;
    g_sys_ofile[sys_no].f_inode = ino;

    if (openmode & FAPPEND) {
        g_sys_ofile[sys_no].f_off = ino->di_size;
    } else {
        g_sys_ofile[sys_no].f_off = 0;
    }

    /* Link user fd → system slot */
    g_user[uid].u_ofile[user_fd] = sys_no;

    return (unsigned short)user_fd;
}
