// worldgen-cli entry point (spec P0-04 section 4.1). Arguments are taken as
// UTF-8: argv on POSIX, the UTF-16 arguments of wmain on Windows, where the
// narrow argv would be in the ANSI code page.

#include <string>
#include <vector>

#include "cli.hpp"
#include "platform.hpp"

#if defined(_WIN32)

// determinism-lint: allow(wchar-type) -- Windows UTF-16 API boundary, converted to UTF-8 at once (P0-04 4.1)
int wmain(int argc, wchar_t** argv) {
    cli::platform::prepare_stdio();
    try {
        int bad_index = 0;
        const auto args = cli::platform::utf8_arguments(argc, argv, bad_index);
        if (!args) {
            // determinism-lint: allow(number-text) -- int argument index in a stderr diagnostic; integer to_string has no locale or platform variation
            cli::print_error("argument " + std::to_string(bad_index) + " is not valid UTF-16");
            cli::print_hint("");  // a usage error: the hint line of P0-04 4.1
            return 2;
        }
        return cli::run(*args);
    } catch (...) {
        cli::print_error("internal error: out of memory reading the arguments");
        return 1;
    }
}

#else

int main(int argc, char** argv) {
    cli::platform::prepare_stdio();
    try {
        std::vector<std::string> args;
        for (int i = 1; i < argc; ++i) {
            args.emplace_back(argv[i]);
        }
        return cli::run(args);
    } catch (...) {
        cli::print_error("internal error: out of memory reading the arguments");
        return 1;
    }
}

#endif
