#include "gtest/gtest.h"
#include "network/NetworkProtocol.h"
#include "network/ServerConfig.h"
#include "network/UserManager.h"
#include "network/TransportEncryptor.h"
#include "network/ServerStorage.h"
#include "network/Logger.h"
#include "core/FileInfo.h"
#include <fstream>
#include <sys/stat.h>

namespace {
    std::string tmpDir() {
        std::string d = "/tmp/net_gtest_" + std::to_string(getpid());
        mkdir(d.c_str(), 0755);
        return d;
    }
    void rmrf(const std::string& path) {
        std::string cmd = "rm -rf " + path + " 2>/dev/null";
        (void)system(cmd.c_str());
    }
}

// ===================== NetworkMessage =====================

TEST(NetworkMessageTest, DefaultConstructor) {
    NetworkMessage msg;
    EXPECT_EQ(msg.type, 0u);
    EXPECT_EQ(msg.reserved, 0u);
    EXPECT_TRUE(msg.payload.empty());
}

TEST(NetworkMessageTest, ParamConstructor) {
    std::vector<uint8_t> p = {'A', 'B', 'C'};
    NetworkMessage msg(0x0001, p);
    EXPECT_EQ(msg.type, 0x0001u);
    EXPECT_TRUE(msg.reserved == 0u);
    EXPECT_EQ(msg.payload, p);
}

TEST(NetworkMessageTest, MakeNoPayload) {
    auto msg = NetworkMessage::make(MessageType::CLIENT_HELLO);
    EXPECT_EQ(msg.type, static_cast<uint16_t>(MessageType::CLIENT_HELLO));
    EXPECT_TRUE(msg.payload.empty());
}

TEST(NetworkMessageTest, MakeWithPayload) {
    std::vector<uint8_t> p = {0x01, 0x02};
    auto msg = NetworkMessage::make(MessageType::LOGIN_REQUEST, p);
    EXPECT_EQ(msg.type, static_cast<uint16_t>(MessageType::LOGIN_REQUEST));
    EXPECT_EQ(msg.payload, p);
}

TEST(NetworkMessageTest, SerializeEmptyPayload) {
    auto msg = NetworkMessage::make(MessageType::BACKUP_COMPLETE);
    auto data = msg.serialize();
    EXPECT_EQ(data.size(), 8u); // header only
    // Big-endian: type (2B) + reserved (2B) + payloadLen:0 (4B)
    EXPECT_EQ(data[0], 0x00u);
    EXPECT_EQ(data[1], 0x0Au);
    EXPECT_EQ(data[2], 0x00u);
    EXPECT_EQ(data[3], 0x00u);
    EXPECT_EQ(data[4], 0x00u);
    EXPECT_EQ(data[5], 0x00u);
    EXPECT_EQ(data[6], 0x00u);
    EXPECT_EQ(data[7], 0x00u);
}

TEST(NetworkMessageTest, SerializeWithPayload) {
    std::vector<uint8_t> p = {0xAA, 0xBB};
    auto msg = NetworkMessage::make(MessageType::ERROR_MESSAGE, p);
    auto data = msg.serialize();
    EXPECT_EQ(data.size(), 10u); // 8 + 2
    EXPECT_EQ(data[8], 0xAAu);
    EXPECT_EQ(data[9], 0xBBu);
}

TEST(NetworkMessageTest, Deserialize) {
    auto original = NetworkMessage::make(MessageType::REGISTER_RESPONSE,
                                         std::vector<uint8_t>{0x00, 0x01, 0x02});
    auto data = original.serialize();
    auto restored = NetworkMessage::deserialize(data.data(), data.size());
    EXPECT_EQ(restored.type, static_cast<uint16_t>(MessageType::REGISTER_RESPONSE));
    EXPECT_EQ(restored.payload.size(), 3u);
    EXPECT_EQ(restored.payload[0], 0x00u);
    EXPECT_EQ(restored.payload[1], 0x01u);
    EXPECT_EQ(restored.payload[2], 0x02u);
}

TEST(NetworkMessageTest, DeserializeTooSmall) {
    std::vector<uint8_t> small = {0x01, 0x02, 0x03}; // < 8 bytes
    auto msg = NetworkMessage::deserialize(small.data(), small.size());
    EXPECT_EQ(msg.type, 0u);
}

