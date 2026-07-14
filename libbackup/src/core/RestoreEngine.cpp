#include "core/RestoreEngine.h"
#include "core/BackupEngine.h"
#include "metadata/MetadataStore.h"
#include <iostream>
#include <fstream>
#include <algorithm>
#include <stdexcept>
#include <cstring>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

namespace {
    /// 仅当 errno 不是 EEXIST（目录已存在）时输出 warning
    void warnSyscall(const char* call, const std::string& path) {
        if (errno == EEXIST) return;
        if (errno == EPERM || errno == EACCES) return;  // 权限不足是预期的
        std::cerr << "Warning: " << call << " failed on '" << path
                  << "': " << std::strerror(errno) << std::endl;
    }
}

RestoreEngine::RestoreEngine() {}

void RestoreEngine::setPackManager(PackManager* mgr) {
    packMgr_ = mgr;
}

void RestoreEngine::setCompressManager(CompressManager* mgr) {
    compressMgr_ = mgr;
}

void RestoreEngine::setEncryptManager(EncryptManager* mgr) {
    encryptMgr_ = mgr;
}

bool RestoreEngine::run(const std::string& inputFile,
                        const std::string& destDir,
                        const std::string& password) {
    std::cout << "=== Restore ===" << std::endl;
    std::cout << "Input: " << inputFile << std::endl;
    std::cout << "Output: " << destDir << std::endl;

    // 1. 打开备份文件
    std::ifstream in(inputFile, std::ios::binary);
    if (!in) {
        std::cerr << "Error: cannot open input file: " << inputFile << std::endl;
        return false;
    }

    // 2. 读取头部
    MetadataStore metaStore;
    BackupHeader hdr = metaStore.loadHeader(in);
    if (hdr.magic != BackupHeader::MAGIC) {
        return false;
    }

    std::cout << "  Flags: pack=" << ((hdr.flags & BackupFlags::FLAG_PACK) ? "yes" : "no")
              << " compress=" << ((hdr.flags & BackupFlags::FLAG_COMPRESS) ? "yes" : "no")
              << " encrypt=" << ((hdr.flags & BackupFlags::FLAG_ENCRYPT) ? "yes" : "no")
              << std::endl;
    std::cout << "  Algos: packAlgo=" << static_cast<int>(hdr.packAlgo)
              << " compressAlgo=" << static_cast<int>(hdr.compressAlgo)
              << " encryptAlgo=" << static_cast<int>(hdr.encryptAlgo) << std::endl;

    // 3. 读取元数据
    std::vector<FileInfo> files = metaStore.loadMetadata(in, hdr.metaSize);
    std::cout << "  Metadata loaded: " << files.size() << " entries." << std::endl;

    // 4. 根据头部 algoId 选择策略
    IPackStrategy* packStrategy = nullptr;
    ICompressStrategy* compressStrategy = nullptr;
    IEncryptStrategy* encryptStrategy = nullptr;

    if (hdr.flags & BackupFlags::FLAG_PACK) {
        if (!packMgr_) {
            std::cerr << "Error: backup is packed but no PackManager provided." << std::endl;
            return false;
        }
        packStrategy = packMgr_->selectById(hdr.packAlgo);
        if (!packStrategy) {
            std::cerr << "Error: unknown pack algo ID " << static_cast<int>(hdr.packAlgo) << std::endl;
            return false;
        }
        std::cout << "  Unpack strategy: " << packStrategy->name() << std::endl;
    }

    if (hdr.flags & BackupFlags::FLAG_COMPRESS) {
        if (!compressMgr_) {
            std::cerr << "Error: backup is compressed but no CompressManager provided." << std::endl;
            return false;
        }
        compressStrategy = compressMgr_->selectById(hdr.compressAlgo);
        if (!compressStrategy) {
            std::cerr << "Error: unknown compress algo ID " << static_cast<int>(hdr.compressAlgo) << std::endl;
            return false;
        }
        std::cout << "  Decompress strategy: " << compressStrategy->name() << std::endl;
    }

    if (hdr.flags & BackupFlags::FLAG_ENCRYPT) {
        if (!encryptMgr_) {
            std::cerr << "Error: backup is encrypted but no EncryptManager provided." << std::endl;
            return false;
        }
        encryptStrategy = encryptMgr_->selectById(hdr.encryptAlgo);
        if (!encryptStrategy) {
            std::cerr << "Error: unknown encrypt algo ID " << static_cast<int>(hdr.encryptAlgo) << std::endl;
            return false;
        }
        std::cout << "  Decrypt strategy: " << encryptStrategy->name() << std::endl;
    }

    // 5. 读取 payload
    in.seekg(0, std::ios::end);
    size_t fileSize = static_cast<size_t>(in.tellg());
    in.seekg(0, std::ios::beg);

    size_t headerSize = 4 + 4 + 4 + 1 + 1 + 1 + 4 + hdr.metaSize;
    size_t payloadSize = fileSize - headerSize;

    std::vector<uint8_t> payload(payloadSize);
    in.seekg(static_cast<std::streamoff>(headerSize), std::ios::beg);
    in.read(reinterpret_cast<char*>(payload.data()), static_cast<std::streamsize>(payloadSize));
    in.close();

    std::cout << "  Payload read: " << payload.size() << " bytes." << std::endl;

    // 6. 逆向处理（根据 flags）
    if (hdr.flags & BackupFlags::FLAG_ENCRYPT) {
        std::cout << "  Decrypting..." << std::endl;
        payload = encryptStrategy->decrypt(payload, password);
        std::cout << "  Payload after decrypt: " << payload.size() << " bytes." << std::endl;
    }

    if (hdr.flags & BackupFlags::FLAG_COMPRESS) {
        std::cout << "  Decompressing..." << std::endl;
        payload = compressStrategy->decompress(payload);
        std::cout << "  Payload after decompress: " << payload.size() << " bytes." << std::endl;
    }

    std::vector<FileInfo> restoredFiles;

    if (hdr.flags & BackupFlags::FLAG_PACK) {
        std::cout << "  Unpacking..." << std::endl;
        packStrategy->unpack(payload, destDir, restoredFiles);
    } else {
        std::cout << "  Restoring from raw format..." << std::endl;
        if (!restoreFromRaw(payload, destDir, restoredFiles)) {
            return false;
        }
    }

    std::cout << "  Restored " << restoredFiles.size() << " file entries from archive." << std::endl;

    // 7. 按顺序恢复：先目录 → 再符号链接/特殊文件 → 再普通文件 → 最后恢复元数据

    auto mkdirRecursive = [](const std::string& path) {
        size_t pos = 0;
        while ((pos = path.find('/', pos + 1)) != std::string::npos) {
            std::string sub = path.substr(0, pos);
            if (mkdir(sub.c_str(), 0755) != 0 && errno != EEXIST) {
                std::cerr << "Warning: mkdir failed on '" << sub
                          << "': " << std::strerror(errno) << std::endl;
            }
        }
        if (mkdir(path.c_str(), 0755) != 0 && errno != EEXIST) {
            std::cerr << "Warning: mkdir failed on '" << path
                      << "': " << std::strerror(errno) << std::endl;
        }
    };

    // 7a. 创建所有目录（按路径排序）
    std::vector<const FileInfo*> dirs;
    for (const auto& info : files) {
        if (info.isDirectory()) dirs.push_back(&info);
    }
    std::sort(dirs.begin(), dirs.end(), [](const FileInfo* a, const FileInfo* b) {
        return a->relativePath < b->relativePath;
    });
    for (const auto* info : dirs) {
        std::string fullPath = destDir + "/" + info->relativePath;
        mkdirRecursive(fullPath);
    }

    // 7b. 创建符号链接
    for (const auto& info : files) {
        if (info.isSymlink()) {
            std::string fullPath = destDir + "/" + info.relativePath;
            size_t lastSlash = fullPath.rfind('/');
            if (lastSlash != std::string::npos) {
                mkdirRecursive(fullPath.substr(0, lastSlash));
            }
            unlink(fullPath.c_str());  // 失败也继续（文件可能不存在）
            if (symlink(info.symlinkTarget.c_str(), fullPath.c_str()) != 0) {
                warnSyscall("symlink", fullPath);
            }
        }
    }

    // 7c. 创建特殊文件（管道、设备）
    for (const auto& info : files) {
        std::string fullPath = destDir + "/" + info.relativePath;
        if (info.isFifo()) {
            size_t lastSlash = fullPath.rfind('/');
            if (lastSlash != std::string::npos) mkdirRecursive(fullPath.substr(0, lastSlash));
            unlink(fullPath.c_str());
            if (mkfifo(fullPath.c_str(), info.permissions) != 0) {
                warnSyscall("mkfifo", fullPath);
            }
        } else if (info.isDevice()) {
            size_t lastSlash = fullPath.rfind('/');
            if (lastSlash != std::string::npos) mkdirRecursive(fullPath.substr(0, lastSlash));
            unlink(fullPath.c_str());
            if (mknod(fullPath.c_str(), info.fileType | info.permissions, info.deviceId) != 0) {
                warnSyscall("mknod", fullPath);
            }
        }
    }

    // 7d. 写普通文件
    for (const auto& info : files) {
        if (!info.isRegular()) continue;

        std::string fullPath = destDir + "/" + info.relativePath;

        const FileInfo* rf = nullptr;
        for (const auto& r : restoredFiles) {
            if (r.relativePath == info.relativePath) {
                rf = &r;
                break;
            }
        }

        if (!rf) continue;

        size_t lastSlash = fullPath.rfind('/');
        if (lastSlash != std::string::npos) {
            mkdirRecursive(fullPath.substr(0, lastSlash));
        }

        std::ofstream fout(fullPath, std::ios::binary);
        if (fout && !rf->content.empty()) {
            fout.write(reinterpret_cast<const char*>(rf->content.data()),
                       static_cast<std::streamsize>(rf->content.size()));
        }
        fout.close();
    }

    // 8. 恢复元数据
    restoreMetadata(destDir, files);

    std::cout << "Restore complete!" << std::endl;
    return true;
}

