/**
 * BuildCommand.cpp
 * agentvst build <schema.json> --sources <file.cpp> [--config Debug|Release]
 *
 * Validates the schema, then invokes CMake to configure and build the plugin.
 * Plugin IDs are generated deterministically from the schema metadata.
 */
#include "BuildCommand.h"
#include "SchemaParser.h"
#include <iostream>
#include <string>
#include <vector>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>

namespace AgentVST::CLI {

namespace fs = std::filesystem;

int runBuild(const std::vector<std::string>& args) {
    if (args.empty()) {
        std::cerr << "Usage: agentvst build <schema.json>"
                     " --sources <dsp.cpp> [--config Debug|Release]\n";
        return 1;
    }

    std::string schemaPath;
    std::vector<std::string> sources;
    std::string config = "Release";

    // Parse arguments
    for (std::size_t i = 0; i < args.size(); ++i) {
        if (args[i] == "--sources") {
            while (i + 1 < args.size() && args[i + 1][0] != '-')
                sources.push_back(args[++i]);
        } else if (args[i] == "--config" && i + 1 < args.size()) {
            config = args[++i];
        } else if (schemaPath.empty()) {
            schemaPath = args[i];
        }
    }

    if (schemaPath.empty()) {
        std::cerr << "[ERROR] No schema path provided.\n";
        return 1;
    }

    // 1. Validate schema
    std::cout << "Step 1/3 — Validating schema: " << schemaPath << "\n";
    AgentVST::SchemaParser parser;
    AgentVST::PluginSchema schema;
    try {
        schema = parser.parseFile(schemaPath);
        std::cout << "  OK: " << schema.metadata.name
                  << " v" << schema.metadata.version << "\n";
    } catch (const std::exception& e) {
        std::cerr << "[ERROR] Schema validation failed:\n  " << e.what() << "\n";
        return 1;
    }

    // 2. Generate a temporary CMakeLists.txt in a build scratch dir
    const fs::path buildDir = fs::current_path() / ("build_" + schema.metadata.name);
    const fs::path scratchCMake = buildDir / "CMakeLists.txt";
    fs::create_directories(buildDir);

    std::cout << "Step 2/3 — Generating build files in: " << buildDir.string() << "\n";

    std::ostringstream sourcesStr;
    for (const auto& src : sources)
        sourcesStr << "        " << fs::absolute(src).string() << "\n";

    const std::string frameworkSrc = AGENTVST_SOURCE_DIR;
    const std::string schemAbs     = fs::absolute(schemaPath).string();

    std::string cmakeContent =
        "cmake_minimum_required(VERSION 3.22)\n"
        "project(" + schema.metadata.name + "Build)\n"
        "set(CMAKE_CXX_STANDARD 17)\n"
        "list(APPEND CMAKE_MODULE_PATH \"" + frameworkSrc + "/cmake\")\n"
        "include(\"" + frameworkSrc + "/cmake/agentvst_ids.cmake\")\n"
        "include(\"" + frameworkSrc + "/cmake/agentvst_plugin.cmake\")\n"
        "add_subdirectory(\"" + frameworkSrc + "/framework\" framework)\n"
        "agentvst_add_plugin(\n"
        "    NAME     " + schema.metadata.name + "\n"
        "    SCHEMA   \"" + schemAbs + "\"\n"
        "    VENDOR   \"" + schema.metadata.vendor + "\"\n"
        "    VERSION  \"" + schema.metadata.version + "\"\n"
        "    CATEGORY \"" + schema.metadata.category + "\"\n"
        "    SOURCES\n" + sourcesStr.str() +
        ")\n";

    {
        std::ofstream f(scratchCMake);
        f << cmakeContent;
    }
    std::cout << "  OK\n";

    // 3. Run cmake configure + build
    std::cout << "Step 3/3 — Building (" << config << ")...\n";

    const std::string cmakeBin =
#ifdef _WIN32
        "cmake";
#else
        "cmake";
#endif

    std::string configureCmd = cmakeBin + " -B \"" + buildDir.string() + "/build\""
        + " -S \"" + buildDir.string() + "\""
        + " -DCMAKE_BUILD_TYPE=" + config;
    std::string buildCmd = cmakeBin + " --build \"" + buildDir.string() + "/build\""
        + " --config " + config + " --parallel";

    if (std::system(configureCmd.c_str()) != 0) {
        std::cerr << "[ERROR] CMake configure failed.\n";
        return 1;
    }
    if (std::system(buildCmd.c_str()) != 0) {
        std::cerr << "[ERROR] CMake build failed.\n";
        return 1;
    }

    std::cout << "\n[OK] Build complete. Artifacts in: "
              << (buildDir / "build").string() << "\n";
    return 0;
}

} // namespace AgentVST::CLI
