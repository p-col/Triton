#include <iostream>
#include <sstream>
#include <stdexcept>

#include <ImulIRBuilder.h>
#include <Registers.h>
#include <SMT2Lib.h>
#include <SymbolicExpression.h>


/*
 * Pin protip (thanks s.lecomte):
 *
 * case 1: (1 intel operand) -> 4 Pin operands
 * op1(explicit) : source1 (read)
 * op2(implicit) : source2 (AL/AX/EAX) (read)
 * op3(implicit) : destination (AH 8 bits, DX 16 bits, EDX 32bits) (write)
 * op4(implicit) : Eflags
 * op3 = op2 * op1
 *
 * case 2 (2 intel operands) -> 3 Pin operands
 * op1(explicit) : source1 and destination (read and write)
 * op2(explicit) : source2 (read)
 * op3(implicit) : Eflags 
 * op1 = op1 * op2
 *
 * case 3 (3 intel oeprands) -> 4 Pin operands
 * op1(explicit) : destination (write)
 * op2(explicit) : source1 (read)
 * op3(explicit) : source2 (read)
 * op4(implicit) : Eflags
 * op1 = op2 * op3
*/


ImulIRBuilder::ImulIRBuilder(uint64 address, const std::string &disassembly):
  BaseIRBuilder(address, disassembly) {
}


void ImulIRBuilder::regImm(AnalysisProcessor &ap, Inst &inst) const {
  SymbolicExpression *se;
  smt2lib::smtAstAbstractNode *expr, *op1, *op2;
  uint64 reg     = this->operands[0].getValue();
  uint64 imm     = this->operands[1].getValue();
  uint32 regSize = this->operands[0].getSize();

  /* Create the SMT semantic */
  op1 = ap.buildSymbolicRegOperand(reg, regSize);
  op2 = smt2lib::bv(imm, regSize * REG_SIZE);

  /* Finale expr */
  expr = smt2lib::extract((regSize * REG_SIZE) - 1, 0,
            smt2lib::bvmul(
              smt2lib::sx(regSize * REG_SIZE, op1),
              smt2lib::sx(regSize * REG_SIZE, op2)
            )
          );

  /* Create the symbolic expression */
  se = ap.createRegSE(inst, expr, reg, regSize);

  /* Apply the taint */
  ap.aluSpreadTaintRegImm(se, reg);

  /* Add the symbolic flags expression to the current inst */
  EflagsBuilder::cfImul(inst, se, ap, regSize, op1);
  EflagsBuilder::ofImul(inst, se, ap, regSize, op1);
}


