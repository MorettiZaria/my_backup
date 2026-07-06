#ifndef BACKUP_PACKMANAGER_H
#define BACKUP_PACKMANAGER_H

#include "IPackStrategy.h"
#include <vector>
#include <memory>
#include <string>

/// 打包策略管理器：注册/选择
class PackManager {
public:
    void registerStrategy(std::unique_ptr<IPackStrategy> strategy);
    IPackStrategy* select(const std::string& name);
    IPackStrategy* selectById(uint8_t algoId);
    std::vector<std::string> listNames() const;

private:
    std::vector<std::unique_ptr<IPackStrategy>> strategies_;
};

#endif // BACKUP_PACKMANAGER_H
