#pragma once
#include <string>
#include <vector>

enum class Op {
    PUSHC, LOAD, STORE,
    ADD, SUB, MUL, DIV, MOD, NEG,
    LT, LE, GT, GE, EQ, NE,
    JMP, JMPF, PRINT, HALT,
    TABLE_STUB                          // any table stage: Phase 3
};

struct Instr {
    Op op;
    long long arg = 0;                  // constant, or jump target
    std::string name;                   // variable name, or a stage description
};

const char* opName(Op o);
void dumpCode(const std::vector<Instr>& code);