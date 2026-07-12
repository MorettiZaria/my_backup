#ifndef BACKUP_INDEXPACKSTRATEGY_H
#define BACKUP_INDEXPACKSTRATEGY_H

#include "IPackStrategy.h"

/// 算法2：索引式打包（类 ZIP central directory）
/// 格式: [文件数据区...] [中央目录:N条Entry] [EOCD尾记录:32B]
class IndexPackStrategy : public IPackStrategy {
public:
    std::vector<uint8_t> pack(const std::string& baseDir,
                              const std::vector<FileInfo>& files) override;

    void unpack(const std::vector<uint8_t>& data,
                const std::string& destDir,
                std::vector<FileInfo>& outFiles) override;

    std::string name() const override { return "index"; }
    uint8_t algoId() const override { return 2; }

private:
    static constexpr uint32_t EOCD_SIGNATURE = 0x49445831; // "IDX1"

    struct DirEntry {
        std::string path;
        uint64_t    offset;       // 文件数据在数据区的偏移
        uint64_t    size;         // 文件数据大小
        uint16_t    fileType;
        uint16_t    permissions;
        uint32_t    uid;
        uint32_t    gid;
        uint64_t    mtime;
        std::string linkTarget;

        std::vector<uint8_t> serialize() const;
        static DirEntry deserialize(const uint8_t* data, size_t& offset);
    };

    void writeUint16(std::vector<uint8_t>& buf, uint16_t val);
    void writeUint32(std::vector<uint8_t>& buf, uint32_t val);
    void writeUint64(std::vector<uint8_t>& buf, uint64_t val);
    uint16_t readUint16(const uint8_t* data, size_t& offset);
    uint32_t readUint32(const uint8_t* data, size_t& offset);
    uint64_t readUint64(const uint8_t* data, size_t& offset);
};

#endif // BACKUP_INDEXPACKSTRATEGY_H