TEST(NetworkMessageTest, DeserializeIncompletePayload) {
    // header says payload is 100 bytes but only 10 bytes total available
    std::vector<uint8_t> data(10, 0);
    data[4] = 0x00; // payloadLen big-endian = 100
    data[7] = 100;
    auto msg = NetworkMessage::deserialize(data.data(), data.size());
    EXPECT_EQ(msg.type, 0u); // should fail silently
}

TEST(NetworkMessageTest, RoundTripAllTypes) {
    std::vector<MessageType> types = {
        MessageType::CLIENT_HELLO, MessageType::SERVER_HELLO,
        MessageType::LOGIN_REQUEST, MessageType::LOGIN_RESPONSE,
        MessageType::BACKUP_START, MessageType::BACKUP_COMPLETE,
        MessageType::RESTORE_REQUEST, MessageType::RESTORE_COMPLETE,
        MessageType::BACKUP_LIST_REQUEST, MessageType::BACKUP_LIST_RESPONSE,
        MessageType::LOGOUT
    };
    for (auto t : types) {
        std::vector<uint8_t> p = {0xDE, 0xAD, 0xBE, 0xEF};
        auto msg = NetworkMessage::make(t, p);
        auto data = msg.serialize();
        auto restored = NetworkMessage::deserialize(data.data(), data.size());
        EXPECT_EQ(restored.type, static_cast<uint16_t>(t));
        EXPECT_EQ(restored.payload, p);
    }
}

// ===================== BE read/write helpers =====================

TEST(BEHelpersTest, WriteReadUint16) {
    std::vector<uint8_t> buf;
    writeUint16BE(buf, 0x1234);
    EXPECT_EQ(buf.size(), 2u);
    size_t off = 0;
    EXPECT_EQ(readUint16BE(buf.data(), off), 0x1234u);
}

TEST(BEHelpersTest, WriteReadUint32) {
    std::vector<uint8_t> buf;
    writeUint32BE(buf, 0xDEADBEEF);
    EXPECT_EQ(buf.size(), 4u);
    size_t off = 0;
    EXPECT_EQ(readUint32BE(buf.data(), off), 0xDEADBEEFu);
}

TEST(BEHelpersTest, WriteReadUint64) {
    std::vector<uint8_t> buf;
    writeUint64BE(buf, 0x0102030405060708ULL);
    EXPECT_EQ(buf.size(), 8u);
    size_t off = 0;
    EXPECT_EQ(readUint64BE(buf.data(), off), 0x0102030405060708ULL);
}

TEST(BEHelpersTest, WriteReadString) {
    std::vector<uint8_t> buf;
    writeStringBE(buf, "Hello");
    size_t off = 0;
    EXPECT_EQ(readStringBE(buf.data(), off), "Hello");
}

TEST(BEHelpersTest, WriteReadEmptyString) {
    std::vector<uint8_t> buf;
    writeStringBE(buf, "");
    size_t off = 0;
    EXPECT_EQ(readStringBE(buf.data(), off), "");
}

TEST(BEHelpersTest, ZeroValues) {
    std::vector<uint8_t> buf;
    writeUint16BE(buf, 0);
    writeUint32BE(buf, 0);
    writeUint64BE(buf, 0);
    size_t off = 0;
    EXPECT_EQ(readUint16BE(buf.data(), off), 0u);
    EXPECT_EQ(readUint32BE(buf.data(), off), 0u);
    EXPECT_EQ(readUint64BE(buf.data(), off), 0u);
}

TEST(BEHelpersTest, MaxValues) {
    std::vector<uint8_t> buf;
    writeUint16BE(buf, 0xFFFF);
    writeUint32BE(buf, 0xFFFFFFFF);
    writeUint64BE(buf, 0xFFFFFFFFFFFFFFFFULL);
    size_t off = 0;
    EXPECT_EQ(readUint16BE(buf.data(), off), 0xFFFFu);
    EXPECT_EQ(readUint32BE(buf.data(), off), 0xFFFFFFFFu);
    EXPECT_EQ(readUint64BE(buf.data(), off), 0xFFFFFFFFFFFFFFFFULL);
}

// ===================== ServerConfig =====================

TEST(ServerConfigTest, DefaultValues) {
    ServerConfig cfg;
    EXPECT_EQ(cfg.port(), 8848u);
    EXPECT_EQ(cfg.storagePath(), "./server_data");
    EXPECT_TRUE(cfg.logFile().empty());
    EXPECT_EQ(cfg.maxConnections(), 100);
}

