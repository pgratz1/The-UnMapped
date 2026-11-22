// Self-test snippet: non-ASCII characters written directly in string and
// character literals (non-ascii-literal, P4-02 audit). Comments may hold
// them: ° é ü — this line is not reported. Not compiled.
#include <string>
#include <string_view>

constexpr std::string_view UNIT = "°C";  // expect: non-ascii-literal
constexpr std::string_view LATER = "tenths of a degree (°C)";  // expect: non-ascii-literal
const char* two = "é" "ü";  // expect: non-ascii-literal
const char* raw = R"x(Höhe)x";  // expect: non-ascii-literal
const char8_t* u8s = u8"naïve";  // expect: non-ascii-literal
const wchar_t wide = L'ß';  // expect: non-ascii-literal, wchar-type
const char32_t c32 = U'€';  // expect: non-ascii-literal
const char* multi = "a°b°c";  // expect: non-ascii-literal

// The fixed form: hex escapes, the literal split after the last one.
constexpr std::string_view FIXED = "\xC2\xB0" "C";
constexpr std::string_view ESC = "\u00B0";
const char quote = '\'';
const char* tricky = "a \" ° b";  // expect: non-ascii-literal
int sep = 1'000;
