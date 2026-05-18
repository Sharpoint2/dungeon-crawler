#include <cerrno>
#include <climits>
#include <cstring>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <string>
#include <string_view>
#include <vector>
#include <unistd.h>

namespace {

std::string shellQuote(const std::string& value) {
    std::string quoted = "'";
    for (char ch : value) {
        if (ch == '\'') {
            quoted += "'\\''";
        } else {
            quoted += ch;
        }
    }
    quoted += "'";
    return quoted;
}

std::string getExecutableDirectory(char* argv0) {
    char resolvedPath[PATH_MAX];
    ssize_t length = readlink("/proc/self/exe", resolvedPath, sizeof(resolvedPath) - 1);
    if (length > 0) {
        resolvedPath[length] = '\0';
        return std::filesystem::path(resolvedPath).parent_path().string();
    }

    std::filesystem::path fallback = std::filesystem::absolute(argv0 ? argv0 : "./dungeon_crawler");
    return fallback.parent_path().string();
}

struct TerminalCandidate {
    std::string command;
    std::vector<std::string> arguments;
};

bool hasDisplayServer() {
    return std::getenv("DISPLAY") != nullptr || std::getenv("WAYLAND_DISPLAY") != nullptr;
}

bool tryLaunchTerminal(const TerminalCandidate& candidate, const std::string& gamePath) {
    std::vector<std::string> commandArguments;
    commandArguments.reserve(candidate.arguments.size() + 2);
    for (const auto& argument : candidate.arguments) {
        commandArguments.push_back(argument);
    }
    commandArguments.push_back("bash");
    commandArguments.push_back("-lc");
    commandArguments.push_back("exec " + shellQuote(gamePath));

    std::vector<char*> argv;
    argv.reserve(commandArguments.size() + 2);
    argv.push_back(const_cast<char*>(candidate.command.c_str()));
    for (auto& argument : commandArguments) {
        argv.push_back(argument.data());
    }
    argv.push_back(nullptr);

    execvp(candidate.command.c_str(), argv.data());
    return false;
}

int runGameDirectly(const std::string& gamePath) {
    execl(gamePath.c_str(), gamePath.c_str(), static_cast<char*>(nullptr));
    std::cerr << "Unable to launch game binary: " << std::strerror(errno) << std::endl;
    return 1;
}

}  // namespace

int main(int argc, char* argv[]) {
    const std::string launcherDirectory = getExecutableDirectory(argc > 0 ? argv[0] : nullptr);
    const std::string gamePath = (std::filesystem::path(launcherDirectory) / "dungeon_crawler_game").string();

    if (!std::filesystem::exists(gamePath)) {
        std::cerr << "Missing game binary: " << gamePath << std::endl;
        return 1;
    }

    if (hasDisplayServer()) {
        const std::vector<TerminalCandidate> candidates = {
            {"x-terminal-emulator", {"-e"}},
            {"gnome-terminal", {"--"}},
            {"konsole", {"-e"}},
            {"xterm", {"-e"}},
            {"kitty", {}},
            {"alacritty", {"-e"}},
            {"xfce4-terminal", {"-e"}},
            {"mate-terminal", {"--"}},
            {"ptyxis", {"--"}},
            {"terminator", {"-x"}},
            {"tilix", {"-e"}},
            {"lxterminal", {"-e"}}
        };

        for (const auto& candidate : candidates) {
            tryLaunchTerminal(candidate, gamePath);
        }
    }

    return runGameDirectly(gamePath);
}