TEST(ServerConfigTest, Setters) {
    ServerConfig cfg;
    cfg.setPort(1234);
    cfg.setStoragePath("/data/backup");
    cfg.setLogFile("/var/log/app.log");
    cfg.setMaxConnections(50);
    EXPECT_EQ(cfg.port(), 1234u);
    EXPECT_EQ(cfg.storagePath(), "/data/backup");
    EXPECT_EQ(cfg.logFile(), "/var/log/app.log");
    EXPECT_EQ(cfg.maxConnections(), 50);
}

TEST(ServerConfigTest, LoadFromString) {
    std::string tdir = tmpDir();
    std::string cfgPath = tdir + "/test.cfg";
    {
        std::ofstream out(cfgPath);
        out << "[server]\n";
        out << "port = 9999\n";
        out << "storage_path = /srv/data\n";
        out << "log_file = /tmp/server.log\n";
        out << "max_connections = 200\n";
        out.close();
    }

    ServerConfig cfg;
    EXPECT_TRUE(cfg.load(cfgPath));
    EXPECT_EQ(cfg.port(), 9999u);
    EXPECT_EQ(cfg.storagePath(), "/srv/data");
    EXPECT_EQ(cfg.logFile(), "/tmp/server.log");
    EXPECT_EQ(cfg.maxConnections(), 200);

    rmrf(tdir);
}

TEST(ServerConfigTest, LoadWithComments) {
    std::string tdir = tmpDir();
    std::string cfgPath = tdir + "/comment.cfg";
    {
        std::ofstream out(cfgPath);
        out << "# This is a comment\n";
        out << " ; Semicolon comment\n";
        out << "[server]\n";
        out << "port = 5000\n";
        out << "\n";
        out << "# End\n";
        out.close();
    }

    ServerConfig cfg;
    EXPECT_TRUE(cfg.load(cfgPath));
    EXPECT_EQ(cfg.port(), 5000u);

    rmrf(tdir);
}

TEST(ServerConfigTest, LoadQuotedValues) {
    std::string tdir = tmpDir();
    std::string cfgPath = tdir + "/quote.cfg";
    {
        std::ofstream out(cfgPath);
        out << "[server]\n";
        out << "storage_path = \"/data/my backup\"\n";
        out << "log_file = '/var/log/my.log'\n";
        out.close();
    }

    ServerConfig cfg;
    EXPECT_TRUE(cfg.load(cfgPath));
    EXPECT_EQ(cfg.storagePath(), "/data/my backup");
    EXPECT_EQ(cfg.logFile(), "/var/log/my.log");

    rmrf(tdir);
}

TEST(ServerConfigTest, LoadNonexistentFile) {
    ServerConfig cfg;
    EXPECT_FALSE(cfg.load("/nonexistent/config_file_xyz_123.cfg"));
}

TEST(ServerConfigTest, LoadInvalidPort) {
    std::string tdir = tmpDir();
    std::string cfgPath = tdir + "/bad.cfg";
    {
        std::ofstream out(cfgPath);
        out << "[server]\n";
        out << "port = abc\n";
        out.close();
    }

    ServerConfig cfg;
    EXPECT_TRUE(cfg.load(cfgPath));
    EXPECT_EQ(cfg.port(), 8848u); // stays default

    rmrf(tdir);
}

TEST(ServerConfigTest, WriteExample) {
    std::string tdir = tmpDir();
    std::string cfgPath = tdir + "/example.cfg";
    EXPECT_TRUE(ServerConfig::writeExample(cfgPath));

    ServerConfig cfg;
    EXPECT_TRUE(cfg.load(cfgPath));
    EXPECT_EQ(cfg.port(), 8848u);
    EXPECT_EQ(cfg.storagePath(), "./server_data");
    EXPECT_EQ(cfg.maxConnections(), 100);

    rmrf(tdir);
}

TEST(ServerConfigTest, WriteExampleFailure) {
    EXPECT_FALSE(ServerConfig::writeExample("/dev/null/invalid/path/xyz.cfg"));
}

// ===================== UserManager =====================

TEST(UserManagerTest, NewUserManagerEmpty) {
    std::string tdir = tmpDir();
    std::string dbPath = tdir + "/users.db";
    UserManager mgr(dbPath);
    EXPECT_FALSE(mgr.userExists("testuser"));
    EXPECT_FALSE(mgr.authenticateUser("testuser", "password"));
    std::vector<uint8_t> salt, hash;
    EXPECT_FALSE(mgr.getUserHash("testuser", salt, hash));
    rmrf(tdir);
}

