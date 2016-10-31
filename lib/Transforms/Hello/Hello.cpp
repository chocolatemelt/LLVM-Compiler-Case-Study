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
#include <stack>

#include "llvm/ADT/Statistic.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/InstVisitor.h"
#include "llvm/IR/Instruction.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/Pass.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Transforms/Utils/BasicBlockUtils.h"

#define DEBUG_TYPE "hello"

using namespace llvm;

namespace {
	// Collect all use-defs into a container
	void getUseDef(User *I, std::stack<Value *> &bucket, Function *f) {
		for(User::op_iterator i = I->op_begin(), e = I->op_end(); i != e; ++i) {
			Value *v = *i;
			if (Instruction *w = dyn_cast<Instruction>(v)) {
				bucket.push(w);
				getUseDef(w, bucket, f);
			}
		}
	}

	/// Emit all necessary dependencies for Instruction I
	void handleDeps(IRBuilder<> &builder, Instruction &I) {
		std::stack<Value *> bucket;
		Function *f = I.getParent()->getParent();
		getUseDef(&I, bucket, f);
		while(bucket.size()) {
			Value *v = bucket.top();
			bucket.pop();
			// TODO
			// invert it
			// push inverted instruction onto end of new function
			switch (v->getOpcode()) {
				case Instruction::Add:
					newInstruction = builder.CreateSub(lookup(v->getOperand(0)), lookup(v->getOperand(1)));
					break;

				case Instruction::Sub:
					newInstruction = builder.CreateAdd(lookup(v->getOperand(0)), lookup(v->getOperand(1)));
					break;

				case Instruction::Mul:
					newInstruction = builder.CreateSDiv(lookup(v->getOperand(0)), lookup(v->getOperand(1)));
					break;

				case Instruction::Div:
					newInstruction = builder.CreateMul(lookup(v->getOperand(0)), lookup(v->getOperand(1)));
					break;

				default:
					assert(0 && "visitBinaryOperator died");
					break;
			}
		}
	}
}

// Handles the reversal of the code
struct GlobalInverter:
	public InstVisitor<GlobalInverter> {
		IRBuilder<> &builder;
		void visitStoreInst(StoreInst &SI) {
			handleDeps(builder, SI);
		}
	};

// Driver, runs the reverser pass, and then constructs the inverse function
struct Hello : public FunctionPass {
	static char ID;
	Hello() : FunctionPass(ID) {}

	bool runOnFunction(Function &F) override {
		if (F.getName() == "foo") {
			GlobalInverter inverter;
			inverter.visit(F);
			return true;
		}
		return false;
	}
};
}

char Hello::ID = 0;
static RegisterPass<Hello> X("hello", "Hello World Pass");
