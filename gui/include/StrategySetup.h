#ifndef GUI_STRATEGYSETUP_H
#define GUI_STRATEGYSETUP_H

#include "pack/PackManager.h"
#include "compress/CompressManager.h"
#include "encrypt/EncryptManager.h"

/// 策略工厂：创建并持有所有已注册的策略管理器
/// 供 GUI 各 Tab 使用，避免重复注册代码
struct StrategySetup {
    PackManager packMgr;
    CompressManager compressMgr;
    EncryptManager encryptMgr;

    StrategySetup();
};

#endif // GUI_STRATEGYSETUP_H
