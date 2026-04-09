/**
 * ValidateDspCommand.cpp
 * agentvst validate-dsp <file1.cpp> [file2.cpp ...]
 *
 * Scans agent DSP source files for prohibited real-time patterns.
 * These are operations that must NEVER appear in processSample():
 *   - Dynamic heap allocation (new, malloc, vector resize)
 *   - Blocking synchronization (mutex, unique_lock)
 *   - Console / file I/O (cout, printf, fwrite)
 *
 * Exits 0 if clean, 1 if any violations are found.
 */
#include "ValidateDspCommand.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <regex>

namespace AgentVST::CLI {

struct ProhibitedPattern {
    std::string regex;
    std::string description;
    std::string suggestion;
};

static const std::vector<ProhibitedPattern> kPatterns = {
    { R"(\bnew\s+\w)",
      "Dynamic heap allocation (new)",
      "Pre-allocate in prepare() instead." },
    { R"(\bmalloc\s*\()",
      "Dynamic heap allocation (malloc)",
      "Pre-allocate in prepare() instead." },
    { R"(\bcalloc\s*\()",
      "Dynamic heap allocation (calloc)",
      "Pre-allocate in prepare() instead." },
    { R"(\brealloc\s*\()",
      "Dynamic heap allocation (realloc)",
      "Pre-allocate in prepare() instead." },
    { R"(std\s*::\s*vector\s*<[^>]+>\s*\w+\s*;)",
      "std::vector local variable (may allocate)",
      "Declare as member and size in prepare()." },
    { R"(\.push_back\s*\()",
      "std::vector::push_back (may reallocate)",
      "Use a fixed-size array sized in prepare()." },
    { R"(\.emplace_back\s*\()",
      "std::vector::emplace_back (may reallocate)",
      "Use a fixed-size array sized in prepare()." },
    { R"(std\s*::\s*cout)",
      "std::cout (system call / I/O)",
      "Use async logging outside the audio callback." },
    { R"(\bprintf\s*\()",
      "printf (system call / I/O)",
      "Remove all console output from processSample()." },
    { R"(\bfprintf\s*\()",
      "fprintf (system call / I/O)",
      "Remove all file I/O from processSample()." },
    { R"(std\s*::\s*mutex)",
      "std::mutex (blocking synchronisation)",
      "Use std::atomic<> for shared state instead." },
    { R"(std\s*::\s*unique_lock)",
      "std::unique_lock (blocking lock)",
      "Use lock-free communication (atomics, SPSC queue)." },
    { R"(std\s*::\s*lock_guard)",
      "std::lock_guard (blocking lock)",
      "Use lock-free communication (atomics, SPSC queue)." },
    { R"(std\s*::\s*scoped_lock)",
      "std::scoped_lock (blocking lock)",
      "Use lock-free communication (atomics, SPSC queue)." },
    { R"(\bDBG\s*\()",
      "JUCE DBG() macro (system call on audio thread)",
      "Remove DBG() calls from processSample()." },
};

int runValidateDsp(const std::vector<std::string>& args) {
    if (args.empty()) {
        std::cerr << "Usage: agentvst validate-dsp <file.cpp> [file2.cpp ...]\n";
        return 1;
    }

    int totalViolations = 0;
    int filesChecked    = 0;

    for (const auto& filePath : args) {
        std::ifstream f(filePath);
        if (!f.is_open()) {
            std::cerr << "[WARN] Cannot open file: " << filePath << "\n";
            continue;
        }

        ++filesChecked;
        std::cout << "Scanning: " << filePath << "\n";

        std::string line;
        int lineNum = 0;
        int fileViolations = 0;

        while (std::getline(f, line)) {
            ++lineNum;

            // Skip comment lines (simple heuristic)
            std::string trimmed = line;
            auto first = trimmed.find_first_not_of(" \t");
            if (first != std::string::npos &&
                (trimmed.substr(first, 2) == "//" || trimmed.substr(first, 2) == "/*"))
                continue;

            for (const auto& pat : kPatterns) {
                std::regex re(pat.regex, std::regex::ECMAScript);
                if (std::regex_search(line, re)) {
                    std::cout << "  [VIOLATION] " << filePath
                              << ":" << lineNum << "\n"
                              << "    Pattern  : " << pat.description << "\n"
                              << "    Line     : " << line << "\n"
                              << "    Fix      : " << pat.suggestion << "\n\n";
                    ++fileViolations;
                    ++totalViolations;
                }
            }
        }

        if (fileViolations == 0)
            std::cout << "  [OK] No violations.\n";
    }

    std::cout << "\n─────────────────────────────────\n"
              << "Files scanned : " << filesChecked << "\n"
              << "Total violations: " << totalViolations << "\n";

    if (totalViolations > 0) {
        std::cerr << "\n[FAIL] DSP code contains " << totalViolations
                  << " real-time violation(s). Fix before building.\n";
        return 1;
    }

    std::cout << "[OK] All DSP files are real-time safe.\n";
    return 0;
}

} // namespace AgentVST::CLI
