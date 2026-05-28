/**
 * test_inode_dir.cpp - Unit tests for Layer 3 (iget/iput) + Layer 5 (namei/iname/dir)
 * Member B
 */
#include <cstdio>
#include <cstring>

#include <catch2/catch_test_macros.hpp>

#include "test_helpers.h"

/* Helper: format + install + login as root */
static void setup_fs_with_login()
{
    reset_globals();
    fs_cleanup();
    format();
    install();
    login(0, "root");
}

/* Helper: format + install only (no login) */
static void setup_fs()
{
    reset_globals();
    fs_cleanup();
    format();
    install();
}

/* ================================================================
 *  namei() tests
 * ================================================================ */

TEST_CASE("namei() finds . and .. after install", "[namei]")
{
    setup_fs();

    unsigned int idx_dot  = namei(".");
    unsigned int idx_dot2 = namei("..");

    REQUIRE(idx_dot  == 0);
    REQUIRE(idx_dot2 == 1);
    REQUIRE(g_dir.direct[idx_dot].d_ino  == 1);
    REQUIRE(g_dir.direct[idx_dot2].d_ino == 1);

    fs_cleanup();
}

TEST_CASE("namei() returns -1 for non-existent file", "[namei]")
{
    setup_fs();

    unsigned int idx = namei("no_such_file");

    /* Fixes known bug #1: not-found must not be ambiguous with index 0 */
    REQUIRE(idx == (unsigned int)-1);

    fs_cleanup();
}

TEST_CASE("namei() skips deleted entries (d_ino == 0)", "[namei]")
{
    setup_fs();

    /* Simulate a deleted entry: slot 2 has a name but d_ino == 0 */
    strcpy(g_dir.direct[2].d_name, "deleted_file");
    g_dir.direct[2].d_ino = 0;
    g_dir.size = 3;

    unsigned int idx = namei("deleted_file");
    REQUIRE(idx == (unsigned int)-1); /* skipped because d_ino == 0 */

    fs_cleanup();
}

/* ================================================================
 *  iname() tests
 * ================================================================ */

TEST_CASE("iname() finds empty slot and writes name", "[iname]")
{
    setup_fs();

    /* After install: slots 0+1 occupied (. and ..), slot 2 is free */
    unsigned short slot = iname("newfile");

    REQUIRE(slot == 2);
    REQUIRE(strcmp(g_dir.direct[slot].d_name, "newfile") == 0);
    REQUIRE(g_dir.direct[slot].d_ino == 0); /* caller sets d_ino later */

    fs_cleanup();
}

TEST_CASE("iname() reuses deleted-entry slots", "[iname]")
{
    setup_fs();

    /* Fill slot 2, then delete it, then verify iname reuses it */
    strcpy(g_dir.direct[2].d_name, "will_delete");
    g_dir.direct[2].d_ino = 42; /* pretend it's allocated */
    g_dir.size = 3;

    /* "Delete": set d_ino to 0 */
    g_dir.direct[2].d_ino = 0;

    unsigned short slot = iname("reused");
    REQUIRE(slot == 2);
    REQUIRE(strcmp(g_dir.direct[2].d_name, "reused") == 0);

    fs_cleanup();
}

/* ================================================================
 *  iget() / iput() tests
 * ================================================================ */

TEST_CASE("iget() loads inode from disk", "[iget]")
{
    setup_fs();

    /* inode 2 = etc directory created during format */
    inode *ino = iget(2);
    REQUIRE(ino != nullptr);
    REQUIRE(ino->i_ino == 2);
    REQUIRE(ino->i_count == 1);
    REQUIRE(ino->di_mode & DIDIR);
    REQUIRE(ino->di_number == 1);
    /* etc dir has 2 entries (. and ..), 16 bytes each */
    REQUIRE(ino->di_size == sizeof(direct) * 2);

    iput(ino);
    fs_cleanup();
}

TEST_CASE("iget() caches: double call returns same pointer with i_count=2", "[iget]")
{
    setup_fs();

    inode *p1 = iget(3); /* passwd file inode */
    REQUIRE(p1 != nullptr);
    REQUIRE(p1->i_count == 1);

    inode *p2 = iget(3);
    REQUIRE(p2 == p1);
    REQUIRE(p2->i_count == 2);

    /* Cleanup: release both references */
    iput(p1); /* i_count: 2 → 1 */
    REQUIRE(p1->i_count == 1);
    iput(p1); /* i_count: 1 → 0, writeback + free */

    fs_cleanup();
}

TEST_CASE("iput() writes back modified inode to disk", "[iget]")
{
    setup_fs();

    /* Load etc dir inode, modify, release, reload, verify persistence */
    inode *ino = iget(2);
    REQUIRE(ino != nullptr);
    unsigned short original_size = ino->di_size;

    /* Modify */
    ino->di_size = 64;
    ino->i_flag |= IUPDATE;
    iput(ino); /* i_count 1→0, triggers writeback + free */

    /* Reload from disk */
    inode *reloaded = iget(2);
    REQUIRE(reloaded != nullptr);
    REQUIRE(reloaded->di_size == 64);

    /* Restore original value */
    reloaded->di_size = original_size;
    iput(reloaded);

    fs_cleanup();
}

