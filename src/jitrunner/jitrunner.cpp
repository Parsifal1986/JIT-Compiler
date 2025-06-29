
#include "jitrunner.hpp"
#include "../asm/asmcmd.hpp"
#include "../asm/asmstruct.hpp"
#include "../asm/asmdata.hpp"

// -------- Helpers ---------
static inline int64_t asInt(const llvm::APInt &A) { return A.getSExtValue(); }

JITRunner::JITRunner(llvm::Module &M)
  : module(M), data_layout(M.getDataLayout()) {
}

int64_t JITRunner::runModule() {
  llvm::Function *Main = module.getFunction("main");
  if (!Main) {
    throw std::runtime_error("No function called 'main'.");
  }
  if (!Main->arg_empty()) {
    throw std::runtime_error("main() with arguments not supported.");
  }
  return execFunction(Main, {});
}

int64_t JITRunner::execFunction(llvm::Function *F, const std::vector<int64_t> &Args) {
  std::unordered_map<const llvm::Value *, int64_t>* old_localval_map = localval_map;
  localval_map = new std::unordered_map<const llvm::Value *, int64_t>();
  // Map arguments (none for now).
  for (size_t i = 0; i < Args.size(); ++i) {
    if (i >= F->arg_size()) {
      throw std::runtime_error("Too many arguments passed to function.");
    }
    llvm::Argument *Arg = &*std::next(F->arg_begin(), i);
    localval_map->emplace(Arg, Args[i]);
  }
  // Start from the entry block
  llvm::BasicBlock &Entry = F->getEntryBlock();
  int64_t ret = execBasicBlock(&Entry);
  localval_map = old_localval_map;
  return ret;
}

JITRunner::BasicBlockExecutor* JITRunner::constructBasicBlockExecutor(llvm::BasicBlock* BB, llvm::BasicBlock::iterator startline) {
  BasicBlockExecutor* BBExec = new BasicBlockExecutor();
  asmcode::AsmBlock AB(BBExec->localval_map);
  AB.regSave();
  bool flag = 0;
  for (auto it = startline; it != BB->end() && !flag; ++it) {
    llvm::Instruction& I = *it;
    switch (I.getOpcode()) {
    case llvm::Instruction::Add:
    case llvm::Instruction::Sub:
    case llvm::Instruction::Mul:
    case llvm::Instruction::SDiv:
    case llvm::Instruction::SRem:
    case llvm::Instruction::ICmp: {
      AB.addBinary(&I);
      break;
    }
    case llvm::Instruction::PHI: {
      AB.addPhi(&I);
      break;
    }
    case llvm::Instruction::Load: {
      AB.addLoad(&I);
      break;
    }
    case llvm::Instruction::Store: {
      AB.addStore(&I);
      break;
    }
    case llvm::Instruction::GetElementPtr: {
      AB.addGetElementPtr(&I, data_layout);
      break;
    }
    case llvm::Instruction::Ret:
    case llvm::Instruction::Br: {
      BBExec->terminator = &I;
      break;
    }
    case llvm::Instruction::Call: {
      BBExec->terminator = &I;
      BBExec->next_segment = constructBasicBlockExecutor(BB, std::next(it));
      flag = 1;
      break;
    }
    case llvm::Instruction::Alloca: {
      BBExec->localval_map[&I] = new int64_t(allocateMemory(I.getType()));
      break;
    }

    default:
      // Ignore other instructions in this threshold mode
      break;
    }
  }
  AB.regLoad();
  AB.addRet();
  // printf("AsmBlock for BB:\n%s", AB.toString().c_str());
  unsigned char* encode;
  size_t encode_size, count;

  AB.encode(&encode, &encode_size, &count);

  // for (int i = 0; i < encode_size; i += 4) {
  //   printf("0x%02x%02x%02x%02x\n", encode[i + 3], encode[i + 2], encode[i + 1], encode[i]);
  // }
  // printf("\n");

  // std::cout << "Encoded " << count << " instructions, size = " << encode_size << std::endl;

  void* exec = mmap(nullptr, encode_size, PROT_READ | PROT_WRITE | PROT_EXEC, MAP_ANON | MAP_PRIVATE, -1, 0);

  if (exec == MAP_FAILED) {
    throw std::runtime_error("Failed to allocate executable memory: " + std::string(strerror(errno)));
  }

  std::memcpy(exec, encode, encode_size);

  BBExec->execFunc = reinterpret_cast<void(*)()>(exec);

  return BBExec;
}

