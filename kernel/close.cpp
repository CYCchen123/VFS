/**
 * close.cpp - File close
 * Release user + system open file table entries, iput inode
 * Layer 4 (Member C)
 */
#include "filesys.h"

void close(unsigned int uid, unsigned short cfd)
{
    /* Validate user file descriptor */
    if (cfd >= NOFILE) {
        g_vfs_errno = E_VFS_INVAL;
        return;
    }

    unsigned short sys_no = g_user[uid].u_ofile[cfd];

    /* Validate system file-table index */
    if (sys_no >= SYSOPENFILE) {
        g_vfs_errno = E_VFS_INVAL;
        return;
    }

    /* Release the inode reference */
    inode *ino = g_sys_ofile[sys_no].f_inode;
    if (ino) {
        iput(ino);
    }

    /* Release the system open-file-table slot */
    if (g_sys_ofile[sys_no].f_count > 0) {
        g_sys_ofile[sys_no].f_count--;
    }

    /* Mark user file descriptor as free */
    g_user[uid].u_ofile[cfd] = SYSOPENFILE + 1;
}
