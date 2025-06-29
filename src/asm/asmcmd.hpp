#ifndef ASMCMD_HPP
#define ASMCMD_HPP

#include "../util/util.hpp"
#include "asmdata.hpp"

namespace asmcode {

inline void write_uint32(unsigned char* buf, uint32_t val) {
  buf[0] = val & 0xff;
  buf[1] = (val >> 8) & 0xff;
  buf[2] = (val >> 16) & 0xff;
  buf[3] = (val >> 24) & 0xff;
}

class Instruction {
public:
  Instruction() = default;

  virtual ~Instruction() = default;

  virtual std::string toString() const = 0;

  virtual unsigned char* encode() const = 0;

  virtual int64_t size() const = 0;
};

class binary : public Instruction {
public:
  enum Opcode {
    ADD,
    SUB,
    MUL,
    DIV,
    MOD,
    AND,
    OR,
    XOR,
    SHL,
    SHR,
    ASHR,

    EQ,
    NE,
    SLT,
    SLE,
    SGT,
    SGE
  };

  binary(const Opcode op, const asmcode::Register &target, const asmcode::Register &lhs, const asmcode::Register &rhs) : target(target), lhs(lhs), rhs(rhs) {
    switch (op) {
      case ADD:
        this->op = "add";
        break;
      case SUB:
        this->op = "sub";
        break;
      case MUL:
        this->op = "mul";
        break;
      case MOD:
        this->op = "mod";
        break;
      case AND:
        this->op = "and";
        break;
      case OR:
        this->op = "or";
        break;
      case XOR:
        this->op = "xor";
        break;
      case SHL:
        this->op = "shl";
        break;
      case SHR:
        this->op = "shr";
        break;
      case ASHR:
        this->op = "ashr";
        break;
      case DIV:
        this->op = "div";
        break;
      case EQ:
        this->op = "eq";
        break;
      case NE:
        this->op = "ne";
        break;
      case SLT:
        this->op = "slt";
        break;
      case SLE:
        this->op = "sle";
        break;
      case SGT:
        this->op = "sgt";
        break;
      case SGE:
        this->op = "sge";
        break;

      default:
        break;
    }
  }

  std::string toString() const override {
    return op + " " + target.toString() + ", " + lhs.toString() + ", " + rhs.toString();
  }

  unsigned char* encode() const override {
    uint32_t opcode = 0x33;  // OP opcode
    uint32_t funct3, funct7;

    if (op == "add") { funct3 = 0x0; funct7 = 0x00; }
    else if (op == "sub") { funct3 = 0x0; funct7 = 0x20; }
    else if (op == "mul") { funct3 = 0x0; funct7 = 0x01; }
    else if (op == "div") { funct3 = 0x4; funct7 = 0x01; }
    else if (op == "mod") { funct3 = 0x6; funct7 = 0x01; } // rem
    else if (op == "and") { funct3 = 0x7; funct7 = 0x00; }
    else if (op == "or") { funct3 = 0x6; funct7 = 0x00; }
    else if (op == "xor") { funct3 = 0x4; funct7 = 0x00; }
    else if (op == "shl") { funct3 = 0x1; funct7 = 0x00; } // sll
    else if (op == "shr") { funct3 = 0x5; funct7 = 0x00; } // srl
    else if (op == "ashr") { funct3 = 0x5; funct7 = 0x20; } // sra
    else if (op == "slt") { funct3 = 0x2; funct7 = 0x00; }
    else if (op == "sge") { funct3 = 0x2; funct7 = 0x01; } // pseudo
    else if (op == "sgt") { funct3 = 0x3; funct7 = 0x01; } // pseudo
    else if (op == "eq") { funct3 = 0x0; funct7 = 0x01; } // pseudo
    else if (op == "ne") { funct3 = 0x1; funct7 = 0x01; } // pseudo
    else {
      throw std::runtime_error("Unsupported op: " + op);
    }

    uint32_t inst = (funct7 << 25) | (rhs.id() << 20) | (lhs.id() << 15) |
      (funct3 << 12) | (target.id() << 7) | opcode;

    unsigned char* buf = new unsigned char[4];
    write_uint32(buf, inst);
    return buf;
  }

  int64_t size() const override {
    return 4; // Each instruction is 4 bytes
  }

private:
  asmcode::Register target;
  asmcode::Register lhs;
  asmcode::Register rhs;
  std::string op;
};

class ld : public Instruction {
public:
  ld(const asmcode::Register &reg, const asmcode::Register &address, const asmcode::Immediate &offset = asmcode::Immediate(0)) : reg(reg), address(address), offset(offset) {
  }

  std::string toString() const override {
    return "ld " + reg.toString() + ", " + offset.toString() + "(" + address.toString() + ")";
  }

  unsigned char* encode() const override {
    uint32_t opcode = 0x03; // LOAD
    uint32_t funct3 = 0x3;  // LD (64-bit)
    uint32_t imm = offset.getValue();       // offset 0

    uint32_t inst = (imm << 20) | (address.id() << 15) |
      (funct3 << 12) | (reg.id() << 7) | opcode;

    unsigned char* buf = new unsigned char[4];
    write_uint32(buf, inst);
    return buf;
  }

