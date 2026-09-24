#include "vm.hpp"
#include <iostream>

bool Vm::fail(const std::string& msg) {
    std::cerr << "runtime error: " << msg << "\n";
    return false;
}

bool Vm::run(const std::vector<Instr>& code) {
    env_.clear();
    tables_.clear();
    stats_ = RunStats{};
    return exec(code, nullptr, nullptr);
}

bool Vm::exec(const std::vector<Instr>& code,
              const std::unordered_map<std::string, long long>* row,
              long long* result) {
    std::vector<long long> stack;       // each activation gets its own stack
    size_t pc = 0;

    auto pop = [&]() -> long long {
        if (stack.empty()) return 0;
        long long v = stack.back();
        stack.pop_back();
        return v;
    };

    while (pc < code.size()) {
        const Instr& in = code[pc];
        ++stats_.instructions;

        switch (in.op) {
        case Op::PUSHC: stack.push_back(in.arg); ++pc; break;
        case Op::LOAD: {
            if (row) {
                auto it = row->find(in.name);
                if (it != row->end()) { stack.push_back(it->second); ++pc; break; }
            }
            stack.push_back(env_[in.name]); ++pc; break;
        }
        case Op::STORE: env_[in.name] = pop(); ++pc; break;

        case Op::ADD: { long long b = pop(), a = pop(); stack.push_back(a + b); ++pc; break; }
        case Op::SUB: { long long b = pop(), a = pop(); stack.push_back(a - b); ++pc; break; }
        case Op::MUL: { long long b = pop(), a = pop(); stack.push_back(a * b); ++pc; break; }
        case Op::DIV: { long long b = pop(), a = pop();
                        if (b == 0) return fail("division by zero");
                        stack.push_back(a / b); ++pc; break; }
        case Op::MOD: { long long b = pop(), a = pop();
                        if (b == 0) return fail("modulo by zero");
                        stack.push_back(a % b); ++pc; break; }
        case Op::NEG: stack.push_back(-pop()); ++pc; break;

        case Op::LT: { long long b = pop(), a = pop(); stack.push_back(a <  b); ++pc; break; }
        case Op::LE: { long long b = pop(), a = pop(); stack.push_back(a <= b); ++pc; break; }
        case Op::GT: { long long b = pop(), a = pop(); stack.push_back(a >  b); ++pc; break; }
        case Op::GE: { long long b = pop(), a = pop(); stack.push_back(a >= b); ++pc; break; }
        case Op::EQ: { long long b = pop(), a = pop(); stack.push_back(a == b); ++pc; break; }
        case Op::NE: { long long b = pop(), a = pop(); stack.push_back(a != b); ++pc; break; }

        case Op::JMP:  pc = static_cast<size_t>(in.arg); break;
        case Op::JMPF: { long long c = pop();
                         pc = c ? pc + 1 : static_cast<size_t>(in.arg); break; }

        case Op::PRINT: std::cout << pop() << "\n"; ++pc; break;

        case Op::TABLE:
            if (!tableOp(in)) return false;
            ++pc;
            break;

        case Op::HALT:
            // a fragment ends with its return value on top of the stack
            if (result) *result = stack.empty() ? 0 : stack.back();
            return true;
        }
    }
    if (result) *result = stack.empty() ? 0 : stack.back();
    return true;
}

void Vm::countStage(const Table& t) {
    stats_.rowsProcessed  += static_cast<long long>(t.rows.size());
    stats_.cellsProcessed += static_cast<long long>(t.rows.size() * t.cols.size());
}

// filter, derive and rowpass: run per-row fragments over the table.
// A plain filter or derive is a rowpass with one step. A fused rowpass runs
// several steps on each row in order, so the row is visited once, and a
// failing filter step drops the row before any later step runs.
struct RowStep { bool filter; std::string col, frag; };

bool Vm::rowStage(const Instr& in, const Table& src, Table& out) {
    std::vector<RowStep> steps;
    std::vector<std::string> parts;
    if (in.name == "rowpass") {
        std::string cur;
        for (char c : in.b) { if (c == ';') { parts.push_back(cur); cur.clear(); } else cur += c; }
        parts.push_back(cur);
    } else {
        parts.push_back((in.name == "filter" ? "F:" : "D:") + in.b);
    }
    for (const auto& p : parts) {
        RowStep st;
        st.filter = p[0] == 'F';
        std::string body = p.substr(2);
        if (st.filter) st.frag = body;
        else {
            auto k = body.find(":=");
            st.col = body.substr(0, k);
            st.frag = body.substr(k + 2);
        }
        if (!frags_.count(st.frag)) return fail("missing fragment " + st.frag);
        steps.push_back(st);
    }

    out.cols = src.cols;
    out.groupKeys.clear();
    for (const auto& st : steps) if (!st.filter) out.cols.push_back({st.col, "int"});
    out.rows.clear();

    // Column values seen by the fragments. Strings are interned so that
    // == and != against a string literal compare ids.
    std::unordered_map<std::string, long long> rowEnv;
    for (size_t r = 0; r < src.rows.size(); ++r) {
        const auto& row = src.rows[r];
        for (size_t k = 0; k < src.cols.size(); ++k) {
            const auto& t = src.cols[k].type;
            rowEnv[src.cols[k].name] =
                t == "string" ? internString(row[k].s)
              : t == "float"  ? static_cast<long long>(row[k].f)
              : row[k].i;
        }
        std::vector<Cell> nr = row;
        bool keep = true;
        for (const auto& st : steps) {
            long long v = 0;
            if (!exec(frags_[st.frag], &rowEnv, &v)) {
                std::cerr << "  in " << in.name << " (" << st.frag << ") at row "
                          << r + 1 << "\n";
                return false;
            }
            if (st.filter) { if (!v) { keep = false; break; } }
            else {
                Cell c; c.i = v;
                nr.push_back(c);
                rowEnv[st.col] = v;          // later steps can read the new column
            }
        }
        if (keep) out.rows.push_back(std::move(nr));
    }
    return true;
}

bool Vm::tableOp(const Instr& in) {
    const std::string& k = in.name;
    std::string err;

    if (k == "load") {
        std::string file = in.a.substr(1, in.a.size() - 2);   // strip quotes
        std::string path = (file.empty() || file[0] == '/' || baseDir_.empty())
                         ? file : baseDir_ + "/" + file;
        Table t;
        if (!loadCsv(path, parseSchemaSpec(in.b), t, err)) return fail(err);
        tables_[in.r] = std::move(t);
        return true;
    }
    if (k == "show") {
        auto it = tables_.find(in.a);
        if (it == tables_.end()) return fail("show: no table '" + in.a + "'");
        printTable(in.a, it->second);
        return true;
    }

    auto it = tables_.find(in.a);
    if (it == tables_.end()) return fail(k + ": no table '" + in.a + "'");
    const Table& src = it->second;

    if (k == "copy") { tables_[in.r] = src; return true; }

    countStage(src);
    Table out;
    bool ok = true;
    if      (k == "filter" || k == "derive" || k == "rowpass") ok = rowStage(in, src, out);
    else if (k == "select")    ok = opSelect(src, in.b, out, err);
    else if (k == "group_by")  ok = opGroupBy(src, in.b, out, err);
    else if (k == "aggregate") ok = opAggregate(src, in.b, out, err);
    else if (k == "sort")      ok = opSort(src, in.b, out, err);
    else if (k == "limit")     ok = opLimit(src, in.b, out, err);
    else return fail("unknown table operation '" + k + "'");

    if (!ok) return err.empty() ? false : fail(err);
    tables_[in.r] = std::move(out);
    return true;
}
