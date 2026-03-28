#pragma once
#include <utils/symbols/trait.h>
#include <utils/symbols/function.h>
#include <utils/symbols/variable.h>
#include <utils/symbols/struct.h>
#include <string>
#include <unordered_map>

namespace bloop {

struct Module {
    std::string name;
    std::unordered_map<std::string, Variable> Vars;
    std::unordered_map<std::string, FuncOverload> FuncOverloads;
    std::unordered_map<std::string, Struct> Structs;
    std::unordered_map<std::string, Trait> Traits;
    std::unordered_map<std::string, Module *> Submods;
    std::unordered_map<std::string, Module *> Imports;
};

}