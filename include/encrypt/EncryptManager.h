#ifndef BACKUP_ENCRYPTMANAGER_H
#define BACKUP_ENCRYPTMANAGER_H

#include "IEncryptStrategy.h"
#include <vector>
#include <memory>
#include <string>

class EncryptManager {
public:
    void registerStrategy(std::unique_ptr<IEncryptStrategy> strategy);
    IEncryptStrategy* select(const std::string& name);
    IEncryptStrategy* selectById(uint8_t algoId);
    std::vector<std::string> listNames() const;

private:
    std::vector<std::unique_ptr<IEncryptStrategy>> strategies_;
};

#endif // BACKUP_ENCRYPTMANAGER_H
