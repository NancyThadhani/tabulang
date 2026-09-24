#pragma once
#include "tac.hpp"
#include "bytecode.hpp"
#include <map>
#include <set>

class CodeGen {
public:
    // Lowers one quadruple stream to TabVM bytecode.
    std::vector<Instr> generate(const Stream& s);
    bool failed() const { return failed_; }

private:
    void push(std::vector<Instr>& out, const std::string& operand);
    static bool isNumber(const std::string& s);
    static bool isTableStage(const std::string& op);

    bool failed_ = false;
    std::set<std::string> tables_;      // names known to hold a table value

    std::map<int, int> quadToInstr_;    // quadruple index -> instruction index
    std::vector<std::pair<int,int>> patches_;   // instruction index, target quad
};