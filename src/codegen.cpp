#include "codegen.hpp"
#include <iostream>
#include <iomanip>

const char* opName(Op o) {
    switch (o) {
        case Op::PUSHC: return "PUSHC";
        case Op::LOAD:  return "LOAD";
        case Op::STORE: return "STORE";
        case Op::ADD:   return "ADD";
        case Op::SUB:   return "SUB";
        case Op::MUL:   return "MUL";
        case Op::DIV:   return "DIV";
        case Op::MOD:   return "MOD";
        case Op::NEG:   return "NEG";
        case Op::LT:    return "LT";
        case Op::LE:    return "LE";
        case Op::GT:    return "GT";
        case Op::GE:    return "GE";
        case Op::EQ:    return "EQ";
        case Op::NE:    return "NE";
        case Op::JMP:   return "JMP";
        case Op::JMPF:  return "JMPF";
        case Op::PRINT: return "PRINT";
        case Op::HALT:  return "HALT";
        case Op::TABLE_STUB: return "TABLE";
    }
    return "?";
}

void dumpCode(const std::vector<Instr>& code) {
    std::cout << "--- TabVM bytecode (" << code.size() << " instructions)\n";
    for (size_t i = 0; i < code.size(); ++i) {
        std::cout << std::setw(3) << i << "  "
                  << std::left << std::setw(8) << opName(code[i].op);
        if (code[i].op == Op::PUSHC || code[i].op == Op::JMP ||
            code[i].op == Op::JMPF)
            std::cout << code[i].arg;
        else if (!code[i].name.empty())
            std::cout << code[i].name;
        std::cout << std::right << "\n";
    }
}

bool CodeGen::isNumber(const std::string& s) {
    if (s.empty()) return false;
    size_t i = (s[0] == '-') ? 1 : 0;
    if (i >= s.size()) return false;
    for (; i < s.size(); ++i) if (!isdigit((unsigned char)s[i])) return false;
    return true;
}

void CodeGen::push(std::vector<Instr>& out, const std::string& operand) {
    if (isNumber(operand)) out.push_back({Op::PUSHC, std::stoll(operand), ""});
    else                   out.push_back({Op::LOAD, 0, operand});
}

std::vector<Instr> CodeGen::generate(const Stream& s) {
    std::vector<Instr> out;
    quadToInstr_.clear();
    patches_.clear();

    for (size_t i = 0; i < s.code.size(); ++i) {
        const Quad& q = s.code[i];
        quadToInstr_[static_cast<int>(i)] = static_cast<int>(out.size());

        if (q.op == "=") {
            push(out, q.a1);
            out.push_back({Op::STORE, 0, q.res});
        }
        else if (q.op == "+" || q.op == "-" || q.op == "*" ||
                 q.op == "/" || q.op == "%") {
            push(out, q.a1);
            push(out, q.a2);
            Op o = q.op == "+" ? Op::ADD : q.op == "-" ? Op::SUB
                 : q.op == "*" ? Op::MUL : q.op == "/" ? Op::DIV : Op::MOD;
            out.push_back({o, 0, ""});
            out.push_back({Op::STORE, 0, q.res});
        }
        else if (q.op == "neg") {
            push(out, q.a1);
            out.push_back({Op::NEG, 0, ""});
            out.push_back({Op::STORE, 0, q.res});
        }
        else if (q.op == "print") {
            push(out, q.a1);
            out.push_back({Op::PRINT, 0, ""});
        }
        else if (q.op.size() > 2 && q.op.compare(0, 2, "if") == 0) {
            // ifOP a b target  ->  compare, jump if false past the taken branch
            std::string cmp = q.op.substr(2);
            push(out, q.a1);
            push(out, q.a2);
            Op o = cmp == "<"  ? Op::LT : cmp == "<=" ? Op::LE
                 : cmp == ">"  ? Op::GT : cmp == ">=" ? Op::GE
                 : cmp == "==" ? Op::EQ : Op::NE;
            out.push_back({o, 0, ""});
            // JMPF jumps when the test fails; the taken target is patched below
            out.push_back({Op::JMPF, 0, ""});
            int falseJump = static_cast<int>(out.size()) - 1;
            out.push_back({Op::JMP, 0, ""});
            patches_.push_back({static_cast<int>(out.size()) - 1,
                                q.res.empty() ? -1 : std::stoi(q.res)});
            // JMPF lands just past the JMP, which is the fall-through path
            out[falseJump].arg = static_cast<long long>(out.size());
        }
        else if (q.op == "goto") {
            out.push_back({Op::JMP, 0, ""});
            patches_.push_back({static_cast<int>(out.size()) - 1,
                                q.res.empty() ? -1 : std::stoi(q.res)});
        }
        else if (q.op == "ret") {
            push(out, q.a1);
            out.push_back({Op::HALT, 0, ""});
        }
        else {
            // load, filter, derive, group_by, aggregate, sort, limit, show
            out.push_back({Op::TABLE_STUB, 0, q.op + " " + q.a1});
        }
    }

    quadToInstr_[static_cast<int>(s.code.size())] = static_cast<int>(out.size());

    for (const auto& p : patches_) {
        auto it = quadToInstr_.find(p.second);
        out[p.first].arg = (it == quadToInstr_.end())
                         ? static_cast<long long>(out.size())
                         : it->second;
    }

    out.push_back({Op::HALT, 0, ""});
    return out;
}