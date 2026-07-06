#ifndef BACKUP_RESTOREENGINE_H
#define BACKUP_RESTOREENGINE_H

#include "FileInfo.h"
#include "pack/PackManager.h"
#include "compress/CompressManager.h"
#include "encrypt/EncryptManager.h"
#include <string>
#include <vector>

/**
 * 还原引擎：编排完整还原流程
 * 根据备份文件头部 flags + algoId 决定使用哪种策略并执行哪些反向步骤
 */
class RestoreEngine {
public:
    RestoreEngine();

    /// 设置策略管理器（引擎根据文件头 algoId 自动选择策略）
    void setPackManager(PackManager* mgr);
    void setCompressManager(CompressManager* mgr);
    void setEncryptManager(EncryptManager* mgr);

    /// 执行还原
    /// @param inputFile 备份文件路径
    /// @param destDir   还原目标目录
    /// @param password  解密密码（仅当备份启用了加密时使用）
    /// @return 成功返回 true
    bool run(const std::string& inputFile,
             const std::string& destDir,
             const std::string& password = "");

private:
    PackManager*      packMgr_      = nullptr;
    CompressManager*  compressMgr_  = nullptr;
    EncryptManager*   encryptMgr_   = nullptr;

    /// 恢复文件元数据（权限、属主、时间戳）
    bool restoreMetadata(const std::string& destDir,
                         const std::vector<FileInfo>& files);

    /// 从纯数据还原文件（无打包时的回退方案）
    bool restoreFromRaw(const std::vector<uint8_t>& data,
                        const std::string& destDir,
                        std::vector<FileInfo>& outFiles);
};

#endif // BACKUP_RESTOREENGINE_H
