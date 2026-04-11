#pragma once
#include <filesystem>
#include <utils/modules/module.h>
#include <string>

namespace bloop {

namespace fs = std::filesystem;

enum VisitState : uint8_t {
    Unvisited,
    Visiting,
    Visited
};

struct FileNode {
    std::string                 ImportName;
    fs::path                    PhysicalPath;
    fs::path                    ProjectRootPath;
    bloop::VisitState           State = Unvisited;
    
    std::vector<std::string>    Dependencies;
    
    Module                     *Mod = nullptr; 
};

}