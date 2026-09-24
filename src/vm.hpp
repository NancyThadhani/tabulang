#pragma once
#include "bytecode.hpp"
#include "table.hpp"
#include <map>
#include <unordered_map>

// Counters for the run-time side of the optimization metrics.
struct RunStats {
    long long instructions = 0;       // main program plus every per-row fragment
    long long rowsProcessed = 0;      // rows entering each table stage, summed
    long long cellsProcessed = 0;     // rows x columns entering each stage, summed
};

class Vm {
public:
    // frags: bytecode for each per-row fragment, by name (P0, P1, ...).
    // baseDir: directory that relative CSV paths are resolved against.
    Vm(std::map<std::string, std::vector<Instr>> frags, std::string baseDir)
        : frags_(std::move(frags)), baseDir_(std::move(baseDir)) {}

    // Runs the main program. Returns false on a runtime error.
    bool run(const std::vector<Instr>& code);

    const RunStats& stats() const { return stats_; }

private:
    // The dispatch loop, shared by the main program and per-row fragments.
    // When row is non-null, LOAD looks there first, so a column shadows an
    // outer scalar exactly as it does in semantic analysis.
    bool exec(const std::vector<Instr>& code,
              const std::unordered_map<std::string, long long>* row,
              long long* result);

    bool tableOp(const Instr& in);
    bool rowStage(const Instr& in, const Table& src, Table& out);
    void countStage(const Table& t);
    bool fail(const std::string& msg);

    std::map<std::string, long long> env_;
    std::map<std::string, Table> tables_;
    std::map<std::string, std::vector<Instr>> frags_;
    std::string baseDir_;
    RunStats stats_;
};
