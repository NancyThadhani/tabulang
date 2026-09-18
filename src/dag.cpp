#include "dag.hpp"
#include <iostream>
#include <algorithm>

static bool isNumber(const std::string& s) {
    if (s.empty()) return false;
    size_t i = (s[0] == '-') ? 1 : 0;
    if (i >= s.size()) return false;
    for (; i < s.size(); ++i) if (!isdigit((unsigned char)s[i])) return false;
    return true;
}

// A compiler-generated temporary: lowercase t followed by digits.
static bool isTemp(const std::string& s) {
    if (s.size() < 2 || s[0] != 't') return false;
    for (size_t i = 1; i < s.size(); ++i)
        if (!isdigit((unsigned char)s[i])) return false;
    return true;
}

// Operations the DAG models. Table stages, print, show, jumps and ret are not;
// they flush the current DAG and pass through unchanged.
static bool isArith(const std::string& op) {
    return op == "+" || op == "-" || op == "*" || op == "/" || op == "%";
}

int Dag::leaf(const std::string& name) {
    auto it = current_.find(name);
    if (it != current_.end()) return it->second;

    DagNode n;
    n.id = static_cast<int>(nodes_.size());
    n.value = name;
    n.isConst = isNumber(name);
    if (!n.isConst) n.labels.push_back(name);
    nodes_.push_back(n);
    current_[name] = n.id;
    return n.id;
}

int Dag::find(const std::string& op, int l, int r) const {
    for (const auto& n : nodes_)
        if (n.op == op && n.left == l && n.right == r) return n.id;
    return -1;
}

int Dag::node(const std::string& op, int l, int r) {
    int existing = find(op, l, r);
    if (existing >= 0) return existing;          // CSE, during construction

    DagNode n;
    n.id = static_cast<int>(nodes_.size());
    n.op = op; n.left = l; n.right = r;
    nodes_.push_back(n);
    return n.id;
}

void Dag::detach(const std::string& name) {
    for (auto& n : nodes_) {
        auto it = std::find(n.labels.begin(), n.labels.end(), name);
        if (it != n.labels.end()) n.labels.erase(it);
    }
}

void Dag::attach(int id, const std::string& name) {
    detach(name);                                 // the old value is now stale
    nodes_[id].labels.push_back(name);
    current_[name] = id;
}

std::string Dag::fold(const std::string& op, const std::string& a,
                      const std::string& b, bool& ok) const {
    ok = true;
    long long x = std::stoll(a), y = std::stoll(b);
    if (op == "+") return std::to_string(x + y);
    if (op == "-") return std::to_string(x - y);
    if (op == "*") return std::to_string(x * y);
    if (op == "/" && y != 0) return std::to_string(x / y);
    if (op == "%" && y != 0) return std::to_string(x % y);
    ok = false;
    return "";
}

std::string Dag::nameOf(int id) const {
    const DagNode& n = nodes_[id];
    if (!n.labels.empty()) return n.labels[0];
    if (!n.value.empty()) return n.value;
    return "_n" + std::to_string(n.id);
}

void Dag::markLive(const std::set<std::string>& alsoLive) {
    // A name is live out of this segment if it is a user variable, or if it is
    // a temporary that something later in the stream still reads.
    liveName_ = alsoLive;
    for (auto& n : nodes_)
        for (const auto& lb : n.labels)
            if (!isTemp(lb)) liveName_.insert(lb);

    std::vector<int> work;
    for (auto& n : nodes_) {
        for (const auto& lb : n.labels)
            if (liveName_.count(lb)) { n.live = true; break; }
        if (n.live) work.push_back(n.id);
    }
    while (!work.empty()) {                       // liveness flows to operands
        int id = work.back(); work.pop_back();
        for (int c : {nodes_[id].left, nodes_[id].right}) {
            if (c >= 0 && !nodes_[c].live) {
                nodes_[c].live = true;
                work.push_back(c);
            }
        }
    }
}

void Dag::emitNode(int id, std::set<int>& done) {
    if (id < 0 || done.count(id)) return;
    done.insert(id);

    const DagNode& n = nodes_[id];

    if (n.op.empty()) {                           // a leaf: only its live copies
        for (const auto& lb : n.labels)
            if (lb != n.value && liveName_.count(lb))
                out_.push_back({"=", n.value, "", lb});
        return;
    }

    emitNode(n.left, done);
    emitNode(n.right, done);

    std::string res = nameOf(id);
    out_.push_back({n.op, nameOf(n.left), nameOf(n.right), res});

    for (size_t i = 1; i < n.labels.size(); ++i)  // extra live names become copies
        if (liveName_.count(n.labels[i]))
            out_.push_back({"=", res, "", n.labels[i]});
}

// Emits the current segment's DAG and starts a fresh one.
void Dag::flush(const std::set<std::string>& alsoLive) {
    if (nodes_.empty()) return;

    markLive(alsoLive);
    if (keep_) segments_.push_back(nodes_);       // snapshot before clearing

    std::set<int> done;
    for (const auto& n : nodes_)
        if (n.live) emitNode(n.id, done);

    nodes_.clear();
    current_.clear();
}

Dag::Dag(const Stream& s, int first, int last, bool keepNodes)
    : keep_(keepNodes) {
    last = std::min(last, static_cast<int>(s.code.size()) - 1);

    // every name read at or after index j, anywhere in the stream
    auto usedFrom = [&](int j) {
        std::set<std::string> out;
        for (int k = j; k < static_cast<int>(s.code.size()); ++k) {
            if (!s.code[k].a1.empty()) out.insert(s.code[k].a1);
            if (!s.code[k].a2.empty()) out.insert(s.code[k].a2);
        }
        return out;
    };

    for (int i = first; i <= last; ++i) {
        const Quad& q = s.code[i];

        if (q.op == "=") {                        // a copy: no node is created
            int src = leaf(q.a1);
            attach(src, q.res);                   // copy propagation
            continue;
        }

        if (isArith(q.op)) {
            int l = leaf(q.a1);
            int r = leaf(q.a2);

            if (nodes_[l].isConst && nodes_[r].isConst) {
                bool ok = false;
                std::string v = fold(q.op, nodes_[l].value, nodes_[r].value, ok);
                if (ok) {                          // constant folding
                    int c = leaf(v);
                    attach(c, q.res);
                    continue;
                }
            }

            int n = node(q.op, l, r);
            attach(n, q.res);
            continue;
        }

        // Not modelled by the DAG: flush what is live from here on, then
        // pass the quadruple through unchanged.
        flush(usedFrom(i));
        out_.push_back(q);
    }

    flush(usedFrom(last + 1));                    // end of block
}

void Dag::dump(int blockId) const {
    if (segments_.empty()) {
        std::cout << "--- DAG for B" << blockId << " (no arithmetic)\n";
        return;
    }
    for (size_t seg = 0; seg < segments_.size(); ++seg) {
        const auto& ns = segments_[seg];
        std::cout << "--- DAG for B" << blockId;
        if (segments_.size() > 1) std::cout << " segment " << seg;
        std::cout << " (" << ns.size() << " nodes)\n";
        for (const auto& n : ns) {
            std::cout << "  n" << n.id << "  ";
            if (n.op.empty()) std::cout << "leaf " << n.value;
            else std::cout << n.op << "(n" << n.left << ", n" << n.right << ")";
            if (!n.labels.empty()) {
                std::cout << "   labels: ";
                for (size_t i = 0; i < n.labels.size(); ++i)
                    std::cout << (i ? ", " : "") << n.labels[i];
            }
            if (!n.live) std::cout << "   [dead]";
            std::cout << "\n";
        }
    }
}