int64_t JITRunner::runBasicBlockExecutor(BasicBlockExecutor& BBExec) {
  for (auto it : BBExec.localval_map) {
    try {
      *it.second = getValue(it.first);
    } catch(...) {

    }
  }

  BBExec.execFunc();

  printf("Finish executing BasicBlockExecutor.\n");
  
  for (auto& it : BBExec.localval_map) {
    try {
      storeValue(it.first, *(it.second));
    } catch(...) {

    }
  }

  if (llvm::isa<llvm::ReturnInst>(BBExec.terminator)) {
    llvm::ReturnInst& RI = llvm::cast<llvm::ReturnInst>(*BBExec.terminator);
    if (RI.getNumOperands() == 0) {
      return 0;
    }
    return getValue(RI.getOperand(0));
  } else if (llvm::isa<llvm::BranchInst>(BBExec.terminator)) {
    llvm::BranchInst& BI = llvm::cast<llvm::BranchInst>(*BBExec.terminator);
    if (BI.isUnconditional()) {
      return execBasicBlock(BI.getSuccessor(0));
    } else {
      int64_t cond = getValue(BI.getCondition());
      return execBasicBlock(cond ? BI.getSuccessor(0) : BI.getSuccessor(1));
    }
  } else if (llvm::isa<llvm::CallInst>(BBExec.terminator)) {
    llvm::CallInst& CI = llvm::cast<llvm::CallInst>(*BBExec.terminator);
    llvm::Function* Callee = CI.getCalledFunction();
    if (!Callee || Callee->isDeclaration()) {
      throw std::runtime_error("External function call not allowed.");
    }
    std::vector<int64_t> argVals;
    for (llvm::Use& U : CI.args()) {
      llvm::Value* V = U.get();
      argVals.push_back(getValue(V));
    }
    int64_t ret = execFunction(Callee, argVals);
    storeValue(&CI, ret);
    return runBasicBlockExecutor(*BBExec.next_segment);
  }

  throw std::runtime_error("BasicBlockExecutor did not end with a return or branch instruction.");
}

int64_t JITRunner::execBasicBlock(llvm::BasicBlock *BB) {
  if (bb_map.find(BB) != bb_map.end()) {
    bb_map[BB]++;
  } else {
    bb_map[BB] = 1;
  }
  
  if (bb_map[BB] > threshold) {
    if (fn_map.find(BB) != fn_map.end()) {
      BasicBlockExecutor &BBExec = *fn_map[BB];
      return runBasicBlockExecutor(BBExec);
    } else {
      BasicBlockExecutor* BBExec = constructBasicBlockExecutor(BB, BB->begin());
      fn_map[BB] = BBExec;
      return runBasicBlockExecutor(*BBExec);
    }
  } else {
    std::unordered_map<llvm::Value *, int64_t> PhiBuffer;
    for (llvm::Instruction& I : *BB) {
      if (llvm::isa<llvm::PHINode>(I)) {
        llvm::PHINode& PN = llvm::cast<llvm::PHINode>(I);
        // Naïve: choose incoming based on first predecessor (works because we
        // evaluate edge‑by‑edge).
        llvm::BasicBlock *Pred = PN.getIncomingBlock(0);
        llvm::Value *Incoming = PN.getIncomingValueForBlock(Pred);
        PhiBuffer[&I] = getValue(Incoming);
      } else {
        if (!PhiBuffer.empty()) {
          for (auto &pair : PhiBuffer) {
            storeValue(pair.first, pair.second);
            PhiBuffer.erase(pair.first);
          }
        }
        if (llvm::isa<llvm::ReturnInst>(I)) {
          llvm::ReturnInst &RI = llvm::cast<llvm::ReturnInst>(I);
          if (RI.getNumOperands() == 0)
            return 0;
          return getValue(RI.getOperand(0));
        } else if (llvm::isa<llvm::BranchInst>(I)) {
          llvm::BranchInst &BI = llvm::cast<llvm::BranchInst>(I);
          if (BI.isUnconditional()) {
            return execBasicBlock(BI.getSuccessor(0));
          } else {
            int64_t cond = getValue(BI.getCondition());
            return execBasicBlock(cond ? BI.getSuccessor(0) : BI.getSuccessor(1));
          }
        } else {
          // Compute & memoize result of non‑terminator instruction
          storeValue(&I, visitInst(&I));
        }
      }
    }
  }
  throw std::runtime_error("Fell off end of basic block - malformed IR.");
}

