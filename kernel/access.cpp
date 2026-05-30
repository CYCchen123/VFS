/**
 * access.cpp - File access permission check
 * owner / group / other  x  r / w / x
 * Layer 4 (Member C)
 */
#include "filesys.h"

unsigned int access(unsigned int uid, inode *inode, unsigned short mode)
{
    /* uid is the user-table index; map to actual UID / GID */
    unsigned short user_uid = g_user[uid].u_uid;
    unsigned short user_gid = g_user[uid].u_gid;

    /* Root (uid == 0) bypasses all permission checks */
    if (user_uid == 0)
        return 1;

    unsigned short perm_mask = 0;

    if (user_uid == inode->di_uid) {
        /* Owner */
        switch (mode) {
        case READ_MODE:    perm_mask = UDIREAD;    break;
        case WRITE_MODE:   perm_mask = UDIWRITE;   break;
        case EXECUTE_MODE: perm_mask = UDIEXECUTE; break;
        default: return 0;
        }
    } else if (user_gid == inode->di_gid) {
        /* Group */
        switch (mode) {
        case READ_MODE:    perm_mask = GDIREAD;    break;
        case WRITE_MODE:   perm_mask = GDIWRITE;   break;
        case EXECUTE_MODE: perm_mask = GDIEXECUTE; break;
        default: return 0;
        }
    } else {
        /* Other */
        switch (mode) {
        case READ_MODE:    perm_mask = ODIREAD;    break;
        case WRITE_MODE:   perm_mask = ODIWRITE;   break;
        case EXECUTE_MODE: perm_mask = ODIEXECUTE; break;
        default: return 0;
        }
    }

    return (inode->di_mode & perm_mask) ? 1 : 0;
}
