#pragma once
#include "tac.hpp"
#include <map>
#include <set>

// A node of the DAG for one basic block segment.
// Leaves are variables and constants; internal nodes are operations.
struct DagNode {
    int id = 0;
    std::string op;                   // empty for a leaf
    int left = -1, right = -1;        // child node ids
    std::string value;                // leaf: the variable name or constant
    std::vector<std::string> labels;  // names whose current value this node holds
    bool isConst = false;
    bool live = false;
};

class Dag {
public:
    // Builds and optimizes code[first..last] of the given stream.
    // keepNodes records each segment's nodes so dump() can show them.
    Dag(const Stream& s, int first, int last, bool keepNodes = false);

    void dump(int blockId) const;

    // The optimized quadruples: CSE, constant folding, copy propagation,
    // dead code elimination.
    const std::vector<Quad>& result() const { return out_; }

private:
    int leaf(const std::string& name);
    int node(const std::string& op, int l, int r);
    int find(const std::string& op, int l, int r) const;
    void attach(int n, const std::string& name);
    void detach(const std::string& name);
    std::string fold(const std::string& op, const std::string& a,
                     const std::string& b, bool& ok) const;

    void flush(const std::set<std::string>& alsoLive);
    void markLive(const std::set<std::string>& alsoLive);
    void emitNode(int n, std::set<int>& done);
    std::string nameOf(int id) const;

    std::vector<DagNode> nodes_;            // DAG of the current segment
    std::map<std::string, int> current_;    // name -> node holding its value now
    std::vector<Quad> out_;                 // the optimized quadruples
    std::set<std::string> liveName_;        // names live out of the current segment
    bool keep_ = false;
    std::vector<std::vector<DagNode>> segments_;   // snapshots, for dump()
};