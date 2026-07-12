#include "core/BackupEngine.h"
#include "core/RestoreEngine.h"
#include "core/StrategyFactory.h"

// 网络模块
#include "network/BackupServer.h"
#include "network/NetworkBackupClient.h"
#include "network/NetworkRestoreClient.h"
#include "network/UserManager.h"
#include "network/Logger.h"
#include "network/ServerConfig.h"

#include <iostream>
#include <string>
#include <cstring>
#include <cstdlib>

void printUsage() {
    std::cout << R"(Usage:
  backup backup <source_dir> <output_file> [options]
  backup restore <input_file> <dest_dir> [options]

  backup server start --port <port> --storage <path>
  backup remote-backup <source_dir> --server <host:port> --username <name> --password <pass> [options]
  backup remote-restore <dest_dir> --server <host:port> --username <name> --password <pass> [options]
  backup remote-list --server <host:port> --username <name> --password <pass>
  backup user register --server <host:port> --username <name> --password <pass>
  backup user login --server <host:port> --username <name> --password <pass>

Options:
  --pack <tar|index>        Pack strategy (default: tar)
  --compress <rle|huffman>  Compress strategy (default: none)
  --encrypt <xor|vigenere>  Encrypt strategy (default: none)
  --password <password>     Password for file encryption (local mode)
  --file-password <pwd>     Password for file encryption (remote mode)
  --backup-id <id>          Backup ID for restore (remote mode, default: latest)
  --port <port>             Server port (default: 8848)
  --storage <path>          Server storage path (default: ./server_data)
  --server <host:port>      Server address (remote mode)
  --username <name>         Username (remote mode)
  --help                    Show this help

Examples:
  backup backup /home/user/data ./mybackup.bak --pack tar
  backup restore ./mybackup.bak /home/user/restored --password secret
  backup server start --port 8848 --storage ./server_data
  backup remote-backup /home/user/data --server 127.0.0.1:8848 --username zaria --password mypass --pack tar
  backup remote-restore /home/user/restored --server 127.0.0.1:8848 --username zaria --password mypass
)";
}

