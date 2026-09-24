// gencsv: writes a deterministic sales CSV for benchmarking.
// usage: gencsv <rows> <out.csv>
// A fixed-seed linear congruential generator makes every run identical,
// so benchmark numbers are reproducible on any machine.
#include <cstdio>
#include <cstdlib>

int main(int argc, char** argv) {
    if (argc != 3) { std::fprintf(stderr, "usage: gencsv <rows> <out.csv>\n"); return 2; }
    long long n = std::atoll(argv[1]);
    std::FILE* f = std::fopen(argv[2], "w");
    if (!f) { std::fprintf(stderr, "cannot write %s\n", argv[2]); return 2; }

    const char* regions[]  = {"North", "South", "East", "West", "Central"};
    const char* products[] = {"Laptop", "Phone", "Tablet", "Monitor", "Printer", "Router"};
    unsigned long long x = 20260924ULL;
    auto next = [&]() { x = x * 6364136223846793005ULL + 1442695040888963407ULL; return x >> 33; };

    std::fprintf(f, "region,product,revenue,units,discount,returned\n");
    for (long long i = 0; i < n; ++i) {
        long long units = 1 + next() % 30;
        long long revenue = units * (20 + next() % 180);
        std::fprintf(f, "%s,%s,%lld,%lld,%llu,%s\n",
                     regions[next() % 5], products[next() % 6], revenue, units,
                     next() % 25, next() % 10 == 0 ? "true" : "false");
    }
    std::fclose(f);
    return 0;
}
