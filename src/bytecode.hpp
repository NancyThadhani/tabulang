#pragma once
#include <string>
#include <vector>

enum class Op {
    PUSHC, LOAD, STORE,
    ADD, SUB, MUL, DIV, MOD, NEG,
    LT, LE, GT, GE, EQ, NE,
    JMP, JMPF, PRINT, HALT,
    TABLE                               // one pipeline stage, load, show or table copy
};

struct Instr {
    Op op;
    long long arg = 0;                  // constant, or jump target
    std::string name;                   // variable name, or the stage kind for TABLE
    std::string a, b, r;                // TABLE only: input, stage argument, output

    Instr(Op o, long long v = 0, std::string n = "",
          std::string in = "", std::string arg = "", std::string out = "")
        : op(o), arg(v), name(std::move(n)),
          a(std::move(in)), b(std::move(arg)), r(std::move(out)) {}
};

const char* opName(Op o);
void dumpCode(const std::vector<Instr>& code);

// String literals and string column values are interned to integer ids, so the
// integer-valued VM can compare them with EQ and NE. Ids start at 1, never 0.
long long internString(const std::string& s);
