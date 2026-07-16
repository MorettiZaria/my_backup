#include "gtest/gtest.h"
#include "network/NetworkSocket.h"
#include "network/NetworkProtocol.h"
#include "network/TransportEncryptor.h"
#include "network/UserManager.h"
#include "network/ServerStorage.h"
#include "network/ServerSession.h"
#include "network/ServerConfig.h"
#include "network/BackupServer.h"
#include "core/FileInfo.h"
#include <sys/stat.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <thread>
#include <chrono>
#include <unistd.h>
#include <fstream>
#include <signal.h>

namespace {
    std::string tmpDir() {
        std::string d = "/tmp/netloop_gtest_" + std::to_string(getpid());
        mkdir(d.c_str(), 0755);
        return d;
    }
    void rmrf(const std::string& path) {
        std::string cmd = "rm -rf " + path + " 2>/dev/null";
        (void)system(cmd.c_str());
    }

    // Find an available port
    uint16_t findFreePort() {
        int fd = socket(AF_INET, SOCK_STREAM, 0);
        struct sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = INADDR_ANY;
        addr.sin_port = 0;
        if (bind(fd, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr)) < 0) {
            close(fd);
            return 18849;
        }
        socklen_t len = sizeof(addr);
        getsockname(fd, reinterpret_cast<struct sockaddr*>(&addr), &len);
        uint16_t port = ntohs(addr.sin_port);
        close(fd);
        return port;
    }
}

// ===================== NetworkSocket Tests =====================

TEST(NetworkSocketTest, DefaultInvalid) {
    NetworkSocket s;
    EXPECT_FALSE(s.isValid());
    EXPECT_EQ(s.fd(), -1);
}

TEST(NetworkSocketTest, MoveConstructor) {
    NetworkSocket s1;
    NetworkSocket s2(std::move(s1));
    EXPECT_FALSE(s2.isValid());
}

TEST(NetworkSocketTest, MoveAssignment) {
    NetworkSocket s1;
    NetworkSocket s2;
    s2 = std::move(s1);
    EXPECT_FALSE(s2.isValid());
}

TEST(NetworkSocketTest, ServerBindAndAccept) {
    uint16_t port = findFreePort();
    NetworkSocket server;
    ASSERT_TRUE(server.bindAndListen(port, 1));
    EXPECT_TRUE(server.isValid());
}

