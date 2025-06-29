#ifndef ASM_DATA_HPP
#define ASM_DATA_HPP

#include <cstdint>
#include <vector>
#include <string>

namespace asmcode {
class Value {
public:
  Value() = default;

  virtual ~Value() = default;

  virtual std::string toString() const = 0;
};

class Register : public Value {
public:
  Register(const std::string &name) : name(name) {};

  ~Register() override = default;

  std::string toString() const override {
    return name;
  }

  int64_t id() const {
    return reg_map.at(name);
  }
private:
  std::string name;
  std::unordered_map<std::string, int64_t> reg_map = {
    {"zero", 0},
    {"ra", 1},
    {"sp", 2},
    {"gp", 3},
    {"tp", 4},
    {"t0", 5},
    {"t1", 6},
    {"t2", 7},
    {"s0", 8},
    {"s1", 9},
    {"a0", 10},
    {"a1", 11},
    {"a2", 12},
    {"a3", 13},
    {"a4", 14},
    {"a5", 15},
    {"a6", 16},
    {"a7", 17},
    {"s2", 18},
    {"s3", 19},
    {"s4", 20},
    {"s5", 21},
    {"s6", 22},
    {"s7", 23},
    {"s8", 24},
    {"s9", 25},
    {"s10", 26},
    {"s11", 27},
    {"t3", 28},
    {"t4", 29},
    {"t5", 30},
    {"t6", 31}
  };
};

class Immediate : public Value {
public:
  Immediate(int64_t value) : value(value) {};

  ~Immediate() override = default;
  
  std::string toString() const override {
    return std::to_string(value);
  }

  int64_t getValue() const {
    return value;
  }

private:
  int64_t value;
};
}

#endif // ASM_DATA_HPP