void ImulIRBuilder::regReg(AnalysisProcessor &ap, Inst &inst) const {
  SymbolicExpression *se;
  smt2lib::smtAstAbstractNode *expr, *op1, *op2, *op3;
  uint64 reg1     = this->operands[0].getValue();
  uint32 regSize1 = this->operands[0].getSize();
  uint64 reg2     = this->operands[1].getValue();
  uint32 regSize2 = this->operands[1].getSize();
  uint64 imm      = 0;

  if (this->operands[2].getType() == IRBuilderOperand::IMM)
    imm = this->operands[2].getValue();

  /* Create the SMT semantic */
  op1 = ap.buildSymbolicRegOperand(reg1, regSize1);
  op2 = ap.buildSymbolicRegOperand(reg2, regSize2);
  op3 = smt2lib::bv(imm, regSize2 * REG_SIZE);

  /* Case 1 */
  if (this->operands[0].isReadOnly()) {

    /* Expr */
    expr = smt2lib::bvmul(
             smt2lib::sx(regSize2 * REG_SIZE, op2),
             smt2lib::sx(regSize1 * REG_SIZE, op1)
           );

    switch (regSize1) {

      case BYTE_SIZE:
        /* RAX */
        se = ap.createRegSE(inst, smt2lib::extract(WORD_SIZE_BIT - 1, 0, expr), ID_RAX, WORD_SIZE);
        ap.aluSpreadTaintRegReg(se, ID_RAX, reg2);
        break;

      case WORD_SIZE:
        /* RDX */
        se = ap.createRegSE(inst, smt2lib::extract(DWORD_SIZE_BIT - 1, WORD_SIZE_BIT, expr), ID_RDX, WORD_SIZE);
        ap.aluSpreadTaintRegReg(se, ID_RDX, reg2);
        /* RAX */
        se = ap.createRegSE(inst, smt2lib::extract(WORD_SIZE_BIT - 1, 0, expr), ID_RAX, WORD_SIZE);
        ap.aluSpreadTaintRegReg(se, ID_RAX, reg2);
        break;

      case DWORD_SIZE:
        /* RDX */
        se = ap.createRegSE(inst, smt2lib::extract(QWORD_SIZE_BIT - 1, DWORD_SIZE_BIT, expr), ID_RDX, DWORD_SIZE);
        ap.aluSpreadTaintRegReg(se, ID_RDX, reg2);
        /* RAX */
        se = ap.createRegSE(inst, smt2lib::extract(DWORD_SIZE_BIT - 1, 0, expr), ID_RAX, DWORD_SIZE);
        ap.aluSpreadTaintRegReg(se, ID_RAX, reg2);
        break;

      case QWORD_SIZE:
        /* RDX */
        se = ap.createRegSE(inst, smt2lib::extract(DQWORD_SIZE_BIT - 1, QWORD_SIZE_BIT, expr), ID_RDX, QWORD_SIZE);
        ap.aluSpreadTaintRegReg(se, ID_RDX, reg2);
        /* RAX */
        se = ap.createRegSE(inst, smt2lib::extract(QWORD_SIZE_BIT - 1, 0, expr), ID_RAX, QWORD_SIZE);
        ap.aluSpreadTaintRegReg(se, ID_RAX, reg2);
        break;

      default:
        throw std::runtime_error("ImulIRBuilder::reg - Invalid operand size");
    }

  }

  /* Case 2 */
  else if (this->operands[0].isReadAndWrite()) {
    /* Expr */
    expr = smt2lib::extract((regSize1 * REG_SIZE) - 1, 0,
              smt2lib::bvmul(
                smt2lib::sx(regSize1 * REG_SIZE, op1),
                smt2lib::sx(regSize2 * REG_SIZE, op2)
              )
            );

    /* Create the symbolic expression */
    se = ap.createRegSE(inst, expr, reg1, regSize1);

    /* Apply the taint */
    ap.aluSpreadTaintRegReg(se, reg1, reg2);
  }

  /* Case 3 */
  else if (this->operands[0].isWriteOnly()) {
    /* Expr */
    expr = smt2lib::extract((regSize1 * REG_SIZE) - 1, 0,
              smt2lib::bvmul(
                smt2lib::sx(regSize2 * REG_SIZE, op2),
                smt2lib::sx(regSize2 * REG_SIZE, op3)
              )
            );

    /* Create the symbolic expression */
    se = ap.createRegSE(inst, expr, reg1, regSize1);

    /* Apply the taint */
    ap.aluSpreadTaintRegReg(se, reg1, reg2);
  }

  else {
    throw std::runtime_error("ImulIRBuilder::regReg - Invalid operand");
  }

  /* Add the symbolic flags expression to the current inst */
  EflagsBuilder::cfImul(inst, se, ap, regSize1, op1);
  EflagsBuilder::ofImul(inst, se, ap, regSize1, op1);
}


