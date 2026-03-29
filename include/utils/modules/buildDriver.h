#pragma once
#include <nlohmann/json.hpp>
#include <toml++/toml.h>
#include <utils/compiler.h>
#include <iostream>
#include <fstream>
#include <regex>

namespace fs = std::filesystem;
using namespace nlohmann;
using namespace toml;

namespace bloop {

enum VisitState : uint8_t {
    Unvisited,
    Visiting,
    Visited
};

struct FileNode {
    std::string ImportName;
    fs::path    PhysicalPath;
    VisitState  State = Unvisited;
    
    std::vector<std::string> Dependencies;
    
    Module *Mod = nullptr; 
};

struct Manifest {
    std::string PackageName;
    fs::path    MainFilePath;
};

class BuildDriver {
    fs::path _projectRoot;
    fs::path _vendorRoot;
    fs::path _stdRoot;
    fs::path _registryPath;

    std::unordered_map<std::string, FileNode> _graph;
    std::vector<std::string> _buildOrder;

public:
    BuildDriver(fs::path p, fs::path v, fs::path s, fs::path r) : _projectRoot(p), _vendorRoot(v), _stdRoot(s), _registryPath(r) {}

    ~BuildDriver() {
        for (auto &[_, node] : _graph) {
            delete node.Mod;
        }
    }

    void
    Execute(const std::string &entryPoint) {
        std::cout << "[1/3] Scanning dependencies...\n";
        scan(entryPoint);

        std::cout << "[2/3] Resolving build graph...\n";
        sort(entryPoint);

        std::cout << "[3/3] Executing compilation pipeline:\n";
        for (const auto &name : _buildOrder) {
            FileNode &node = _graph[name];
            node.Mod = new Module(name, AccessModifier::Pub);
            
            std::cout << "  -> " << name << " (" << node.PhysicalPath.string() << ")\n";

            if (isBitcodeFresh(name)) {
                std::cout << "Loading cached module: " << name << '\n';
                //loadBitcodeInto(node.Mod, name + ".blmod");
            }
            else {
                std::cout << "Compiling module: " << name << '\n';
                if (!Compile(node.PhysicalPath, node.Mod)) {
                    this->~BuildDriver();
                    exit(1);
                }
            }
            
        }
    }

private:
    Manifest
    resolveManifest(const std::string &packageName) {
        fs::path registryEntry = _registryPath / (packageName + ".json");
        if (!fs::exists(registryEntry)) {
            llvm::errs() << llvm::errs().RED << "Package not found in registry: " << packageName << '\n' << llvm::errs().RESET;
            this->~BuildDriver();
            exit(1);
        }

        std::ifstream file(registryEntry);
        json data = json::parse(file);
        
        fs::path tomlPath = data["manifest_path"].get<std::string>();
        
        std::ifstream tomlFile(tomlPath);
        if (!tomlFile.is_open()) {
            llvm::errs() << llvm::errs().RED << "Package " << packageName << " does not have manifest file (bloop.toml)\n" << llvm::errs().RESET;
            this->~BuildDriver();
            exit(1);
        }
        table toml = parse_file(tomlPath.string()).table();
        auto package = toml["package"].as_table();
        #define AS_STR(v, k) (*v->get_as<std::string>(k))->c_str()
        return { AS_STR(package, "name"), tomlPath.parent_path() / AS_STR(package, "root") };
        #undef AS_STR
    }

    std::vector<std::string>
    collectImports(const fs::path &filepath) {
        std::vector<std::string> imports;
        
        std::ifstream file(filepath, std::ios::in | std::ios::binary);
        if (!file) {
            return imports;
        }
        
        std::string content((std::istreambuf_iterator<char>(file)),
                             std::istreambuf_iterator<char>());
        
        std::regex importRegex(R"(using\s+([a-zA-Z0-9_]+(?:\s*\.\s*[a-zA-Z0-9_]+)*))");

        auto wordsBegin = std::sregex_iterator(content.begin(), content.end(), importRegex);
        auto wordsEnd = std::sregex_iterator();

        for (std::sregex_iterator i = wordsBegin; i != wordsEnd; ++i) {
            std::string rawPath = (*i)[1].str();
            
            rawPath.erase(std::remove_if(rawPath.begin(), rawPath.end(),
                                                 [](unsigned char c) { return isspace(c); }),
                           rawPath.end());
            
            imports.push_back(rawPath);
        }
        
        return imports;
    }

    fs::path
    resolvePath(const std::string &importName) {
        std::string relPath = importName;
        std::replace(relPath.begin(), relPath.end(), '.', '/');
        relPath += ".bl";

        fs::path localPath = _projectRoot / relPath;
        if (fs::exists(localPath)) {
            return localPath;
        }

        return resolveImportToPath(importName);
    }

    fs::path
    resolveImportToPath(const std::string &fullImport) {
        size_t dotPos = fullImport.find('.');
        std::string packageName = (dotPos == std::string::npos) ? fullImport : fullImport.substr(0, dotPos);
        Manifest m = resolveManifest(packageName);
        return m.MainFilePath;
    }

    bool
    isBitcodeFresh(const std::string &modName) {
        fs::path source = resolvePath(modName);
        fs::path bitcode = source.parent_path().parent_path() / "build" / "obj" / (modName + ".blmod");
        if (!fs::exists(bitcode)) {
            return false;
        }
        return fs::last_write_time(bitcode) > fs::last_write_time(source);
    }

    void
    scan(const std::string &modName) {
        if (_graph.count(modName)) {
            return;
        }

        fs::path fullPath = resolvePath(modName);
        FileNode node { .ImportName = modName, .PhysicalPath = fullPath, .Dependencies = collectImports(fullPath) };
        _graph[modName] = node;
        for (const auto& dep : node.Dependencies) {
            scan(dep);
        }
    }

    void
    sort(const std::string &modName) {
        auto &node = _graph[modName];

        if (node.State == VisitState::Visiting) {
            llvm::errs() << llvm::errs().RED << "Found circular import between `" << modName << "` and `" << node.ImportName << "`\n" << llvm::errs().RESET;
            this->~BuildDriver();
            exit(1);
        }
        else if (node.State == VisitState::Visited) {
            return;
        }

        node.State = VisitState::Visiting;
        for (const auto &dep : node.Dependencies) {
            sort(dep);
        }
        node.State = VisitState::Visited;
        _buildOrder.push_back(modName);
    }
};

}