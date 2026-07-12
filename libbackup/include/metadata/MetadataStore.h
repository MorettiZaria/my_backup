#ifndef BACKUP_METADATASTORE_H
#define BACKUP_METADATASTORE_H

#include "core/FileInfo.h"
#include "MetadataSerializer.h"
#include <vector>
#include <cstdint>
#include <string>

/**
 * 在备份文件头部读写元数据块，包含魔数和版本校验。
 *
 * 备份文件格式:
 * [MAGIC:4B][VERSION:4B][flags:4B][packAlgo:1B][compressAlgo:1B][encryptAlgo:1B]
 * [metaSize:4B][metaBytes:N B]
 * [payload...]
 */
struct BackupHeader {
    static constexpr uint32_t MAGIC   = 0x424B5550; // "BKUP"
    static constexpr uint32_t VERSION = 1;

    uint32_t magic;
    uint32_t version;
    uint32_t flags;          // bit0=PACK, bit1=COMPRESS, bit2=ENCRYPT
    uint8_t  packAlgo;       // 0=无, 1=tar, 2=index
    uint8_t  compressAlgo;   // 0=无, 1=rle, 2=huffman
    uint8_t  encryptAlgo;    // 0=无, 1=xor, 2=vigenere
    uint32_t metaSize;       // 元数据块字节数
};

class MetadataStore {
public:
    MetadataStore();

    /// 将元数据写入输出流头部
    void saveHeader(std::ostream& out,
                    const std::vector<FileInfo>& files,
                    uint32_t flags,
                    uint8_t packAlgo,
                    uint8_t compressAlgo,
                    uint8_t encryptAlgo);

    /// 从输入流读取头部信息
    BackupHeader loadHeader(std::istream& in);

    /// 从输入流读取元数据（需先调用 loadHeader）
    std::vector<FileInfo> loadMetadata(std::istream& in, uint32_t metaSize);

private:
    MetadataSerializer serializer_;
};

#endif // BACKUP_METADATASTORE_H