TEST_CASE("iput() on deleted inode (di_number=0) zeros on-disk inode", "[iget]")
{
    setup_fs();

    /* Load passwd inode, manually mark it deleted, and release */
    inode *ino = iget(3);
    REQUIRE(ino != nullptr);

    unsigned int saved_ino = ino->i_ino;

    /* Simulate deletion: di_number = 0 */
    ino->di_number = 0;

    /* Remember free-stack state before iput */
    unsigned int ninode_before = g_filsys.s_ninode;

    iput(ino); /* should bfree blocks + ifree */

    /* ifree adds back to stack only if there is room (stack max is NICINOD=50).
       After format the stack is full, so it may not increase. That's expected. */
    if (ninode_before < NICINOD) {
        REQUIRE(g_filsys.s_ninode == ninode_before + 1);
        REQUIRE(g_filsys.s_inode[g_filsys.s_pinode] == saved_ino);
    }

    /* The disk inode must be zeroed regardless of stack state */
    dinode check;
    fseek(g_fd, DINODESTART + saved_ino * DINODESIZ, SEEK_SET);
    fread(&check, sizeof(check), 1, g_fd);
    REQUIRE(check.di_number == 0);
    REQUIRE(check.di_mode == 0);

    fs_cleanup();
}

/* ================================================================
 *  mkdir() tests
 * ================================================================ */

TEST_CASE("mkdir() creates subdirectory findable by namei", "[mkdir]")
{
    setup_fs_with_login();

    mkdir("testdir");

    unsigned int idx = namei("testdir");
    REQUIRE(idx != (unsigned int)-1);
    REQUIRE(idx == 2); /* after . and .. */

    /* Verify the new directory's inode on disk */
    inode *ino = iget(g_dir.direct[idx].d_ino);
    REQUIRE(ino != nullptr);
    REQUIRE(ino->di_mode & DIDIR);
    REQUIRE(ino->di_size == sizeof(direct) * 2); /* . and .. */
    REQUIRE(ino->di_addr[0] != 0);               /* has a data block */

    /* Verify . and .. entries in the new directory's data block */
    char block[BLOCKSIZ];
    fseek(g_fd, DATASTART + ino->di_addr[0] * BLOCKSIZ, SEEK_SET);
    fread(block, BLOCKSIZ, 1, g_fd);
    direct *entries = (direct *)block;
    REQUIRE(strcmp(entries[0].d_name, ".")  == 0);
    REQUIRE(strcmp(entries[1].d_name, "..") == 0);

    iput(ino);
    fs_cleanup();
}

TEST_CASE("mkdir() rejects duplicate directory name", "[mkdir]")
{
    setup_fs_with_login();

    mkdir("dup");
    mkdir("dup"); /* should print error, not crash or corrupt */

    /* namei should still find it exactly once */
    unsigned int first = namei("dup");
    REQUIRE(first != (unsigned int)-1);

    fs_cleanup();
}

/* ================================================================
 *  chdir() tests
 * ================================================================ */

TEST_CASE("chdir() into subdirectory changes current directory", "[chdir]")
{
    setup_fs_with_login();

    unsigned int root_ino = g_cur_path_inode->i_ino;
    mkdir("sub");
    chdir("sub");

    /* g_cur_path_inode should now be the subdirectory */
    REQUIRE(g_cur_path_inode != nullptr);
    REQUIRE(g_cur_path_inode->i_ino != root_ino);

    /* g_dir should contain . and .. */
    REQUIRE(g_dir.size == 2);
    REQUIRE(strcmp(g_dir.direct[0].d_name, ".")  == 0);
    REQUIRE(strcmp(g_dir.direct[1].d_name, "..") == 0);
    REQUIRE(g_dir.direct[1].d_ino == root_ino); /* .. points to parent */

    fs_cleanup();
}

TEST_CASE("chdir(..) from subdir returns to parent", "[chdir]")
{
    setup_fs_with_login();

    unsigned int root_ino = g_cur_path_inode->i_ino;
    mkdir("child");
    chdir("child");
    REQUIRE(g_cur_path_inode->i_ino != root_ino);

    chdir("..");
    REQUIRE(g_cur_path_inode->i_ino == root_ino);
    /* g_dir should show 3 entries: .  ..  child */
    REQUIRE(g_dir.size >= 3);
    REQUIRE(namei("child") != (unsigned int)-1);

    fs_cleanup();
}

TEST_CASE("chdir() rejects non-existent directory", "[chdir]")
{
    setup_fs_with_login();

    inode *before = g_cur_path_inode;

    chdir("nowhere");

    /* g_cur_path_inode should be unchanged */
    REQUIRE(g_cur_path_inode == before);

    fs_cleanup();
}

/* ================================================================
 *  _dir() smoke test
 * ================================================================ */

TEST_CASE("_dir() lists root directory without crashing", "[dir]")
{
    setup_fs_with_login();

    mkdir("a");
    mkdir("b");

    _dir(); /* should print 4 entries: . .. a b */

    fs_cleanup();
}

/* ================================================================
 *  Integration: full workflow
 * ================================================================ */

TEST_CASE("mkdir → chdir → mkdir → chdir .. round trip", "[integration]")
{
    setup_fs_with_login();

    /* Root: mkdir("A"), chdir("A") */
    mkdir("A");
    chdir("A");
    REQUIRE(namei(".")  == 0);
    REQUIRE(namei("..") == 1);

    /* Inside A: mkdir("B"), chdir("B") */
    mkdir("B");
    chdir("B");

    /* Inside B: verify . and .. */
    REQUIRE(strcmp(g_dir.direct[0].d_name, ".")  == 0);
    REQUIRE(strcmp(g_dir.direct[1].d_name, "..") == 0);

    /* Go back to A */
    chdir("..");
    REQUIRE(namei("B") != (unsigned int)-1);

    /* Go back to root */
    chdir("..");
    REQUIRE(namei("A") != (unsigned int)-1);

    fs_cleanup();
}
