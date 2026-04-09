#include "CommandRegistry.h"
#include <iostream>
#include <iomanip>

namespace AgentVST::CLI {

void CommandRegistry::registerCommand(Command cmd) {
    commands_[cmd.name] = std::move(cmd);
}

int CommandRegistry::dispatch(const std::vector<std::string>& args) const {
    if (args.empty()) {
        printHelp();
        return 0;
    }

    const std::string& name = args[0];

    if (name == "--help" || name == "-h" || name == "help") {
        printHelp();
        return 0;
    }

    auto it = commands_.find(name);
    if (it == commands_.end()) {
        std::cerr << "agentvst: unknown command '" << name << "'\n"
                  << "Run 'agentvst --help' for a list of commands.\n";
        return 1;
    }

    std::vector<std::string> subArgs(args.begin() + 1, args.end());
    return it->second.run(subArgs);
}

void CommandRegistry::printHelp() const {
    std::cout << "AgentVST CLI  v" AGENTVST_VERSION "\n\n"
              << "USAGE\n"
              << "  agentvst <command> [arguments]\n\n"
              << "COMMANDS\n";

    for (const auto& [name, cmd] : commands_) {
        std::cout << "  " << std::left << std::setw(20) << cmd.usage
                  << "  " << cmd.description << "\n";
    }

    std::cout << "\n  agentvst --help               Show this help message\n";
}

} // namespace AgentVST::CLI
