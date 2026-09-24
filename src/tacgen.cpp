#include "tacgen.hpp"
#include <iomanip>

void dumpStream(const Stream& s) {
    std::cout << "--- " << s.name << " (" << s.code.size() << " quadruples)\n";
    for (size_t i = 0; i < s.code.size(); ++i) {
        const Quad& q = s.code[i];
        std::cout << std::setw(3) << i << "  "
                  << std::left << std::setw(10) << q.op
                  << std::setw(12) << (q.a1.empty() ? "_" : q.a1)
                  << std::setw(12) << (q.a2.empty() ? "_" : q.a2)
                  << (q.res.empty() ? "_" : q.res)
                  << std::right << "\n";
    }
}

int TacGen::quadCount() const {
    int n = static_cast<int>(main_.code.size());
    for (const auto& f : frags_) n += static_cast<int>(f.code.size());
    return n;
}

void TacGen::generate(Node* program) {
    for (auto& k : program->kids) stmt(k.get(), main_);
}

void TacGen::block(Node* n, Stream& s) {
    for (auto& k : n->kids) stmt(k.get(), s);
}

static bool isBoolOp(const std::string& op) {
    return op == "&&" || op == "||" || op == "==" || op == "!=" ||
           op == "<"  || op == "<=" || op == ">"  || op == ">=";
}

void TacGen::stmt(Node* n, Stream& s) {
    if (!n) return;

    switch (n->kind) {
    case N::LetDecl:
    case N::Assign: {
        if (n->kids.empty()) break;
        std::string place = expr(n->kids[0].get(), s);
        s.emit("=", place, "", n->text);
        break;
    }

    case N::TableDecl: {
        std::string t = pipeline(n->kids[0].get(), s);
        s.emit("=", t, "", n->text);
        break;
    }

    case N::Show:
        s.emit("show", n->text, "", "");
        break;

    case N::Print: {
        std::string p = expr(n->kids[0].get(), s);
        s.emit("print", p, "", "");
        break;
    }

    case N::Block:
        block(n, s);
        break;

    case N::If: {
        List t, f;
        boolExpr(n->kids[0].get(), s, t, f);
        backpatch(s, t, s.next());                 // true falls into the then-part
        if (n->kids.size() > 1) stmt(n->kids[1].get(), s);

        if (n->kids.size() > 2) {                  // has an else
            int skip = s.emit("goto", "", "", "");
            backpatch(s, f, s.next());
            stmt(n->kids[2].get(), s);
            backpatch(s, makelist(skip), s.next());
        } else {
            backpatch(s, f, s.next());
        }
        break;
    }

    case N::While: {
        int top = s.next();                        // where the condition begins
        List t, f;
        boolExpr(n->kids[0].get(), s, t, f);
        backpatch(s, t, s.next());                 // true enters the body
        if (n->kids.size() > 1) stmt(n->kids[1].get(), s);
        int back = s.emit("goto", "", "", "");
        backpatch(s, makelist(back), top);         // back edge
        backpatch(s, f, s.next());                 // false leaves the loop
        break;
    }

    default:
        break;
    }
}

std::string TacGen::expr(Node* n, Stream& s) {
    if (!n) return "";

    switch (n->kind) {
    case N::IntLit:
    case N::FloatLit:
        return n->text;
    case N::StrLit:
        return "\"" + n->text + "\"";
    case N::BoolLit:
        return n->text == "true" ? "1" : "0";
    case N::Ident:
        return n->text;

    case N::Unary: {
        std::string a = expr(n->kids[0].get(), s);
        std::string t = newTemp();
        s.emit(n->text == "-" ? "neg" : "not", a, "", t);
        return t;
    }

    case N::Binary: {
        // A boolean expression used as a value is materialised as 0 or 1.
        if (isBoolOp(n->text)) {
            List tl, fl;
            boolExpr(n, s, tl, fl);
            std::string t = newTemp();
            backpatch(s, tl, s.next());
            s.emit("=", "1", "", t);
            int skip = s.emit("goto", "", "", "");
            backpatch(s, fl, s.next());
            s.emit("=", "0", "", t);
            backpatch(s, makelist(skip), s.next());
            return t;
        }

        std::string a = expr(n->kids[0].get(), s);
        std::string b = expr(n->kids[1].get(), s);

        // constant folding on integer literals, the one fold done this early
        if (n->kids[0]->kind == N::IntLit && n->kids[1]->kind == N::IntLit) {
            long long x = std::stoll(a), y = std::stoll(b), r = 0;
            bool ok = true;
            if (n->text == "+") r = x + y;
            else if (n->text == "-") r = x - y;
            else if (n->text == "*") r = x * y;
            else if (n->text == "/" && y != 0) r = x / y;
            else if (n->text == "%" && y != 0) r = x % y;
            else ok = false;
            if (ok) return std::to_string(r);
        }

        std::string t = newTemp();
        s.emit(n->text, a, b, t);
        return t;
    }

    default:
        return "";
    }
}