TEST(NetworkSocketTest, ClientConnectToServer) {
    uint16_t port = findFreePort();
    // Start server in a thread
    std::thread serverThread([port]() {
        NetworkSocket server;
        server.bindAndListen(port, 1);
        NetworkSocket client = server.accept();
        // Echo back
        auto msg = client.receiveMessage();
        client.sendMessage(msg);
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    NetworkSocket client;
    ASSERT_TRUE(client.connect("127.0.0.1", port));
    EXPECT_TRUE(client.isValid());

    auto hello = NetworkMessage::make(MessageType::CLIENT_HELLO);
    EXPECT_TRUE(client.sendMessage(hello));

    auto resp = client.receiveMessage();
    EXPECT_EQ(resp.type, static_cast<uint16_t>(MessageType::CLIENT_HELLO));

    serverThread.join();
}

TEST(NetworkSocketTest, ConnectInvalidHost) {
    NetworkSocket s;
    EXPECT_FALSE(s.connect("999.999.999.999", 1234));
}

TEST(NetworkSocketTest, ConnectRefused) {
    uint16_t port = findFreePort();
    NetworkSocket s;
    // Port should be free since we just allocated it but didn't bind
    EXPECT_FALSE(s.connect("127.0.0.1", port));
}

TEST(NetworkSocketTest, SetReceiveTimeout) {
    NetworkSocket s;
    EXPECT_FALSE(s.setReceiveTimeout(1)); // invalid fd
}

TEST(NetworkSocketTest, ReceiveOnInvalid) {
    NetworkSocket s;
    auto msg = s.receiveMessage();
    EXPECT_EQ(msg.type, 0u);
}

TEST(NetworkSocketTest, SendOnInvalid) {
    NetworkSocket s;
    auto msg = NetworkMessage::make(MessageType::CLIENT_HELLO);
    EXPECT_FALSE(s.sendMessage(msg));
}

TEST(NetworkSocketTest, SendReceiveRoundTrip) {
    uint16_t port = findFreePort();
    std::thread serverThread([port]() {
        NetworkSocket server;
        server.bindAndListen(port, 1);
        NetworkSocket sock = server.accept();
        auto msg = sock.receiveMessage();
        EXPECT_EQ(msg.type, static_cast<uint16_t>(MessageType::FILE_DATA));
        sock.sendMessage(msg);
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    NetworkSocket client;
    client.connect("127.0.0.1", port);
    std::vector<uint8_t> payload = {0x01, 0x02, 0x03};
    auto msg = NetworkMessage::make(MessageType::FILE_DATA, payload);
    EXPECT_TRUE(client.sendMessage(msg));
    auto resp = client.receiveMessage();
    EXPECT_EQ(resp.payload, payload);

    serverThread.join();
}

// ===================== ServerSession via Local Client =====================

// 完整注册→登录流程：先注册新用户，登出后用该用户登录
TEST(ServerSessionTest, RegisterAndLoginFlow) {
    std::string tdir = tmpDir();

    // 预先生成凭证（注册和登录共用）
    std::string username = "testuser";
    auto salt = UserManager::generateSalt();
    auto hash = UserManager::computeHash(username, salt);

    // --- 第一步：注册 ---
    {
        uint16_t port = findFreePort();
        std::thread serverThread([port, &tdir]() {
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
        client.sendMessage(NetworkMessage::make(MessageType::CLIENT_HELLO, std::move(hp)));
        auto sh = client.receiveMessage();
        ASSERT_EQ(sh.type, static_cast<uint16_t>(MessageType::SERVER_HELLO));

        // Register
        std::vector<uint8_t> rp;
        writeStringBE(rp, username);
        rp.insert(rp.end(), salt.begin(), salt.end());
        rp.insert(rp.end(), hash.begin(), hash.end());
        client.sendMessage(NetworkMessage::make(MessageType::REGISTER_REQUEST, std::move(rp)));

        auto regResp = client.receiveMessage();
        ASSERT_EQ(regResp.type, static_cast<uint16_t>(MessageType::REGISTER_RESPONSE));
        ASSERT_EQ(regResp.payload[0], 0u);  // success

        // 登出
        client.sendMessage(NetworkMessage(static_cast<uint16_t>(MessageType::LOGOUT), {}));
        serverThread.join();
    }

    // --- 第二步：用刚才注册的账号登录 ---
    {
        uint16_t port = findFreePort();
        std::thread serverThread([port, &tdir]() {
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
        client.sendMessage(NetworkMessage::make(MessageType::CLIENT_HELLO, std::move(hp)));
        auto sh = client.receiveMessage();
        ASSERT_EQ(sh.type, static_cast<uint16_t>(MessageType::SERVER_HELLO));

        // Login
        std::vector<uint8_t> lp;
        writeStringBE(lp, username);
        client.sendMessage(NetworkMessage::make(MessageType::LOGIN_REQUEST, std::move(lp)));

        auto saltMsg = client.receiveMessage();
        ASSERT_EQ(saltMsg.type, static_cast<uint16_t>(MessageType::LOGIN_SALT));

        // 发送密码哈希
        client.sendMessage(NetworkMessage::make(MessageType::LOGIN_PROOF, hash));
        auto loginResp = client.receiveMessage();
        ASSERT_EQ(loginResp.type, static_cast<uint16_t>(MessageType::LOGIN_RESPONSE));
        EXPECT_EQ(loginResp.payload[0], 0u);  // login success

        // 登出
        client.sendMessage(NetworkMessage(static_cast<uint16_t>(MessageType::LOGOUT), {}));
        serverThread.join();
    }

    rmrf(tdir);
}

// ===================== TransportEncryptor wired encrypt/decrypt =====================

TEST(TransportEncryptorWiredTest, EncryptedCommunication) {
    uint16_t port = findFreePort();
    std::vector<uint8_t> serverSalt = {0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF, 0x00, 0x11};

    std::thread serverThread([port, &serverSalt]() {
        NetworkSocket server;
        server.bindAndListen(port, 1);
        NetworkSocket sock = server.accept();
        TransportEncryptor te;
        te.initSession("sharedkey", serverSalt);

        auto msg = sock.receiveMessage();
        // decrypt
        msg.payload = te.decrypt(msg.payload, 0);
        // encrypt response
        msg.payload = te.encrypt(msg.payload, 0);
        sock.sendMessage(msg);
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    NetworkSocket client;
    client.connect("127.0.0.1", port);
    TransportEncryptor te;
    te.initSession("sharedkey", serverSalt);

    std::vector<uint8_t> plain = {0x10, 0x20, 0x30, 0x40};
    auto enc = te.encrypt(plain, 0);
    auto msg = NetworkMessage::make(MessageType::FILE_DATA, enc);
    client.sendMessage(msg);
    auto resp = client.receiveMessage();
    auto dec = te.decrypt(resp.payload, 0);
    EXPECT_EQ(dec, plain);

    serverThread.join();
}
