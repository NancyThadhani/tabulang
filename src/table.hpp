#pragma once
#include <string>
#include <vector>

// One cell. Which field is meaningful depends on the column's type:
// int and bool use i, float uses f, string uses s.
struct Cell {
    long long i = 0;
    double f = 0.0;
    std::string s;
};

struct ColInfo {
    std::string name;
    std::string type;                 // int | float | bool | string
};

// A table value at run time. group_by only records keys; the next
// aggregate stage consumes them.
struct Table {
    std::vector<ColInfo> cols;
    std::vector<std::vector<Cell>> rows;
    std::vector<std::string> groupKeys;

    int colIndex(const std::string& name) const;   // -1 if absent
};

// Parses "region:string,revenue:int" into column records.
std::vector<ColInfo> parseSchemaSpec(const std::string& spec);

// Reads a CSV file with a header row, checked against the declared schema.
// On failure returns false and writes a message naming the line and column.
bool loadCsv(const std::string& path, const std::vector<ColInfo>& schema,
             Table& out, std::string& err);

// Pure table operators. Each returns false and fills err on a runtime error.
bool opSelect(const Table& in, const std::string& cols, Table& out, std::string& err);
bool opGroupBy(const Table& in, const std::string& keys, Table& out, std::string& err);
bool opAggregate(const Table& in, const std::string& spec, Table& out, std::string& err);
bool opSort(const Table& in, const std::string& spec, Table& out, std::string& err);
bool opLimit(const Table& in, const std::string& n, Table& out, std::string& err);

void printTable(const std::string& name, const Table& t);
std::string cellText(const Cell& c, const std::string& type);
