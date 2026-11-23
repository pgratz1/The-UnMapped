// Entry point of the GDExtension: registers the classes with Godot.
// The symbol name is the entry_symbol of game/unmapped.gdextension.

#include <gdextension_interface.h>

#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/core/defs.hpp>
#include <godot_cpp/godot.hpp>

#include "viewer/layer_map_view.hpp"
#include "viewer/viewer_model.hpp"
#include "viewer/viewer_screen.hpp"
#include "viewer/year_marks.hpp"
#include "world_file.hpp"
#include "world_layer.hpp"

namespace {

void initialize_unmapped(godot::ModuleInitializationLevel level) {
    if (level != godot::MODULE_INITIALIZATION_LEVEL_SCENE) {
        return;
    }
    // Data access.
    GDREGISTER_CLASS(unmapped::WorldLayer);
    GDREGISTER_CLASS(unmapped::WorldFile);
    // World viewer: model, then the flat view.
    GDREGISTER_CLASS(unmapped::ViewerModel);
    GDREGISTER_CLASS(unmapped::LayerMapView);
    GDREGISTER_CLASS(unmapped::YearMarks);
    GDREGISTER_CLASS(unmapped::ViewerScreen);
}

void uninitialize_unmapped(godot::ModuleInitializationLevel /*level*/) {}

}  // namespace

extern "C" {

GDExtensionBool GDE_EXPORT unmapped_gdext_init(GDExtensionInterfaceGetProcAddress get_proc_address,
                                               GDExtensionClassLibraryPtr library,
                                               GDExtensionInitialization* initialization) {
    godot::GDExtensionBinding::InitObject init(get_proc_address, library, initialization);
    init.register_initializer(initialize_unmapped);
    init.register_terminator(uninitialize_unmapped);
    init.set_minimum_library_initialization_level(godot::MODULE_INITIALIZATION_LEVEL_SCENE);
    return init.init();
}

}  // extern "C"
