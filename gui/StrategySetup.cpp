#include "StrategySetup.h"
#include "pack/TarPackStrategy.h"
#include "pack/IndexPackStrategy.h"
#include "compress/RleCompressStrategy.h"
#include "compress/HuffmanCompressStrategy.h"
#include "encrypt/XorEncryptStrategy.h"
#include "encrypt/VigenereEncryptStrategy.h"

StrategySetup::StrategySetup() {
    packMgr.registerStrategy(std::make_unique<TarPackStrategy>());
    packMgr.registerStrategy(std::make_unique<IndexPackStrategy>());

    compressMgr.registerStrategy(std::make_unique<RleCompressStrategy>());
    compressMgr.registerStrategy(std::make_unique<HuffmanCompressStrategy>());

    encryptMgr.registerStrategy(std::make_unique<XorEncryptStrategy>());
    encryptMgr.registerStrategy(std::make_unique<VigenereEncryptStrategy>());
}
