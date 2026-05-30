/**
 * name.cpp - Directory entry lookup
 * namei(): search dir entry by name (fixes bug #1: return -1 for not-found)
 * iname(): find empty dir entry slot
 * Layer 5 (Member B)
 */
#include <cstdio>
#include <cstring>

#include "filesys.h"

unsigned int namei(const char *name)
{
    for (int i = 0; i < g_dir.size; i++) {
        if (strcmp(g_dir.direct[i].d_name, name) == 0
            && g_dir.direct[i].d_ino != 0) {
            return (unsigned int)i;
        }
    }
    /* Fixes known bug #1: return unambiguous sentinel instead of 0 */
    return (unsigned int)-1;
}

unsigned short iname(const char *name)
{
    for (int i = 0; i < DIRNUM; i++) {
        if (g_dir.direct[i].d_ino == 0) {
            strcpy(g_dir.direct[i].d_name, name);
            return (unsigned short)i;
        }
    }
    printf("Error: directory full (max %d entries).\n", DIRNUM);
    return 0;
}
