# gdext

The C++ GDExtension: `world.db` readers bound into Godot, runtime generators, gameplay and the world viewer's logic (design doc section 10). Owned by the viewer engineer and started in phase P2. It will be a separate, optional CMake target: `worldgen` and `worldgen-cli` must keep building without it, and compiled libraries are produced under the build root, never copied into the repository.
