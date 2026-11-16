// Self-test snippet for the rules added in P1: nothing in this file may be
// reported. Not compiled.
//
// In comments: std::thread t; std::atomic<int> a; thread_local x; getenv("X")
// std::reduce std::priority_queue directory_iterator strtod( std::to_string
// #pragma omp parallel  sqlite3_bind_double  fwrite(p, 1, n, f);
// rng.next() + rng.next()  -ffast-math  /fp:fast
/* #include <thread>
   #include <atomic>
   std::cout << 1.5; */
#include <cstdint>
#include <cstdio>
#include <string_view>

namespace worldgen {

// In string literals.
constexpr std::string_view s1 = "std::thread std::async thread_local hardware_concurrency()";
constexpr std::string_view s2 = "getenv(\"HOME\") std::reduce(a, b) std::priority_queue<int>";
constexpr std::string_view s3 = R"(fwrite(p, 1, n, f); printf("%f", x); rng.next() + rng.next())";
constexpr std::string_view s4 = "#pragma STDC FP_CONTRACT ON";

// Identifiers that merely contain the banned names.
std::uint32_t threads = 1;
std::uint32_t thread_count = threads;
int my_thread = 0;
int reduce_rows(int);
int atomically = reduce_rows(1);
int getenv_cached(int);
int env = getenv_cached(2);
int printf_like(int);
int pl = printf_like(3);
int to_string_count = 0;
struct Heap { void make_heap(); void push_heap(); };
void write_file_atomically(std::string_view path);
void run_blocks(std::uint32_t count, std::uint32_t threads);
int sqlite3_bind_int64(void*, int, long long);
long long sqlite3_column_int64(void*, int);
struct directory_entry_like {};

// Members and other namespaces with the same names.
struct Log { int printf(const char*); int fwrite(int); int getenv(const char*); int reduce(); };
int use(Log& g, Log* p) { return g.printf("x") + p->fwrite(1) + g.getenv("X") + g.reduce(); }
namespace other { int getenv(const char*); int strtod(int); }
int z = other::getenv("X") + other::strtod(1);
std::string message(const LoadError& e) { return e.to_string(); }

// Write results that are used.
bool write_stream(std::FILE* stream, std::string_view text) noexcept {
    if (std::fwrite(text.data(), 1, text.size(), stream) != text.size()) {
        return false;
    }
    const bool flush_failed = std::fflush(stdout) != 0;
    return std::fputs("x", stream) >= 0 && !flush_failed;
}
int put(std::FILE* f) { return std::fputc('x', f); }
int declared_fwrite(const void*, std::size_t, std::size_t, std::FILE*);

// One draw per statement, in a condition and its body, in a loop header and
// its body, in separate statements of a lambda.
std::uint64_t draws(Rng& rng) {
    const std::uint64_t a = rng.next();
    const std::uint64_t b = rng.next();
    if (rng.chance(HALF)) {
        return rng.next();
    }
    if (rng.chance(HALF)) return rng.next_u32();
    for (std::uint64_t i = rng.uniform_u64(4); i < rng.uniform_u64(9); ++i) {
        consume(rng.next());
    }
    while (rng.chance(HALF)) consume(rng.next());
    auto f = [&rng] { const auto x = rng.next(); return x + rng.next(); };
    rng.shuffle(v);
    it.next_sibling();
    // Sequenced, or on different state: a conditional evaluates its condition
    // first and then one branch; declarators, braced-init-list elements and
    // comma operands are evaluated left to right; two generators share no
    // state (aliases of one generator are not detected).
    const std::uint64_t c = rng.chance(HALF) ? rng.next() : rng.uniform_u64(6);
    const std::uint64_t d = rng.next(), e = rng.next();
    const Grid g{2U + rng.uniform_u64(25U), 2U + rng.uniform_u64(25U)};
    const bool same = rng.next() == twin.next();
    std::uint64_t s0 = splitmix64_next(x0) ^ splitmix64_next(x1);
    return a ^ b;
}

// Thread identity of other kinds of objects is not matched by name alone.
std::uint64_t ident = node.id + layer.id;

}  // namespace worldgen
