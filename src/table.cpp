#include "table.hpp"
#include <algorithm>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <sstream>

int Table::colIndex(const std::string& name) const {
    for (size_t i = 0; i < cols.size(); ++i)
        if (cols[i].name == name) return static_cast<int>(i);
    return -1;
}

static std::vector<std::string> splitOn(const std::string& s, char sep) {
    std::vector<std::string> out;
    std::string cur;
    for (char c : s) {
        if (c == sep) { out.push_back(cur); cur.clear(); }
        else cur += c;
    }
    out.push_back(cur);
    return out;
}

static std::string trim(const std::string& s) {
    size_t a = s.find_first_not_of(" \t\r\n");
    if (a == std::string::npos) return "";
    size_t b = s.find_last_not_of(" \t\r\n");
    return s.substr(a, b - a + 1);
}

std::vector<ColInfo> parseSchemaSpec(const std::string& spec) {
    std::vector<ColInfo> out;
    if (spec.empty()) return out;
    for (const auto& part : splitOn(spec, ',')) {
        auto colon = part.find(':');
        out.push_back({part.substr(0, colon), part.substr(colon + 1)});
    }
    return out;
}

// Converts one CSV field to a cell of the declared type.
static bool parseCell(const std::string& raw, const std::string& type,
                      Cell& c, std::string& why) {
    std::string v = trim(raw);
    try {
        if (type == "int") {
            size_t used = 0;
            c.i = std::stoll(v, &used);
            if (used != v.size()) throw std::invalid_argument("trailing");
        } else if (type == "float") {
            size_t used = 0;
            c.f = std::stod(v, &used);
            if (used != v.size()) throw std::invalid_argument("trailing");
        } else if (type == "bool") {
            if (v == "true" || v == "1") c.i = 1;
            else if (v == "false" || v == "0") c.i = 0;
            else throw std::invalid_argument("bool");
        } else {
            c.s = v;
        }
    } catch (...) {
        why = "expected " + type + ", found '" + v + "'";
        return false;
    }
    return true;
}

bool loadCsv(const std::string& path, const std::vector<ColInfo>& schema,
             Table& out, std::string& err) {
    std::ifstream in(path);
    if (!in) { err = "cannot open '" + path + "'"; return false; }

    std::string line;
    if (!std::getline(in, line)) { err = "'" + path + "' is empty"; return false; }

    // Map each declared column to its position in the header.
    auto header = splitOn(line, ',');
    for (auto& h : header) h = trim(h);
    std::vector<int> pos;
    for (const auto& c : schema) {
        auto it = std::find(header.begin(), header.end(), c.name);
        if (it == header.end()) {
            err = path + ": declared column '" + c.name + "' is not in the CSV header";
            return false;
        }
        pos.push_back(static_cast<int>(it - header.begin()));
    }

    out.cols = schema;
    out.rows.clear();
    int lineNo = 1;
    while (std::getline(in, line)) {
        ++lineNo;
        if (trim(line).empty()) continue;
        auto fields = splitOn(line, ',');
        if (fields.size() != header.size()) {
            err = path + ":" + std::to_string(lineNo) + ": expected " +
                  std::to_string(header.size()) + " fields, found " +
                  std::to_string(fields.size());
            return false;
        }
        std::vector<Cell> row(schema.size());
        for (size_t k = 0; k < schema.size(); ++k) {
            std::string why;
            if (!parseCell(fields[pos[k]], schema[k].type, row[k], why)) {
                err = path + ":" + std::to_string(lineNo) + ": column '" +
                      schema[k].name + "' " + why;
                return false;
            }
        }
        out.rows.push_back(std::move(row));
    }
    return true;
}

