#ifndef INTERPRETER_HPP
#define INTERPRETER_HPP

#include <stdexcept>
#include <map>
#include "../util/util.hpp"

class JITRunner {

private:
  struct BasicBlockExecutor {
    std::unordered_map<llvm::Value *, int64_t*> localval_map;
    void (*execFunc)();
    llvm::Instruction* terminator = nullptr;
    BasicBlockExecutor* next_segment = nullptr;
  };

public:
  JITRunner(llvm::Module &M);

  int64_t runModule();

private:
  int64_t execFunction(llvm::Function *F, const std::vector<int64_t> &Args);

  int64_t execBasicBlock(llvm::BasicBlock *BB);

  BasicBlockExecutor* constructBasicBlockExecutor(llvm::BasicBlock* BB, llvm::BasicBlock::iterator startline);

  int64_t runBasicBlockExecutor(BasicBlockExecutor &BBExec);

  int64_t visitInst(llvm::Instruction *I);

  int64_t getValue(llvm::Value *V);

  int64_t allocateMemory(llvm::Type *TempDIBasicType);

  void storeValue(llvm::Value *V, int64_t Value);

private:
  std::unordered_map<const llvm::Value *, int64_t> globalval_map;
  std::unordered_map<const llvm::Value *, int64_t>* localval_map;
  std::unordered_map<const llvm::BasicBlock *, unsigned long long> bb_map;
  std::unordered_map<const llvm::BasicBlock*, BasicBlockExecutor*> fn_map;

  const llvm::DataLayout &data_layout;

  llvm::Module &module;

  unsigned long long threshold = 1; // Threshold for basic block execution
};

#endif