#include "core/BackupEngine.h"
#include "core/RestoreEngine.h"
#include "pack/TarPackStrategy.h"
#include "pack/IndexPackStrategy.h"
#include "pack/PackManager.h"
#include "compress/RleCompressStrategy.h"
#include "compress/HuffmanCompressStrategy.h"
#include "compress/CompressManager.h"
#include "encrypt/XorEncryptStrategy.h"
#include "encrypt/VigenereEncryptStrategy.h"
#include "encrypt/EncryptManager.h"
#include <iostream>
#include <string>
#include <cstring>

void printUsage() {
    std::cout << R"(Usage:
  backup backup <source_dir> <output_file> [options]
  backup restore <input_file> <dest_dir> [options]

Options:
  --pack <tar|index>        Pack strategy (default: tar)
  --compress <rle|huffman>  Compress strategy (default: none)
  --encrypt <xor|vigenere>  Encrypt strategy (default: none)
  --password <password>     Password for encryption
  --help                    Show this help

Examples:
  backup backup /home/user/data ./mybackup.bak --pack tar
  backup backup /home/user/data ./mybackup.bak --pack index --compress rle
  backup backup /home/user/data ./mybackup.bak --pack tar --encrypt xor --password secret
  backup restore ./mybackup.bak /home/user/restored --password secret
)";
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

    if (command != "backup" && command != "restore") {
        std::cerr << "Error: unknown command '" << command << "'" << std::endl;
        printUsage();
        return 1;
    }

    // 解析选项
    std::string sourceOrInput;
    std::string destOrOutput;
    std::string packName = "tar";
    std::string compressName;
    std::string encryptName;
    std::string password;

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

    // 初始化管理器并注册所有策略（供备份和还原共用）
    PackManager packMgr;
    packMgr.registerStrategy(std::make_unique<TarPackStrategy>());
    packMgr.registerStrategy(std::make_unique<IndexPackStrategy>());

    CompressManager compressMgr;
    compressMgr.registerStrategy(std::make_unique<RleCompressStrategy>());
    compressMgr.registerStrategy(std::make_unique<HuffmanCompressStrategy>());

    EncryptManager encryptMgr;
    encryptMgr.registerStrategy(std::make_unique<XorEncryptStrategy>());
    encryptMgr.registerStrategy(std::make_unique<VigenereEncryptStrategy>());

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
                std::cerr << "Error: password required for encryption." << std::endl;
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
        // 还原时，引擎根据文件头中的 algoId 自动从 Manager 中选择策略
        // 只需提供 Manager 即可，不需要提前知道用了哪种算法
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
