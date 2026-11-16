// Self-test snippet for tools/lint/determinism_lint.py. Not compiled.
// Every line that must be reported carries a trailing "expect: <rule>" marker;
// the lint must report exactly these (line, rule) pairs.
#include <cmath>  // expect: banned-include
#include <random>  // expect: banned-include
#include <chrono>  // expect: banned-include
#include <ctime>  // expect: banned-include
#  include <unordered_map>  // expect: banned-include
#include <unordered_set>  // expect: banned-include
#include <locale>  // expect: banned-include
#include <math.h>  // expect: banned-include
#include <cstdint>
#include <algorithm>

float f1 = 0;  // expect: float-type
double d1 = 0;  // expect: float-type
long double ld1 = 0;  // expect: float-type
static_assert(sizeof(std::float64_t) == 8);  // expect: float-type

long l1 = 0;  // expect: long-type
unsigned long ul1 = 0;  // expect: long-type
long int li1 = 0;  // expect: long-type
signed long sl1 = 0;  // expect: long-type
wchar_t wc = 0;  // expect: wchar-type

__int128 w1 = 0;  // expect: wide-int-type
unsigned __int128 w2 = 0;  // expect: wide-int-type
auto w3 = _umul128(1, 2, nullptr);  // expect: wide-int-type

void f(std::vector<int>& v, Gen& g, Obj& o) {
    std::hash<int> h;  // expect: std-hash
    std::shuffle(v.begin(), v.end(), g);  // expect: std-random
    std::ranges::shuffle(v, g);  // expect: std-random
    std::sample(v.begin(), v.end(), out, 3, g);  // expect: std-random
    std::mt19937_64 eng;  // expect: std-random
    std::uniform_int_distribution<int> dist(0, 9);  // expect: std-random
    int r = rand();  // expect: c-rand
    std::srand(1);  // expect: c-rand
    auto t = time(nullptr);  // expect: wall-clock
    auto c = std::clock();  // expect: wall-clock
    auto t2 = ::time(nullptr);  // expect: wall-clock
    if (r) return time(nullptr) + rand();  // expect: wall-clock, c-rand
    auto now = std::chrono::steady_clock::now();  // expect: wall-clock
    std::unordered_map<int, int> m;  // expect: unordered-container
    setlocale(LC_ALL, "");  // expect: locale
    std::cout.imbue(std::locale(""));  // expect: locale, stream-format
    auto s = std::sqrt(2);  // expect: std-math
    auto p = std::pow(2, 3);  // expect: std-math
    std::sort(v.begin(), v.end());  // expect: unstable-sort
    std::ranges::sort(v);  // expect: unstable-sort
    std::nth_element(v.begin(), v.begin() + 1, v.end());  // expect: unstable-sort
    std::partition(v.begin(), v.end(), pred);  // expect: unstable-sort
    std::  // expect: unstable-sort
        sort(v.begin(), v.end());  // split across lines: reported where it starts
}