bool opSelect(const Table& in, const std::string& cols, Table& out, std::string& err) {
    std::vector<int> idx;
    out.cols.clear();
    for (const auto& name : splitOn(cols, ',')) {
        int k = in.colIndex(name);
        if (k < 0) { err = "select: no column '" + name + "'"; return false; }
        idx.push_back(k);
        out.cols.push_back(in.cols[k]);
    }
    out.rows.clear();
    out.rows.reserve(in.rows.size());
    for (const auto& r : in.rows) {
        std::vector<Cell> nr;
        for (int k : idx) nr.push_back(r[k]);
        out.rows.push_back(std::move(nr));
    }
    return true;
}

bool opGroupBy(const Table& in, const std::string& keys, Table& out, std::string& err) {
    out = in;
    out.groupKeys = splitOn(keys, ',');
    for (const auto& k : out.groupKeys)
        if (in.colIndex(k) < 0) { err = "group_by: no column '" + k + "'"; return false; }
    return true;
}

// One aggregate request, parsed from "total=sum(revenue)".
struct AggReq { std::string out, fn, col; };

static std::vector<AggReq> parseAggSpec(const std::string& spec) {
    std::vector<AggReq> out;
    for (const auto& part : splitOn(spec, ',')) {
        auto eq = part.find('='), lp = part.find('('), rp = part.find(')');
        out.push_back({part.substr(0, eq), part.substr(eq + 1, lp - eq - 1),
                       part.substr(lp + 1, rp - lp - 1)});
    }
    return out;
}

static double asDouble(const Cell& c, const std::string& type) {
    return type == "float" ? c.f : static_cast<double>(c.i);
}

bool opAggregate(const Table& in, const std::string& spec, Table& out, std::string& err) {
    auto reqs = parseAggSpec(spec);

    std::vector<int> keyIdx;
    for (const auto& k : in.groupKeys) keyIdx.push_back(in.colIndex(k));

    std::vector<int> colIdx;
    for (const auto& r : reqs) {
        int k = in.colIndex(r.col);
        if (k < 0 && r.fn != "count") { err = "aggregate: no column '" + r.col + "'"; return false; }
        colIdx.push_back(k);
    }

    // Output schema: the group keys, then one column per aggregate.
    out.cols.clear();
    out.groupKeys.clear();
    for (int k : keyIdx) out.cols.push_back(in.cols[k]);
    for (size_t j = 0; j < reqs.size(); ++j) {
        std::string t = "int";
        if (reqs[j].fn == "mean" || reqs[j].fn == "avg") t = "float";
        else if (reqs[j].fn != "count" && in.cols[colIdx[j]].type == "float") t = "float";
        out.cols.push_back({reqs[j].out, t});
    }

    // Groups keep first-appearance order, so output is deterministic.
    std::map<std::string, size_t> groupOf;
    std::vector<std::vector<Cell>> keyCells;
    std::vector<std::vector<double>> acc;     // running sum, min or max
    std::vector<long long> count;

    for (const auto& row : in.rows) {
        std::string key;
        for (int k : keyIdx) key += cellText(row[k], in.cols[k].type) + '\x1f';

        auto it = groupOf.find(key);
        size_t g;
        if (it == groupOf.end()) {
            g = keyCells.size();
            groupOf[key] = g;
            std::vector<Cell> kc;
            for (int k : keyIdx) kc.push_back(row[k]);
            keyCells.push_back(kc);
            std::vector<double> a(reqs.size(), 0.0);
            for (size_t j = 0; j < reqs.size(); ++j)
                if (reqs[j].fn == "min" || reqs[j].fn == "max")
                    a[j] = asDouble(row[colIdx[j]], in.cols[colIdx[j]].type);
            acc.push_back(a);
            count.push_back(0);
        } else {
            g = it->second;
        }

        ++count[g];
        for (size_t j = 0; j < reqs.size(); ++j) {
            if (reqs[j].fn == "count") continue;
            double v = asDouble(row[colIdx[j]], in.cols[colIdx[j]].type);
            if (reqs[j].fn == "min")      acc[g][j] = std::min(acc[g][j], v);
            else if (reqs[j].fn == "max") acc[g][j] = std::max(acc[g][j], v);
            else                          acc[g][j] += v;          // sum, mean
        }
    }

    out.rows.clear();
    for (size_t g = 0; g < keyCells.size(); ++g) {
        std::vector<Cell> r = keyCells[g];
        for (size_t j = 0; j < reqs.size(); ++j) {
            Cell c;
            const std::string& t = out.cols[keyIdx.size() + j].type;
            double v = reqs[j].fn == "count" ? static_cast<double>(count[g])
                     : (reqs[j].fn == "mean" || reqs[j].fn == "avg")
                           ? acc[g][j] / static_cast<double>(count[g])
                           : acc[g][j];
            if (t == "float") c.f = v; else c.i = static_cast<long long>(v);
            r.push_back(c);
        }
        out.rows.push_back(std::move(r));
    }
    return true;
}

