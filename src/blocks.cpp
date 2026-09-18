#include "blocks.hpp"
#include <map>
#include <iostream>

bool Cfg::isJump(const std::string& op) {
    return op == "goto" || (op.size() > 2 && op.compare(0, 2, "if") == 0);
}

bool Cfg::isConditional(const std::string& op) {
    return op.size() > 2 && op.compare(0, 2, "if") == 0;
}

int Cfg::target(const Quad& q) {
    if (q.res.empty()) return -1;
    try { return std::stoi(q.res); } catch (...) { return -1; }
}

Cfg::Cfg(const Stream& s) {
    name_ = s.name;
    const auto& code = s.code;
    if (code.empty()) return;

    // 1. find the leaders
    std::map<int, std::string> leaders;          // index -> reason
    leaders[0] = "first quadruple";

    for (size_t i = 0; i < code.size(); ++i) {
        if (!isJump(code[i].op)) continue;
        int t = target(code[i]);
        if (t >= 0 && t < static_cast<int>(code.size()))
            leaders.emplace(t, "jump target");
        if (i + 1 < code.size())
            leaders.emplace(static_cast<int>(i) + 1, "follows a jump");
    }

    // 2. cut the blocks
    std::vector<int> starts;
    for (const auto& kv : leaders) starts.push_back(kv.first);

    std::map<int, int> blockOf;                  // quadruple index -> block id
    for (size_t b = 0; b < starts.size(); ++b) {
        Block blk;
        blk.id = static_cast<int>(b);
        blk.first = starts[b];
        blk.last = (b + 1 < starts.size()) ? starts[b + 1] - 1
                                           : static_cast<int>(code.size()) - 1;
        blk.why = leaders[starts[b]];
        blockOf[blk.first] = blk.id;
        blocks_.push_back(blk);
    }

    // 3. add the edges
    for (auto& b : blocks_) {
        const Quad& lastQ = code[b.last];

        if (isConditional(lastQ.op)) {
            int t = target(lastQ);
            if (blockOf.count(t)) b.succ.push_back(blockOf[t]);      // taken
            if (b.last + 1 < static_cast<int>(code.size()) &&
                blockOf.count(b.last + 1))
                b.succ.push_back(blockOf[b.last + 1]);               // not taken
        } else if (lastQ.op == "goto") {
            int t = target(lastQ);
            if (blockOf.count(t)) b.succ.push_back(blockOf[t]);
        } else if (b.last + 1 < static_cast<int>(code.size()) &&
                   blockOf.count(b.last + 1)) {
            b.succ.push_back(blockOf[b.last + 1]);                   // fall through
        }
    }
}

void Cfg::dump() const {
    std::cout << "--- CFG for " << name_ << " (" << blocks_.size() << " blocks)\n";
    for (const auto& b : blocks_) {
        std::cout << "  B" << b.id << "  quadruples " << b.first << ".." << b.last
                  << "   leader: " << b.why << "\n";
        std::cout << "        successors: ";
        if (b.succ.empty()) std::cout << "(exit)";
        for (size_t i = 0; i < b.succ.size(); ++i)
            std::cout << (i ? ", " : "") << "B" << b.succ[i];
        std::cout << "\n";
    }
}