// 解析 host:port 格式
static bool parseHostPort(const std::string& addr, std::string& host, uint16_t& port) {
    size_t colon = addr.find(':');
    if (colon == std::string::npos) {
        host = addr;
        port = 8848;
    } else {
        host = addr.substr(0, colon);
        port = static_cast<uint16_t>(std::stoi(addr.substr(colon + 1)));
    }
    return true;
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        printUsage();
        return 1;
    }

    std::string command = argv[1];

    if (command == "--help" || command == "-h") {
        printUsage();
        return 0;
    }

    // ===========================================================
    // 服务器模式
    // ===========================================================
    if (command == "server") {
        if (argc < 3 || std::string(argv[2]) != "start") {
            std::cerr << "Usage: backup server start [--config <path>] [--port <port>] [--storage <path>] [--log-file <path>]" << std::endl;
            return 1;
        }

        ServerConfig config;
        std::string configPath;

        // 先解析 CLI 参数：--config 优先加载
        int i = 3;
        while (i < argc) {
            std::string opt = argv[i];
            if (opt == "--config" && i + 1 < argc) {
                configPath = argv[++i];
            }
            ++i;
        }

        // 加载配置文件
        if (!configPath.empty()) {
            config.load(configPath);
        }

        // 再解析其余 CLI 参数（CLI 参数覆盖配置文件）
        i = 3;
        while (i < argc) {
            std::string opt = argv[i];
            if (opt == "--port" && i + 1 < argc) {
                config.setPort(static_cast<uint16_t>(std::stoi(argv[++i])));
            } else if (opt == "--storage" && i + 1 < argc) {
                config.setStoragePath(argv[++i]);
            } else if (opt == "--log-file" && i + 1 < argc) {
                config.setLogFile(argv[++i]);
            } else if (opt == "--config" && i + 1 < argc) {
                ++i;  // 跳过值（已处理）
            }
            ++i;
        }

        // 初始化 Logger（如果配置文件指定了 log_file）
        if (!config.logFile().empty()) {
            Logger::instance().init(config.logFile(), false);
        }

        BackupServer server(config);
        return server.start() ? 0 : 1;
    }

    // ===========================================================
    // 用户管理
    // ===========================================================
    if (command == "user") {
        if (argc < 3) {
            std::cerr << "Usage: backup user register|login --server <host:port> --username <name> --password <pass>" << std::endl;
            return 1;
        }
        std::string sub = argv[2];

        std::string serverAddr, username, password;
        int i = 3;
        while (i < argc) {
            std::string opt = argv[i];
            if (opt == "--server" && i + 1 < argc) serverAddr = argv[++i];
            else if (opt == "--username" && i + 1 < argc) username = argv[++i];
            else if (opt == "--password" && i + 1 < argc) password = argv[++i];
            ++i;
        }

        if (serverAddr.empty() || username.empty() || password.empty()) {
            std::cerr << "Error: --server, --username, --password are required." << std::endl;
            return 1;
        }

        std::string host;
        uint16_t port;
        parseHostPort(serverAddr, host, port);

        NetworkRestoreClient client(host, port, username, password);
        // user register/login 复用 login 握手流程
        // register: 先尝试 login，失败则自动 register
        // 简单方案：用 NetworkBackupClient 的 doRegister 逻辑
        // 这里直接用自定义流程——连接、握手、发送登录或注册
        NetworkSocket sock;
        if (!sock.connect(host, port)) {
            std::cerr << "Error: cannot connect to server." << std::endl;
            return 1;
        }

        // Handshake
        std::vector<uint8_t> hp;
        writeUint16BE(hp, 1);
        if (!sock.sendMessage(NetworkMessage::make(MessageType::CLIENT_HELLO, std::move(hp)))) {
            std::cerr << "Error: handshake failed." << std::endl;
            return 1;
        }
        NetworkMessage hello = sock.receiveMessage();
        if (hello.type != static_cast<uint16_t>(MessageType::SERVER_HELLO)) {
            std::cerr << "Error: server rejected connection." << std::endl;
            return 1;
        }

        if (sub == "register") {
            auto salt = UserManager::generateSalt();
            auto pwHash = UserManager::computeHash(password, salt);
            std::vector<uint8_t> payload;
            writeStringBE(payload, username);
            payload.insert(payload.end(), salt.begin(), salt.end());
            payload.insert(payload.end(), pwHash.begin(), pwHash.end());
            if (!sock.sendMessage(NetworkMessage::make(MessageType::REGISTER_REQUEST, std::move(payload)))) {
                std::cerr << "Error: failed to send register request." << std::endl;
                return 1;
            }
            NetworkMessage resp = sock.receiveMessage();
            if (resp.type == static_cast<uint16_t>(MessageType::REGISTER_RESPONSE) &&
                !resp.payload.empty() && resp.payload[0] == 0) {
                std::cout << "User registered successfully." << std::endl;
                return 0;
            }
            std::cerr << "Error: registration failed (user may already exist)." << std::endl;
            return 1;
        } else if (sub == "login") {
            std::vector<uint8_t> challenge;
            if (hello.payload.size() >= 10) {
                size_t off = 0;
                readUint16BE(hello.payload.data(), off);
                challenge.assign(hello.payload.begin() + static_cast<ptrdiff_t>(off), hello.payload.end());
            }
            // 第一步：发送用户名
            std::vector<uint8_t> payload;
            writeStringBE(payload, username);
            if (!sock.sendMessage(NetworkMessage::make(MessageType::LOGIN_REQUEST, std::move(payload)))) {
                std::cerr << "Error: failed to send login request." << std::endl;
                return 1;
            }
            // 第二步：接收盐值
            NetworkMessage saltMsg = sock.receiveMessage();
            if (saltMsg.type != static_cast<uint16_t>(MessageType::LOGIN_SALT)) {
                std::cerr << "Error: unexpected response (expected salt)." << std::endl;
                return 1;
            }
            // 第三步：用正确的盐值计算哈希
            auto computedHash = UserManager::computeHash(password, saltMsg.payload);
            if (!sock.sendMessage(NetworkMessage::make(MessageType::LOGIN_PROOF, computedHash))) {
                std::cerr << "Error: failed to send login proof." << std::endl;
                return 1;
            }
            // 第四步：接收结果
            NetworkMessage resp = sock.receiveMessage();
            if (resp.type == static_cast<uint16_t>(MessageType::LOGIN_RESPONSE) &&
                !resp.payload.empty() && resp.payload[0] == 0) {
                std::cout << "Login successful." << std::endl;
                return 0;
            }
            std::cerr << "Error: login failed (wrong password?)." << std::endl;
            return 1;
        } else {
            std::cerr << "Usage: backup user register|login ..." << std::endl;
            return 1;
        }
    }

    // ===========================================================
    // 远程备份
    // ===========================================================
    if (command == "remote-backup") {
        std::string sourceDir, serverAddr, username, password;
        std::string packName = "tar", compressName, encryptName, filePassword;

        int i = 2;
        if (i < argc && argv[i][0] != '-') sourceDir = argv[i++];
        while (i < argc) {
            std::string opt = argv[i];
            if (opt == "--server" && i + 1 < argc) serverAddr = argv[++i];
            else if (opt == "--username" && i + 1 < argc) username = argv[++i];
            else if (opt == "--password" && i + 1 < argc) password = argv[++i];
            else if (opt == "--pack" && i + 1 < argc) packName = argv[++i];
            else if (opt == "--compress" && i + 1 < argc) compressName = argv[++i];
            else if (opt == "--encrypt" && i + 1 < argc) encryptName = argv[++i];
            else if (opt == "--file-password" && i + 1 < argc) filePassword = argv[++i];
            ++i;
        }

        if (sourceDir.empty() || serverAddr.empty() || username.empty() || password.empty()) {
            std::cerr << "Error: source_dir, --server, --username, --password are required." << std::endl;
            return 1;
        }

        std::string host;
        uint16_t port;
        parseHostPort(serverAddr, host, port);

        // 初始化策略
        PackManager packMgr;
        CompressManager compressMgr;
        EncryptManager encryptMgr;
        registerAllStrategies(packMgr, compressMgr, encryptMgr);

        auto* pack = packMgr.select(packName);
        if (!pack) {
            std::cerr << "Error: unknown pack strategy '" << packName << "'" << std::endl;
            return 1;
        }

        ICompressStrategy* compress = nullptr;
        if (!compressName.empty()) {
            compress = compressMgr.select(compressName);
            if (!compress) {
                std::cerr << "Error: unknown compress strategy '" << compressName << "'" << std::endl;
                return 1;
            }
        }

        IEncryptStrategy* encrypt = nullptr;
        if (!encryptName.empty()) {
            encrypt = encryptMgr.select(encryptName);
            if (!encrypt) {
                std::cerr << "Error: unknown encrypt strategy '" << encryptName << "'" << std::endl;
                return 1;
            }
            if (filePassword.empty()) {
                std::cerr << "Error: --file-password required when using --encrypt." << std::endl;
                return 1;
            }
        }

        NetworkBackupClient client(host, port, username, password);
        return client.run(sourceDir, pack, compress, encrypt, filePassword) ? 0 : 1;
    }

    // ===========================================================
    // 远程还原
    // ===========================================================
    if (command == "remote-restore") {
        std::string destDir, serverAddr, username, password;
        std::string backupId, filePassword;

        int i = 2;
        if (i < argc && argv[i][0] != '-') destDir = argv[i++];
        while (i < argc) {
            std::string opt = argv[i];
            if (opt == "--server" && i + 1 < argc) serverAddr = argv[++i];
            else if (opt == "--username" && i + 1 < argc) username = argv[++i];
            else if (opt == "--password" && i + 1 < argc) password = argv[++i];
            else if (opt == "--backup-id" && i + 1 < argc) backupId = argv[++i];
            else if (opt == "--file-password" && i + 1 < argc) filePassword = argv[++i];
            ++i;
        }

        if (destDir.empty() || serverAddr.empty() || username.empty() || password.empty()) {
            std::cerr << "Error: dest_dir, --server, --username, --password are required." << std::endl;
            return 1;
        }

        std::string host;
        uint16_t port;
        parseHostPort(serverAddr, host, port);

        NetworkRestoreClient client(host, port, username, password);
        return client.run(destDir, backupId, filePassword) ? 0 : 1;
    }

    // ===========================================================
    // 列出远程备份
    // ===========================================================
    if (command == "remote-list") {
        std::string serverAddr, username, password;

        int i = 2;
        while (i < argc) {
            std::string opt = argv[i];
            if (opt == "--server" && i + 1 < argc) serverAddr = argv[++i];
            else if (opt == "--username" && i + 1 < argc) username = argv[++i];
            else if (opt == "--password" && i + 1 < argc) password = argv[++i];
            ++i;
        }

        if (serverAddr.empty() || username.empty() || password.empty()) {
            std::cerr << "Error: --server, --username, --password are required." << std::endl;
            return 1;
        }

        std::string host;
        uint16_t port;
        parseHostPort(serverAddr, host, port);

        NetworkRestoreClient client(host, port, username, password);
        return client.listBackups() ? 0 : 1;
    }

    // ===========================================================
    // 本地备份（保持原有逻辑）
    // ===========================================================
    if (command == "backup" || command == "restore") {
        // 解析选项
        std::string sourceOrInput;
        std::string destOrOutput;
        std::string packName = "tar";
        std::string compressName;
        std::string encryptName;
        std::string password;  // 本地模式的文件加密密码

        int argIdx = 2;

        if (argIdx < argc && argv[argIdx][0] != '-') {
            sourceOrInput = argv[argIdx++];
        }
        if (argIdx < argc && argv[argIdx][0] != '-') {
            destOrOutput = argv[argIdx++];
        }

        while (argIdx < argc) {
            std::string opt = argv[argIdx];
            if (opt == "--pack" && argIdx + 1 < argc) {
                packName = argv[++argIdx];
            } else if (opt == "--compress" && argIdx + 1 < argc) {
                compressName = argv[++argIdx];
            } else if (opt == "--encrypt" && argIdx + 1 < argc) {
                encryptName = argv[++argIdx];
            } else if (opt == "--password" && argIdx + 1 < argc) {
                password = argv[++argIdx];
            } else if (opt == "--help") {
                printUsage();
                return 0;
            } else {
                std::cerr << "Warning: unknown option '" << opt << "'" << std::endl;
            }
            ++argIdx;
        }

        if (sourceOrInput.empty() || destOrOutput.empty()) {
            std::cerr << "Error: missing required arguments." << std::endl;
            printUsage();
            return 1;
        }

        // 初始化管理器并注册所有策略
        PackManager packMgr;
        CompressManager compressMgr;
        EncryptManager encryptMgr;
        registerAllStrategies(packMgr, compressMgr, encryptMgr);

        if (command == "backup") {
            auto* pack = packMgr.select(packName);
            if (!pack) {
                std::cerr << "Error: unknown pack strategy '" << packName << "'" << std::endl;
                std::cerr << "Available: ";
                for (auto& n : packMgr.listNames()) std::cerr << n << " ";
                std::cerr << std::endl;
                return 1;
            }

            ICompressStrategy* compress = nullptr;
            if (!compressName.empty()) {
                compress = compressMgr.select(compressName);
                if (!compress) {
                    std::cerr << "Error: unknown compress strategy '" << compressName << "'" << std::endl;
                    return 1;
                }
            }

            IEncryptStrategy* encrypt = nullptr;
            if (!encryptName.empty()) {
                encrypt = encryptMgr.select(encryptName);
                if (!encrypt) {
                    std::cerr << "Error: unknown encrypt strategy '" << encryptName << "'" << std::endl;
                    return 1;
                }
                if (password.empty()) {
                    std::cerr << "Error: --password required for encryption." << std::endl;
                    return 1;
                }
            }

            BackupEngine engine;
            engine.setPackStrategy(pack);
            engine.setCompressStrategy(compress);
            engine.setEncryptStrategy(encrypt);

            if (!engine.run(sourceOrInput, destOrOutput, password)) {
                return 1;
            }

        } else if (command == "restore") {
            RestoreEngine engine;
            engine.setPackManager(&packMgr);
            engine.setCompressManager(&compressMgr);
            engine.setEncryptManager(&encryptMgr);

            if (!engine.run(sourceOrInput, destOrOutput, password)) {
                return 1;
            }
        }

        return 0;
    }

    // ===========================================================
    // 未知命令
    // ===========================================================
    std::cerr << "Error: unknown command '" << command << "'" << std::endl;
    printUsage();
    return 1;
}
