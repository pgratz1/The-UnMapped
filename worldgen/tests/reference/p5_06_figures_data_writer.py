"""Writes worldgen/tests/history/p5_06_figures_data.hpp from p3_data_reference's 0.27.0 listings.
usage (from the repository root): python3 -B worldgen/tests/reference/p5_06_figures_data_writer.py \
    worldgen/tests/reference worldgen/tests/history/p5_06_figures_data.hpp
The comment and includes before the namespace are kept from the existing file."""
import sys, os
refdir, out = sys.argv[1], sys.argv[2]
sys.path.insert(0, refdir)
import p3_data_reference as d
d.use_version("0.27.0")
old = open(out).read() if os.path.exists(out) else ""
head = old[:old.index("namespace p506data {")]   # the comment and includes are kept as they are
L = [head + "namespace p506data {\n\n"]
L.append("// 4.4: game/data/history/figures.toml as the spec states it.\n")
L.append('inline constexpr std::string_view FIGURES_TOML = R"toml(' + d.figures_file_027() + ')toml";\n\n')
L.append("// 4.1: race -> temper_weights (steady, warlike, peaceable, grasping, just), general, hero, magic permille.\n"
         "struct RaceFields {\n    std::string_view race;\n    std::array<std::int64_t, 5> temper;\n"
         "    std::int64_t general, hero, magic;\n};\n"
         "inline constexpr std::array<RaceFields, %d> RACES = {{\n" % len(d.RACE_FIELDS_027))
for k in sorted(d.RACE_FIELDS_027):
    tw, g, h, m = d.RACE_FIELDS_027[k]
    L.append('    {"%s", {%s}, %d, %d, %d},\n' % (k, ", ".join(map(str, tw)), g, h, m))
L.append("}};\n\n")
L.append("// 4.2: culture -> regnal_permille (person_forms [\"{root}\"] in every culture).\n"
         "struct CultureFields {\n    std::string_view culture;\n    std::int64_t regnal_permille;\n"
         "    std::string_view tower_form0, library_form0, title0;\n};\n"
         "inline constexpr std::array<CultureFields, %d> CULTURES = {{\n" % len(d.CULTURE_FIELDS_027))
for k in sorted(d.CULTURE_FIELDS_027):
    tf, lf, bt, rp = d.CULTURE_FIELDS_027[k]
    L.append('    {"%s", %d, "%s", "%s", "%s"},\n' % (k, rp, tf[0], lf[0], bt[0]))
L.append("}};\n\n")
L.append("// 4.3: monster kind -> slayer_epithet.\nstruct Slayer {\n    std::string_view kind, epithet;\n};\n"
         "inline constexpr std::array<Slayer, %d> SLAYERS = {{\n" % len(d.SLAYER_EPITHET_027))
for k in sorted(d.SLAYER_EPITHET_027):
    L.append('    {"%s", "%s"},\n' % (k, d.SLAYER_EPITHET_027[k]))
L.append("}};\n\n")
L.append("// 25.3: history.monsters.undead_regions_ref; 25.1: the two history.peoples fields removed.\n"
         "inline constexpr std::int64_t UNDEAD_REGIONS_REF = %d;\n" % d.UNDEAD_REGIONS_REF_027)
L.append("inline constexpr std::array<std::string_view, %d> PEOPLES_REMOVED = {%s};\n\n"
         % (len(d.PEOPLES_REMOVED_027), ", ".join('"%s"' % x for x in d.PEOPLES_REMOVED_027)))
L.append("// The files of the 0.27.0 kinds race, culture, monster and history (p3_data_reference.SPEC_FILES_027), as the\n"
         "// spec states them: \"<directory>/<file name>\" -> text. Literals are cut in pieces of at most 4000 bytes (MSVC's\n"
         "// limit on one string literal).\n"
         "inline std::map<std::string, std::string> files_027() {\n    std::map<std::string, std::string> f;\n")
for kind in ("race", "culture", "monster", "history"):
    directory = d.KINDS[kind][0]
    for n in sorted(d.SPEC_FILES[kind]):
        t = d.SPEC_FILES[kind][n]
        pieces = [t[i:i + 4000] for i in range(0, len(t), 4000)] or [""]
        L.append('    f["%s/%s"] = std::string(' % (directory, n) + "\n".join('R"p506(' + p + ')p506"' for p in pieces) + ");\n")
L.append("    return f;\n}\n\n}  // namespace p506data\n")
open(out, "w", newline="\n").write("".join(L))
