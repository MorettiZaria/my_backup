#ifndef BACKUP_STRATEGYFACTORY_H
#define BACKUP_STRATEGYFACTORY_H

#include "pack/PackManager.h"
#include "compress/CompressManager.h"
#include "encrypt/EncryptManager.h"
#include "pack/TarPackStrategy.h"
#include "pack/IndexPackStrategy.h"
#include "compress/RleCompressStrategy.h"
#include "compress/HuffmanCompressStrategy.h"
#include "encrypt/XorEncryptStrategy.h"
#include "encrypt/VigenereEncryptStrategy.h"

/// 向三个管理器注册所有内置策略（打包/压缩/加密各 2 种）
/// 用于 CLI 和 GUI 的统一初始化，消除重复代码
inline void registerAllStrategies(PackManager& packMgr,
                                  CompressManager& compressMgr,
                                  EncryptManager& encryptMgr) {
    packMgr.registerStrategy(std::make_unique<TarPackStrategy>());
    packMgr.registerStrategy(std::make_unique<IndexPackStrategy>());

    compressMgr.registerStrategy(std::make_unique<RleCompressStrategy>());
    compressMgr.registerStrategy(std::make_unique<HuffmanCompressStrategy>());

    encryptMgr.registerStrategy(std::make_unique<XorEncryptStrategy>());
    encryptMgr.registerStrategy(std::make_unique<VigenereEncryptStrategy>());
}

#endif // BACKUP_STRATEGYFACTORY_H
