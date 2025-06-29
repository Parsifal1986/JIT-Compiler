#ifndef PARSER
#define PARSER

#include <memory>
#include <string>
#include <stdexcept>
#include "../util/util.hpp"

/// Load an LLVM IR (".ll") or bitcode (".bc") file into a Module.
/// Throws std::runtime_error on failure.
std::unique_ptr<llvm::Module> loadModuleFromFile(const std::string &Filename, llvm::LLVMContext &Ctx);

#endif // PARSER