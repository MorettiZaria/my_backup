#include "core/BackupEngine.h"
#include "core/FileScanner.h"
#include "metadata/MetadataSerializer.h"
#include "metadata/MetadataStore.h"
#include <iostream>
#include <fstream>

BackupEngine::BackupEngine() {}

void BackupEngine::setPackStrategy(IPackStrategy* strategy) {
    packStrategy_ = strategy;
}

void BackupEngine::setCompressStrategy(ICompressStrategy* strategy) {
    compressStrategy_ = strategy;
}

void BackupEngine::setEncryptStrategy(IEncryptStrategy* strategy) {
    encryptStrategy_ = strategy;
}

std::vector<uint8_t> BackupEngine::concatFiles(const std::string& /*baseDir*/,
                                               const std::vector<FileInfo>& files) {
    // 简单拼接：遍历所有普通文件，用分隔符隔开
    // 格式: [文件数:4B]
    //       [pathLen:2B][path:N B][contentLen:8B][content:N B] ...
    std::vector<uint8_t> result;

    auto w32 = [&](uint32_t v) {
        for (int i = 0; i < 4; ++i) result.push_back((v >> (i*8)) & 0xFF);
    };
    auto w16 = [&](uint16_t v) {
        result.push_back(v & 0xFF);
        result.push_back((v >> 8) & 0xFF);
    };
    auto w64 = [&](uint64_t v) {
        for (int i = 0; i < 8; ++i) result.push_back((v >> (i*8)) & 0xFF);
    };

    uint32_t fileCount = 0;
    for (const auto& f : files) {
        if (f.isRegular() && !f.content.empty()) ++fileCount;
    }
    w32(fileCount);

    for (const auto& f : files) {
        if (f.isRegular() && !f.content.empty()) {
            w16(static_cast<uint16_t>(f.relativePath.size()));
            result.insert(result.end(), f.relativePath.begin(), f.relativePath.end());
            w64(static_cast<uint64_t>(f.content.size()));
            result.insert(result.end(), f.content.begin(), f.content.end());
        }
    }

    return result;
}

bool BackupEngine::run(const std::string& sourceDir,
                       const std::string& outputFile,
                       const std::string& password) {
    std::cout << "=== Backup ===" << std::endl;
    std::cout << "Source: " << sourceDir << std::endl;
    std::cout << "Output: " << outputFile << std::endl;

    // 1. 扫描源目录
    std::cout << "Scanning directory tree..." << std::endl;
    FileScanner scanner;
    std::vector<FileInfo> files = scanner.scan(sourceDir);
    std::cout << "  Found " << files.size() << " entries." << std::endl;

    // 1b. 给所有相对路径加上源目录的 basename 前缀
    //     这样还原时会在目标目录下创建同名顶级文件夹
    std::string srcBase;
    size_t lastSlash = sourceDir.rfind('/');
    if (lastSlash != std::string::npos) {
        srcBase = sourceDir.substr(lastSlash + 1);
    } else {
        srcBase = sourceDir;  // 没有斜杠，直接就是目录名
    }
    if (!srcBase.empty() && srcBase != ".") {
        for (auto& f : files) {
            f.relativePath = srcBase + "/" + f.relativePath;
        }
        std::cout << "  Prefixed paths with: " << srcBase << "/" << std::endl;
    }

    // 2. 序列化元数据
    MetadataSerializer serializer;
    std::vector<uint8_t> metaBytes = serializer.serialize(files);
    std::cout << "  Metadata: " << metaBytes.size() << " bytes." << std::endl;

    // 3. 数据准备
    uint32_t flags = 0;
    uint8_t packAlgo = 0, compressAlgo = 0, encryptAlgo = 0;
    std::vector<uint8_t> payload;

    if (packStrategy_) {
        std::cout << "  Packing with: " << packStrategy_->name() << std::endl;
        payload = packStrategy_->pack(sourceDir, files);
        flags |= BackupFlags::FLAG_PACK;
        packAlgo = packStrategy_->algoId();
    } else {
        std::cout << "  No packing (raw concat)." << std::endl;
        payload = concatFiles(sourceDir, files);
    }

    std::cout << "  Payload after pack: " << payload.size() << " bytes." << std::endl;

    // 4. 压缩（可选）
    if (compressStrategy_) {
        std::cout << "  Compressing with: " << compressStrategy_->name() << std::endl;
        payload = compressStrategy_->compress(payload);
        flags |= BackupFlags::FLAG_COMPRESS;
        compressAlgo = compressStrategy_->algoId();
        std::cout << "  Payload after compress: " << payload.size() << " bytes." << std::endl;
    }

    // 5. 加密（可选）
    if (encryptStrategy_) {
        std::cout << "  Encrypting with: " << encryptStrategy_->name() << std::endl;
        payload = encryptStrategy_->encrypt(payload, password);
        flags |= BackupFlags::FLAG_ENCRYPT;
        encryptAlgo = encryptStrategy_->algoId();
        std::cout << "  Payload after encrypt: " << payload.size() << " bytes." << std::endl;
    }

    // 6. 写入文件
    std::ofstream out(outputFile, std::ios::binary);
    if (!out) {
        std::cerr << "Error: cannot create output file: " << outputFile << std::endl;
        return false;
    }

    MetadataStore metaStore;
    metaStore.saveHeader(out, files, flags, packAlgo, compressAlgo, encryptAlgo);

    out.write(reinterpret_cast<const char*>(payload.data()),
              static_cast<std::streamsize>(payload.size()));
    out.close();

    std::cout << "Backup complete!" << std::endl;
    std::cout << "  Files: " << files.size() << std::endl;
    std::cout << "  Total output size: " << payload.size() << " bytes" << std::endl;

    return true;
}
