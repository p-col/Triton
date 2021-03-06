
#ifndef   TRITONPYOBJECT_H
#define   TRITONPYOBJECT_H

#include <python2.7/Python.h>
#include "IRBuilder.h"
#include "IRBuilderOperand.h"
#include "Inst.h"
#include "SMT2Lib.h"
#include "SymbolicEngine.h"
#include "SymbolicVariable.h"
#include "TritonOperand.h"


PyObject *PyInstruction(IRBuilder *irb);
PyObject *PyInstruction(Inst *inst);
PyObject *PyOperand(TritonOperand operand);
PyObject *PySmtAstNode(smt2lib::smtAstAbstractNode *node);
PyObject *PySymbolicExpression(SymbolicExpression *expression);
PyObject *PySymbolicVariable(SymbolicVariable *symVar);

// SmtAstNode ===================================

typedef struct {
  PyObject_HEAD
  smt2lib::smtAstAbstractNode *node;
} SmtAstNode_Object;

extern PyTypeObject SmtAstNode_Type;

#define PySmtAstNode_Check(v) ((v)->ob_type == &SmtAstNode_Type)
#define PySmtAstNode_AsSmtAstNode(v) (((SmtAstNode_Object *)(v))->node)

// SymbolicExpression ===========================

typedef struct {
  PyObject_HEAD
  SymbolicExpression *expression;
} SymbolicExpression_Object;

extern PyTypeObject SymbolicExpression_Type;

#define PySymbolicExpression_Check(v) ((v)->ob_type == &SymbolicExpression_Type)
#define PySymbolicExpression_AsSymbolicExpression(v) (((SymbolicExpression_Object *)(v))->expression)

// SymbolicVariable =============================

typedef struct {
  PyObject_HEAD
  SymbolicVariable *variable;
} SymbolicVariable_Object;

extern PyTypeObject SymbolicVariable_Type;

#define PySymbolicVariable_Check(v) ((v)->ob_type == &SymbolicVariable_Type)
#define PySymbolicVariable_AsSymbolicVariable(v) (((SymbolicVariable_Object *)(v))->variable)

#endif     /* !__TRITONPYOBJECT_H__ */

