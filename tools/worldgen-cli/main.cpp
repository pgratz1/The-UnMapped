// worldgen-cli: scaffold. Prints the version and usage; the subcommands are
// implemented by the generator engineer from the specs in docs/specs/.

#include <cstdio>
#include <string_view>

#include "worldgen/version.hpp"

namespace {

void print_usage(std::FILE* out) {
    std::fputs(
        "usage: worldgen-cli <command> [options]\n"
        "\n"
        "commands (not implemented yet):\n"
        "  generate   generate a world file\n"
        "  render     export a map layer as an image\n"
        "  report     write a text history report\n"
        "  hash       print the hash of a world file\n"
        "\n"
        "options:\n"
        "  --version  print the generator version\n"
        "  --help     print this message\n",
        out);
}

}  // namespace

int main(int argc, char** argv) {
    if (argc < 2) {
        print_usage(stderr);
        return 2;
    }
    const std::string_view arg{argv[1]};
    if (arg == "--version") {
        const std::string_view version = worldgen::generator_version();
        std::printf("worldgen-cli %.*s\n", static_cast<int>(version.size()), version.data());
        return 0;
    }
    if (arg == "--help" || arg == "-h") {
        print_usage(stdout);
        return 0;
    }
    std::fprintf(stderr, "worldgen-cli: unknown command '%.*s'\n\n", static_cast<int>(arg.size()), arg.data());
    print_usage(stderr);
    return 2;
}
