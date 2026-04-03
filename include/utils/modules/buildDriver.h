#pragma once
#include <nlohmann/json.hpp>
#include <toml++/toml.h>
#include <unordered_set>
#include <utils/compilation.h>
#include <utils/bitcode/deserializer.h>
#include <utils/compiler.h>
#include <utils/modules/fileNode.h>
#include <iostream>
#include <fstream>
#include <sstream>

using namespace nlohmann;

namespace bloop {

struct Manifest {
    std::string PackageName;
    fs::path    MainFilePath;
    fs::path    Path;
};

class BuildDriver {
    fs::path _projectRoot;
    fs::path _vendorRoot;
    fs::path _stdRoot;
    fs::path _registryPath;
    fs::path _curFilePath;

    std::unordered_map<std::string, FileNode> _graph;
    std::vector<std::string> _buildOrder;

public:
    BuildDriver(fs::path p, fs::path v, fs::path s, fs::path r) 
        : _projectRoot(p), _vendorRoot(v), _stdRoot(s), _registryPath(r) {}

    ~BuildDriver() {
        for (auto &[_, node] : _graph) {
            delete node.Mod;
        }
    }

    static fs::path
    GetProjectRoot(fs::path curPath) {
        while (curPath != curPath.root_path() && !fs::exists(curPath / "bloop.toml")) {
            curPath = curPath.parent_path();
        }
        if (curPath == curPath.root_path()) {
            llvm::errs() << llvm::errs().RED << "Manifest file 'bloop.toml' does not exist\n" << llvm::errs().RESET;
            exit(1);
        }
        return curPath;
    }

    void
    BuildProj() {
        Manifest manif = ParseToml("main", _projectRoot / "bloop.toml");
        std::cout << "Building package: " << manif.PackageName << " (" << manif.MainFilePath << ")\n";
        _curFilePath = manif.MainFilePath;

        std::ifstream file(manif.MainFilePath);
        if (!file.is_open()) {
            llvm::errs() << llvm::errs().RED << "Error: Could not open the file " << manif.MainFilePath << '\n' << llvm::errs().RESET;
            exit(1);
        }
        std::stringstream ss;
        ss << file.rdbuf();
        Lexer lex;
        std::vector<std::string> deps = lex.PeekDependencies(ss.str());

        FileNode mainNode {
            .ImportName = manif.PackageName,
            .PhysicalPath = manif.MainFilePath,
            .ProjectRootPath = manif.Path.parent_path(),
            .Dependencies = deps
        };
        _graph[manif.PackageName] = mainNode;

        std::cout << "[1/4] Scanning dependencies...\n";
        for (const auto &d : deps) {
            scan(d);
        }

        std::cout << "[2/4] Resolving build graph...\n";
        sort(manif.PackageName); 

        std::cout << "[3/4] Executing compilation pipeline:\n";
        std::vector<std::string> objs;
        auto curArtefactDir = _projectRoot / "build" / "obj";

        std::unordered_set<std::string> recompiledModules;

        for (const auto &name : _buildOrder) {
            FileNode &node = _graph[name];
            node.Mod = new Module(name, Pub);
            
            auto artefactsDir = node.ProjectRootPath / "build" / "obj";
            auto relPath = node.PhysicalPath.lexically_relative(node.ProjectRootPath);
            auto objPath = (artefactsDir / relPath.parent_path() / (node.PhysicalPath.stem().string() + ".o")).lexically_normal();
            auto bitcodePath = objPath;
            bitcodePath.replace_extension(".blmod");

            std::cout << "  -> " << name << " (" << node.PhysicalPath.string() << ")\n";

            bool depsRecompiled = false;
            for (const auto &dep : node.Dependencies) {
                if (recompiledModules.count(dep)) {
                    depsRecompiled = true;
                    break;
                }
            }

            bool isFresh = !depsRecompiled &&
                           isArtefactFresh(node.PhysicalPath, bitcodePath) &&
                           isArtefactFresh(node.PhysicalPath, objPath);

            if (isFresh) {
                for (const auto &dep : node.Dependencies) {
                    FileNode &depNode = _graph[dep];
                    auto depArtefactsDir = depNode.ProjectRootPath / "build" / "obj";
                    auto depRelPath = depNode.PhysicalPath.lexically_relative(depNode.ProjectRootPath);
                    auto depBitcodePath = (depArtefactsDir / depRelPath.parent_path() / (depNode.PhysicalPath.stem().string() + ".blmod")).lexically_normal();
                    
                    if (fs::exists(depBitcodePath) && fs::last_write_time(objPath) < fs::last_write_time(depBitcodePath)) {
                        isFresh = false;
                        break;
                    }
                }
            }

            if (isFresh) {
                std::cout << "     [Cached] Loading module: " << name << '\n';
                Deserializer deserializer;
                deserializer.DeserializeInto(node.Mod, bitcodePath.string());
                objs.push_back(objPath.string());
            }
            else {
                std::cout << "     [Compiling] module: " << name << '\n';
                auto compileRes = Compile(_graph, _projectRoot, node.PhysicalPath, objPath, node.Mod);
                if (!compileRes.first) {
                    exit(1);
                }
                objs.push_back(compileRes.second);
            }
            std::cout << '\n';
        }

        std::cout << "[4/4] Linking executable...\n";
        std::string targetTripleStr = llvm::sys::getDefaultTargetTriple();
        llvm::Triple triple(targetTripleStr);
        auto exePath = curArtefactDir / GetOutputName(manif.PackageName, triple);
        if (LinkObjectFiles(exePath, objs)) {
            std::cout << "SUCCESS: " << exePath.string() << "\n";
        }
    }

