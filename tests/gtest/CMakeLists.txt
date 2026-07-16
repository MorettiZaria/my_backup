# GTest 单元测试子项目
# 链接 libbackup 静态库，测试各模块的算法正确性

set(GTEST_SOURCES
    test_rle.cpp
    test_huffman.cpp
    test_encrypt.cpp
    test_metadata.cpp
    test_managers.cpp
    test_pack.cpp
    test_backup_restore.cpp
    test_network_nosocket.cpp
    test_filescanner.cpp
    test_metadata_store.cpp
    test_network_loopback.cpp
    test_server_session.cpp
    test_filter.cpp
)

add_executable(backup_gtest ${GTEST_SOURCES})
target_link_libraries(backup_gtest PRIVATE libbackup GTest::gtest GTest::gtest_main)
target_include_directories(backup_gtest PRIVATE
    ${PROJECT_SOURCE_DIR}/libbackup/include
    ${PROJECT_SOURCE_DIR}/libbackup/src
)
target_compile_options(backup_gtest PRIVATE -Wall -Wextra -Wpedantic)

# 注册到 CTest
add_test(NAME UnitTests COMMAND backup_gtest)
