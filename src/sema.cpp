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
        if (!n->kids.empty()) {
            std::string t = expr(n->kids[0].get());
            if (!t.empty() && t == "string")
                ;                                    // printing a string is fine
        }
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

    case N::TableDecl:
        // Schema inference arrives in step 4. For now the table is declared
        // with an empty schema so later references resolve.
        {
            Symbol s;
            s.name = n->text; s.kind = SymKind::Table;
            s.line = n->line; s.col = n->col;
            if (!sym_.declare(s))
                err(n, "'" + n->text + "' is already declared in this scope");
        }
        break;

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
        if (!s) { err(n, "undeclared identifier '" + n->text + "'"); t = ""; }
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