void ImulIRBuilder::regMem(AnalysisProcessor &ap, Inst &inst) const {
  SymbolicExpression *se;
  smt2lib::smtAstAbstractNode *expr, *op1, *op2, *op3;
  uint64 reg1     = this->operands[0].getValue();
  uint32 regSize1 = this->operands[0].getSize();
  uint64 mem2     = this->operands[1].getValue();
  uint32 memSize2 = this->operands[1].getSize();
  uint64 imm     = 0;

  if (this->operands[2].getType() == IRBuilderOperand::IMM)
    imm = this->operands[2].getValue();

  /* Create the SMT semantic */
  op1 = ap.buildSymbolicRegOperand(reg1, regSize1);
  op2 = ap.buildSymbolicMemOperand(mem2, memSize2);
  op3 = smt2lib::bv(imm, memSize2 * REG_SIZE);

  /* Case 1 */
  if (this->operands[0].isReadOnly()) {

    /* Expr */
    expr = smt2lib::bvmul(
             smt2lib::sx(memSize2 * REG_SIZE, op2),
             smt2lib::sx(regSize1 * REG_SIZE, op1)
           );

    switch (regSize1) {

      case BYTE_SIZE:
        /* RAX */
        se = ap.createRegSE(inst, smt2lib::extract(WORD_SIZE_BIT - 1, 0, expr), ID_RAX, WORD_SIZE);
        ap.aluSpreadTaintRegMem(se, ID_RAX, mem2, memSize2);
        break;

      case WORD_SIZE:
        /* RDX */
        se = ap.createRegSE(inst, smt2lib::extract(DWORD_SIZE_BIT - 1, WORD_SIZE_BIT, expr), ID_RDX, WORD_SIZE);
        ap.aluSpreadTaintRegMem(se, ID_RDX, mem2, memSize2);
        /* RAX */
        se = ap.createRegSE(inst, smt2lib::extract(WORD_SIZE_BIT - 1, 0, expr), ID_RAX, WORD_SIZE);
        ap.aluSpreadTaintRegMem(se, ID_RAX, mem2, memSize2);
        break;

      case DWORD_SIZE:
        /* RDX */
        se = ap.createRegSE(inst, smt2lib::extract(QWORD_SIZE_BIT - 1, DWORD_SIZE_BIT, expr), ID_RDX, DWORD_SIZE);
        ap.aluSpreadTaintRegMem(se, ID_RDX, mem2, memSize2);
        /* RAX */
        se = ap.createRegSE(inst, smt2lib::extract(DWORD_SIZE_BIT - 1, 0, expr), ID_RAX, DWORD_SIZE);
        ap.aluSpreadTaintRegMem(se, ID_RAX, mem2, memSize2);
        break;

      case QWORD_SIZE:
        /* RDX */
        se = ap.createRegSE(inst, smt2lib::extract(DQWORD_SIZE_BIT - 1, QWORD_SIZE_BIT, expr), ID_RDX, QWORD_SIZE);
        ap.aluSpreadTaintRegMem(se, ID_RDX, mem2, memSize2);
        /* RAX */
        se = ap.createRegSE(inst, smt2lib::extract(QWORD_SIZE_BIT - 1, 0, expr), ID_RAX, QWORD_SIZE);
        ap.aluSpreadTaintRegMem(se, ID_RAX, mem2, memSize2);
        break;

      default:
        throw std::runtime_error("ImulIRBuilder::reg - Invalid operand size");
    }

  }

  /* Case 2 */
  else if (this->operands[0].isReadAndWrite()) {
    /* Expr */
    expr = smt2lib::extract((regSize1 * REG_SIZE) - 1, 0,
              smt2lib::bvmul(
                smt2lib::sx(regSize1 * REG_SIZE, op1),
                smt2lib::sx(memSize2 * REG_SIZE, op2)
              )
            );

    /* Create the symbolic expression */
    se = ap.createRegSE(inst, expr, reg1, regSize1);

    /* Apply the taint */
    ap.aluSpreadTaintRegMem(se, reg1, mem2, memSize2);
  }

  /* Case 3 */
  else if (this->operands[0].isWriteOnly()) {
    /* Expr */
    expr = smt2lib::extract((regSize1 * REG_SIZE) - 1, 0,
              smt2lib::bvmul(
                smt2lib::sx(memSize2 * REG_SIZE, op2),
                smt2lib::sx(memSize2 * REG_SIZE, op3)
              )
            );

    /* Create the symbolic expression */
    se = ap.createRegSE(inst, expr, reg1, regSize1);

    /* Apply the taint */
    ap.aluSpreadTaintRegMem(se, reg1, mem2, memSize2);
  }

  else {
    throw std::runtime_error("ImulIRBuilder::regReg - Invalid operand");
  }

  /* Add the symbolic flags expression to the current inst */
  EflagsBuilder::cfImul(inst, se, ap, regSize1, op1);
  EflagsBuilder::ofImul(inst, se, ap, regSize1, op1);
}


void ImulIRBuilder::memImm(AnalysisProcessor &ap, Inst &inst) const {
  TwoOperandsTemplate::stop(this->disas);
}


void ImulIRBuilder::memReg(AnalysisProcessor &ap, Inst &inst) const {
  TwoOperandsTemplate::stop(this->disas);
}


Inst *ImulIRBuilder::process(AnalysisProcessor &ap) const {
  this->checkSetup();

  Inst *inst = new Inst(ap.getThreadID(), this->address, this->disas);

  try {
    this->templateMethod(ap, *inst, this->operands, "IMUL");
    ap.incNumberOfExpressions(inst->numberOfExpressions()); /* Used for statistics */
    ControlFlow::rip(*inst, ap, this->nextAddress);
  }
  catch (std::exception &e) {
    delete inst;
    throw;
  }

  return inst;
}

