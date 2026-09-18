#include "errors.hpp"
#include "token.hpp"
#include <iostream>
#include <string>

static void usage() {
    std::cout << "tblc - TabuLang compiler\n"
              << "usage: tblc <source.tbl> [--dump-tokens] [-O1]\n";
}

int main(int argc, char** argv) {
    if (argc < 2) { usage(); return 0; }

    std::string path;
    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        if (a == "--help") { usage(); return 0; }
        else if (!a.empty() && a[0] == '-') {
            std::cerr << "unknown option " << a << "\n";
            return 2;
        } else path = a;
    }

    ErrorLog errors;
    std::cout << "tblc: " << KEYWORDS.size() << " keywords, "
              << OPERATORS.size() << " operators registered\n";
    std::cout << "source: " << path << " (front end not wired in yet)\n";
    return errors.any() ? 1 : 0;
}