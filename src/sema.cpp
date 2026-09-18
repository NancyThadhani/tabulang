#include "sema.hpp"

bool Sema::assignable(const std::string& from, const std::string& to) const {
    if (from.empty() || to.empty()) return true;       // already reported
    if (from == to) return true;
    return from == "int" && to == "float";             // widening only
}

void Sema::analyze(Node* program) {
    for (auto& k : program->kids) stmt(k.get());
}

void Sema::block(Node* n) {
    sym_.push();
    for (auto& k : n->kids) stmt(k.get());
    sym_.pop();
}

void Sema::stmt(Node* n) {
    if (!n) return;

    switch (n->kind) {
    case N::LetDecl: {
        std::string rhs = n->kids.empty() ? "" : expr(n->kids[0].get());
        if (!assignable(rhs, n->type))
            typeErr(n, "cannot initialise '" + n->text + "' of type " + n->type +
                       " with an expression of type " + rhs);
        Symbol s;
        s.name = n->text; s.kind = SymKind::Var; s.type = n->type;
        s.line = n->line; s.col = n->col;
        if (!sym_.declare(s)) {
            Symbol* prev = sym_.lookupInnermost(n->text);
            err(n, "'" + n->text + "' is already declared in this scope at line " +
                   std::to_string(prev ? prev->line : 0));
        }
        break;
    }

    case N::Assign: {
        Symbol* s = sym_.lookup(n->text);
        if (!s) {
            err(n, "undeclared identifier '" + n->text + "'");
            if (!n->kids.empty()) expr(n->kids[0].get());
            break;
        }
        if (s->kind == SymKind::Table) {
            typeErr(n, "'" + n->text + "' is a table and cannot be assigned a scalar");
            break;
        }
        std::string rhs = n->kids.empty() ? "" : expr(n->kids[0].get());
        if (!assignable(rhs, s->type))
            typeErr(n, "cannot assign " + rhs + " to '" + n->text +
                       "' of type " + s->type);
        break;
    }

    case N::Show: {
        Symbol* s = sym_.lookup(n->text);
        if (!s) err(n, "undeclared identifier '" + n->text + "'");
        else if (s->kind != SymKind::Table)
            typeErr(n, "'" + n->text + "' is not a table, so it cannot be shown");
        break;
    }

    case N::Print:
        if (!n->kids.empty()) expr(n->kids[0].get());
        break;

    case N::If:
    case N::While: {
        if (!n->kids.empty()) {
            std::string c = expr(n->kids[0].get());
            if (!c.empty() && c != "bool")
                typeErr(n->kids[0].get(),
                        std::string(n->kind == N::If ? "if" : "while") +
                        " condition must be bool, found " + c);
        }
        for (size_t i = 1; i < n->kids.size(); ++i) stmt(n->kids[i].get());
        break;
    }

    case N::Block:
        block(n);
        break;

    case N::TableDecl: {
        Schema sch;
        if (!n->kids.empty()) sch = pipeline(n->kids[0].get());
        Symbol s;
        s.name = n->text; s.kind = SymKind::Table; s.schema = sch;
        s.line = n->line; s.col = n->col;
        if (!sym_.declare(s))
            err(n, "'" + n->text + "' is already declared in this scope");
        break;
    }

    default:
        break;
    }
}

std::string Sema::expr(Node* n) {
    if (!n) return "";
    std::string t;

    switch (n->kind) {
    case N::IntLit:   t = "int";    break;
    case N::FloatLit: t = "float";  break;
    case N::StrLit:   t = "string"; break;
    case N::BoolLit:  t = "bool";   break;

    case N::Ident: {
        Symbol* s = sym_.lookup(n->text);
        if (!s) {
            err(n, "undeclared identifier '" + n->text + "'" +
                   (inStage_ ? suggest(stageSchema_, n->text) : ""));
            t = "";
        }
        else if (s->kind == SymKind::Table) {
            typeErr(n, "'" + n->text + "' is a table and cannot be used in an expression");
            t = "";
        } else t = s->type;
        break;
    }

    case N::Binary: t = binary(n); break;
    case N::Unary:  t = unary(n);  break;
    default: t = ""; break;
    }

    n->type = t;
    return t;
}