    static Manifest
    ParseToml(const std::string &packageName, const fs::path &path) {
        if (!fs::exists(path)) {
            llvm::errs() << llvm::errs().RED << "Package " << packageName << " does not have manifest file (bloop.toml) at " << path << '\n' << llvm::errs().RESET;
            exit(1);
        }
        
        toml::table toml = toml::parse_file(path.string()).table();
        
        std::string name = toml["package"]["name"].value_or(packageName);
        std::string root = toml["package"]["root"].value_or("src/main.bl");
        
        return { name, path.parent_path() / root, path };
    }

private:
    Manifest
    resolveManifest(const std::string &packageName) {
        fs::path registryEntry = _registryPath / (packageName + ".json");
        if (!fs::exists(registryEntry)) {
            llvm::errs() << llvm::errs().RED << "Package not found in registry: " << packageName << '\n' << llvm::errs().RESET;
            exit(1);
        }

        std::ifstream file(registryEntry);
        json data = json::parse(file);
        
        fs::path tomlPath = data["manifest_path"].get<std::string>();
        return ParseToml(packageName, tomlPath);
    }

    std::pair<fs::path, fs::path> // path to main file and path to root of project
    resolvePath(const std::string &importName) {
        std::string pathStr = importName;
        std::replace(pathStr.begin(), pathStr.end(), '.', '/');
        fs::path relPath(pathStr);
        relPath.replace_extension(".bl");

        fs::path localPath = _curFilePath.parent_path() / relPath;
        if (fs::exists(localPath)) {
            return { localPath, _projectRoot };
        }
        else {
            localPath.replace_extension("");
            localPath = localPath / "main.bl";
            if (fs::exists(localPath)) {
                return { localPath, _projectRoot };
            }
        }

        return resolveImportToPath(importName);
    }

    std::pair<fs::path, fs::path> // path to main file and path to root of project
    resolveImportToPath(const std::string &fullImport) {
        size_t dotPos = fullImport.find('.');
        std::string packageName = (dotPos == std::string::npos) ? fullImport : fullImport.substr(0, dotPos);
        Manifest m = resolveManifest(packageName);
        
        if (dotPos == std::string::npos) {
            return { m.MainFilePath, m.Path.parent_path() };
        }

        std::string subPath = fullImport.substr(dotPos + 1);
        std::replace(subPath.begin(), subPath.end(), '.', '/');
        fs::path targetPath = m.Path.parent_path() / (subPath + ".bl");
        
        if (!fs::exists(targetPath)) {
            llvm::errs() << llvm::errs().RED << "Module not found: " << targetPath << '\n' << llvm::errs().RESET;
            exit(1);
        }

        return { targetPath, m.Path.parent_path() };
    }

    bool
    isArtefactFresh(const fs::path &source, const fs::path &artefact) {
        if (!fs::exists(artefact)) {
            return false;
        }
        return fs::last_write_time(artefact) > fs::last_write_time(source);
    }

    void
    scan(const std::string &modName) {
        if (_graph.count(modName)) {
            return;
        }

        auto fullPath = resolvePath(modName);
        
        std::ifstream file(fullPath.first);
        if (!file.is_open()) {
            llvm::errs() << llvm::errs().RED << "Failed to open module file: " << fullPath.first << '\n' << llvm::errs().RESET;
            exit(1);
        }
        std::stringstream ss;
        ss << file.rdbuf();
        Lexer lex;
        
        FileNode node { 
            .ImportName = modName, 
            .PhysicalPath = fullPath.first, 
            .ProjectRootPath = fullPath.second, 
            .Dependencies = lex.PeekDependencies(ss.str()) 
        };
        
        _graph[modName] = node;
        fs::path curFilePath = _curFilePath;
        _curFilePath = node.PhysicalPath;
        for (const auto &dep : node.Dependencies) {
            scan(dep);
        }
        _curFilePath = curFilePath;
    }

    void
    sort(const std::string &modName) {
        auto &node = _graph[modName];

        if (node.State == VisitState::Visiting) {
            llvm::errs() << llvm::errs().RED << "Found circular import between `" << modName << "` and `" << node.ImportName << "`\n" << llvm::errs().RESET;
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