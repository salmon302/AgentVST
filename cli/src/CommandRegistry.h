#pragma once
#include <string>
#include <vector>
#include <functional>
#include <map>

namespace AgentVST::CLI {

struct Command {
    std::string name;
    std::string usage;
    std::string description;
    std::function<int(const std::vector<std::string>& args)> run;
};

class CommandRegistry {
public:
    void registerCommand(Command cmd);
    int  dispatch(const std::vector<std::string>& args) const;
    void printHelp() const;

private:
    std::map<std::string, Command> commands_;
};

} // namespace AgentVST::CLI
