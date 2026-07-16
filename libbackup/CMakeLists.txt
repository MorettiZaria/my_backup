# libbackup — 共享核心静态库
# 包含所有算法实现和网络协议代码，供 client/server/gui 链接

add_library(libbackup STATIC
    # core
    src/core/FileInfo.cpp
    src/core/FileScanner.cpp
    src/core/BackupEngine.cpp
    src/core/RestoreEngine.cpp
    # pack
    src/pack/TarPackStrategy.cpp
    src/pack/IndexPackStrategy.cpp
    src/pack/PackManager.cpp
    # compress
    src/compress/RleCompressStrategy.cpp
    src/compress/HuffmanCompressStrategy.cpp
    src/compress/CompressManager.cpp
    # encrypt
    src/encrypt/XorEncryptStrategy.cpp
    src/encrypt/VigenereEncryptStrategy.cpp
    src/encrypt/EncryptManager.cpp
    # metadata
    src/metadata/MetadataSerializer.cpp
    src/metadata/MetadataStore.cpp
    # filter
    src/filter/FilterRule.cpp
    src/filter/CompositeFilter.cpp
    src/filter/FilterParser.cpp
    # network (shared protocol, socket, encryption, user auth)
    src/network/NetworkProtocol.cpp
    src/network/NetworkSocket.cpp
    src/network/TransportEncryptor.cpp
    src/network/UserManager.cpp
    src/network/NetworkBackupClient.cpp
    src/network/NetworkRestoreClient.cpp
    src/network/BackupServer.cpp
    src/network/ServerSession.cpp
    src/network/ServerConfig.cpp
    src/network/ServerStorage.cpp
    src/network/Logger.cpp
)

target_include_directories(libbackup PUBLIC include)
target_compile_options(libbackup PRIVATE -Wall -Wextra -Wpedantic)
