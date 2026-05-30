/**
 * install.cpp - System install / mount
 * Open virtual disk file, load superblock, init memory structures
 * Layer 1-2 (Member A)
 */
#include <cstdlib>
#include <cstring>

#include "filesys.h"

void install() {
  g_fd = fopen("filesystem", "r+b");
  if (!g_fd) {
    g_vfs_errno = E_VFS_IO;
    printf("Error: %s\n", vfs_strerror(E_VFS_IO));
    exit(1);
  }

  fseek(g_fd, BLOCKSIZ, SEEK_SET);
  fread(&g_filsys, sizeof(g_filsys), 1, g_fd);

  for (int i = 0; i < NHINO; i++) {
    g_hinode[i].i_forw = nullptr;
  }

  for (int i = 0; i < SYSOPENFILE; i++) {
    g_sys_ofile[i].f_count = 0;
    g_sys_ofile[i].f_inode = nullptr;
    g_sys_ofile[i].f_flag = 0;
    g_sys_ofile[i].f_off = 0;
  }

  for (int i = 0; i < USERNUM; i++) {
    g_user[i].u_uid = 0;
    g_user[i].u_gid = 0;
    g_user[i].u_default_mode = DEFAULTMODE;
    for (int j = 0; j < NOFILE; j++) {
      g_user[i].u_ofile[j] = SYSOPENFILE + 1;
    }
  }

  dinode pwd_inode;
  fseek(g_fd, DINODESTART + 3 * DINODESIZ, SEEK_SET);
  fread(&pwd_inode, sizeof(dinode), 1, g_fd);

  char block[BLOCKSIZ];
  memset(block, 0, BLOCKSIZ);
  fseek(g_fd, DATASTART + pwd_inode.di_addr[0] * BLOCKSIZ, SEEK_SET);
  fread(block, BLOCKSIZ, 1, g_fd);
  memcpy(g_pwd, block, sizeof(pwd) * PWDNUM);
  
  g_cur_path_inode = iget(1);
  if (g_cur_path_inode){
    memset(block, 0, BLOCKSIZ);
    fseek(g_fd, DATASTART + g_cur_path_inode->di_addr[0] * BLOCKSIZ, SEEK_SET);
    fread(block, BLOCKSIZ, 1, g_fd);
    memset(&g_dir, 0, sizeof(g_dir));
    int n = g_cur_path_inode->di_size / sizeof(direct);
    if (n > (int)(BLOCKSIZ / sizeof(direct))) n = BLOCKSIZ / sizeof(direct);
    memcpy(g_dir.direct, block, n * sizeof(direct));
    g_dir.size = n;
  }

  g_user_id = -1;

  printf("Install completed: virtual disk loaded and ready.\n");
}