  int64_t size() const override {
    return 4; // Each instruction is 4 bytes
  }

private:
  asmcode::Register reg;
  asmcode::Register address;
  asmcode::Immediate offset;
};

class st : public Instruction {
public:
  st(const asmcode::Register& reg, const asmcode::Register& address, const asmcode::Immediate& offset = asmcode::Immediate(0)) : reg(reg), address(address), offset(offset) {
  }

  std::string toString() const override {
    return "sd " + reg.toString() + ", " + offset.toString() + "(" + address.toString() + ")";
  }

  unsigned char* encode() const override {
    uint32_t opcode = 0x23; // STORE
    uint32_t funct3 = 0x3;  // SD (64-bit)
    uint32_t imm = offset.getValue();       // offset 0
    uint32_t imm11_5 = (imm >> 5) & 0x7F;
    uint32_t imm4_0 = imm & 0x1F;

    uint32_t inst = (imm11_5 << 25) | (reg.id() << 20) | (address.id() << 15) |
      (funct3 << 12) | (imm4_0 << 7) | opcode;

    unsigned char* buf = new unsigned char[4];
    write_uint32(buf, inst);
    return buf;
  }

  int64_t size() const override {
    return 4; // Each instruction is 4 bytes
  }

private:
  asmcode::Register reg;
  asmcode::Register address;
  asmcode::Immediate offset;
};

class li : public Instruction {
public:
  li(const asmcode::Register &reg, const asmcode::Immediate &imm) : reg(reg), imm(imm) {
  }

  int64_t signextend(int64_t value, int bits) const {
    int64_t mask = (1LL << (bits - 1));
    return (value ^ mask) - mask;
  }

  std::string toString() const override {
    // std::string ret;
    // ret = "lui " + reg.toString() + ", " + std::to_string(imm.getValue() >> 44) + "\n";
    // ret += "addi " + reg.toString() + ", " + reg.toString() + ", " + std::to_string(imm.getValue() >> 32 & 0xFFF) + "\n";
    // ret += "slli " + reg.toString() + ", " + reg.toString() + ", 32\n";
    // ret += "lui x3, " + std::to_string(imm.getValue() >> 12 & 0xFFFFF) + "\n";
    // ret += "addi x3, x3, " + std::to_string(signextend(imm.getValue() & 0xFFF, 12)) + "\n";
    // ret += "add " + reg.toString() + ", " + reg.toString() + ", x3\n";
    // return ret;
    return "li " + reg.toString() + ", " + imm.toString();
  }

  unsigned char* encode() const override {
    int64_t value = imm.getValue();

    // printf("value = 0x%llx\n", value);

    int32_t hi32 = static_cast<int32_t>(value >> 32);
    int32_t lo32 = static_cast<int32_t>(value & 0xFFFFFFFF);

    uint32_t instrs[6];
    uint32_t reg_id = reg.id();
    uint32_t temp_id = 19;
    int32_t hi_upper = hi32 >> 12;
    int32_t hi_lower = hi32 & 0xFFF;

    if (lo32 & 0x80000000) {
      hi_lower += 1;
    }

    if (hi_lower & 0x800) {
      hi_upper += 1;
    }

    instrs[0] = ((hi_upper & 0xFFFFF) << 12) | (reg_id << 7) | 0x37;
    instrs[1] = ((hi_lower & 0xFFF) << 20) | (reg_id << 15) | (0x0 << 12) | (reg_id << 7) | 0x13;
    instrs[2] = (32 << 20) | (reg_id << 15) | (0x1 << 12) | (reg_id << 7) | 0x13;

    int32_t lo_upper = lo32 >> 12;
    int32_t lo_lower = lo32 & 0xFFF;

    if (lo_lower & 0x800) {
      lo_upper += 1;
    }

    instrs[3] = ((lo_upper & 0xFFFFF) << 12) | (temp_id << 7) | 0x37;
    instrs[4] = ((lo_lower & 0xFFF) << 20) | (temp_id << 15) | (0x0 << 12) | (temp_id << 7) | 0x13;
    instrs[5] = (temp_id << 20) | (reg_id << 15) | (0x0 << 12) | (reg_id << 7) | 0x33;


    // 转为字节序列
    unsigned char* buf = new unsigned char[6 * 4];
    for (int i = 0; i < 6; ++i) {
      write_uint32(buf + i * 4, instrs[i]);
    }

    return buf;
  }

  int64_t size() const override {
    return 6 * 4; // Each instruction is 4 bytes, total 6 instructions
  }

private:
  asmcode::Register reg;
  asmcode::Immediate imm;
};

class ret : public Instruction {
public:
  ret() = default;

  std::string toString() const override {
    return "ret";
  }

  unsigned char* encode() const override {
    unsigned char* buf = new unsigned char[4];
    uint32_t inst = 0x00008067;
    write_uint32(buf, inst);
    return buf;
  }

  int64_t size() const override {
    return 4; // Each instruction is 4 bytes
  }
};

}


#endif // ASMCMD_HPP