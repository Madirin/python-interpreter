#include "type_registry.hpp"
#include "object.hpp"
#include <typeindex>
#include <vector>
#include <memory>

void register_builtin_methods() {

    TypeRegistry &registry = TypeRegistry::instance();

    // PyInt
    std::type_index intType = typeid(PyInt);
    registry.registerType(intType);

    registry.registerMethod(
        intType,
        "__add__",
        [](ObjectPtr self, const std::vector<ObjectPtr> &args) -> ObjectPtr {
            return self->__add__(args.at(0));
        }
    );

    registry.registerMethod(
        intType,
        "__sub__",
        [](ObjectPtr self, const std::vector<ObjectPtr> &args) -> ObjectPtr {
            return self->__sub__(args.at(0));
        }
    );

  
    // PyBool
    std::type_index boolType = typeid(PyBool);
    registry.registerType(boolType);

    registry.registerMethod(
        boolType,
        "__add__",
        [](ObjectPtr self, const std::vector<ObjectPtr> &args) -> ObjectPtr {
            return self->__add__(args.at(0));
        }
    );

    registry.registerMethod(
        boolType,
        "__sub__",
        [](ObjectPtr self, const std::vector<ObjectPtr> &args) -> ObjectPtr {
            return self->__sub__(args.at(0));
        }
    );

    
    // PyFloat
    std::type_index floatType = typeid(PyFloat);
    registry.registerType(floatType);

    registry.registerMethod(
        floatType,
        "__add__",
        [](ObjectPtr self, const std::vector<ObjectPtr> &args) -> ObjectPtr {
            return self->__add__(args.at(0));
        }
    );

    registry.registerMethod(
        floatType,
        "__sub__",
        [](ObjectPtr self, const std::vector<ObjectPtr> &args) -> ObjectPtr {
            return self->__sub__(args.at(0));
        }
    );


    // PyString
    std::type_index stringType = typeid(PyString);
    registry.registerType(stringType);


    registry.registerMethod(
        stringType,
        "__add__",
        [](ObjectPtr self, const std::vector<ObjectPtr> &args) -> ObjectPtr {
            return self->__add__(args.at(0));
        }
    );

    
    registry.registerMethod(
        stringType,
        "__getitem__",
        [](ObjectPtr self, const std::vector<ObjectPtr> &args) -> ObjectPtr {
            return self->__getitem__(args.at(0));
        }
    );

    
    registry.registerMethod(
        stringType,
        "__contains__",
        [](ObjectPtr self, const std::vector<ObjectPtr> &args) -> ObjectPtr {
            bool result = self->__contains__(args.at(0));
            return std::make_shared<PyBool>(result);
        }
    );

    
    // PyList
    std::type_index listType = typeid(PyList);
    registry.registerType(listType);

    
    registry.registerMethod(
        listType,
        "__add__",
        [](ObjectPtr self, const std::vector<ObjectPtr> &args) -> ObjectPtr {
            return self->__add__(args.at(0));
        }
    );

    
    registry.registerMethod(
        listType,
        "__getitem__",
        [](ObjectPtr self, const std::vector<ObjectPtr> &args) -> ObjectPtr {
            return self->__getitem__(args.at(0));
        }
    );

    
    registry.registerMethod(
        listType,
        "__contains__",
        [](ObjectPtr self, const std::vector<ObjectPtr> &args) -> ObjectPtr {
            bool result = self->__contains__(args.at(0));
            return std::make_shared<PyBool>(result);
        }
    );


    // PyDict
    std::type_index dictType = typeid(PyDict);
    registry.registerType(dictType);

    
    registry.registerMethod(
        dictType,
        "__getitem__",
        [](ObjectPtr self, const std::vector<ObjectPtr> &args) -> ObjectPtr {
            return self->__getitem__(args.at(0));
        }
    );

    
    registry.registerMethod(
        dictType,
        "__contains__",
        [](ObjectPtr self, const std::vector<ObjectPtr> &args) -> ObjectPtr {
            bool result = self->__contains__(args.at(0));
            return std::make_shared<PyBool>(result);
        }
    );

    // PySet
    std::type_index setType = typeid(PySet);
    registry.registerType(setType);

    
    registry.registerMethod(
        setType,
        "__add__",
        [](ObjectPtr self, const std::vector<ObjectPtr> &args) -> ObjectPtr {
            return self->__add__(args.at(0));
        }
    );

    
    registry.registerMethod(
        setType,
        "__contains__",
        [](ObjectPtr self, const std::vector<ObjectPtr> &args) -> ObjectPtr {
            bool result = self->__contains__(args.at(0));
            return std::make_shared<PyBool>(result);
        }
    );
}


//     register_builtin_methods();

