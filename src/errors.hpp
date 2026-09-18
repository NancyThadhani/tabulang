#pragma once
#include <string>
#include <vector>
#include <iostream>

struct CompileError {
    std::string phase;      // "lexical" | "syntax" | "semantic" | "type"
    int line, col;
    std::string message;
};

// Phases append here and keep going, so one run reports many errors.
class ErrorLog {
public:
    void add(const std::string& phase, int line, int col, const std::string& msg) {
        items_.push_back({phase, line, col, msg});
    }
    bool any() const { return !items_.empty(); }
    const std::vector<CompileError>& items() const { return items_; }

    void report(const std::string& file) const {
        for (const auto& e : items_)
            std::cerr << file << ":" << e.line << ":" << e.col
                      << " " << e.phase << " error: " << e.message << "\n";
    }
private:
    std::vector<CompileError> items_;
};