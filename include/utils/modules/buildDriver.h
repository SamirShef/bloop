#pragma once
#include <nlohmann/json.hpp>
#include <toml++/toml.h>
#include <unordered_set>
#include <utils/compilation.h>
#include <utils/bitcode/deserializer.h>
#include <utils/compiler.h>
#include <utils/modules/fileNode.h>
#include <utils/splitString.h>
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

    using Clock = std::chrono::high_resolution_clock;
    using TimePoint = std::chrono::time_point<Clock>;

    struct ResolveResult {
        std::string BaseModuleName;
        fs::path MainFilePath;
        fs::path ProjectRoot;
    };

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
        auto startTotal = Clock::now();
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
        std::vector<std::string> rawDeps = lex.PeekDependencies(ss.str());

        FileNode mainNode {
            .ImportName = manif.PackageName,
            .PhysicalPath = manif.MainFilePath,
            .ProjectRootPath = manif.Path.parent_path(),
            .Dependencies = {}
        };
        _graph[manif.PackageName] = mainNode;

        auto startScan = Clock::now();
        std::cout << "[1/4] Scanning dependencies...\n";
        for (const auto &d : rawDeps) {
            ResolveResult res = resolvePath(d);
            _graph[manif.PackageName].Dependencies.push_back(res.BaseModuleName);
            scan(d);
        }

        auto startSort = Clock::now();
        std::cout << "[2/4] Resolving build graph...\n";
        sort(manif.PackageName); 

        auto startComp = Clock::now();
        std::cout << "[3/4] Executing compilation pipeline:\n";
        std::vector<std::string> objs;
        auto curArtefactDir = _projectRoot / "build" / "obj";

        std::unordered_set<std::string> recompiledModules;

        for (const auto &name : _buildOrder) {
            auto startMod = Clock::now();
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
                std::cout << "     [Compiling] Module: " << name << '\n';
                auto compileRes = Compile(_graph, _projectRoot, node.PhysicalPath, objPath, node.Mod);
                if (!compileRes.first) {
                    exit(1);
                }
                objs.push_back(compileRes.second);
            }
            auto endMod = Clock::now();
            std::cout << "     [Info] Time left: ";
            printDuration(startMod, endMod);
            std::cout << '\n';
        }

        auto startLink = Clock::now();
        std::cout << "[4/4] Linking executable...\n";
        std::string targetTripleStr = llvm::sys::getDefaultTargetTriple();
        llvm::Triple triple(targetTripleStr);
        auto exePath = curArtefactDir / GetOutputName(manif.PackageName, triple);
        if (LinkObjectFiles(exePath, objs)) {
            auto endTotal = Clock::now();
            llvm::outs() << llvm::outs().GREEN << "SUCCESS: " << llvm::outs().RESET << exePath.string() << "\n";
            std::cout << std::string(std::string("SUCCESS: ").length() + exePath.string().length(), '-') << '\n';
            std::cout << "Total build time: ";
            printDuration(startTotal, endTotal, true);
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
    void
    printDuration(TimePoint start, TimePoint end, bool highlight = false) {
        auto diff = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
        if (highlight) {
            std::cout << "\033[1;32m";
        }
        
        if (diff < 1000) {
            std::cout << diff << "ms";
        }
        else {
            std::cout << std::fixed << std::setprecision(2) << diff / 1000.0 << "s";
        }
        
        if (highlight) {
            std::cout << "\033[0m";
        }
        std::cout << '\n';
    }

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

    ResolveResult
    resolvePath(const std::string &importName) {
        std::vector<std::string> segments = splitString(importName, '.');

        for (int i = segments.size(); i > 0; --i) {
            std::string currentModName = segments[0];
            for (int j = 1; j < i; ++j) {
                currentModName += "." + segments[j];
            }

            std::string relPathStr = segments[0];
            for (int j = 1; j < i; ++j) {
                relPathStr += "/" + segments[j];
            }
            fs::path relPath(relPathStr);

            fs::path localPathFile = _curFilePath.parent_path() / (relPathStr + ".bl");
            fs::path localPathMain = _curFilePath.parent_path() / relPath / "main.bl";

            if (fs::exists(localPathFile)) {
                return { currentModName, localPathFile, _projectRoot };
            }
            if (fs::exists(localPathMain)) {
                return { currentModName, localPathMain, _projectRoot };
            }

            std::string packageName = segments[0];
            fs::path registryEntry = _registryPath / (packageName + ".json");
            
            if (fs::exists(registryEntry)) {
                std::ifstream file(registryEntry);
                json data = json::parse(file);
                fs::path tomlPath = data["manifest_path"].get<std::string>();
                Manifest m = ParseToml(packageName, tomlPath);

                if (i == 1) {
                    return { currentModName, m.MainFilePath, m.Path.parent_path() };
                }
                else {
                    std::string subPathStr = segments[1];
                    for (int j = 2; j < i; ++j) {
                        subPathStr += "/" + segments[j];
                    }
                    fs::path targetPathFile = m.MainFilePath.parent_path() / (subPathStr + ".bl");
                    fs::path targetPathMain = m.MainFilePath.parent_path() / subPathStr / "main.bl";

                    if (fs::exists(targetPathFile)) {
                        return { currentModName, targetPathFile, m.Path.parent_path() };
                    }
                    if (fs::exists(targetPathMain)) {
                        return { currentModName, targetPathMain, m.Path.parent_path() };
                    }
                }
            }
        }

        llvm::errs() << llvm::errs().RED << "Module or file not found for import: " << importName << '\n' << llvm::errs().RESET;
        exit(1);
    }

    bool
    isArtefactFresh(const fs::path &source, const fs::path &artefact) {
        if (!fs::exists(artefact)) {
            return false;
        }
        return fs::last_write_time(artefact) > fs::last_write_time(source);
    }

    void
    scan(const std::string &rawDep) {
        ResolveResult res = resolvePath(rawDep);

        if (_graph.count(res.BaseModuleName)) {
            return;
        }

        std::ifstream file(res.MainFilePath);
        if (!file.is_open()) {
            llvm::errs() << llvm::errs().RED << "Failed to open module file: " << res.MainFilePath << '\n' << llvm::errs().RESET;
            exit(1);
        }
        std::stringstream ss;
        ss << file.rdbuf();
        Lexer lex;
        
        std::vector<std::string> childRawDeps = lex.PeekDependencies(ss.str());
        
        FileNode node { 
            .ImportName = res.BaseModuleName, 
            .PhysicalPath = res.MainFilePath, 
            .ProjectRootPath = res.ProjectRoot, 
            .Dependencies = {}
        };
        
        _graph[res.BaseModuleName] = node;
        
        fs::path prevFilePath = _curFilePath;
        _curFilePath = res.MainFilePath;
        
        for (const auto &dep : childRawDeps) {
            ResolveResult childRes = resolvePath(dep);
            _graph[res.BaseModuleName].Dependencies.push_back(childRes.BaseModuleName);
            scan(dep);
        }
        
        _curFilePath = prevFilePath;
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