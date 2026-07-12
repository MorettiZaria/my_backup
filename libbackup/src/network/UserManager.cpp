#include "network/UserManager.h"
#include "network/NetworkProtocol.h"
#include <fstream>
#include <iostream>
#include <cstring>
#include <random>
#include <algorithm>

UserManager::UserManager(const std::string& dbPath) : dbPath_(dbPath) {
    loadUsers();
}

// ===== LCG 随机数（复用 XOR 加密中的算法） =====

namespace {
    struct LCG {
        uint32_t state;
        explicit LCG(uint32_t seed) : state(seed) {}
        uint8_t next() {
            state = 1103515245u * state + 12345u;
            return static_cast<uint8_t>(state & 0xFF);
        }
    };
}

// ===== 密码哈希 =====

std::vector<uint8_t> UserManager::computeHash(const std::string& password,
                                               const std::vector<uint8_t>& salt) {
    // 简单但正确的哈希：将密码字节与盐值混合后通过 LCG 迭代
    uint32_t seed = 0xDEADBEEF;  // 固定初始值，避免 seed=0 的问题

    // 混合密码
    for (char c : password) {
        seed = seed * 131 + static_cast<uint8_t>(c);
    }

    // 混合盐值
    for (uint8_t b : salt) {
        seed = seed * 131 + b;
    }

    // 多轮 LCG 迭代产生 32 字节输出
    LCG lcg(seed);
    // 消耗前 32 次
    for (int i = 0; i < 32; ++i) lcg.next();

    std::vector<uint8_t> hash(32);
    uint8_t accum = 0;
    for (int i = 0; i < 32; ++i) {
        // 每轮迭代 4 次，混合输出
        uint8_t b = lcg.next();
        accum = accum ^ b;
        // 状态推进使输出更分散
        for (int j = 0; j < 3; ++j) lcg.next();
        hash[i] = b ^ accum;
    }

    return hash;
}

std::vector<uint8_t> UserManager::generateSalt() {
    std::vector<uint8_t> salt(8);
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<int> dist(0, 255);
    for (int i = 0; i < 8; ++i) {
        salt[i] = static_cast<uint8_t>(dist(gen));
    }
    return salt;
}

std::vector<uint8_t> UserManager::computeChallengeResponse(
    const std::vector<uint8_t>& storedHash,
    const std::vector<uint8_t>& challenge) {
    // response = computeHash(storedHash_as_string, challenge)
    std::string hashStr(storedHash.begin(), storedHash.end());
    return computeHash(hashStr, challenge);
}

// ===== 用户数据库读写 =====

void UserManager::loadUsers() {
    users_.clear();
    std::ifstream in(dbPath_, std::ios::binary);
    if (!in) return;  // 文件不存在 = 空数据库

    // [userCount:4B]
    uint32_t userCount = 0;
    in.read(reinterpret_cast<char*>(&userCount), 4);
    if (!in) return;

    for (uint32_t i = 0; i < userCount; ++i) {
        UserEntry entry;
        // [salt:8B]
        entry.salt.resize(8);
        in.read(reinterpret_cast<char*>(entry.salt.data()), 8);
        // [hash:32B]
        entry.passwordHash.resize(32);
        in.read(reinterpret_cast<char*>(entry.passwordHash.data()), 32);
        // [nameLen:2B]
        uint16_t nameLen = 0;
        in.read(reinterpret_cast<char*>(&nameLen), 2);
        // [name:N B]
        entry.name.resize(nameLen);
        in.read(&entry.name[0], nameLen);

        if (in) users_.push_back(std::move(entry));
    }
}

void UserManager::saveUsers() {
    std::ofstream out(dbPath_, std::ios::binary);
    if (!out) {
        std::cerr << "Warning: cannot write user database." << std::endl;
        return;
    }

    uint32_t userCount = static_cast<uint32_t>(users_.size());
    out.write(reinterpret_cast<const char*>(&userCount), 4);

    for (const auto& entry : users_) {
        out.write(reinterpret_cast<const char*>(entry.salt.data()), 8);
        out.write(reinterpret_cast<const char*>(entry.passwordHash.data()), 32);
        uint16_t nameLen = static_cast<uint16_t>(entry.name.size());
        out.write(reinterpret_cast<const char*>(&nameLen), 2);
        out.write(entry.name.data(), nameLen);
    }
}

// ===== 公开接口 =====

bool UserManager::userExists(const std::string& name) {
    for (const auto& u : users_) {
        if (u.name == name) return true;
    }
    return false;
}

bool UserManager::registerUser(const std::string& name, const std::string& password) {
    if (userExists(name)) return false;

    UserEntry entry;
    entry.name = name;
    entry.salt = generateSalt();
    entry.passwordHash = computeHash(password, entry.salt);

    users_.push_back(std::move(entry));
    saveUsers();
    return true;
}

bool UserManager::registerUserRaw(const std::string& name,
                                   const std::vector<uint8_t>& salt,
                                   const std::vector<uint8_t>& hash) {
    if (userExists(name)) return false;

    UserEntry entry;
    entry.name = name;
    entry.salt = salt;
    entry.passwordHash = hash;

    users_.push_back(std::move(entry));
    saveUsers();
    return true;
}

bool UserManager::authenticateUser(const std::string& name, const std::string& password) {
    for (const auto& u : users_) {
        if (u.name == name) {
            auto computed = computeHash(password, u.salt);
            return computed == u.passwordHash;
        }
    }
    return false;
}

bool UserManager::getUserHash(const std::string& name,
                               std::vector<uint8_t>& outSalt,
                               std::vector<uint8_t>& outHash) {
    for (const auto& u : users_) {
        if (u.name == name) {
            outSalt = u.salt;
            outHash = u.passwordHash;
            return true;
        }
    }
    return false;
}
