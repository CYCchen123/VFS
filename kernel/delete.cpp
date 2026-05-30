/**
 * delete.cpp - File deletion
 * Decrement di_number; iput handles actual reclamation
 * Layer 4 (Member C)
 */
#include <cstdio>
#include <cstring>
#include "filesys.h"

void delete_file(const char *filename)
{
    /* ---------- 1. Locate the directory entry ---------- */
    unsigned int di = namei(filename);

    if (di >= DIRNUM || g_dir.direct[di].d_ino == DIEMPTY) {
        g_vfs_errno = E_VFS_NOENT;
        printf("Error: %s\n", vfs_strerror(g_vfs_errno));
        return;
    }

    /* ---------- 2. Get the inode ---------- */
    inode *ino = iget(g_dir.direct[di].d_ino);
    if (!ino) {
        g_vfs_errno = E_VFS_IO;
        printf("Error: %s\n", vfs_strerror(g_vfs_errno));
        return;
    }

    /* ---------- 3. Decrement link count ---------- */
    if (ino->di_number > 0) {
        ino->di_number--;
    }
    ino->i_flag |= IUPDATE;

    /* ---------- 4. If no more links, clear the directory entry ---------- */
    if (ino->di_number == 0) {
        g_dir.direct[di].d_ino = DIEMPTY;
        /* Compact directory: shift remaining entries down */
        for (int i = di; i < g_dir.size - 1; i++) {
            g_dir.direct[i] = g_dir.direct[i + 1];
        }
        /* Clear the last (now duplicate) entry */
        memset(&g_dir.direct[g_dir.size - 1], 0, sizeof(direct));
        g_dir.size--;
    }

    /* ---------- 5. Release inode (iput reclaims blocks if di_number==0) ---------- */
    iput(ino);
}
