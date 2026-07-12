#include "encrypt/EncryptManager.h"

void EncryptManager::registerStrategy(std::unique_ptr<IEncryptStrategy> strategy) {
    strategies_.push_back(std::move(strategy));
}

IEncryptStrategy* EncryptManager::select(const std::string& name) {
    for (auto& s : strategies_) {
        if (s->name() == name) return s.get();
    }
    return nullptr;
}

IEncryptStrategy* EncryptManager::selectById(uint8_t algoId) {
    for (auto& s : strategies_) {
        if (s->algoId() == algoId) return s.get();
    }
    return nullptr;
}

std::vector<std::string> EncryptManager::listNames() const {
    std::vector<std::string> names;
    for (const auto& s : strategies_) {
        names.push_back(s->name());
    }
    return names;
}
