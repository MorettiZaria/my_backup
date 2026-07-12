#include "StrategySetup.h"
#include "core/StrategyFactory.h"

StrategySetup::StrategySetup() {
    registerAllStrategies(packMgr, compressMgr, encryptMgr);
}
