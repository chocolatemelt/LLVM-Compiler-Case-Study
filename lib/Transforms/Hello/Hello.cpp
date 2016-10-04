//===- Hello.cpp - Example code from "Writing an LLVM Pass" ---------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This file implements two versions of the LLVM "Hello World" pass described
// in docs/WritingAnLLVMPass.html
//
//===----------------------------------------------------------------------===//

#include "llvm/ADT/Statistic.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/InstVisitor.h"
#include "llvm/IR/Instruction.h"
#include "llvm/Pass.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Transforms/Utils/BasicBlockUtils.h"

using namespace llvm;

#define DEBUG_TYPE "hello"

namespace {
  struct BinaryInverter:
		public InstVisitor<BinaryInverter> {
			void visitBinaryOperator(BinaryOperator &bo) {
				auto inverted = Instruction::Add;
				switch(bo.getOpcode()) {
					case Instruction::Add:
						inverted = Instruction::Sub;
						break;
					case Instruction::Sub:
						inverted = Instruction::Add;
						break;
					case Instruction::Mul:
						inverted = Instruction::UDiv;
						break;
					case Instruction::UDiv: case Instruction::SDiv:
						inverted = Instruction::Mul;
						break;
					case Instruction::FAdd:
						inverted = Instruction::FSub;
						break;
					case Instruction::FSub:
						inverted = Instruction::FAdd;
						break;
					case Instruction::FMul:
						inverted = Instruction::FDiv;
						break;
					case Instruction::FDiv:
						inverted = Instruction::FMul;
						break;
					default: break;
				}
				auto binverted = BinaryOperator::Create(inverted, bo.getOperand(0), bo.getOperand(1));
				ReplaceInstWithInst(&bo, binverted);
				didChange = true;
			}
			bool didChange = false;
		};

  struct Hello : public FunctionPass {
    static char ID; // Pass identification, replacement for typeid
    Hello() : FunctionPass(ID) {}

    bool runOnFunction(Function &F) override {
			BinaryInverter op;
			op.visit(F);
			return op.didChange;
    }
  };
}

char Hello::ID = 0;
static RegisterPass<Hello> X("hello", "Hello World Pass");
