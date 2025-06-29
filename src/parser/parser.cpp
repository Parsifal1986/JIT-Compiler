#include "parser.hpp"

std::unique_ptr<llvm::Module> loadModuleFromFile(const std::string &Filename, llvm::LLVMContext &Ctx) {
  llvm::SMDiagnostic Err;
  auto Mod = llvm::parseIRFile(Filename, Err, Ctx);
  if (!Mod) {
    std::string msg;
    llvm::raw_string_ostream os(msg);
    Err.print("loadModule", os);
    throw std::runtime_error(os.str());
  }
  return Mod;
}