int64_t JITRunner::visitInst(llvm::Instruction* I) {
  switch (I->getOpcode()) {
    case llvm::Instruction::Add:
    case llvm::Instruction::Sub:
    case llvm::Instruction::Mul:
    case llvm::Instruction::SDiv:
    case llvm::Instruction::SRem: {
      auto *B = llvm::cast<llvm::BinaryOperator>(I);
      int64_t lhs = getValue(B->getOperand(0));
      int64_t rhs = getValue(B->getOperand(1));
      switch (I->getOpcode()) {
      case llvm::Instruction::Add:
        return lhs + rhs;
      case llvm::Instruction::Sub:
        return lhs - rhs;
      case llvm::Instruction::Mul:
        return lhs * rhs;
      case llvm::Instruction::SDiv:
        if (rhs == 0)
          throw std::runtime_error("divide by zero");
        return lhs / rhs;
      case llvm::Instruction::SRem:
        if (rhs == 0)
          throw std::runtime_error("mod by zero");
        return lhs % rhs;
      default:
        break;
      }
    }
    case llvm::Instruction::ICmp: {
      auto *C = llvm::cast<llvm::ICmpInst>(I);
      int64_t lhs = getValue(C->getOperand(0));
      int64_t rhs = getValue(C->getOperand(1));
      switch (C->getPredicate()) {
      case llvm::CmpInst::ICMP_EQ:
        return lhs == rhs;
      case llvm::CmpInst::ICMP_NE:
        return lhs != rhs;
      case llvm::CmpInst::ICMP_SGT:
        return lhs > rhs;
      case llvm::CmpInst::ICMP_SGE:
        return lhs >= rhs;
      case llvm::CmpInst::ICMP_SLT:
        return lhs < rhs;
      case llvm::CmpInst::ICMP_SLE:
        return lhs <= rhs;
      default:
        throw std::runtime_error("Unsupported ICmp predicate");
      }
    }
    case llvm::Instruction::Call: {
      auto* CI = llvm::cast<llvm::CallInst>(I);
      llvm::Function* Callee = CI->getCalledFunction();
      if (!Callee || Callee->isDeclaration())
        throw std::runtime_error("External function call not allowed.");

      std::vector<int64_t> argVals;
      for (llvm::Use& U : CI->args()) {
        llvm::Value* V = U.get();
        argVals.push_back(getValue(V));
      }
      // Recursive call
      int64_t ret = execFunction(Callee, argVals);
      return ret;
    }
    case llvm::Instruction::PHI: {
      auto* PN = llvm::cast<llvm::PHINode>(I);
      // Naïve: choose incoming based on first predecessor (works because we
      // evaluate edge‑by‑edge).
      llvm::BasicBlock* Pred = PN->getIncomingBlock(0);
      llvm::Value* Incoming = PN->getIncomingValueForBlock(Pred);
      return getValue(Incoming);
    }
    case llvm::Instruction::Alloca: {
      auto* A = llvm::cast<llvm::AllocaInst>(I);
      llvm::Type* T = A->getAllocatedType();
      uint64_t Align = A->getAlign().value();
      int64_t ptr = allocateMemory(T);
      return ptr;
    }
    case llvm::Instruction::Load: {
      auto* LI = llvm::cast<llvm::LoadInst>(I);
      int64_t ptr = getValue(LI->getPointerOperand());
      if (ptr == 0)
        throw std::runtime_error("Dereferencing null pointer.");
      // Assume pointer points to an int64_t
      return *(reinterpret_cast<int64_t *>(ptr));
    }
    case llvm::Instruction::Store: {
      auto* SI = llvm::cast<llvm::StoreInst>(I);
      int64_t ptr = getValue(SI->getPointerOperand());
      if (ptr == 0)
        throw std::runtime_error("Dereferencing null pointer.");
      int64_t value = getValue(SI->getValueOperand());
      *(reinterpret_cast<int64_t *>(ptr)) = value;
      return 0; // Store does not return a value.
    }
    case llvm::Instruction::GetElementPtr: {
      auto* GEP = llvm::cast<llvm::GetElementPtrInst>(I);
      int64_t basePtr = getValue(GEP->getPointerOperand());
      if (basePtr == 0)
        throw std::runtime_error("Dereferencing null pointer in GEP");
      llvm::Type* curTy = GEP->getSourceElementType();
      int64_t offset = 0;
      auto idxIt = GEP->idx_begin();
      int64_t idxVal = getValue(*idxIt);
      if (idxVal != 0) {
        offset += idxVal * static_cast<int64_t>(data_layout.getTypeAllocSize(curTy));
      }
      for (++idxIt; idxIt != GEP->idx_end(); ++idxIt) {
        int64_t idxVal = getValue(*idxIt);
        if (curTy->isStructTy()) {
          auto* STy = llvm::cast<llvm::StructType>(curTy);
          const auto* SL = data_layout.getStructLayout(STy);

          if (!llvm::isa<llvm::Constant>(*idxIt))
            throw std::runtime_error("Non-constant struct index in GEP");

          unsigned fieldNo = static_cast<unsigned>(idxVal);
          if (fieldNo >= STy->getNumElements())
            throw std::runtime_error("Struct field index out of range in GEP");

          offset += static_cast<int64_t>(SL->getElementOffset(fieldNo));
          curTy = STy->getElementType(fieldNo);
        } else if (curTy->isArrayTy()) {
          const auto* ATy = llvm::cast<llvm::ArrayType>(curTy);
          llvm::Type* EltT = ATy->getElementType();
          uint64_t eltSize = data_layout.getTypeAllocSize(EltT);
          offset += idxVal * static_cast<int64_t>(eltSize);
          curTy = EltT;
        } else {
          throw std::runtime_error("Unsupported type in GEP traversal");
        }
      }

      return basePtr + offset;
    }
    case llvm::Instruction::SExt: {
      auto* SExt = llvm::cast<llvm::SExtInst>(I);
      int64_t value = getValue(SExt->getOperand(0));
      // Sign-extend the value to 64 bits
      return (int64_t)(int64_t(value));
    }
    default:
      throw std::runtime_error("Unsupported instruction: " + std::string(I->getOpcodeName()));
  }
}

