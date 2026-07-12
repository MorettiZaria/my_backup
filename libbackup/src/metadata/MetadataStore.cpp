#include "metadata/MetadataStore.h"
#include <iostream>
#include <cstring>

MetadataStore::MetadataStore() {}

void MetadataStore::saveHeader(std::ostream& out,
                               const std::vector<FileInfo>& files,
                               uint32_t flags,
                               uint8_t packAlgo,
                               uint8_t compressAlgo,
                               uint8_t encryptAlgo) {
    // 序列化元数据
    std::vector<uint8_t> metaBytes = serializer_.serialize(files);

    // 写入头部
    auto writeU32 = [&](uint32_t v) {
        out.write(reinterpret_cast<const char*>(&v), 4);
    };
    auto writeU8 = [&](uint8_t v) {
        out.write(reinterpret_cast<const char*>(&v), 1);
    };

    writeU32(BackupHeader::MAGIC);
    writeU32(BackupHeader::VERSION);
    writeU32(flags);
    writeU8(packAlgo);
    writeU8(compressAlgo);
    writeU8(encryptAlgo);
    writeU32(static_cast<uint32_t>(metaBytes.size()));

    // 写入元数据块
    out.write(reinterpret_cast<const char*>(metaBytes.data()),
              static_cast<std::streamsize>(metaBytes.size()));
}

BackupHeader MetadataStore::loadHeader(std::istream& in) {
    BackupHeader hdr{};

    auto readU32 = [&]() -> uint32_t {
        uint32_t v = 0;
        in.read(reinterpret_cast<char*>(&v), 4);
        return v;
    };
    auto readU8 = [&]() -> uint8_t {
        uint8_t v = 0;
        in.read(reinterpret_cast<char*>(&v), 1);
        return v;
    };

    hdr.magic   = readU32();
    hdr.version = readU32();

    if (hdr.magic != BackupHeader::MAGIC) {
        std::cerr << "Error: invalid magic number. Not a backup file." << std::endl;
        return hdr;
    }
    if (hdr.version != BackupHeader::VERSION) {
        std::cerr << "Error: unsupported version " << hdr.version << std::endl;
        return hdr;
    }

    hdr.flags        = readU32();
    hdr.packAlgo     = readU8();
    hdr.compressAlgo = readU8();
    hdr.encryptAlgo  = readU8();
    hdr.metaSize     = readU32();

    return hdr;
}

std::vector<FileInfo> MetadataStore::loadMetadata(std::istream& in, uint32_t metaSize) {
    std::vector<uint8_t> metaBytes(metaSize);
    in.read(reinterpret_cast<char*>(metaBytes.data()),
            static_cast<std::streamsize>(metaSize));
    return serializer_.deserialize(metaBytes);
}
