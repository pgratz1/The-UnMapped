// Self-test snippet: nothing in this file may be reported. Not compiled.
//
// Banned words in comments are ignored: float double long rand( time( std::sort
// #include <cmath>  std::hash  std::unordered_map  __int128  wchar_t
/* Block comments too: double, std::shuffle(v), setlocale(LC_ALL, "")
   #include <random>
   long l = 0; */
#include <cstdint>
#include <string_view>
#include "worldgen/core/fx.hpp"

namespace worldgen {

// Fixed-width and long long types are fine.
std::int64_t a = 0;
std::uint64_t b = 0;
long long ll = 0;
unsigned long long ull = 0;
long
    long split_ll = 0;

// Banned words inside string and character literals are ignored.
constexpr std::string_view s1 = "float double long rand(1) time(0) std::sort #include <cmath>";
constexpr std::string_view s2 = u8"std::hash<long> std::unordered_set";
constexpr std::string_view s3 = R"lint(raw: double "long" std::shuffle( )" still raw )lint";
constexpr std::string_view s4 = "escaped \" quote then double";
constexpr char c1 = '"';
constexpr char c2 = '\'';
constexpr std::int64_t sep = 1'000'000;  // digit separators are not char literals
constexpr std::int64_t hex = 0xFFFF'FFFF;
const char* after = "double";  // still a literal after the separators above

// Identifiers that merely contain banned words.
int doubled = 0;
int long_name = 0;
int floaty = 0;
int my_rand(int);
int runtime(int);
int lifetime = runtime(1);
int x = my_rand(2);

// Members and other namespaces with the same names are not the C functions.
struct Clock { int time(); int clock(); int rand(); };
int use(Clock& k, Clock* p) { return k.time() + p->clock() + k.rand() + Clock{}.time(); }
namespace other { int time(int); }
int z = other::time(1);

// The project's own functions and std algorithms that are fine.
void ok(std::vector<int>& v, Rng& rng) {
    rng.shuffle(v);
    std::stable_sort(v.begin(), v.end());
    std::is_sorted(v.begin(), v.end());
    std::partition_point(v.begin(), v.end(), pred);
    std::stable_partition(v.begin(), v.end(), pred);
    auto q = worldgen::sqrt(Fx::ONE);
    auto h = hash_words(TAG_SUB, {1, 2});
}

}  // namespace worldgen
