#pragma once
#include "bytecode.hpp"
#include <map>

class Vm {
public:
    // Returns the number of instructions executed.
    long long run(const std::vector<Instr>& code);

private:
    std::vector<long long> stack_;
    std::map<std::string, long long> env_;
};