TEST(UserManagerTest, RegisterAndAuthenticate) {
    std::string tdir = tmpDir();
    std::string dbPath = tdir + "/users.db";
    UserManager mgr(dbPath);
    EXPECT_TRUE(mgr.registerUser("alice", "secret123"));
    EXPECT_TRUE(mgr.userExists("alice"));
    EXPECT_TRUE(mgr.authenticateUser("alice", "secret123"));
    EXPECT_FALSE(mgr.authenticateUser("alice", "wrongpassword"));
    rmrf(tdir);
}

TEST(UserManagerTest, DuplicateRegister) {
    std::string tdir = tmpDir();
    std::string dbPath = tdir + "/users.db";
    UserManager mgr(dbPath);
    EXPECT_TRUE(mgr.registerUser("bob", "bobpass"));
    EXPECT_FALSE(mgr.registerUser("bob", "bobpass"));
    rmrf(tdir);
}

TEST(UserManagerTest, GetUserHash) {
    std::string tdir = tmpDir();
    std::string dbPath = tdir + "/users.db";
    UserManager mgr(dbPath);
    EXPECT_TRUE(mgr.registerUser("carol", "carolpass"));
    std::vector<uint8_t> salt, hash;
    EXPECT_TRUE(mgr.getUserHash("carol", salt, hash));
    EXPECT_EQ(salt.size(), 8u);
    EXPECT_EQ(hash.size(), 32u);
    rmrf(tdir);
}

TEST(UserManagerTest, RegisterUserRaw) {
    std::string tdir = tmpDir();
    std::string dbPath = tdir + "/users.db";
    UserManager mgr(dbPath);
    std::vector<uint8_t> salt = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08};
    std::vector<uint8_t> hash(32, 0xAA);
    EXPECT_TRUE(mgr.registerUserRaw("dave", salt, hash));
    EXPECT_TRUE(mgr.userExists("dave"));
    std::vector<uint8_t> outSalt, outHash;
    EXPECT_TRUE(mgr.getUserHash("dave", outSalt, outHash));
    EXPECT_EQ(outSalt, salt);
    EXPECT_EQ(outHash, hash);
    rmrf(tdir);
}

TEST(UserManagerTest, PersistAndReload) {
    std::string tdir = tmpDir();
    std::string dbPath = tdir + "/users.db";
    {
        UserManager mgr(dbPath);
        EXPECT_TRUE(mgr.registerUser("eve", "evepass"));
        EXPECT_TRUE(mgr.registerUser("frank", "frankpass"));
    }
    {
        UserManager mgr(dbPath);
        EXPECT_TRUE(mgr.userExists("eve"));
        EXPECT_TRUE(mgr.userExists("frank"));
        EXPECT_TRUE(mgr.authenticateUser("eve", "evepass"));
        EXPECT_TRUE(mgr.authenticateUser("frank", "frankpass"));
    }
    rmrf(tdir);
}

TEST(UserManagerTest, GenerateSalt) {
    auto salt = UserManager::generateSalt();
    EXPECT_EQ(salt.size(), 8u);
    // Should generate different salts each time (random)
    auto salt2 = UserManager::generateSalt();
    // Very unlikely to be the same
    bool different = false;
    for (size_t i = 0; i < salt.size(); ++i) {
        if (salt[i] != salt2[i]) { different = true; break; }
    }
    EXPECT_TRUE(different);
}

TEST(UserManagerTest, ComputeHashDeterministic) {
    std::vector<uint8_t> salt(8, 0x00);
    auto h1 = UserManager::computeHash("password", salt);
    auto h2 = UserManager::computeHash("password", salt);
    EXPECT_EQ(h1, h2);
    EXPECT_EQ(h1.size(), 32u);
    EXPECT_EQ(h2.size(), 32u);
}

TEST(UserManagerTest, ComputeHashDifferentPasswords) {
    std::vector<uint8_t> salt(8, 0x00);
    auto h1 = UserManager::computeHash("pass1", salt);
    auto h2 = UserManager::computeHash("pass2", salt);
    EXPECT_NE(h1, h2);
}

TEST(UserManagerTest, ComputeChallengeResponse) {
    std::vector<uint8_t> storedHash(32, 0x11);
    std::vector<uint8_t> challenge(8, 0x22);
    auto resp = UserManager::computeChallengeResponse(storedHash, challenge);
    EXPECT_EQ(resp.size(), 32u);
}

// ===================== TransportEncryptor =====================