std::string Sema::binary(Node* n) {
    std::string a = expr(n->kids[0].get());
    std::string b = expr(n->kids[1].get());
    const std::string& op = n->text;

    if (a.empty() || b.empty()) return "";          // already reported

    if (op == "&&" || op == "||") {
        if (a != "bool" || b != "bool")
            typeErr(n, "operator " + op + " requires bool operands, found " +
                       a + " and " + b);
        return "bool";
    }

    if (op == "==" || op == "!=") {
        bool ok = (a == b) || (numeric(a) && numeric(b));
        if (!ok) typeErr(n, "cannot compare " + a + " with " + b);
        return "bool";
    }

    if (op == "<" || op == "<=" || op == ">" || op == ">=") {
        bool ok = (numeric(a) && numeric(b)) || (a == "string" && b == "string");
        if (!ok) typeErr(n, "operator " + op + " requires ordered operands, found " +
                            a + " and " + b);
        return "bool";
    }

    // arithmetic
    if (!numeric(a) || !numeric(b)) {
        typeErr(n, "operator " + op + " requires numeric operands, found " +
                   a + " and " + b);
        return "";
    }
    if (op == "%" && (a == "float" || b == "float")) {
        typeErr(n, "operator % requires int operands, found " + a + " and " + b);
        return "int";
    }
    return (a == "float" || b == "float") ? "float" : "int";   // int promotes
}

std::string Sema::unary(Node* n) {
    std::string a = expr(n->kids[0].get());
    if (a.empty()) return "";
    if (n->text == "-") {
        if (!numeric(a)) {
            typeErr(n, "unary - requires a numeric operand, found " + a);
            return "";
        }
        return a;
    }
    if (a != "bool") typeErr(n, "unary ! requires a bool operand, found " + a);
    return "bool";
}

/* ---------- schema inference ---------- */

const Column* Sema::findCol(const Schema& s, const std::string& name) const {
    for (const auto& c : s) if (c.name == name) return &c;
    return nullptr;
}

// One edit, or one adjacent transposition. Cheap, and it catches the typos
// an analyst actually makes: regoin for region, reveune for revenue.
static bool withinOne(const std::string& a, const std::string& b) {
    if (a == b) return false;
    size_t la = a.size(), lb = b.size();
    if (la > lb + 1 || lb > la + 1) return false;

    size_t i = 0, j = 0, edits = 0;
    while (i < la && j < lb) {
        if (a[i] == b[j]) { ++i; ++j; continue; }
        if (++edits > 1) break;
        if (la == lb) { ++i; ++j; }
        else if (la < lb) ++j;
        else ++i;
    }
    if (edits <= 1) {
        if (i < la || j < lb) ++edits;
        if (edits <= 1) return true;
    }

    // adjacent transposition
    if (la == lb) {
        size_t diff = 0, first = 0;
        for (size_t k = 0; k < la; ++k)
            if (a[k] != b[k]) { if (diff++ == 0) first = k; }
        if (diff == 2 && first + 1 < la &&
            a[first] == b[first + 1] && a[first + 1] == b[first])
            return true;
    }
    return false;
}

std::string Sema::suggest(const Schema& s, const std::string& name) const {
    for (const auto& c : s)
        if (withinOne(c.name, name)) return "; did you mean '" + c.name + "'?";
    return "";
}

std::string Sema::aggregateType(const std::string& fn, const std::string& colType,
                                Node* at) {
    if (fn == "count") return "int";                 // count ignores the column type
    if (fn == "sum" || fn == "min" || fn == "max") {
        if (!numeric(colType)) {
            typeErr(at, fn + "() requires a numeric column, found " + colType);
            return "int";
        }
        return colType;                              // int stays int
    }
    if (fn == "mean" || fn == "avg") {
        if (!numeric(colType)) {
            typeErr(at, fn + "() requires a numeric column, found " + colType);
            return "float";
        }
        return "float";                              // always widens
    }
    err(at, "unknown aggregate function '" + fn + "'");
    return colType;
}

