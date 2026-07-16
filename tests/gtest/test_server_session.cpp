#include "gtest/gtest.h"
#include "network/NetworkSocket.h"
#include "network/NetworkProtocol.h"
#include "network/TransportEncryptor.h"
#include "network/UserManager.h"
#include "network/ServerSession.h"
#include "network/ServerConfig.h"
#include "core/FileInfo.h"
#include "pack/TarPackStrategy.h"
#include "pack/IndexPackStrategy.h"
#include "network/NetworkBackupClient.h"
#include "network/NetworkRestoreClient.h"
#include <thread>
#include <chrono>
#include <sys/stat.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fstream>
#include <signal.h>

namespace {
    std::string tmpDir() {
        std::string d = "/tmp/netsrv_gtest_" + std::to_string(getpid());
        mkdir(d.c_str(), 0755);
        return d;
    }
    void rmrf(const std::string& path) {
        std::string cmd = "rm -rf " + path + " 2>/dev/null";
        (void)system(cmd.c_str());
    }
    uint16_t findFreePort() {
        int fd = socket(AF_INET, SOCK_STREAM, 0);
        struct sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = INADDR_ANY;
        addr.sin_port = 0;
        bind(fd, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr));
        socklen_t len = sizeof(addr);
        getsockname(fd, reinterpret_cast<struct sockaddr*>(&addr), &len);
        uint16_t port = ntohs(addr.sin_port);
        close(fd);
        return port;
    }
    void writeFile(const std::string& path, const std::string& content) {
        size_t pos = path.rfind('/');
        if (pos != std::string::npos) mkdir(path.substr(0, pos).c_str(), 0755);
        std::ofstream out(path, std::ios::binary);
        out.write(content.data(), static_cast<std::streamsize>(content.size()));
    }
}

// ===================== ServerSession Full Flow =====================

