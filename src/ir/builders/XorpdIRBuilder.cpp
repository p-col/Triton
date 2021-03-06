#include <iostream>
#include <sstream>
#include <stdexcept>

#include <XorpdIRBuilder.h>
#include <Registers.h>
#include <SMT2Lib.h>
#include <SymbolicExpression.h>


XorpdIRBuilder::XorpdIRBuilder(uint64 address, const std::string &disassembly):
  BaseIRBuilder(address, disassembly) {
}


void XorpdIRBuilder::regImm(AnalysisProcessor &ap, Inst &inst) const {
  TwoOperandsTemplate::stop(this->disas);
}


void XorpdIRBuilder::regReg(AnalysisProcessor &ap, Inst &inst) const {
  SymbolicExpression *se;
  smt2lib::smtAstAbstractNode *expr, *op1, *op2;
  uint64 reg1     = this->operands[0].getValue();
  uint64 reg2     = this->operands[1].getValue();
  uint32 regSize1 = this->operands[0].getSize();
  uint32 regSize2 = this->operands[1].getSize();

  /* Create the SMT semantic */
  op1 = ap.buildSymbolicRegOperand(reg1, regSize1);
  op2 = ap.buildSymbolicRegOperand(reg2, regSize2);

  // Final expr
  expr = smt2lib::bvxor(op1, op2);

  /* Create the symbolic expression */
  se = ap.createRegSE(inst, expr, reg1, regSize1);

  /* Apply the taint */
  ap.aluSpreadTaintRegReg(se, reg1, reg2);

}


void XorpdIRBuilder::regMem(AnalysisProcessor &ap, Inst &inst) const {
  SymbolicExpression *se;
  smt2lib::smtAstAbstractNode *expr, *op1, *op2;
  uint32 readSize = this->operands[1].getSize();
  uint64 mem      = this->operands[1].getValue();
  uint64 reg      = this->operands[0].getValue();
  uint32 regSize  = this->operands[1].getSize();

  /* Create the SMT semantic */
  op1 = ap.buildSymbolicRegOperand(reg, regSize);
  op2 = ap.buildSymbolicMemOperand(mem, readSize);

  // Final expr
  expr = smt2lib::bvxor(op1, op2);

  /* Create the symbolic expression */
  se = ap.createRegSE(inst, expr, reg, regSize);

  /* Apply the taint */
  ap.aluSpreadTaintRegMem(se, reg, mem, readSize);

}


void XorpdIRBuilder::memImm(AnalysisProcessor &ap, Inst &inst) const {
  TwoOperandsTemplate::stop(this->disas);
}


void XorpdIRBuilder::memReg(AnalysisProcessor &ap, Inst &inst) const {
  TwoOperandsTemplate::stop(this->disas);
}


Inst *XorpdIRBuilder::process(AnalysisProcessor &ap) const {
  this->checkSetup();

  Inst *inst = new Inst(ap.getThreadID(), this->address, this->disas);

  try {
    this->templateMethod(ap, *inst, this->operands, "XORPD");
    ap.incNumberOfExpressions(inst->numberOfExpressions()); /* Used for statistics */
    ControlFlow::rip(*inst, ap, this->nextAddress);
  }
  catch (std::exception &e) {
    delete inst;
    throw;
  }

  return inst;
}

