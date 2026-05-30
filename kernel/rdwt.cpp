/**
 * rdwt.cpp - File read / write operations
 * vfs_read():  read data from file (cross-block aware)
 * vfs_write(): write data to file (allocates blocks on demand)
 * Layer 4 (Member C)
 */
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include "filesys.h"

unsigned int vfs_read(unsigned int fd, char *buf, unsigned int size)
{
    /* ---------- 1. Validate file descriptor ---------- */
    if (fd >= NOFILE) {
        g_vfs_errno = E_VFS_INVAL;
        return 0;
    }

    unsigned short sys_no = g_user[g_user_id].u_ofile[fd];
    if (sys_no >= SYSOPENFILE) {
        g_vfs_errno = E_VFS_INVAL;
        return 0;
    }

    /* ---------- 2. Validate open mode ---------- */
    if (!(g_sys_ofile[sys_no].f_flag & FREAD)) {
        g_vfs_errno = E_VFS_NOPERM;
        return 0;
    }

    inode *ino = g_sys_ofile[sys_no].f_inode;
    if (!ino) {
        g_vfs_errno = E_VFS_IO;
        return 0;
    }

    /* ---------- 3. Clip size to available data ---------- */
    unsigned int off = g_sys_ofile[sys_no].f_off;
    if (off >= ino->di_size) {
        return 0;  /* EOF */
    }
    if (off + size > ino->di_size) {
        size = ino->di_size - off;
    }

    /* ---------- 4. Block-level read loop ---------- */
    unsigned int remaining = size;
    unsigned int total_read = 0;
    char *buf_ptr = buf;

    while (remaining > 0) {
        unsigned int block_index = off / BLOCKSIZ;
        unsigned int block_offset = off % BLOCKSIZ;

        if (block_index >= NADDR) break;          /* beyond max file size */
        unsigned int block_num = ino->di_addr[block_index];
        if (block_num == 0) break;                /* sparse / unallocated */

        unsigned int chunk = BLOCKSIZ - block_offset;
        if (chunk > remaining) chunk = remaining;

        if (fseek(g_fd, DATASTART + block_num * BLOCKSIZ + block_offset, SEEK_SET) != 0) {
            g_vfs_errno = E_VFS_IO;
            break;
        }
        size_t n = fread(buf_ptr, 1, chunk, g_fd);
        if (n == 0) break;  /* read error or unexpected EOF */

        buf_ptr    += n;
        off        += (unsigned int)n;
        remaining  -= (unsigned int)n;
        total_read += (unsigned int)n;

        if (n < chunk) break;  /* short read — stop */
    }

    /* ---------- 5. Update file offset ---------- */
    g_sys_ofile[sys_no].f_off = off;

    return total_read;
}

unsigned int vfs_write(unsigned int fd, const char *buf, unsigned int size)
{
    /* ---------- 1. Validate file descriptor ---------- */
    if (fd >= NOFILE) {
        g_vfs_errno = E_VFS_INVAL;
        return 0;
    }

    unsigned short sys_no = g_user[g_user_id].u_ofile[fd];
    if (sys_no >= SYSOPENFILE) {
        g_vfs_errno = E_VFS_INVAL;
        return 0;
    }

    /* ---------- 2. Validate open mode ---------- */
    if (!(g_sys_ofile[sys_no].f_flag & (FWRITE | FAPPEND))) {
        g_vfs_errno = E_VFS_NOPERM;
        return 0;
    }

    inode *ino = g_sys_ofile[sys_no].f_inode;
    if (!ino) {
        g_vfs_errno = E_VFS_IO;
        return 0;
    }

    /* ---------- 3. Block-level write loop ---------- */
    unsigned int off = g_sys_ofile[sys_no].f_off;
    unsigned int remaining = size;
    unsigned int total_written = 0;
    const char *buf_ptr = buf;

    while (remaining > 0) {
        unsigned int block_index = off / BLOCKSIZ;
        unsigned int block_offset = off % BLOCKSIZ;

        if (block_index >= NADDR) {
            g_vfs_errno = E_VFS_NOSPC;   /* file would exceed 10 direct blocks */
            break;
        }

        /* Allocate a new block if needed */
        unsigned int block_num = ino->di_addr[block_index];
        if (block_num == 0) {
            block_num = balloc();
            if (block_num == DISKFULL) {
                g_vfs_errno = E_VFS_NOSPC;
                break;
            }
            ino->di_addr[block_index] = block_num;
            ino->i_flag |= IUPDATE;
        }

        unsigned int chunk = BLOCKSIZ - block_offset;
        if (chunk > remaining) chunk = remaining;

        if (chunk < BLOCKSIZ) {
            /* Partial-block write: read-modify-write to preserve existing data */
            char temp[BLOCKSIZ];
            memset(temp, 0, BLOCKSIZ);
            fseek(g_fd, DATASTART + block_num * BLOCKSIZ, SEEK_SET);
            fread(temp, BLOCKSIZ, 1, g_fd);   /* may read 0 for freshly alloc'd block */
            memcpy(temp + block_offset, buf_ptr, chunk);
            fseek(g_fd, DATASTART + block_num * BLOCKSIZ, SEEK_SET);
            if (fwrite(temp, BLOCKSIZ, 1, g_fd) != 1) {
                g_vfs_errno = E_VFS_IO;
                break;
            }
        } else {
            /* Full-block write: write directly */
            fseek(g_fd, DATASTART + block_num * BLOCKSIZ, SEEK_SET);
            if (fwrite(buf_ptr, chunk, 1, g_fd) != 1) {
                g_vfs_errno = E_VFS_IO;
                break;
            }
        }

        buf_ptr       += chunk;
        off           += chunk;
        remaining     -= chunk;
        total_written += chunk;
    }

    /* ---------- 4. Update inode size and file offset ---------- */
    if (off > ino->di_size) {
        ino->di_size = off;
        ino->i_flag |= IUPDATE;
    }
    g_sys_ofile[sys_no].f_off = off;

    return total_written;
}
