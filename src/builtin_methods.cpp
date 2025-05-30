#include "type_registry.hpp"

void register_builtin_methods() {
    TypeRegistry::instance().registerBuiltins();
}

// main():
//     register_builtin_methods();
