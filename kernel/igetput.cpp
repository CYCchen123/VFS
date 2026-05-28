/**
 * igetput.cpp - Memory inode get / put
 * iget(): hash lookup + disk load
 * iput(): refcount --, writeback + free when zero
 * Layer 3 (Member B)
 */
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "filesys.h"

inode *iget(unsigned int dinodeid)
{
    unsigned int hash = dinodeid % NHINO;

    /* Walk hash chain for cached inode */
    inode *p = g_hinode[hash].i_forw;
    while (p) {
        if (p->i_ino == dinodeid) {
            p->i_count++;
            return p;
        }
        p = p->i_forw;
    }

    /* Miss: allocate new inode, load from disk */
    inode *new_inode = (inode *)malloc(sizeof(inode));
    if (!new_inode) {
        printf("Error: malloc inode failed.\n");
        return nullptr;
    }
    memset(new_inode, 0, sizeof(inode));

    dinode disk_inode;
    fseek(g_fd, DINODESTART + dinodeid * DINODESIZ, SEEK_SET);
    fread(&disk_inode, sizeof(dinode), 1, g_fd);

    new_inode->di_number = disk_inode.di_number;
    new_inode->di_mode   = disk_inode.di_mode;
    new_inode->di_uid    = disk_inode.di_uid;
    new_inode->di_gid    = disk_inode.di_gid;
    new_inode->di_size   = disk_inode.di_size;
    memcpy(new_inode->di_addr, disk_inode.di_addr, sizeof(disk_inode.di_addr));

    new_inode->i_ino   = dinodeid;
    new_inode->i_count = 1;
    new_inode->i_flag  = 0;

    /* Insert at head of hash chain */
    new_inode->i_forw = g_hinode[hash].i_forw;
    new_inode->i_back = (inode *)&g_hinode[hash];
    if (g_hinode[hash].i_forw) {
        g_hinode[hash].i_forw->i_back = new_inode;
    }
    g_hinode[hash].i_forw = new_inode;

    return new_inode;
}

void iput(inode *pinode)
{
    if (!pinode) return;

    pinode->i_count--;
    if (pinode->i_count > 0) return;

    /* i_count == 0: last reference released */
    if (pinode->di_number == 0) {
        /* File deleted: reclaim all data blocks */
        for (int i = 0; i < NADDR; i++) {
            if (pinode->di_addr[i] != 0) {
                bfree(pinode->di_addr[i]);
                pinode->di_addr[i] = 0;
            }
        }
        /* Reclaim disk inode */
        ifree(pinode->i_ino);

        /* Zero the on-disk inode to mark it free */
        dinode empty;
        memset(&empty, 0, sizeof(empty));
        fseek(g_fd, DINODESTART + pinode->i_ino * DINODESIZ, SEEK_SET);
        fwrite(&empty, sizeof(empty), 1, g_fd);
    } else {
        /* File still exists: writeback to disk */
        dinode disk_inode;
        memset(&disk_inode, 0, sizeof(disk_inode));
        disk_inode.di_number = pinode->di_number;
        disk_inode.di_mode   = pinode->di_mode;
        disk_inode.di_uid    = pinode->di_uid;
        disk_inode.di_gid    = pinode->di_gid;
        disk_inode.di_size   = pinode->di_size;
        memcpy(disk_inode.di_addr, pinode->di_addr, sizeof(disk_inode.di_addr));

        fseek(g_fd, DINODESTART + pinode->i_ino * DINODESIZ, SEEK_SET);
        fwrite(&disk_inode, sizeof(disk_inode), 1, g_fd);
    }

    /* Remove from hash chain */
    /* Forward link: pinode->i_back points to predecessor (inode or hinode);
       hinode only has i_forw, which is at the same offset as inode::i_forw */
    if (pinode->i_back) {
        pinode->i_back->i_forw = pinode->i_forw;
    }
    if (pinode->i_forw) {
        pinode->i_forw->i_back = pinode->i_back;
    }

    free(pinode);
}
