#include "pipeopt.hpp"
#include <algorithm>
#include <map>
#include <set>

namespace {

bool isStage(const std::string& op) {
    return op == "filter" || op == "derive" || op == "select" || op == "group_by" ||
           op == "aggregate" || op == "sort" || op == "limit" || op == "rowpass";
}

bool isJump(const std::string& op) {
    return op == "goto" || (op.size() > 2 && op.compare(0, 2, "if") == 0);
}

std::vector<std::string> split(const std::string& s, char sep) {
    std::vector<std::string> out;
    std::string cur;
    for (char c : s) { if (c == sep) { out.push_back(cur); cur.clear(); } else cur += c; }
    out.push_back(cur);
    return out;
}

using Cols = std::vector<std::string>;       // ordered column names

bool has(const Cols& c, const std::string& n) {
    return std::find(c.begin(), c.end(), n) != c.end();
}

// Fragment name of a filter ("P1") or derive ("unit_price:=P0").
std::string fragOf(const Quad& q) {
    if (q.op == "filter") return q.a2;
    auto p = q.a2.find(":=");
    return q.a2.substr(p + 2);
}
std::string derivedCol(const Quad& q) { return q.a2.substr(0, q.a2.find(":=")); }

// The columns a per-row fragment reads: every operand that names a column of
// the incoming schema. A column shadows a scalar, the same rule sema uses.
std::set<std::string> readsOf(const std::string& frag, const Cols& schema,
                              const std::vector<Stream>& frags) {
    std::set<std::string> r;
    for (const auto& f : frags) {
        if (f.name != frag) continue;
        for (const auto& q : f.code) {
            if (has(schema, q.a1)) r.insert(q.a1);
            if (has(schema, q.a2)) r.insert(q.a2);
        }
    }
    return r;
}

// Output schema of one stage, given its input schema.
Cols schemaAfter(const Quad& q, const Cols& in, std::map<std::string, Cols>& keysOf) {
    if (q.op == "derive") { Cols o = in; o.push_back(derivedCol(q)); return o; }
    if (q.op == "select") return split(q.a2, ',');
    if (q.op == "group_by") { keysOf[q.res] = split(q.a2, ','); return in; }
    if (q.op == "aggregate") {
        Cols o = keysOf[q.a1];
        for (const auto& item : split(q.a2, ',')) o.push_back(item.substr(0, item.find('=')));
        return o;
    }
    return in;                                // filter, sort, limit
}

} // namespace