Schema Sema::applyStage(Node* st, const Schema& in) {
    const std::string& name = st->text;

    // Columns of the incoming schema shadow outer variables inside a stage.
    auto pushColumns = [&]() {
        sym_.push();
        inStage_ = true;
        stageSchema_ = in;
        for (const auto& c : in) {
            Symbol s;
            s.name = c.name; s.kind = SymKind::Var; s.type = c.type;
            s.line = st->line; s.col = st->col;
            sym_.declare(s);
        }
    };
    auto popColumns = [&]() { sym_.pop(); inStage_ = false; };

    if (name == "filter") {
        pushColumns();
        std::string t = st->kids.empty() ? "" : expr(st->kids[0].get());
        if (!t.empty() && t != "bool")
            typeErr(st, "filter predicate must be bool, found " + t);
        popColumns();
        return in;                                   // schema unchanged
    }

    if (name == "select") {
        Schema out;
        if (!st->kids.empty())
            for (const auto& id : st->kids[0]->kids) {
                const Column* c = findCol(in, id->text);
                if (!c) err(id.get(), "column '" + id->text + "' is not in scope" +
                                      suggest(in, id->text));
                else out.push_back(*c);
            }
        return out.empty() ? in : out;               // keep something on error
    }

    if (name == "derive") {
        if (st->kids.empty()) return in;
        Node* item = st->kids[0].get();
        pushColumns();
        std::string t = item->kids.empty() ? "" : expr(item->kids[0].get());
        popColumns();
        Schema out = in;
        if (findCol(in, item->text))
            err(item, "column '" + item->text + "' already exists");
        else
            out.push_back({item->text, t.empty() ? "int" : t});
        return out;
    }

    if (name == "group_by") {
        Schema keys;
        if (!st->kids.empty())
            for (const auto& id : st->kids[0]->kids) {
                const Column* c = findCol(in, id->text);
                if (!c) err(id.get(), "column '" + id->text + "' is not in scope" +
                                      suggest(in, id->text));
                else keys.push_back(*c);
            }
        groupKeys_ = keys;                           // consumed by aggregate
        return in;                                   // schema unchanged here
    }

    if (name == "aggregate") {
        Schema out = groupKeys_;                     // keys survive the aggregate
        for (const auto& kid : st->kids) {
            Node* item = kid.get();                  // text = output, type = fn
            std::string argType;
            if (!item->kids.empty()) {
                const std::string& argName = item->kids[0]->text;
                const Column* c = findCol(in, argName);
                if (!c) {
                    err(item->kids[0].get(),
                        "column '" + argName + "' is not in scope" +
                        suggest(in, argName));
                    argType = "int";
                } else argType = c->type;
            }
            out.push_back({item->text, aggregateType(item->type, argType, item)});
        }
        groupKeys_.clear();
        return out;
    }

    if (name == "sort") {
        if (!st->kids.empty()) {
            Node* key = st->kids[0].get();
            const Column* c = findCol(in, key->text);
            if (!c) err(key, "column '" + key->text + "' is not in scope" +
                             suggest(in, key->text));
            else if (c->type == "bool")
                typeErr(key, "cannot sort on a bool column");
        }
        return in;
    }

    if (name == "limit") {
        if (!st->kids.empty()) {
            Node* b = st->kids[0].get();
            if (b->kind != N::IntLit)
                typeErr(b, "limit requires an integer bound");
            else if (std::stoll(b->text) < 0)
                typeErr(b, "limit bound must not be negative");
        }
        return in;
    }

    return in;
}

Schema Sema::pipeline(Node* n) {
    Schema cur;
    bool first = true;

    for (const auto& kid : n->kids) {
        Node* k = kid.get();

        if (first) {
            first = false;
            if (k->kind == N::Load) {
                for (const auto& c : k->kids)
                    cur.push_back({c->text, c->type});
            } else if (k->kind == N::TableRef) {
                Symbol* s = sym_.lookup(k->text);
                if (!s) err(k, "undeclared table '" + k->text + "'");
                else if (s->kind != SymKind::Table)
                    typeErr(k, "'" + k->text + "' is not a table");
                else cur = s->schema;
            }
            continue;
        }

        if (k->kind == N::Stage) cur = applyStage(k, cur);
    }
    return cur;
}