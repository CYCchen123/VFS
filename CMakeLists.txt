cmake_minimum_required(VERSION 3.16)
project(VFS VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# 头文件路径
include_directories(${CMAKE_SOURCE_DIR}/include)

# ====== 源文件 ====== #

# Layer 1-2: 虚拟磁盘 + 块管理层 (成员A)
set(STORAGE_SOURCES
    kernel/format.cpp
    kernel/install.cpp
    kernel/balloc.cpp
    kernel/bfree.cpp
    kernel/ialloc.cpp
    kernel/ifree.cpp
    kernel/error.cpp
)

# Layer 3+5: Inode + 目录管理层 (成员B)
set(INODE_SOURCES
    kernel/igetput.cpp
    kernel/name.cpp
    kernel/dir.cpp
)

# Layer 4: 文件操作 + 访问控制 (成员C)
set(FILEOPS_SOURCES
    kernel/access.cpp
    kernel/creat.cpp
    kernel/open.cpp
    kernel/close.cpp
    kernel/delete.cpp
    kernel/rdwt.cpp
)

# Layer 6+7: 用户管理 + Shell (成员D)
set(USER_SOURCES
    user/log.cpp
    user/halt.cpp
    user/shell.cpp
    user/main.cpp
)

# 所有源文件
set(ALL_SOURCES
    ${STORAGE_SOURCES}
    ${INODE_SOURCES}
    ${FILEOPS_SOURCES}
    ${USER_SOURCES}
)

add_executable(vfs ${ALL_SOURCES})

# 平台相关设置
if(WIN32)
    # Windows: 控制台应用
    set_target_properties(vfs PROPERTIES
        WIN32_EXECUTABLE OFF
    )
endif()

# ====== Unit Tests ======
include(FetchContent)
FetchContent_Declare(
  Catch2
  GIT_REPOSITORY https://github.com/catchorg/Catch2.git
  GIT_TAG        v3.4.0
)
FetchContent_MakeAvailable(Catch2)

# Sources shared between main executable and tests (everything except main.cpp)
set(VFS_LIB_SOURCES
    ${STORAGE_SOURCES}
    ${INODE_SOURCES}
    ${FILEOPS_SOURCES}
    user/log.cpp
    user/halt.cpp
    user/shell.cpp
)

set(TEST_SOURCES
    test/test_globals.cpp
    test/test_format.cpp
    test/test_balloc_bfree.cpp
    test/test_ifree.cpp
    test/test_login.cpp
    test/test_inode_dir.cpp
    test/test_error.cpp
)

add_executable(vfs_test ${VFS_LIB_SOURCES} ${TEST_SOURCES})
target_link_libraries(vfs_test PRIVATE Catch2::Catch2WithMain)

include(CTest)
enable_testing()
include(Catch)
catch_discover_tests(vfs_test)
