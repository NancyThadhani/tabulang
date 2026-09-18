#include "vm.hpp"
#include <iostream>

long long Vm::run(const std::vector<Instr>& code) {
    stack_.clear();
    env_.clear();

    size_t pc = 0;
    long long executed = 0;

    auto pop = [&]() -> long long {
        if (stack_.empty()) return 0;
        long long v = stack_.back();
        stack_.pop_back();
        return v;
    };

    while (pc < code.size()) {
        const Instr& in = code[pc];
        ++executed;

        switch (in.op) {
        case Op::PUSHC: stack_.push_back(in.arg); ++pc; break;
        case Op::LOAD:  stack_.push_back(env_[in.name]); ++pc; break;
        case Op::STORE: env_[in.name] = pop(); ++pc; break;

        case Op::ADD: { long long b = pop(), a = pop(); stack_.push_back(a + b); ++pc; break; }
        case Op::SUB: { long long b = pop(), a = pop(); stack_.push_back(a - b); ++pc; break; }
        case Op::MUL: { long long b = pop(), a = pop(); stack_.push_back(a * b); ++pc; break; }
        case Op::DIV: { long long b = pop(), a = pop();
                        if (b == 0) { std::cerr << "runtime error: division by zero\n"; return executed; }
                        stack_.push_back(a / b); ++pc; break; }
        case Op::MOD: { long long b = pop(), a = pop();
                        if (b == 0) { std::cerr << "runtime error: modulo by zero\n"; return executed; }
                        stack_.push_back(a % b); ++pc; break; }
        case Op::NEG: stack_.push_back(-pop()); ++pc; break;

        case Op::LT: { long long b = pop(), a = pop(); stack_.push_back(a <  b); ++pc; break; }
        case Op::LE: { long long b = pop(), a = pop(); stack_.push_back(a <= b); ++pc; break; }
        case Op::GT: { long long b = pop(), a = pop(); stack_.push_back(a >  b); ++pc; break; }
        case Op::GE: { long long b = pop(), a = pop(); stack_.push_back(a >= b); ++pc; break; }
        case Op::EQ: { long long b = pop(), a = pop(); stack_.push_back(a == b); ++pc; break; }
        case Op::NE: { long long b = pop(), a = pop(); stack_.push_back(a != b); ++pc; break; }

        case Op::JMP:  pc = static_cast<size_t>(in.arg); break;
        case Op::JMPF: { long long c = pop();
                         pc = c ? pc + 1 : static_cast<size_t>(in.arg); break; }

        case Op::PRINT: std::cout << pop() << "\n"; ++pc; break;

        case Op::TABLE_STUB:
            std::cerr << "not implemented in Phase 2: " << in.name << "\n";
            ++pc;
            break;

        case Op::HALT:
            return executed;
        }
    }
    return executed;
}