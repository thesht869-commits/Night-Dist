#include "defines.hpp"
#include "debug/log/Logger.hpp"
#include "Compositor.hpp"
#include "config/ConfigManager.hpp"
#include "init/initHelpers.hpp"
#include "debug/HyprCtl.hpp"
#include "helpers/env/Env.hpp"

#include <csignal>
#include <cstdio>
#include <hyprutils/string/String.hpp>
#include <hyprutils/memory/Casts.hpp>
#include <print>
using namespace Hyprutils::String;
using namespace Hyprutils::Memory;

#include <fcntl.h>
#include <iostream>
#include <iterator>
#include <vector>
#include <stdexcept>
#include <string>
#include <string_view>
#include <span>
#include <filesystem>

static void help() {
    std::println("NightOS (Night-Dist) - Usage: NightOS [arg [...]].\n");
    std::println(R"#(Arguments:
    --help              -h       - Show this message
    --config FILE       -c FILE  - Specify custom NightOS config
    --socket NAME                - Sets the Wayland socket name
    --wayland-fd FD              - Sets the Wayland socket fd
    --safe-mode                  - Starts NightOS in safe mode
    --systeminfo                 - Prints system info for Night-Dist
    --i-am-really-stupid         - Omits root user check
    --verify-config              - Only check config for errors
    --version           -v       - Print NightOS Engine version)#");
}

static void reapZombieChildrenAutomatically() {
    struct sigaction act;
    act.sa_handler = SIG_DFL;
    sigemptyset(&act.sa_mask);
    act.sa_flags = SA_NOCLDWAIT;
#ifdef SA_RESTORER
    act.sa_restorer = NULL;
#endif
    sigaction(SIGCHLD, &act, nullptr);
}

int main(int argc, char** argv) {

    if (!getenv("XDG_RUNTIME_DIR"))
        throwError("XDG_RUNTIME_DIR is not set! NightOS cannot start.");

    std::string cmd = argv[0];
    for (int i = 1; i < argc; ++i)
        cmd += std::string(" ") + argv[i];

    setenv("HYPRLAND_CMD", cmd.c_str(), 1);
    setenv("XDG_BACKEND", "wayland", 1);
    setenv("XDG_SESSION_TYPE", "wayland", 1);
    setenv("_JAVA_AWT_WM_NONREPARENTING", "1", 1);
    setenv("MOZ_ENABLE_WAYLAND", "1", 1);

    std::string configPath;
    std::string socketName;
    int         socketFd   = -1;
    bool         ignoreSudo = false, verifyConfig = false, safeMode = false;
    int          watchdogFd = -1;

    if (argc > 1) {
        std::span<char*> args{argv + 1, sc<std::size_t>(argc - 1)};

        for (auto it = args.begin(); it != args.end(); it++) {
            std::string_view value = *it;

            if (value == "--i-am-really-stupid" && !ignoreSudo) {
                std::println("[ NIGHT-WARN ] Running with root privileges is dangerous!");
                ignoreSudo = true;
            } else if (value == "--socket") {
                if (std::next(it) == args.end()) { help(); return 1; }
                socketName = *std::next(it);
                it++;
            } else if (value == "--wayland-fd") {
                if (std::next(it) == args.end()) { help(); return 1; }
                try {
                    socketFd = std::stoi(*std::next(it));
                    if (fcntl(socketFd, F_GETFD) == -1) throw std::exception();
                } catch (...) {
                    std::println(stderr, "[ NIGHT-ERROR ] Invalid Wayland FD!");
                    return 1;
                }
                it++;
            } else if (value == "-c" || value == "--config") {
                if (std::next(it) == args.end()) { help(); return 1; }
                configPath = *std::next(it);
                try {
                    configPath = std::filesystem::canonical(configPath);
                } catch (...) {
                    std::println(stderr, "[ NIGHT-ERROR ] Config '{}' not found!", configPath);
                    return 1;
                }
                it++;
                continue;
            } else if (value == "-h" || value == "--help") {
                help();
                return 0;
            } else if (value == "-v" || value == "--version") {
                std::println("NightOS Engine based on Hyprland");
                std::println("{}", versionRequest(eHyprCtlOutputFormat::FORMAT_NORMAL, ""));
                return 0;
            } else if (value == "--verify-config") {
                verifyConfig = true;
                continue;
            } else if (value == "--safe-mode") {
                safeMode = true;
                continue;
            } else {
                std::println(stderr, "[ ERROR ] Unknown option '{}' !", value);
                return 1;
            }
        }
    }

    if (!ignoreSudo && NInit::isSudo()) {
        std::println(stderr, "[ ERROR ] NightOS cannot be run as root without --i-am-really-stupid.");
        return 1;
    }

    if (!verifyConfig) {
        std::println("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━");
        std::println("  Welcome to NightOS (Night-Dist Project)");
        std::println("  Engine Status: Operational | The night is yours.");
        std::println("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━");
    }

    try {
        g_pCompositor                     = makeUnique<CCompositor>(verifyConfig);
        g_pCompositor->m_explicitConfigPath = configPath;
    } catch (const std::exception& e) {
        std::println(stderr, "NightOS Core Failure: {}\n", e.what());
        return 1;
    }

    reapZombieChildrenAutomatically();

    if (watchdogFd > 0) g_pCompositor->setWatchdogFd(watchdogFd);
    if (safeMode) g_pCompositor->m_safeMode = true;

    g_pCompositor->initServer(socketName, socketFd);

    if (verifyConfig)
        return !g_pConfigManager->m_lastConfigVerificationWasSuccessful;

    if (!Env::envEnabled("HYPRLAND_NO_RT"))
        NInit::gainRealTime();

    Log::logger->log(Log::DEBUG, "NightOS init finished. Darkness engaged.");

    g_pCompositor->startCompositor();
    g_pCompositor->cleanup();
    g_pCompositor.reset();

    Log::logger->log(Log::DEBUG, "NightOS has reached the end. Sleep well.");

    return EXIT_SUCCESS;
}