bool opSort(const Table& in, const std::string& spec, Table& out, std::string& err) {
    auto sp = spec.find(' ');
    std::string col = spec.substr(0, sp);
    bool desc = sp != std::string::npos && spec.substr(sp + 1) == "desc";
    int k = in.colIndex(col);
    if (k < 0) { err = "sort: no column '" + col + "'"; return false; }

    out = in;
    const std::string type = in.cols[k].type;
    // stable_sort keeps ties in input order, so results are reproducible
    std::stable_sort(out.rows.begin(), out.rows.end(),
        [&](const std::vector<Cell>& a, const std::vector<Cell>& b) {
            bool less;
            if (type == "string")     less = a[k].s < b[k].s;
            else if (type == "float") less = a[k].f < b[k].f;
            else                      less = a[k].i < b[k].i;
            bool greater;
            if (type == "string")     greater = b[k].s < a[k].s;
            else if (type == "float") greater = b[k].f < a[k].f;
            else                      greater = b[k].i < a[k].i;
            return desc ? greater : less;
        });
    return true;
}

bool opLimit(const Table& in, const std::string& n, Table& out, std::string& err) {
    long long lim = std::stoll(n);
    if (lim < 0) { err = "limit: bound must be non-negative"; return false; }
    out.cols = in.cols;
    out.groupKeys.clear();
    size_t take = std::min(static_cast<size_t>(lim), in.rows.size());
    out.rows.assign(in.rows.begin(), in.rows.begin() + take);
    return true;
}

std::string cellText(const Cell& c, const std::string& type) {
    if (type == "string") return c.s;
    if (type == "bool")   return c.i ? "true" : "false";
    if (type == "float") {
        std::ostringstream os;
        os << std::fixed << std::setprecision(2) << c.f;
        return os.str();
    }
    return std::to_string(c.i);
}

void printTable(const std::string& name, const Table& t) {
    std::vector<size_t> w;
    for (const auto& c : t.cols) w.push_back(c.name.size());
    for (const auto& r : t.rows)
        for (size_t k = 0; k < r.size(); ++k)
            w[k] = std::max(w[k], cellText(r[k], t.cols[k].type).size());

    std::cout << name << " (" << t.rows.size() << " rows)\n";
    for (size_t k = 0; k < t.cols.size(); ++k)
        std::cout << (k ? "  " : "") << std::left << std::setw(w[k]) << t.cols[k].name;
    std::cout << "\n";
    for (size_t k = 0; k < t.cols.size(); ++k)
        std::cout << (k ? "  " : "") << std::string(w[k], '-');
    std::cout << "\n";
    for (const auto& r : t.rows) {
        for (size_t k = 0; k < r.size(); ++k) {
            std::string s = cellText(r[k], t.cols[k].type);
            if (t.cols[k].type == "string")
                std::cout << (k ? "  " : "") << std::left << std::setw(w[k]) << s;
            else
                std::cout << (k ? "  " : "") << std::right << std::setw(w[k]) << s;
        }
        std::cout << "\n";
    }
    std::cout << std::left;
}