TEST(TransportEncryptorTest, NotActiveByDefault) {
    TransportEncryptor te;
    EXPECT_FALSE(te.isActive());
}

TEST(TransportEncryptorTest, EncryptWhenInactive) {
    TransportEncryptor te;
    std::vector<uint8_t> data = {0x01, 0x02, 0x03};
    auto enc = te.encrypt(data, 0);
    EXPECT_EQ(enc, data); // unchanged when not active
}

TEST(TransportEncryptorTest, InitAndEncryptDecrypt) {
    TransportEncryptor te;
    std::vector<uint8_t> salt = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08};
    te.initSession("mypassword", salt);
    EXPECT_TRUE(te.isActive());

    std::vector<uint8_t> plain = {0x10, 0x20, 0x30, 0x40, 0x50};
    auto enc = te.encrypt(plain, 0);
    EXPECT_NE(enc, plain); // encrypted should be different
    auto dec = te.decrypt(enc, 0);
    EXPECT_EQ(dec, plain);
}

TEST(TransportEncryptorTest, DifferentSeqNumbers) {
    TransportEncryptor te;
    std::vector<uint8_t> salt = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08};
    te.initSession("key", salt);

    std::vector<uint8_t> plain = {0xAA, 0xBB, 0xCC};
    auto enc0 = te.encrypt(plain, 0);
    auto enc1 = te.encrypt(plain, 1);
    EXPECT_NE(enc0, enc1); // different sequence numbers give different ciphertext
    EXPECT_EQ(te.decrypt(enc0, 0), plain);
    EXPECT_EQ(te.decrypt(enc1, 1), plain);
}

TEST(TransportEncryptorTest, DecryptInactive) {
    TransportEncryptor te;
    std::vector<uint8_t> data = {0x01, 0x02};
    auto dec = te.decrypt(data, 0);
    EXPECT_EQ(dec, data);
}

// ===================== ServerStorage =====================

TEST(ServerStorageTest, ListBackupsEmpty) {
    std::string tdir = tmpDir();
    ServerStorage storage(tdir);
    auto backups = storage.listBackups("nobody");
    EXPECT_TRUE(backups.empty());
    rmrf(tdir);
}

TEST(ServerStorageTest, CreateAndListBackups) {
    std::string tdir = tmpDir();
    ServerStorage storage(tdir);
    std::string id1 = storage.createBackup("user1");
    storage.saveHeader("user1", id1, {0x01}); // listBackups requires header.bin
    std::string id2 = storage.createBackup("user1");
    storage.saveHeader("user1", id2, {0x01});
    EXPECT_FALSE(id1.empty());
    EXPECT_FALSE(id2.empty());
    EXPECT_NE(id1, id2);

    auto backups = storage.listBackups("user1");
    EXPECT_EQ(backups.size(), 2u);
    EXPECT_EQ(backups[0], id1);
    EXPECT_EQ(backups[1], id2);

    rmrf(tdir);
}

TEST(ServerStorageTest, GetLatestBackup) {
    std::string tdir = tmpDir();
    ServerStorage storage(tdir);
    EXPECT_TRUE(storage.getLatestBackupId("user1").empty());
    std::string id = storage.createBackup("user1");
    storage.saveHeader("user1", id, {0x01});
    EXPECT_EQ(storage.getLatestBackupId("user1"), id);

    rmrf(tdir);
}

TEST(ServerStorageTest, SaveAndLoadHeader) {
    std::string tdir = tmpDir();
    ServerStorage storage(tdir);
    std::string id = storage.createBackup("user1");
    std::vector<uint8_t> hdr = {0x01, 0x02, 0x03, 0x04};
    EXPECT_TRUE(storage.saveHeader("user1", id, hdr));
    auto loaded = storage.loadHeader("user1", id);
    EXPECT_EQ(loaded, hdr);

    rmrf(tdir);
}

TEST(ServerStorageTest, SaveAndLoadPayload) {
    std::string tdir = tmpDir();
    ServerStorage storage(tdir);
    std::string id = storage.createBackup("user1");
    std::vector<uint8_t> payload = {0xAA, 0xBB, 0xCC, 0xDD, 0xEE};
    EXPECT_TRUE(storage.savePayload("user1", id, payload));
    auto loaded = storage.loadPayload("user1", id);
    EXPECT_EQ(loaded, payload);

    rmrf(tdir);
}

