// Self-test snippet: environment, directory listing, number/text conversion
// through the platform, formatted streams, SQLite value types, per-file
// floating-point pragmas, discarded write results and two random draws in
// one full expression. Scanned under its own path, so every rule applies.
// Not compiled.
#include <iostream>  // expect: stream-format
#include <sstream>  // expect: stream-format
#include <iomanip>  // expect: stream-format
#include <cstdio>
#include <filesystem>

#pragma float_control(precise, off)  // expect: fp-pragma
#pragma fp_contract(on)  // expect: fp-pragma
#pragma STDC FP_CONTRACT ON  // expect: fp-pragma
#  pragma STDC FENV_ACCESS ON  // expect: fp-pragma
#pragma GCC optimize("fast-math")  // expect: fp-pragma
#pragma clang fp contract(fast)  // expect: fp-pragma
_Pragma("GCC optimize(\"Ofast\")")  // expect: fp-pragma
__attribute__((optimize("fast-math"))) int fast();  // expect: fp-pragma
[[gnu::optimize("fast-math")]] int fast2();  // expect: fp-pragma

void environment() {
    const char* a = getenv("HOME");  // expect: environment
    const char* b = std::getenv("HOME");  // expect: environment
    auto c = _wgetenv(L"HOME");  // expect: environment
    GetEnvironmentVariableW(name, buf, n);  // expect: environment
    char** e = environ;  // expect: environment
}

void directories(const std::filesystem::path& p) {
    for (const auto& e : std::filesystem::directory_iterator(p)) {}  // expect: directory-iteration
    std::filesystem::recursive_directory_iterator it(p);  // expect: directory-iteration
    DIR* d = opendir(".");  // expect: directory-iteration
    FindFirstFileW(pattern, &data);  // expect: directory-iteration
}

void numbers(const char* s, char* buf) {
    auto a = strtod(s, nullptr);  // expect: number-text
    auto b = std::strtol(s, nullptr, 10);  // expect: number-text
    auto c = strtoull(s, nullptr, 10);  // expect: number-text
    int d = atoi(s);  // expect: number-text
    auto f = std::stod(text);  // expect: number-text
    auto g = std::stoi(text);  // expect: number-text
    auto h = std::to_string(42);  // expect: number-text
    std::snprintf(buf, 8, "%d", 1);  // expect: number-text
    if (sscanf(s, "%d", &d) != 1) return;  // expect: number-text
    std::cout << 1.5;  // expect: stream-format
    std::ostringstream os;  // expect: stream-format
    std::wstringstream ws;  // expect: stream-format
    std::cerr << std::setprecision(3);  // expect: stream-format
}

void sqlite(sqlite3_stmt* st, sqlite3_context* cx, sqlite3_value* v) {
    sqlite3_bind_double(st, 1, x);  // expect: sqlite-value-type
    sqlite3_bind_int(st, 1, 7);  // expect: sqlite-value-type
    auto a = sqlite3_column_double(st, 0);  // expect: sqlite-value-type
    auto b = sqlite3_column_int(st, 0);  // expect: sqlite-value-type
    sqlite3_result_double(cx, x);  // expect: sqlite-value-type
    sqlite3_result_int(cx, 1);  // expect: sqlite-value-type
    auto c = sqlite3_value_double(v);  // expect: sqlite-value-type
}

void writes(std::FILE* f, std::string_view t) {
    std::fwrite(t.data(), 1, t.size(), f);  // expect: unchecked-write
    fputs("x", f);  // expect: unchecked-write
    (void)std::fputs("x", f);  // expect: unchecked-write
    static_cast<void>(fwrite(t.data(), 1, t.size(), stdout));  // expect: unchecked-write
    if (t.empty()) std::fflush(stdout);  // expect: unchecked-write
    else fputc('x', f);  // expect: unchecked-write
    { ::fclose(f); }  // expect: unchecked-write
}

void draws(Rng& rng, Rng* p) {
    auto a = rng.next() + rng.next();  // expect: two-draws
    use(rng.uniform_u64(6), rng.uniform_u64(6));  // expect: two-draws
    bool both = p->chance(half) && p -> chance(half);  // expect: two-draws
    auto w = rng.weighted_index(ws) * rng.next_u32();  // expect: two-draws
    auto u = streams.rng().next() - streams.rng().next();  // expect: two-draws
    auto v = sum(f(rng.next()), g[rng.next() % 4]);  // expect: two-draws    auto s = splitmix64_next(x) ^ splitmix64_next(x);  // expect: two-draws
    if (rng.chance(half) || rng.chance_ratio(1, 3)) {  // expect: two-draws
        return;
    }
    auto t = pick(rng.uniform_i64(0, 9),  // expect: two-draws
                  rng.uniform_fx01());  // reported once, where the expression's first draw is
}