PipeReport optimizePipelines(Stream& main, const std::vector<Stream>& frags) {
    PipeReport rep;
    auto& code = main.code;

    // Schema of every table name, found by a forward walk over the quadruples.
    std::map<std::string, Cols> schema;
    std::map<std::string, Cols> keysOf;
    auto recompute = [&]() {
        schema.clear(); keysOf.clear();
        for (const auto& q : code) {
            if (q.op == "load") {
                Cols c;
                for (const auto& part : split(q.a2, ',')) c.push_back(part.substr(0, part.find(':')));
                schema[q.res] = c;
            } else if (q.op == "=" && schema.count(q.a1)) {
                schema[q.res] = schema[q.a1];
            } else if (isStage(q.op) && q.op != "rowpass") {
                schema[q.res] = schemaAfter(q, schema[q.a1], keysOf);
            }
        }
    };
    recompute();

    // ---- 1. predicate pushdown -------------------------------------------
    // A filter directly after a stage S can run before S when S is row-wise
    // and order-insensitive for the filter: derive (if the predicate does not
    // read the derived column), sort, or select. The table temporaries are
    // re-threaded so each stage still reads its predecessor's output.
    bool moved = true;
    while (moved) {
        moved = false;
        for (size_t i = 1; i < code.size(); ++i) {
            Quad& f = code[i];
            Quad& s = code[i - 1];
            if (f.op != "filter" || f.a1 != s.res) continue;
            if (s.op != "derive" && s.op != "sort" && s.op != "select") continue;

            auto reads = readsOf(f.a2, schema[f.a1], frags);
            if (s.op == "derive" && reads.count(derivedCol(s))) continue;

            std::string readList;
            for (const auto& c : reads) readList += (readList.empty() ? "" : ", ") + c;
            rep.log.push_back("pushdown: filter " + f.a2 + " moved above " + s.op +
                              " (reads {" + readList + "})");
            // before: S(in)->A, F(A)->B     after: F(in)->A, S(A)->B
            Quad nf = f, ns = s;
            nf.a1 = s.a1; nf.res = s.res;
            ns.a1 = s.res; ns.res = f.res;
            s = nf; f = ns;
            ++rep.pushed;
            moved = true;
            recompute();
        }
    }

    // ---- 2. projection pruning at load -----------------------------------
    // Backward walk: needed[t] is the set of columns anything downstream reads
    // from table t. A shown table needs all its columns. A load then declares
    // only the needed ones, so unread columns are never parsed or carried.
    std::map<std::string, std::set<std::string>> needed;
    std::set<std::string> used;                // names that are read at all
    for (int i = static_cast<int>(code.size()) - 1; i >= 0; --i) {
        const Quad& q = code[i];
        auto& out = needed[q.res];
        if (q.op == "show") {
            for (const auto& c : schema[q.a1]) needed[q.a1].insert(c);
            used.insert(q.a1);
        } else if (q.op == "=" && schema.count(q.a1)) {
            // a table bound to a name nothing reads keeps every column
            if (!used.count(q.res)) for (const auto& c : schema[q.res]) out.insert(c);
            needed[q.a1].insert(out.begin(), out.end());
            used.insert(q.a1);
        } else if (isStage(q.op)) {
            auto& in = needed[q.a1];
            used.insert(q.a1);
            if (q.op == "filter" || q.op == "derive") {
                auto r = readsOf(fragOf(q), schema[q.a1], frags);
                in.insert(r.begin(), r.end());
                for (const auto& c : out)
                    if (q.op == "filter" || c != derivedCol(q)) in.insert(c);
            } else if (q.op == "select") {
                for (const auto& c : split(q.a2, ',')) in.insert(c);
            } else if (q.op == "group_by") {
                in.insert(out.begin(), out.end());
                for (const auto& c : split(q.a2, ',')) in.insert(c);
            } else if (q.op == "aggregate") {
                for (const auto& c : out) if (has(schema[q.a1], c)) in.insert(c);
                for (const auto& item : split(q.a2, ',')) {
                    auto lp = item.find('('), rp = item.find(')');
                    in.insert(item.substr(lp + 1, rp - lp - 1));
                }
            } else if (q.op == "sort") {
                in.insert(out.begin(), out.end());
                in.insert(q.a2.substr(0, q.a2.find(' ')));
            } else {                                          // limit
                in.insert(out.begin(), out.end());
            }
        }
    }
    for (auto& q : code) {
        if (q.op != "load") continue;
        const auto& need = needed[q.res];
        std::string kept, dropped;
        for (const auto& part : split(q.a2, ',')) {
            std::string name = part.substr(0, part.find(':'));
            if (need.count(name)) kept += (kept.empty() ? "" : ",") + part;
            else { dropped += (dropped.empty() ? "" : ", ") + name; ++rep.pruned; }
        }
        if (!dropped.empty() && !kept.empty()) {
            rep.log.push_back("pruning: load " + q.a1 + " drops unread columns {" + dropped + "}");
            q.a2 = kept;
        }
    }

    // ---- 3. stage fusion --------------------------------------------------
    // Adjacent filter and derive stages are all row-wise, so they fuse into
    // one rowpass quadruple whose argument lists the steps in order:
    //   F:P1;D:unit_price:=P0
    // The VM then visits each row once, and a failing filter step stops the
    // row before any later step runs.
    std::vector<Quad> out;
    std::map<int, int> newIndex;               // old index -> new index
    for (size_t i = 0; i < code.size(); ++i) {
        newIndex[static_cast<int>(i)] = static_cast<int>(out.size());
        const Quad& q = code[i];
        bool rowwise = q.op == "filter" || q.op == "derive";
        if (rowwise && !out.empty()) {
            Quad& prev = out.back();
            bool prevRow = prev.op == "filter" || prev.op == "derive" || prev.op == "rowpass";
            if (prevRow && prev.res == q.a1) {
                std::string step = (q.op == "filter" ? "F:" : "D:") + q.a2;
                if (prev.op != "rowpass") {
                    std::string first = (prev.op == "filter" ? "F:" : "D:") + prev.a2;
                    prev.op = "rowpass";
                    prev.a2 = first;
                }
                prev.a2 += ";" + step;
                prev.res = q.res;
                ++rep.fused;
                rep.log.push_back("fusion: " + q.op + " merged into one row pass -> " + prev.a2);
                continue;
            }
        }
        out.push_back(q);
    }
    newIndex[static_cast<int>(code.size())] = static_cast<int>(out.size());

    // fusion removed quadruples, so jump targets are renumbered
    for (auto& q : out) {
        if (!isJump(q.op) || q.res.empty()) continue;
        auto it = newIndex.find(std::stoi(q.res));
        if (it != newIndex.end()) q.res = std::to_string(it->second);
    }
    code = out;
    return rep;
}
