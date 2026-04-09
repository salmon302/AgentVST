/**
 * main.cpp — AgentVST CLI entry point
 *
 * Dispatches to subcommands:
 *   agentvst validate      <schema.json>
 *   agentvst validate-dsp  <dsp.cpp> [...]
 *   agentvst build         <schema.json> --sources <dsp.cpp>
 *   agentvst init          <PluginName>  [--vendor X] [--version X]
 *   agentvst --help
 */
#include "CommandRegistry.h"
#include "ValidateCommand.h"
#include "ValidateDspCommand.h"
#include "BuildCommand.h"
#include "InitCommand.h"
#include <iostream>
#include <vector>
#include <string>

int main(int argc, char* argv[]) {
    using namespace AgentVST::CLI;

    CommandRegistry registry;

    registry.registerCommand({
        "validate",
        "validate <schema.json>",
        "Validate a plugin.json schema file",
        runValidate
    });

    registry.registerCommand({
        "validate-dsp",
        "validate-dsp <file.cpp> [...]",
        "Scan agent DSP files for real-time violations",
        runValidateDsp
    });

    registry.registerCommand({
        "build",
        "build <schema.json> --sources <dsp.cpp>",
        "Configure and build a plugin from schema + DSP",
        runBuild
    });

    registry.registerCommand({
        "init",
        "init <PluginName> [--vendor X]",
        "Scaffold a new plugin project directory",
        runInit
    });

    const std::vector<std::string> args(argv + 1, argv + argc);
    return registry.dispatch(args);
}
