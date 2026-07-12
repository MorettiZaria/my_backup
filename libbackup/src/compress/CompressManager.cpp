#include "compress/CompressManager.h"

void CompressManager::registerStrategy(std::unique_ptr<ICompressStrategy> strategy) {
    strategies_.push_back(std::move(strategy));
}

ICompressStrategy* CompressManager::select(const std::string& name) {
    for (auto& s : strategies_) {
        if (s->name() == name) return s.get();
    }
    return nullptr;
}

ICompressStrategy* CompressManager::selectById(uint8_t algoId) {
    for (auto& s : strategies_) {
        if (s->algoId() == algoId) return s.get();
    }
    return nullptr;
}

std::vector<std::string> CompressManager::listNames() const {
    std::vector<std::string> names;
    for (const auto& s : strategies_) {
        names.push_back(s->name());
    }
    return names;
}