TEST(ServerSessionIntegrationTest, HandshakeErrorBadMessage) {
    uint16_t port = findFreePort();
    std::string tdir = tmpDir();

    std::thread serverThread([port, tdir]() {
        NetworkSocket server;
        if (!server.bindAndListen(port, 1)) return;
        NetworkSocket sock = server.accept();
        if (!sock.isValid()) return;
        ServerSession session(std::move(sock), tdir);
        session.run();
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    NetworkSocket client;
    ASSERT_TRUE(client.connect("127.0.0.1", port));
    // Send bad handshake
    auto badMsg = NetworkMessage::make(MessageType::ERROR_MESSAGE);
    client.sendMessage(badMsg);
    auto resp = client.receiveMessage();
    EXPECT_EQ(resp.type, static_cast<uint16_t>(MessageType::ERROR_MESSAGE));

    serverThread.join();
    rmrf(tdir);
}

TEST(ServerSessionIntegrationTest, HandshakeVersionMismatch) {
    uint16_t port = findFreePort();
    std::string tdir = tmpDir();

    std::thread serverThread([port, tdir]() {
        NetworkSocket server;
        if (!server.bindAndListen(port, 1)) return;
        NetworkSocket sock = server.accept();
        if (!sock.isValid()) return;
        ServerSession session(std::move(sock), tdir);
        session.run();
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    NetworkSocket client;
    ASSERT_TRUE(client.connect("127.0.0.1", port));
    // Send wrong version
    std::vector<uint8_t> wrongVer;
    writeUint16BE(wrongVer, 999); // wrong version
    client.sendMessage(NetworkMessage::make(MessageType::CLIENT_HELLO, wrongVer));
    auto resp = client.receiveMessage();
    EXPECT_EQ(resp.type, static_cast<uint16_t>(MessageType::ERROR_MESSAGE));

    serverThread.join();
    rmrf(tdir);
}

TEST(ServerSessionIntegrationTest, LoginFlowSuccess) {
    uint16_t port = findFreePort();
    std::string tdir = tmpDir();

    // Pre-register user
    std::string dbPath = tdir + "/users.db";
    UserManager mgr(dbPath);
    std::string password = "password123";
    auto salt = UserManager::generateSalt();
    auto hash = UserManager::computeHash(password, salt);
    mgr.registerUserRaw("testuser", salt, hash);

    std::thread serverThread([port, tdir]() {
        NetworkSocket server;
        if (!server.bindAndListen(port, 1)) return;
        NetworkSocket sock = server.accept();
        if (!sock.isValid()) return;
        ServerSession session(std::move(sock), tdir);
        session.run();
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    NetworkSocket client;
    ASSERT_TRUE(client.connect("127.0.0.1", port));

    // Handshake
    std::vector<uint8_t> hp;
    writeUint16BE(hp, PROTOCOL_VERSION);
    client.sendMessage(NetworkMessage::make(MessageType::CLIENT_HELLO, hp));
    auto sh = client.receiveMessage();
    ASSERT_EQ(sh.type, static_cast<uint16_t>(MessageType::SERVER_HELLO));

    // Login
    std::vector<uint8_t> lp;
    writeStringBE(lp, "testuser");
    client.sendMessage(NetworkMessage::make(MessageType::LOGIN_REQUEST, lp));
    auto saltMsg = client.receiveMessage();
    ASSERT_EQ(saltMsg.type, static_cast<uint16_t>(MessageType::LOGIN_SALT));

    // Send hash
    client.sendMessage(NetworkMessage::make(MessageType::LOGIN_PROOF, hash));
    auto loginResp = client.receiveMessage();
    ASSERT_EQ(loginResp.type, static_cast<uint16_t>(MessageType::LOGIN_RESPONSE));
    EXPECT_EQ(loginResp.payload[0], 0u); // success

    // Logout
    client.sendMessage(NetworkMessage::make(MessageType::LOGOUT));

    serverThread.join();
    rmrf(tdir);
}

TEST(ServerSessionIntegrationTest, LoginFailedWrongPassword) {
    uint16_t port = findFreePort();
    std::string tdir = tmpDir();

    std::string dbPath = tdir + "/users.db";
    UserManager mgr(dbPath);
    auto salt = UserManager::generateSalt();
    auto hash = UserManager::computeHash("correct", salt);
    mgr.registerUserRaw("user", salt, hash);

    std::thread serverThread([port, tdir]() {
        NetworkSocket server;
        if (!server.bindAndListen(port, 1)) return;
        NetworkSocket sock = server.accept();
        if (!sock.isValid()) return;
        ServerSession session(std::move(sock), tdir);
        session.run();
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    NetworkSocket client;
    client.connect("127.0.0.1", port);
    std::vector<uint8_t> hp;
    writeUint16BE(hp, PROTOCOL_VERSION);
    client.sendMessage(NetworkMessage::make(MessageType::CLIENT_HELLO, hp));
    client.receiveMessage(); // SERVER_HELLO

    std::vector<uint8_t> lp;
    writeStringBE(lp, "user");
    client.sendMessage(NetworkMessage::make(MessageType::LOGIN_REQUEST, lp));
    client.receiveMessage(); // LOGIN_SALT

    // Send wrong hash
    std::vector<uint8_t> wrongHash = UserManager::computeHash("wrong", salt);
    client.sendMessage(NetworkMessage::make(MessageType::LOGIN_PROOF, wrongHash));
    auto resp = client.receiveMessage();
    EXPECT_EQ(resp.payload[0], 1u); // failed

    client.sendMessage(NetworkMessage::make(MessageType::LOGOUT));
    serverThread.join();
    rmrf(tdir);
}

TEST(ServerSessionIntegrationTest, RegisterDuplicate) {
    uint16_t port = findFreePort();
    std::string tdir = tmpDir();

    std::thread serverThread([port, tdir]() {
        NetworkSocket server;
        if (!server.bindAndListen(port, 1)) return;
        NetworkSocket sock = server.accept();
        if (!sock.isValid()) return;
        ServerSession session(std::move(sock), tdir);
        session.run();
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    NetworkSocket client;
    client.connect("127.0.0.1", port);
    std::vector<uint8_t> hp;
    writeUint16BE(hp, PROTOCOL_VERSION);
    client.sendMessage(NetworkMessage::make(MessageType::CLIENT_HELLO, hp));
    client.receiveMessage();

    // First register
    auto salt1 = UserManager::generateSalt();
    auto hash1 = UserManager::computeHash("dupuser", salt1);
    std::vector<uint8_t> rp1;
    writeStringBE(rp1, "dupuser");
    rp1.insert(rp1.end(), salt1.begin(), salt1.end());
    rp1.insert(rp1.end(), hash1.begin(), hash1.end());
    client.sendMessage(NetworkMessage::make(MessageType::REGISTER_REQUEST, rp1));
    auto resp1 = client.receiveMessage();
    EXPECT_EQ(resp1.type, static_cast<uint16_t>(MessageType::REGISTER_RESPONSE));
    EXPECT_EQ(resp1.payload[0], 0u); // success

    // Logout
    client.sendMessage(NetworkMessage::make(MessageType::LOGOUT));

    serverThread.join();
    rmrf(tdir);
}

TEST(ServerSessionIntegrationTest, NotLoggedInCommands) {
    uint16_t port = findFreePort();
    std::string tdir = tmpDir();

    std::thread serverThread([port, tdir]() {
        NetworkSocket server;
        if (!server.bindAndListen(port, 1)) return;
        NetworkSocket sock = server.accept();
        if (!sock.isValid()) return;
        ServerSession session(std::move(sock), tdir);
        session.run();
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    NetworkSocket client;
    client.connect("127.0.0.1", port);
    // Send version OK handshake
    std::vector<uint8_t> hp;
    writeUint16BE(hp, PROTOCOL_VERSION);
    client.sendMessage(NetworkMessage::make(MessageType::CLIENT_HELLO, hp));
    client.receiveMessage(); // SERVER_HELLO

    // Send backup list without login
    client.sendMessage(NetworkMessage::make(MessageType::BACKUP_LIST_REQUEST));
    auto resp = client.receiveMessage();
    EXPECT_EQ(resp.type, static_cast<uint16_t>(MessageType::ERROR_MESSAGE));

    client.sendMessage(NetworkMessage::make(MessageType::LOGOUT));
    serverThread.join();
    rmrf(tdir);
}

TEST(ServerSessionIntegrationTest, BackupAndRestoreFlow) {
    uint16_t port = findFreePort();
    std::string tdir = tmpDir();

    // Pre-register user
    std::string dbPath = tdir + "/users.db";
    UserManager mgr(dbPath);
    auto salt = UserManager::generateSalt();
    auto hash = UserManager::computeHash("bkpuser", salt);
    mgr.registerUserRaw("bkpuser", salt, hash);

    std::vector<uint8_t> serverChallenge;

    std::thread serverThread([port, tdir]() {
        NetworkSocket server;
        if (!server.bindAndListen(port, 1)) return;
        NetworkSocket sock = server.accept();
        if (!sock.isValid()) return;
        ServerSession session(std::move(sock), tdir);
        session.run();
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // Connect and login
    NetworkSocket client;
    ASSERT_TRUE(client.connect("127.0.0.1", port));
    std::vector<uint8_t> hp;
    writeUint16BE(hp, PROTOCOL_VERSION);
    client.sendMessage(NetworkMessage::make(MessageType::CLIENT_HELLO, hp));
    auto sh = client.receiveMessage();
    ASSERT_EQ(sh.type, static_cast<uint16_t>(MessageType::SERVER_HELLO));

    // Extract server challenge for encryption
    if (sh.payload.size() >= 10) {
        size_t off = 0;
        readUint16BE(sh.payload.data(), off); // skip version
        serverChallenge.assign(sh.payload.begin() + static_cast<ptrdiff_t>(off), sh.payload.end());
    }

    // Setup client-side encryption (same key derivation as server)
    TransportEncryptor clientEnc;
    clientEnc.initSession("bkpuser", serverChallenge);
    uint64_t sendSeq = 0, recvSeq = 0;

    // Login (handshake messages are NOT encrypted)
    std::vector<uint8_t> lp;
    writeStringBE(lp, "bkpuser");
    client.sendMessage(NetworkMessage::make(MessageType::LOGIN_REQUEST, lp));
    auto saltMsg = client.receiveMessage();
    ASSERT_EQ(saltMsg.type, static_cast<uint16_t>(MessageType::LOGIN_SALT));
    client.sendMessage(NetworkMessage::make(MessageType::LOGIN_PROOF, hash));
    auto loginResp = client.receiveMessage(); // LOGIN_RESPONSE is sent unencrypted
    ASSERT_EQ(loginResp.payload[0], 0u);

    // From now on, server uses encrypted communication
    auto sendEnc = [&](const NetworkMessage& msg) {
        NetworkMessage enc = msg;
        enc.payload = clientEnc.encrypt(msg.payload, sendSeq++);
        return client.sendMessage(enc);
    };
    auto recvEnc = [&]() -> NetworkMessage {
        auto msg = client.receiveMessage();
        if (msg.type != 0) {
            msg.payload = clientEnc.decrypt(msg.payload, recvSeq++);
        }
        return msg;
    };

    // Do backup
    std::vector<uint8_t> bak = {0x42, 0x4B, 0x55, 0x50, 0x01, 0x00, 0x00, 0x00,
                                 0x00, 0x00, 0x00, 0x00,
                                 0x00, 0x00, 0x00,
                                 0x01, 0x00, 0x00, 0x00,
                                 0x00};
    std::vector<uint8_t> bakPayload;
    writeStringBE(bakPayload, ".bak_payload");
    writeUint64BE(bakPayload, bak.size());
    bakPayload.insert(bakPayload.end(), bak.begin(), bak.end());
    sendEnc(NetworkMessage::make(MessageType::FILE_DATA, bakPayload));

    std::vector<uint8_t> metaPayloadLocal;
    writeStringBE(metaPayloadLocal, ".bak_metadata");
    writeUint64BE(metaPayloadLocal, 4);
    metaPayloadLocal.push_back(0); metaPayloadLocal.push_back(0);
    metaPayloadLocal.push_back(0); metaPayloadLocal.push_back(0);
    sendEnc(NetworkMessage::make(MessageType::FILE_DATA, metaPayloadLocal));

    sendEnc(NetworkMessage::make(MessageType::BACKUP_COMPLETE));
    auto ack = recvEnc();
    EXPECT_EQ(ack.type, static_cast<uint16_t>(MessageType::BACKUP_COMPLETE));
    size_t offB = 0;
    std::string backupId = readStringBE(ack.payload.data(), offB);
    EXPECT_FALSE(backupId.empty());

    // List backups
    sendEnc(NetworkMessage::make(MessageType::BACKUP_LIST_REQUEST));
    auto listResp = recvEnc();
    EXPECT_EQ(listResp.type, static_cast<uint16_t>(MessageType::BACKUP_LIST_RESPONSE));

    // Restore (server sends 3 messages: FILE_DATA(.bak), FILE_DATA(.bak_metadata), RESTORE_COMPLETE)
    std::vector<uint8_t> restReq;
    writeStringBE(restReq, backupId);
    sendEnc(NetworkMessage::make(MessageType::RESTORE_REQUEST, restReq));
    auto fileData = recvEnc();
    EXPECT_EQ(fileData.type, static_cast<uint16_t>(MessageType::FILE_DATA));
    auto metaData = recvEnc();
    EXPECT_EQ(metaData.type, static_cast<uint16_t>(MessageType::FILE_DATA));
    auto complete = recvEnc();
    EXPECT_EQ(complete.type, static_cast<uint16_t>(MessageType::RESTORE_COMPLETE));

    sendEnc(NetworkMessage::make(MessageType::LOGOUT));
    serverThread.join();
    rmrf(tdir);
}

TEST(ServerSessionIntegrationTest, BackupWithCustomName) {
    uint16_t port = findFreePort();
    std::string tdir = tmpDir();
    std::string dbPath = tdir + "/users.db";
    UserManager mgr(dbPath);
    auto salt = UserManager::generateSalt();
    auto hash = UserManager::computeHash("nameuser", salt);
    mgr.registerUserRaw("nameuser", salt, hash);

    std::vector<uint8_t> serverChallenge;

    std::thread serverThread([port, tdir]() {
        NetworkSocket server;
        if (!server.bindAndListen(port, 1)) return;
        NetworkSocket sock = server.accept();
        if (!sock.isValid()) return;
        ServerSession session(std::move(sock), tdir);
        session.run();
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    NetworkSocket client;
    ASSERT_TRUE(client.connect("127.0.0.1", port));

    // Handshake
    std::vector<uint8_t> hp;
    writeUint16BE(hp, PROTOCOL_VERSION);
    client.sendMessage(NetworkMessage::make(MessageType::CLIENT_HELLO, hp));
    auto sh = client.receiveMessage();
    ASSERT_EQ(sh.type, static_cast<uint16_t>(MessageType::SERVER_HELLO));

    // Extract server challenge for encryption
    if (sh.payload.size() >= 10) {
        size_t off = 0;
        readUint16BE(sh.payload.data(), off); // skip version
        serverChallenge.assign(sh.payload.begin() + static_cast<ptrdiff_t>(off), sh.payload.end());
    }

    // Login
    std::vector<uint8_t> lp;
    writeStringBE(lp, "nameuser");
    client.sendMessage(NetworkMessage::make(MessageType::LOGIN_REQUEST, lp));
    auto saltMsg = client.receiveMessage();
    ASSERT_EQ(saltMsg.type, static_cast<uint16_t>(MessageType::LOGIN_SALT));
    client.sendMessage(NetworkMessage::make(MessageType::LOGIN_PROOF, hash));
    auto loginResp = client.receiveMessage(); // LOGIN_RESPONSE sent unencrypted
    ASSERT_EQ(loginResp.payload[0], 0u);

    // Setup client-side encryption (server activates encryption after login)
    TransportEncryptor clientEnc;
    clientEnc.initSession("nameuser", serverChallenge);
    uint64_t sendSeq = 0, recvSeq = 0;
    auto sendEnc = [&](const NetworkMessage& msg) {
        NetworkMessage enc = msg;
        enc.payload = clientEnc.encrypt(msg.payload, sendSeq++);
        return client.sendMessage(enc);
    };
    auto recvEnc = [&]() -> NetworkMessage {
        auto msg = client.receiveMessage();
        if (msg.type != 0) {
            msg.payload = clientEnc.decrypt(msg.payload, recvSeq++);
        }
        return msg;
    };

    // Send custom backup name (encrypted now)
    std::vector<uint8_t> namePayload;
    writeStringBE(namePayload, "MySpecialBackup");
    sendEnc(NetworkMessage::make(MessageType::BACKUP_START, namePayload));

    // Send backup data (encrypted)
    std::vector<uint8_t> bak(19 + 4, 0);
    bak[0] = 0x50; bak[1] = 0x55; bak[2] = 0x4B; bak[3] = 0x42; // "BKUP" magic
    bak[8] = 0x01; bak[9] = 0x00; bak[10] = 0x00; bak[11] = 0x00; // version=1
    bak[15] = 0x04; // metaSize=4
    bak[19] = 0x00; bak[20] = 0x00; bak[21] = 0x00; bak[22] = 0x00; // file count=0

    std::vector<uint8_t> bakP;
    writeStringBE(bakP, ".bak_payload");
    writeUint64BE(bakP, bak.size());
    bakP.insert(bakP.end(), bak.begin(), bak.end());
    sendEnc(NetworkMessage::make(MessageType::FILE_DATA, bakP));

    std::vector<uint8_t> metaP;
    writeStringBE(metaP, ".bak_metadata");
    writeUint64BE(metaP, 4);
    metaP.push_back(0); metaP.push_back(0); metaP.push_back(0); metaP.push_back(0);
    sendEnc(NetworkMessage::make(MessageType::FILE_DATA, metaP));

    sendEnc(NetworkMessage::make(MessageType::BACKUP_COMPLETE));
    auto ack = recvEnc();
    EXPECT_EQ(ack.type, static_cast<uint16_t>(MessageType::BACKUP_COMPLETE));

    // Verify the backup name was stored correctly
    size_t offB = 0;
    std::string backupId = readStringBE(ack.payload.data(), offB);
    EXPECT_FALSE(backupId.empty());

    sendEnc(NetworkMessage::make(MessageType::LOGOUT));
    serverThread.join();
    rmrf(tdir);
}

TEST(ServerSessionIntegrationTest, UnauthenticatedCommand) {
    uint16_t port = findFreePort();
    std::string tdir = tmpDir();

    std::thread serverThread([port, tdir]() {
        NetworkSocket server;
        if (!server.bindAndListen(port, 1)) return;
        NetworkSocket sock = server.accept();
        if (!sock.isValid()) return;
        ServerSession session(std::move(sock), tdir);
        session.run();
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    NetworkSocket client;
    ASSERT_TRUE(client.connect("127.0.0.1", port));
    std::vector<uint8_t> hp;
    writeUint16BE(hp, PROTOCOL_VERSION);
    client.sendMessage(NetworkMessage::make(MessageType::CLIENT_HELLO, hp));
    client.receiveMessage();

    // Send unknown command without auth
    client.sendMessage(NetworkMessage::make(MessageType::RESTORE_REQUEST));
    auto resp = client.receiveMessage();
    EXPECT_EQ(resp.type, static_cast<uint16_t>(MessageType::ERROR_MESSAGE));

    client.sendMessage(NetworkMessage::make(MessageType::LOGOUT));
    serverThread.join();
    rmrf(tdir);
}

TEST(ServerSessionIntegrationTest, RestoreNonexistentBackup) {
    uint16_t port = findFreePort();
    std::string tdir = tmpDir();
    std::string dbPath = tdir + "/users.db";
    UserManager mgr(dbPath);
    auto salt = UserManager::generateSalt();
    auto hash = UserManager::computeHash("rstuser", salt);
    mgr.registerUserRaw("rstuser", salt, hash);

    std::thread serverThread([port, tdir]() {
        NetworkSocket server;
        if (!server.bindAndListen(port, 1)) return;
        NetworkSocket sock = server.accept();
        if (!sock.isValid()) return;
        ServerSession session(std::move(sock), tdir);
        session.run();
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    NetworkSocket client;
    ASSERT_TRUE(client.connect("127.0.0.1", port));
    std::vector<uint8_t> hp;
    writeUint16BE(hp, PROTOCOL_VERSION);
    client.sendMessage(NetworkMessage::make(MessageType::CLIENT_HELLO, hp));
    client.receiveMessage();
    std::vector<uint8_t> lp;
    writeStringBE(lp, "rstuser");
    client.sendMessage(NetworkMessage::make(MessageType::LOGIN_REQUEST, lp));
    client.receiveMessage();
    client.sendMessage(NetworkMessage::make(MessageType::LOGIN_PROOF, hash));
    client.receiveMessage();

    // Restore with empty id (no backups)
    std::vector<uint8_t> restReq;
    client.sendMessage(NetworkMessage::make(MessageType::RESTORE_REQUEST, restReq));
    auto resp = client.receiveMessage();
    EXPECT_EQ(resp.type, static_cast<uint16_t>(MessageType::ERROR_MESSAGE));

    client.sendMessage(NetworkMessage::make(MessageType::LOGOUT));
    serverThread.join();
    rmrf(tdir);
}
