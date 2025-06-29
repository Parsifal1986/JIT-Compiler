#ifndef ASMSTRUCT_HPP
#define ASMSTRUCT_HPP

#include "../util/util.hpp"
#include "asmcmd.hpp"

namespace asmcode {

class AsmBlock {
public:
  AsmBlock(std::unordered_map<llvm::Value*, int64_t*> &localval_map) : val_map(localval_map) {
  };

  ~AsmBlock() = default;

  void addBinary(llvm::Instruction* I) {
    ldData(asmcode::Register("s1"), I->getOperand(0));
    ldData(asmcode::Register("s2"), I->getOperand(1));
    asmcode::binary::Opcode op;
    switch (I->getOpcode()) {
      case llvm::Instruction::Add:
        op = asmcode::binary::ADD;
        break;
      case llvm::Instruction::Sub:
        op = asmcode::binary::SUB;
        break;
      case llvm::Instruction::Mul:
        op = asmcode::binary::MUL;
        break;
      case llvm::Instruction::And:
        op = asmcode::binary::AND;
        break;
      case llvm::Instruction::Or:
        op = asmcode::binary::OR;
        break;
      case llvm::Instruction::Xor:
        op = asmcode::binary::XOR;
        break;
      case llvm::Instruction::Shl:
        op = asmcode::binary::SHL;
        break;
      case llvm::Instruction::AShr:
        op = asmcode::binary::ASHR;
        break;
      case llvm::Instruction::SDiv:
        op = asmcode::binary::DIV;
        break;
      case llvm::Instruction::SRem:
        op = asmcode::binary::MOD;
        break;
      case llvm::Instruction::ICmp: {
        auto* CI = llvm::cast<llvm::ICmpInst>(I);
        switch (CI->getPredicate()) {
          case llvm::ICmpInst::ICMP_EQ:
            op = asmcode::binary::EQ;
            break;
          case llvm::ICmpInst::ICMP_NE:
            op = asmcode::binary::NE;
            break;
          case llvm::ICmpInst::ICMP_SLT:
            op = asmcode::binary::SLT;
            break;
          case llvm::ICmpInst::ICMP_SLE:
            op = asmcode::binary::SLE;
            break;
          case llvm::ICmpInst::ICMP_SGT:
            op = asmcode::binary::SGT;
            break;
          case llvm::ICmpInst::ICMP_SGE:
            op = asmcode::binary::SGE;
            break;
          default:
            throw std::runtime_error("Unsupported ICmp predicate in compile mode.");
        }
        break;
      }
      default:
        throw std::runtime_error("Unsupported instruction in threshold mode.");
    }
    instructions.push_back(new asmcode::binary(op, asmcode::Register("s0"), asmcode::Register("s1"), asmcode::Register("s2")));
    stData(asmcode::Register("s0"), I);
  }

  void addLoad(llvm::Instruction* I) {
    llvm::Value* V = llvm::cast<llvm::LoadInst>(I)->getPointerOperand();
    ldData(asmcode::Register("s0"), V);
    instructions.push_back(new asmcode::ld(asmcode::Register("s0"), asmcode::Register("s0")));
    stData(asmcode::Register("s0"), I);
  }

  void addStore(llvm::Instruction* I) {
    llvm::Value* V = llvm::cast<llvm::StoreInst>(I)->getValueOperand();
    llvm::Value* Ptr = llvm::cast<llvm::StoreInst>(I)->getPointerOperand();
    ldData(asmcode::Register("s0"), V);
    ldData(asmcode::Register("s1"), Ptr);
    instructions.push_back(new asmcode::st(asmcode::Register("s0"), asmcode::Register("s1")));
  }

  void addGetElementPtr(llvm::Instruction* I, const llvm::DataLayout &data_layout) {
    auto* GEP = llvm::cast<llvm::GetElementPtrInst>(I);
    llvm::Value* Ptr = GEP->getPointerOperand();
    ldData(asmcode::Register("s0"), Ptr);
    llvm::Type* curTy = GEP->getSourceElementType();
    int64_t offset = 0;
    auto idxIt = GEP->idx_begin();
    int64_t idxVal = llvm::cast<llvm::ConstantInt>(*idxIt)->getValue().getSExtValue();
    if (idxVal != 0) {
      ldData(asmcode::Register("s1"), *idxIt);
      instructions.push_back(new asmcode::li(asmcode::Register("s2"), Immediate(data_layout.getTypeAllocSize(curTy))));
      instructions.push_back(new asmcode::binary(asmcode::binary::MUL, asmcode::Register("s1"), asmcode::Register("s1"), asmcode::Register("s2")));
      instructions.push_back(new asmcode::binary(asmcode::binary::ADD, asmcode::Register("s0"), asmcode::Register("s0"), asmcode::Register("s1")));
    }
    for (++idxIt; idxIt != GEP->idx_end(); ++idxIt) {
      if (curTy->isStructTy()) {
        if (!llvm::isa<llvm::Constant>(*idxIt)) {
          throw std::runtime_error("GEP with non-constant index is not supported in compile mode.");
        }
        idxVal = llvm::cast<llvm::ConstantInt>(*idxIt)->getValue().getSExtValue();

        auto* STy = llvm::cast<llvm::StructType>(curTy);
        const auto* SL = data_layout.getStructLayout(STy);
        unsigned fieldNo = static_cast<unsigned>(idxVal);
        
        curTy = STy->getElementType(fieldNo);

        instructions.push_back(new asmcode::li(asmcode::Register("s1"), Immediate(SL->getElementOffset(fieldNo))));
        instructions.push_back(new asmcode::binary(asmcode::binary::ADD, asmcode::Register("s0"), asmcode::Register("s0"), asmcode::Register("s1")));
      } else if (curTy->isArrayTy()) {
        ldData(asmcode::Register("s1"), *idxIt);

        const auto* ATy = llvm::cast<llvm::ArrayType>(curTy);
        llvm::Type* EltT = ATy->getElementType();
        uint64_t eltSize = data_layout.getTypeAllocSize(EltT);
        
        curTy = EltT;
        instructions.push_back(new asmcode::li(asmcode::Register("s2"), Immediate(eltSize)));
        instructions.push_back(new asmcode::binary(asmcode::binary::MUL, asmcode::Register("s1"), asmcode::Register("s1"), asmcode::Register("s2")));
        instructions.push_back(new asmcode::binary(asmcode::binary::ADD, asmcode::Register("s0"), asmcode::Register("s0"), asmcode::Register("s1")));
      }
    }
    stData(asmcode::Register("s0"), I);
  }

