#ifndef BACKUP_USERMANAGER_H
#define BACKUP_USERMANAGER_H

#include <string>
#include <vector>
#include <cstdint>

/// 用户记录
struct UserEntry {
    std::string name;
    std::vector<uint8_t> salt;        // 8 字节随机盐值
    std::vector<uint8_t> passwordHash; // 32 字节密码哈希
};

/// 用户管理器：基于文件的用户注册/认证
class UserManager {
public:
    explicit UserManager(const std::string& dbPath);

    /// 注册新用户；返回 true=成功, false=用户已存在
    bool registerUser(const std::string& name, const std::string& password);

    /// 验证密码；成功返回 true
    bool authenticateUser(const std::string& name, const std::string& password);

    /// 获取用户存储的哈希（用于挑战-响应）
    bool getUserHash(const std::string& name,
                     std::vector<uint8_t>& outSalt,
                     std::vector<uint8_t>& outHash);

    /// 直接存储预计算好的 salt+hash（用于客户端预先 hash 的情况）
    bool registerUserRaw(const std::string& name,
                         const std::vector<uint8_t>& salt,
                         const std::vector<uint8_t>& hash);

    /// 用户是否存在
    bool userExists(const std::string& name);

    /// 计算密码哈希（公开静态方法，供认证流程使用）
    static std::vector<uint8_t> computeHash(const std::string& password,
                                            const std::vector<uint8_t>& salt);

    /// 生成随机盐值
    static std::vector<uint8_t> generateSalt();

    /// 从种子 + 挑战码计算响应（挑战-响应认证用）
    static std::vector<uint8_t> computeChallengeResponse(
        const std::vector<uint8_t>& storedHash,
        const std::vector<uint8_t>& challenge);

private:
    std::string dbPath_;
    std::vector<UserEntry> users_;

    void loadUsers();
    void saveUsers();
};

#endif // BACKUP_USERMANAGER_H
