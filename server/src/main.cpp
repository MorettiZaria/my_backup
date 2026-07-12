/// Dedicated backup server daemon entry point
/// Usage: backup-server [--config <path>] [--port <port>] [--storage <path>] [--log-file <path>]

#include "network/BackupServer.h"
#include "network/Logger.h"
#include "network/ServerConfig.h"

#include <iostream>
#include <string>
#include <cstring>
#include <cstdlib>

int main(int argc, char* argv[]) {
    ServerConfig config;
    std::string configPath;

    // Parse --config first (so CLI overrides apply after config load)
    for (int i = 1; i < argc; ++i) {
        if (std::string(argv[i]) == "--config" && i + 1 < argc) {
            configPath = argv[++i];
        }
    }

    // Load config file if specified
    if (!configPath.empty()) {
        config.load(configPath);
    }

    // Parse CLI arguments (override config file)
    for (int i = 1; i < argc; ++i) {
        std::string opt = argv[i];
        if (opt == "--port" && i + 1 < argc) {
            config.setPort(static_cast<uint16_t>(std::stoi(argv[++i])));
        } else if (opt == "--storage" && i + 1 < argc) {
            config.setStoragePath(argv[++i]);
        } else if (opt == "--log-file" && i + 1 < argc) {
            config.setLogFile(argv[++i]);
        } else if (opt == "--config" && i + 1 < argc) {
            ++i;  // already handled
        } else if (opt == "--help" || opt == "-h") {
            std::cout << R"(Usage: backup-server [options]

Options:
  --config <path>   Server configuration file
  --port <port>     Listening port (default: 8848)
  --storage <path>  Data storage directory (default: ./server_data)
  --log-file <path> Log file path (default: stdout only)
  --help            Show this help
)";
            return 0;
        }
    }

    // Initialize Logger if log file configured
    if (!config.logFile().empty()) {
        Logger::instance().init(config.logFile(), false);
    }

    BackupServer server(config);
    return server.start() ? 0 : 1;
}
