/**
 * dir.cpp - Directory operations
 * _dir():  list current directory
 * mkdir(): create subdirectory
 * chdir(): change current directory
 * Layer 5 (Member B)
 */
#include <cstdio>
#include <cstring>

#include "filesys.h"

/* Maximum entries per directory block (512 / 16 = 32) */
#define DIRENTS_PER_BLOCK (BLOCKSIZ / sizeof(direct))

/* ---------- permissions string helper ---------- */
static void mode_str(unsigned short mode, char *out)
{
    out[0] = (mode & DIDIR) ? 'd' : '-';
    out[1] = (mode & UDIREAD)    ? 'r' : '-';
    out[2] = (mode & UDIWRITE)   ? 'w' : '-';
    out[3] = (mode & UDIEXECUTE) ? 'x' : '-';
    out[4] = (mode & GDIREAD)    ? 'r' : '-';
    out[5] = (mode & GDIWRITE)   ? 'w' : '-';
    out[6] = (mode & GDIEXECUTE) ? 'x' : '-';
    out[7] = (mode & ODIREAD)    ? 'r' : '-';
    out[8] = (mode & ODIWRITE)   ? 'w' : '-';
    out[9] = (mode & ODIEXECUTE) ? 'x' : '-';
    out[10] = '\0';
}

/* ---------- write g_dir back to an inode's data block ---------- */
static void writeback_dir(inode *ino)
{
    if (!ino) return;

    char block[BLOCKSIZ];
    memset(block, 0, BLOCKSIZ);

    int n = g_dir.size;
    if (n > (int)DIRENTS_PER_BLOCK) n = DIRENTS_PER_BLOCK;
    memcpy(block, g_dir.direct, n * sizeof(direct));

    fseek(g_fd, DATASTART + ino->di_addr[0] * BLOCKSIZ, SEEK_SET);
    fwrite(block, BLOCKSIZ, 1, g_fd);

    ino->di_size = n * sizeof(direct);
    ino->i_flag |= IUPDATE;
}

/* ---------- load directory content from an inode into g_dir ---------- */
static void load_dir(inode *ino)
{
    char block[BLOCKSIZ];
    memset(block, 0, BLOCKSIZ);
    memset(&g_dir, 0, sizeof(g_dir));

    if (!ino) return;

    fseek(g_fd, DATASTART + ino->di_addr[0] * BLOCKSIZ, SEEK_SET);
    fread(block, BLOCKSIZ, 1, g_fd);

    int n = ino->di_size / sizeof(direct);
    if (n > (int)DIRENTS_PER_BLOCK) n = DIRENTS_PER_BLOCK;
    memcpy(g_dir.direct, block, n * sizeof(direct));
    g_dir.size = n;
}

/* ================================================================== */

void _dir()
{
    printf("Directory listing  (size = %d entries)\n", g_dir.size);
    printf("%-16s %-10s %5s  %s\n", "Name", "Perms", "Size", "Blocks");

    for (int i = 0; i < g_dir.size; i++) {
        if (g_dir.direct[i].d_ino == 0) continue;

        inode *ino = iget(g_dir.direct[i].d_ino);
        if (!ino) continue;

        char perm[11];
        mode_str(ino->di_mode, perm);

        if (ino->di_mode & DIDIR) {
            printf("%-16s %-10s %5s  <dir>\n",
                   g_dir.direct[i].d_name, perm, "-");
        } else {
            /* Show block chain (only the entries that are actually used) */
            printf("%-16s %-10s %5u  ",
                   g_dir.direct[i].d_name, perm, ino->di_size);

            int blk_count = (ino->di_size + BLOCKSIZ - 1) / BLOCKSIZ;
            for (int b = 0; b < blk_count && b < NADDR; b++) {
                printf("%u ", ino->di_addr[b]);
            }
            printf("\n");
        }

        iput(ino);
    }
}

/* ================================================================== */

void mkdir(const char *dirname)
{
    /* Check for duplicate name */
    if (namei(dirname) != (unsigned int)-1) {
        printf("Error: '%s' already exists.\n", dirname);
        return;
    }

    /* Get empty directory slot and set name */
    unsigned short slot = iname(dirname);
    if (slot == 0 && g_dir.direct[0].d_ino != 0) {
        /* iname returned 0 but slot 0 is occupied → dir is full */
        return;
    }

    /* Allocate inode and data block */
    inode *ino = ialloc();
    if (!ino) return;

    unsigned int blk = balloc();
    if (blk == DISKFULL) {
        printf("Error: disk full.\n");
        ifree(ino->i_ino);
        iput(ino);
        return;
    }

    /* Write "." and ".." directory entries to the new data block */
    char block[BLOCKSIZ];
    memset(block, 0, BLOCKSIZ);
    direct *entry = (direct *)block;
    strcpy(entry[0].d_name, ".");
    entry[0].d_ino = ino->i_ino;
    strcpy(entry[1].d_name, "..");
    entry[1].d_ino = g_cur_path_inode ? g_cur_path_inode->i_ino : 1;

    fseek(g_fd, DATASTART + blk * BLOCKSIZ, SEEK_SET);
    fwrite(block, BLOCKSIZ, 1, g_fd);

    /* Set inode fields */
    ino->di_mode   = DIDIR | g_user[g_user_id].u_default_mode;
    ino->di_uid    = g_user[g_user_id].u_uid;
    ino->di_gid    = g_user[g_user_id].u_gid;
    ino->di_size   = sizeof(direct) * 2;
    ino->di_addr[0] = blk;
    ino->i_flag   |= IUPDATE;

    /* Update in-memory directory */
    g_dir.direct[slot].d_ino = ino->i_ino;
    if ((int)slot >= g_dir.size) {
        g_dir.size = slot + 1;
    }

    iput(ino);
    printf("Directory '%s' created.\n", dirname);
}

/* ================================================================== */

void chdir(const char *dirname)
{
    unsigned int idx = namei(dirname);
    if (idx == (unsigned int)-1) {
        printf("Error: '%s' not found.\n", dirname);
        return;
    }

    inode *target = iget(g_dir.direct[idx].d_ino);
    if (!target) return;

    if (!(target->di_mode & DIDIR)) {
        printf("Error: '%s' is not a directory.\n", dirname);
        iput(target);
        return;
    }

    /* Writeback current directory */
    writeback_dir(g_cur_path_inode);

    /* Switch to target */
    iput(g_cur_path_inode);
    g_cur_path_inode = target;

    /* Load target's content into g_dir */
    load_dir(target);

    printf("Changed to directory '%s'.\n", dirname);
}
