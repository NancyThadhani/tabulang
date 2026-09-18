#pragma once
#include "tac.hpp"
#include <set>

struct Block {
    int id = 0;
    int first = 0, last = 0;          // inclusive range into the stream
    std::vector<int> succ;            // ids of successor blocks
    std::string why;                  // why its first quadruple is a leader
};

class Cfg {
public:
    explicit Cfg(const Stream& s);

    const std::vector<Block>& blocks() const { return blocks_; }
    const std::string& name() const { return name_; }
    void dump() const;

private:
    static bool isJump(const std::string& op);
    static bool isConditional(const std::string& op);
    static int target(const Quad& q);

    std::string name_;
    std::vector<Block> blocks_;
};