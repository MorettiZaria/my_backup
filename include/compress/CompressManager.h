#ifndef BACKUP_COMPRESSMANAGER_H
#define BACKUP_COMPRESSMANAGER_H

#include "ICompressStrategy.h"
#include <vector>
#include <memory>
#include <string>

class CompressManager {
public:
    void registerStrategy(std::unique_ptr<ICompressStrategy> strategy);
    ICompressStrategy* select(const std::string& name);
    ICompressStrategy* selectById(uint8_t algoId);
    std::vector<std::string> listNames() const;

private:
    std::vector<std::unique_ptr<ICompressStrategy>> strategies_;
};

#endif // BACKUP_COMPRESSMANAGER_H