void TacGen::boolExpr(Node* n, Stream& s, List& truelist, List& falselist) {
    if (!n) return;

    if (n->kind == N::Binary && n->text == "&&") {
        List t1, f1;
        boolExpr(n->kids[0].get(), s, t1, f1);
        backpatch(s, t1, s.next());                // left true: test the right
        List t2, f2;
        boolExpr(n->kids[1].get(), s, t2, f2);
        truelist = t2;
        falselist = merge(f1, f2);                 // either false short-circuits
        return;
    }

    if (n->kind == N::Binary && n->text == "||") {
        List t1, f1;
        boolExpr(n->kids[0].get(), s, t1, f1);
        backpatch(s, f1, s.next());                // left false: test the right
        List t2, f2;
        boolExpr(n->kids[1].get(), s, t2, f2);
        truelist = merge(t1, t2);
        falselist = f2;
        return;
    }

    if (n->kind == N::Unary && n->text == "!") {
        List t, f;
        boolExpr(n->kids[0].get(), s, t, f);
        truelist = f;                              // negation swaps the lists
        falselist = t;
        return;
    }

    if (n->kind == N::Binary) {                    // a relational operator
        std::string a = expr(n->kids[0].get(), s);
        std::string b = expr(n->kids[1].get(), s);
        int jt = s.emit("if" + n->text, a, b, "");
        int jf = s.emit("goto", "", "", "");
        truelist = makelist(jt);
        falselist = makelist(jf);
        return;
    }

    // a bare value used as a condition
    std::string p = expr(n, s);
    int jt = s.emit("if!=", p, "0", "");
    int jf = s.emit("goto", "", "", "");
    truelist = makelist(jt);
    falselist = makelist(jf);
}

std::string TacGen::fragmentFor(Node* e, bool asPredicate) {
    Stream f{"P" + std::to_string(fragN_++), {}};

    if (asPredicate) {
        List t, fl;
        boolExpr(e, f, t, fl);
        std::string r = newTemp();
        backpatch(f, t, f.next());
        f.emit("=", "1", "", r);
        int skip = f.emit("goto", "", "", "");
        backpatch(f, fl, f.next());
        f.emit("=", "0", "", r);
        backpatch(f, makelist(skip), f.next());
        f.emit("ret", r, "", "");
    } else {
        std::string p = expr(e, f);
        f.emit("ret", p, "", "");
    }

    frags_.push_back(f);
    return frags_.back().name;
}

std::string TacGen::stage(Node* st, const std::string& in, Stream& s) {
    const std::string& name = st->text;
    std::string out = newTable();

    if (name == "filter") {
        std::string frag = fragmentFor(st->kids[0].get(), true);
        s.emit("filter", in, frag, out);
    } else if (name == "derive") {
        Node* item = st->kids[0].get();
        std::string frag = fragmentFor(item->kids[0].get(), false);
        s.emit("derive", in, item->text + ":=" + frag, out);
    } else if (name == "select" || name == "group_by") {
        std::string cols;
        for (const auto& id : st->kids[0]->kids)
            cols += (cols.empty() ? "" : ",") + id->text;
        s.emit(name, in, cols, out);
    } else if (name == "aggregate") {
        std::string spec;
        for (const auto& kid : st->kids) {
            Node* item = kid.get();
            spec += (spec.empty() ? "" : ",") + item->text + "=" +
                    item->type + "(" + item->kids[0]->text + ")";
        }
        s.emit("aggregate", in, spec, out);
    } else if (name == "sort") {
        Node* key = st->kids[0].get();
        s.emit("sort", in, key->text + " " + key->type, out);
    } else if (name == "limit") {
        s.emit("limit", in, st->kids[0]->text, out);
    }

    return out;
}

std::string TacGen::pipeline(Node* n, Stream& s) {
    std::string cur;
    bool first = true;

    for (const auto& kid : n->kids) {
        Node* k = kid.get();

        if (first) {
            first = false;
            if (k->kind == N::Load) {
                cur = newTable();
                // the declared schema travels with the load, so the runtime
                // can check the CSV header and parse each field by type
                std::string schema;
                for (const auto& c : k->kids)
                    schema += (schema.empty() ? "" : ",") + c->text + ":" + c->type;
                s.emit("load", "\"" + k->text + "\"", schema, cur);
            } else {
                cur = k->text;                     // an existing table
            }
            continue;
        }

        if (k->kind == N::Stage) cur = stage(k, cur, s);
    }
    return cur;
}