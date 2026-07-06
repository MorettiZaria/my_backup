#ifndef BACKUP_BACKUPENGINE_H
#define BACKUP_BACKUPENGINE_H

#include "FileInfo.h"
#include "pack/IPackStrategy.h"
#include "compress/ICompressStrategy.h"
#include "encrypt/IEncryptStrategy.h"
#include <string>
#include <vector>
#include <cstdint>

/// 备份文件头部的 flags 位定义
namespace BackupFlags {
    constexpr uint32_t FLAG_PACK     = 1 << 0;  // 启用了打包
    constexpr uint32_t FLAG_COMPRESS = 1 << 1;  // 启用了压缩
    constexpr uint32_t FLAG_ENCRYPT  = 1 << 2;  // 启用了加密
}

/**
 * 备份引擎：编排完整备份流程
 * 每个步骤（打包/压缩/加密）独立可选，strategy 为 nullptr 表示跳过该步骤
 */
class BackupEngine {
public:
    BackupEngine();

    /// 设置策略，传入 nullptr 表示跳过该步骤
    void setPackStrategy(IPackStrategy* strategy);
    void setCompressStrategy(ICompressStrategy* strategy);
    void setEncryptStrategy(IEncryptStrategy* strategy);

    /// 执行备份
    /// @param sourceDir  源目录路径
    /// @param outputFile 输出备份文件路径
    /// @param password   加密密码（仅当启用了加密步骤时使用）
    /// @return 成功返回 true
    bool run(const std::string& sourceDir,
             const std::string& outputFile,
             const std::string& password = "");

private:
    IPackStrategy*      packStrategy_     = nullptr;
    ICompressStrategy*  compressStrategy_ = nullptr;
    IEncryptStrategy*   encryptStrategy_  = nullptr;

    /// 简单拼接文件内容（不打包时的回退方案）
    std::vector<uint8_t> concatFiles(const std::string& baseDir,
                                     const std::vector<FileInfo>& files);
};

#endif // BACKUP_BACKUPENGINE_H