bool RestoreEngine::restoreMetadata(const std::string& destDir,
                                    const std::vector<FileInfo>& files) {
    for (const auto& info : files) {
        std::string fullPath = destDir + "/" + info.relativePath;

        // chmod
        if (chmod(fullPath.c_str(), info.permissions) != 0) {
            warnSyscall("chmod", fullPath);
        }

        // chown（通常需要 root 权限，非 root 失败是预期的）
        if (lchown(fullPath.c_str(), info.owner, info.group) != 0 && errno != EPERM) {
            warnSyscall("lchown", fullPath);
        }

        // utimensat：设置 atime 和 mtime
        struct timespec times[2];
        times[0].tv_sec  = info.atime;
        times[0].tv_nsec = 0;
        times[1].tv_sec  = info.mtime;
        times[1].tv_nsec = 0;
        if (utimensat(AT_FDCWD, fullPath.c_str(), times, AT_SYMLINK_NOFOLLOW) != 0) {
            warnSyscall("utimensat", fullPath);
        }
    }

    return true;
}

bool RestoreEngine::restoreFromRaw(const std::vector<uint8_t>& data,
                                   const std::string& /*destDir*/,
                                   std::vector<FileInfo>& outFiles) {
    size_t offset = 0;

    auto r32 = [&]() -> uint32_t {
        if (offset + 4 > data.size()) throw std::runtime_error("restoreFromRaw: unexpected end of data at offset");
        uint32_t v = 0;
        for (int i = 0; i < 4; ++i) v |= static_cast<uint32_t>(data[offset+i]) << (i*8);
        offset += 4; return v;
    };
    auto r16 = [&]() -> uint16_t {
        if (offset + 2 > data.size()) throw std::runtime_error("restoreFromRaw: unexpected end of data at offset");
        uint16_t v = static_cast<uint16_t>(data[offset]) |
                     (static_cast<uint16_t>(data[offset+1]) << 8);
        offset += 2; return v;
    };
    auto r64 = [&]() -> uint64_t {
        if (offset + 8 > data.size()) throw std::runtime_error("restoreFromRaw: unexpected end of data at offset");
        uint64_t v = 0;
        for (int i = 0; i < 8; ++i) v |= static_cast<uint64_t>(data[offset+i]) << (i*8);
        offset += 8; return v;
    };

    if (data.size() < 4) return false;

    uint32_t fileCount = r32();
    // 合理性检查：文件数不应超过 100 万
    if (fileCount > 1000000) {
        std::cerr << "Error: invalid file count in restoreFromRaw (" << fileCount << "), data may be corrupted." << std::endl;
        return false;
    }
    for (uint32_t i = 0; i < fileCount; ++i) {
        if (offset + 2 > data.size()) return false;
        uint16_t pathLen = r16();
        if (offset + pathLen > data.size()) return false;
        std::string path(reinterpret_cast<const char*>(&data[offset]), pathLen);
        offset += pathLen;
        if (offset + 8 > data.size()) return false;
        uint64_t contentLen = r64();
        if (offset + contentLen > data.size()) return false;

        FileInfo info;
        info.relativePath = path;
        info.fileType     = S_IFREG;
        if (contentLen > 0) {
            info.content.assign(data.begin() + static_cast<ptrdiff_t>(offset),
                                data.begin() + static_cast<ptrdiff_t>(offset + contentLen));
        }
        info.fileSize = static_cast<off_t>(contentLen);
        offset += contentLen;

        outFiles.push_back(std::move(info));
    }

    return true;
}
