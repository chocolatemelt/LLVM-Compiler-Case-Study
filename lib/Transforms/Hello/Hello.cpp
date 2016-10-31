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
	// initializer for basic block
	BasicBlock &getBasicBlock(Function *f) {
		return f->front();
	}

	// push instructions into a stack
	void getUseDef(User *I, std::stack<Value *> &bucket, Function *f) {
		for(User::op_iterator i = I->op_begin(), e = I->op_end(); i != e; ++i) {
			Value *v = *i;
			if(Instruction *w = dyn_cast<Instruction>(v)) {
				bucket.push(w);
				getUseDef(w, bucket, f);
			}
		}
	}

	// emit dependencies on instruction
	void handleDeps(IRBuilder<> &builder, Instruction &I) {
		std::stack<Value *> bucket;
		Function *f = I.getParent()->getParent();
		getUseDef(&I, bucket, f);
		// get the first value in the bucket and call it "last"
		// use last in subsequent instruction calls
		Value *last = dyn_cast<Value>(bucket.top());
		bucket.pop();
		while(bucket.size()) {
			Instruction *v = dyn_cast<Instruction>(bucket.top());
			bucket.pop();
			switch (v->getOpcode()) {
				case Instruction::Add:
					last = builder.CreateSub(last, v->getOperand(1));
					break;

				case Instruction::Sub:
					last = builder.CreateAdd(last, v->getOperand(1));
					break;

				case Instruction::Mul:
					last = builder.CreateSDiv(last, v->getOperand(1));
					break;

				case Instruction::SDiv:
					last = builder.CreateMul(last, v->getOperand(1));
					break;

				default:
					assert(0 && "visitBinaryOperator died");
					break;
			}
		}
	}
}

// Handles the reversal of the code
class GlobalInverter :
	public InstVisitor<GlobalInverter> {
		IRBuilder<> builder;

		public:
		Function *func;

		GlobalInverter(Function *f)
			: builder(&getBasicBlock(func))
		{
			func = f;
		}

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
			GlobalInverter inverter(&F);
			inverter.visit(F);
			return true;
		}
		return false;
	}
};

char Hello::ID = 0;
static RegisterPass<Hello> X("hello", "Hello World Pass");
