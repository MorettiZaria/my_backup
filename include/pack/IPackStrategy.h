#ifndef BACKUP_IPACKSTRATEGY_H
#define BACKUP_IPACKSTRATEGY_H

#include "core/FileInfo.h"
#include <vector>
#include <string>
#include <cstdint>

/// 打包策略抽象接口
class IPackStrategy {
public:
    virtual ~IPackStrategy() = default;

    /// 将多个文件打包为一个字节流
    virtual std::vector<uint8_t> pack(const std::string& baseDir,
                                      const std::vector<FileInfo>& files) = 0;

    /// 从字节流解包还原文件到目标目录，同时输出 FileInfo 列表供后续元数据恢复
    virtual void unpack(const std::vector<uint8_t>& data,
                        const std::string& destDir,
                        std::vector<FileInfo>& outFiles) = 0;

    /// 算法名称标识
    virtual std::string name() const = 0;

    /// 算法编号（写入文件头）
    virtual uint8_t algoId() const = 0;
};

#endif // BACKUP_IPACKSTRATEGY_H