TEST(ServerStorageTest, SaveAndLoadMetadata) {
    std::string tdir = tmpDir();
    ServerStorage storage(tdir);
    std::string id = storage.createBackup("user1");
    std::vector<FileInfo> files;
    FileInfo fi;
    fi.relativePath = "test.txt";
    fi.fileType = S_IFREG;
    fi.permissions = 0644;
    fi.content = {0x01, 0x02, 0x03};
    files.push_back(fi);
    EXPECT_TRUE(storage.saveMetadata("user1", id, files));
    auto loaded = storage.loadMetadata("user1", id);
    ASSERT_EQ(loaded.size(), 1u);
    EXPECT_EQ(loaded[0].relativePath, "test.txt");

    rmrf(tdir);
}

TEST(ServerStorageTest, SaveAndGetBackupName) {
    std::string tdir = tmpDir();
    ServerStorage storage(tdir);
    std::string id = storage.createBackup("user1");
    EXPECT_TRUE(storage.saveBackupName("user1", id, "MyBackup"));
    EXPECT_EQ(storage.getBackupName("user1", id), "MyBackup");

    rmrf(tdir);
}

TEST(ServerStorageTest, FindBackupByName) {
    std::string tdir = tmpDir();
    ServerStorage storage(tdir);
    std::string id = storage.createBackup("user1");
    storage.saveHeader("user1", id, {0x01});
    EXPECT_TRUE(storage.saveBackupName("user1", id, "Release v1.0"));
    EXPECT_EQ(storage.findBackupByName("user1", "Release v1.0"), id);
    EXPECT_TRUE(storage.findBackupByName("user1", "NoSuchBackup").empty());

    rmrf(tdir);
}

TEST(ServerStorageTest, GetBackupTimestamp) {
    std::string tdir = tmpDir();
    ServerStorage storage(tdir);
    std::string id = storage.createBackup("user1");
    storage.saveHeader("user1", id, {0x01});
    // Should return positive timestamp
    EXPECT_GT(storage.getBackupTimestamp("user1", id), 0);
    EXPECT_EQ(storage.getBackupTimestamp("user1", "nonexistent"), 0);

    rmrf(tdir);
}

TEST(ServerStorageTest, LoadNonexistentReturnsEmpty) {
    std::string tdir = tmpDir();
    ServerStorage storage(tdir);
    EXPECT_TRUE(storage.loadHeader("nobody", "fake_id").empty());
    EXPECT_TRUE(storage.loadPayload("nobody", "fake_id").empty());
    EXPECT_TRUE(storage.loadMetadata("nobody", "fake_id").empty());

    rmrf(tdir);
}

TEST(ServerStorageTest, DeleteBackup) {
    std::string tdir = tmpDir();
    ServerStorage storage(tdir);
    std::string id = storage.createBackup("user1");
    std::vector<uint8_t> hdr = {0x01};
    storage.saveHeader("user1", id, hdr);
    storage.savePayload("user1", id, {0x02});
    storage.saveMetadata("user1", id, {});
    EXPECT_TRUE(storage.deleteBackup("user1", id));
    EXPECT_TRUE(storage.listBackups("user1").empty());

    rmrf(tdir);
}

TEST(ServerStorageTest, GetBackupNameNonexistent) {
    std::string tdir = tmpDir();
    ServerStorage storage(tdir);
    EXPECT_TRUE(storage.getBackupName("user1", "nonexistent").empty());

    rmrf(tdir);
}

// ===================== Logger =====================

TEST(LoggerTest, InitializedState) {
    Logger& log = Logger::instance();
    // Default: not initialized
    EXPECT_FALSE(log.isInitialized());
}

TEST(LoggerTest, InitAndLog) {
    std::string tdir = tmpDir();
    std::string logPath = tdir + "/test.log";
    Logger& log = Logger::instance();
    log.init(logPath, false);
    EXPECT_TRUE(log.isInitialized());
    log.info("Test info message");
    log.warn("Test warn message");
    log.error("Test error message");

    // Verify log file has content
    std::ifstream check(logPath);
    EXPECT_TRUE(check.good());
    std::string line;
    std::getline(check, line);
    EXPECT_FALSE(line.empty());
    EXPECT_NE(line.find("[INFO]"), std::string::npos);

    rmrf(tdir);
}

TEST(LoggerTest, InitStdout) {
    std::string tdir = tmpDir();
    std::string logPath = tdir + "/stdout.log";
    Logger& log = Logger::instance();
    log.init(logPath, true);
    EXPECT_TRUE(log.isInitialized());
    log.info("stdout message");

    rmrf(tdir);
}