int64_t JITRunner::allocateMemory(llvm::Type *T) {
  if (T->isIntegerTy()) {
    return (int64_t)(new int64_t);
  } else if (T->isArrayTy()) {
    auto* AT = llvm::cast<llvm::ArrayType>(T);
    llvm::Type *ElemType = AT->getElementType();
    if (ElemType->isArrayTy()) {
      int64_t ret = (int64_t)(new int64_t[AT->getNumElements()]);
      for (uint64_t i = 0; i < AT->getNumElements(); ++i) {
        // Allocate each element separately
        int64_t elemPtr = allocateMemory(ElemType);
        ((int64_t *)ret)[i] = elemPtr;
      }
      return ret;
    } else {
      return (int64_t)(malloc(data_layout.getTypeAllocSize(ElemType) * AT->getNumElements()));
    }
  } else if (T->isPointerTy()) {
    return (int64_t)(new int64_t); // Dummy pointer allocation
  } else if (T->isStructTy()) {
    return (int64_t)(malloc(data_layout.getTypeAllocSize(T)));
  } else {
    throw std::runtime_error("Unsupported type for allocation");
  }
  return 0; // No size means no allocation.
}

void JITRunner::storeValue(llvm::Value* V, int64_t val) {
  if (auto* GV = llvm::dyn_cast<llvm::GlobalVariable>(V)) {
    globalval_map[V] = val;
  } else {
    (*localval_map)[V] = val;
  }
}

int64_t JITRunner::getValue(llvm::Value* V) {
  if (auto* CI = llvm::dyn_cast<llvm::ConstantInt>(V)) {
    return asInt(CI->getValue());
  }
  auto it = globalval_map.find(V);
  if (it != globalval_map.end()) {
    return it->second;
  }
  auto localIt = localval_map->find(V);
  if (localIt != localval_map->end()) {
    return localIt->second;
  }
  throw std::runtime_error("Value not computed yet.");
}