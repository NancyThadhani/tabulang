#pragma once
#include <string>
#include <vector>
#include <iostream>

// A quadruple: (op, arg1, arg2, result).
// Jump targets live in `result` and are filled in later by backpatching.
struct Quad {
    std::string op;
    std::string a1;
    std::string a2;
    std::string res;
};

// One numbered stream of quadruples. The main body is one stream; every
// per-row expression inside filter or derive gets its own, with its own
// counter, so backpatched targets never point across a boundary.
struct Stream {
    std::string name;                 // "main", "P0", "P1", ...
    std::vector<Quad> code;

    int emit(const std::string& op, const std::string& a1,
             const std::string& a2, const std::string& res) {
        code.push_back({op, a1, a2, res});
        return static_cast<int>(code.size()) - 1;
    }
    int next() const { return static_cast<int>(code.size()); }
};

using List = std::vector<int>;        // indices of quadruples awaiting a target

inline List makelist(int i) { return List{i}; }

inline List merge(const List& a, const List& b) {
    List out = a;
    out.insert(out.end(), b.begin(), b.end());
    return out;
}

inline void backpatch(Stream& s, const List& l, int target) {
    for (int i : l) s.code[i].res = std::to_string(target);
}

void dumpStream(const Stream& s);