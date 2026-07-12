#ifndef BACKUP_TARPACKSTRATEGY_H
#define BACKUP_TARPACKSTRATEGY_H

#include "IPackStrategy.h"

/// 算法1：类 TAR 顺序打包
/// 格式: [FileHeader(512B)] [文件数据(512B对齐)] ... [全零1024B结束标记]
class TarPackStrategy : public IPackStrategy {
public:
    std::vector<uint8_t> pack(const std::string& baseDir,
                              const std::vector<FileInfo>& files) override;

    void unpack(const std::vector<uint8_t>& data,
                const std::string& destDir,
                std::vector<FileInfo>& outFiles) override;

    std::string name() const override { return "tar"; }
    uint8_t algoId() const override { return 1; }

private:
    static constexpr size_t HEADER_SIZE = 512;
    static constexpr size_t BLOCK_SIZE  = 512;

    /// 构造一个 tar header block
    std::vector<uint8_t> buildHeader(const FileInfo& info);

    /// 解析一个 tar header block，返回 FileInfo（不含 content）
    FileInfo parseHeader(const uint8_t* data);

    /// 写入八进制数字字符串到缓冲区
    void writeOctal(char* buf, size_t len, uint64_t val);

    /// 从八进制数字字符串解析数值
    uint64_t readOctal(const char* buf, size_t len);
};

#endif // BACKUP_TARPACKSTRATEGY_H