  void addPhi(llvm::Instruction* I) {
  }

  void addCall(llvm::Instruction* I) {
    int64_t* val;
    llvm::Value* V = I;
    if (val_map.find(V) == val_map.end()) {
      val = new int64_t;
      val_map[V] = val;
    }
    for (llvm::Use& U : I->operands()) {
      llvm::Value* Op = U.get();
      if (val_map.find(Op) == val_map.end()) {
        val = new int64_t;
        val_map[Op] = val;
      }
    }
  }

  void addRet() {
    instructions.push_back(new asmcode::ret());
  }

  void removeInstruction(size_t index) {
    if (index < instructions.size()) {
      instructions.erase(instructions.begin() + index);
    }
  }

  std::string toString() const {
    std::string result;
    for (const auto& inst : instructions) {
      result += inst->toString() + "\n";
    }
    return result;
  }

  void clear() {
    instructions.clear();
  }

  const std::vector<const Instruction*>& getInstructions() const {
    return instructions;
  }

  size_t size() const {
    return instructions.size();
  }

  void encode(unsigned char** encode, size_t* size, size_t* count) const {
    size_t total_size = 0;
    *count = instructions.size();
    for (const auto& inst : instructions) {
      total_size += inst->size();
    }
    *encode = (unsigned char*)malloc(total_size);
    int64_t offset = 0;
    for (int i = 0;i < *count; ++i) {
      int size = instructions[i]->size();
      unsigned char* encoded_inst = instructions[i]->encode();
      std::memcpy(*encode + offset, encoded_inst, size);
      offset += size;
    }
    *size = total_size;
  }

  void regSave() {
    instructions.push_back(new asmcode::st(asmcode::Register("s0"), asmcode::Register("sp"), Immediate(-8)));
    instructions.push_back(new asmcode::st(asmcode::Register("s1"), asmcode::Register("sp"), Immediate(-16)));
    instructions.push_back(new asmcode::st(asmcode::Register("s2"), asmcode::Register("sp"), Immediate(-24)));
    instructions.push_back(new asmcode::st(asmcode::Register("s3"), asmcode::Register("sp"), Immediate(-32)));
    instructions.push_back(new asmcode::st(asmcode::Register("s4"), asmcode::Register("sp"), Immediate(-40)));
  }

  void regLoad() {
    instructions.push_back(new asmcode::ld(asmcode::Register("s4"), asmcode::Register("sp"), Immediate(-40)));
    instructions.push_back(new asmcode::ld(asmcode::Register("s3"), asmcode::Register("sp"), Immediate(-32)));
    instructions.push_back(new asmcode::ld(asmcode::Register("s2"), asmcode::Register("sp"), Immediate(-24)));
    instructions.push_back(new asmcode::ld(asmcode::Register("s1"), asmcode::Register("sp"), Immediate(-16)));
    instructions.push_back(new asmcode::ld(asmcode::Register("s0"), asmcode::Register("sp"), Immediate(-8)));
  }
  
public:
  std::unordered_map<llvm::Value*, int64_t*> &val_map;

private:
  void ldData(Register R, llvm::Value* V) {
    if (auto* CI = llvm::dyn_cast<llvm::ConstantInt>(V)) {
      instructions.push_back(new asmcode::li(R, Immediate(CI->getValue().getSExtValue())));
    } else {
      int64_t *val = nullptr;
      if (val_map.find(V) != val_map.end()) {
        val = val_map[V];
      } else {
        val = new int64_t;
        val_map[V] = val;
      }
      instructions.push_back(new asmcode::li(R, Immediate((int64_t)val)));
      instructions.push_back(new asmcode::ld(R, R));
    }
  }

  void stData(Register R, llvm::Value* V) {
    int64_t *val = nullptr;
    if (val_map.find(V) != val_map.end()) {
      val = val_map[V];
    } else {
      val = new int64_t;
      val_map[V] = val;
    }
    instructions.push_back(new asmcode::li(Register("s4"), Immediate((int64_t)val)));
    instructions.push_back(new asmcode::st(R, Register("s4")));
  }

private:
  std::vector<const Instruction*> instructions;
};

}

#endif // ASMSTRUCT_HPP