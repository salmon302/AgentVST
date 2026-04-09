/**
 * ValidateCommand.cpp
 * agentvst validate <schema.json>
 *
 * Parses and validates a plugin.json schema file, reporting all errors
 * with actionable messages. Exits 0 on success, 1 on any error.
 */
#include "ValidateCommand.h"
#include "SchemaParser.h"
#include <iostream>
#include <string>
#include <vector>

namespace AgentVST::CLI {

int runValidate(const std::vector<std::string>& args) {
    if (args.empty()) {
        std::cerr << "Usage: agentvst validate <schema.json>\n";
        return 1;
    }

    const std::string& schemaPath = args[0];
    std::cout << "Validating: " << schemaPath << "\n";

    AgentVST::SchemaParser parser;
    try {
        AgentVST::PluginSchema schema = parser.parseFile(schemaPath);

        // Print summary
        std::cout << "  Plugin  : " << schema.metadata.name
                  << " v" << schema.metadata.version << "\n"
                  << "  Vendor  : " << schema.metadata.vendor << "\n"
                  << "  Category: " << schema.metadata.category << "\n"
                  << "  Params  : " << schema.parameters.size() << "\n"
                  << "  Groups  : " << schema.groups.size() << "\n";

        std::cout << "\nParameters:\n";
        for (const auto& p : schema.parameters) {
            std::cout << "  [" << p.type << "] " << p.id
                      << " = " << p.defaultValue
                      << " [" << p.minValue << " .. " << p.maxValue << "]";
            if (!p.unit.empty())
                std::cout << " " << p.unit;
            std::cout << "\n";
        }

        std::cout << "\n[OK] Schema is valid.\n";
        return 0;

    } catch (const AgentVST::SchemaParser::ParseError& e) {
        std::cerr << "\n[ERROR] Parse error:\n  " << e.what() << "\n\n"
                  << "Fix: check the JSON syntax and required fields "
                     "(id, name, type, min, max for float/int params).\n";
        return 1;
    } catch (const AgentVST::SchemaParser::ValidationError& e) {
        std::cerr << "\n[ERROR] Validation error:\n  " << e.what() << "\n\n"
                  << "Fix: ensure parameter IDs are unique and group "
                     "parameter references match defined parameter IDs.\n";
        return 1;
    } catch (const std::exception& e) {
        std::cerr << "\n[ERROR] Unexpected error:\n  " << e.what() << "\n";
        return 1;
    }
}

} // namespace AgentVST::CLI
