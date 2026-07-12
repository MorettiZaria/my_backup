#include "pack/PackManager.h"

void PackManager::registerStrategy(std::unique_ptr<IPackStrategy> strategy) {
    strategies_.push_back(std::move(strategy));
}

IPackStrategy* PackManager::select(const std::string& name) {
    for (auto& s : strategies_) {
        if (s->name() == name) return s.get();
    }
    return nullptr;
}

IPackStrategy* PackManager::selectById(uint8_t algoId) {
    for (auto& s : strategies_) {
        if (s->algoId() == algoId) return s.get();
    }
    return nullptr;
}

std::vector<std::string> PackManager::listNames() const {
    std::vector<std::string> names;
    for (const auto& s : strategies_) {
        names.push_back(s->name());
    }
    return names;
}
