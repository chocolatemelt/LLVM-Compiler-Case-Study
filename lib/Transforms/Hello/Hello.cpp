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
#include <functional>
#include <set>
#include <stack>

#include "llvm/ADT/Statistic.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/InstVisitor.h"
#include "llvm/IR/Instruction.h"
#include "llvm/Pass.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Transforms/Utils/BasicBlockUtils.h"

#define DEBUG_TYPE "hello"

using namespace llvm;

namespace {
	template<typename T> T safe_pop(std::stack<T> &s) {
		auto tmp = s.top();
		s.pop();
		return tmp;
	}

	/* handle globals */
	int getGlobalValue(GlobalVariable *GV) {
		return *GV->getInitializer()->getUniqueInteger().getRawData();
	}

	int getGlobalValue(LoadInst *LI, GlobalVariable *GV) {
		static std::map<LoadInst*, int> loader;
		if(loader.find(LI) == loader.end()) loader[LI] = getGlobalValue(GV);
		return loader[LI];
	}

	void setGlobalValue(GlobalVariable *GV, int val) {
		auto origConstant = dyn_cast<ConstantInt>(GV->getInitializer());
		assert(origConstant != nullptr && "Global Variable wasn't an integer");
		auto constant = ConstantInt::get(origConstant->getType(), val);
		GV->setInitializer(constant);
	}

	/* create the function's inverse */
	Function *constructInverseFunction(
			Function &F,
			LLVMContext &C,
			std::stack<Expr *> &inverse,
			std::stack<GlobalVariable *> &globals) {
		// no args, foo_inverse
		auto mod = F.getEntryBlock().getModule();
		auto foo_inverse = cast<Function>(
				mod->getOrInsertFunction("foo_inverse", Type::getVoidTy(C), NULL));
		auto entry = BasicBlock::Create(foo_inverse->getContext(), "entry", foo_inverse);
		IRBuilder<> builder(entry);

		// build the IR in reverse order
		while (!inverse.empty()) {
			auto expr = safe_pop(inverse);
			auto glob = safe_pop(globals);
			ExprCodeGenVisitor CV(builder, foo_inverse->getContext());
			expr->accept(&CV);
			auto store = builder.CreateStore(CV.get_value(), glob);
			DEBUG(errs() << *store << "\n");
		}
		auto ret = builder.CreateRetVoid();
		DEBUG(errs() << *ret << "\n");

		// replace foo's uses by foo_inverse
		auto foo = cast<Function>(mod->getOrInsertFunction("foo", Type::getVoidTy(C), NULL));
		foo->replaceAllUsesWith(foo_inverse);

		return foo_inverse;
	}

	// Handles the reversal of the code
	struct GlobalReverser :
		public InstVisitor<GlobalReverser> {

			std::stack<Expr *> inverse;
			std::stack<GlobalVariable *> globalStores;

			void visitStoreInst(StoreInst &SI) {
				auto Op = SI.getPointerOperand();
				if (GlobalVariable *GV = dyn_cast<GlobalVariable>(Op)) {
					// Construct the Expression hierarchy and evaluate
					Expr *expr = constructExpr(SI.getOperand(0));
					ExprEvalVisitor EV;
					expr->accept(&EV);
					setGlobalValue(GV, EV.get_evaluation());
					errs() << "... Evaluated " << GV->getName()
						<< "=" << getGlobalValue(GV) << "\n";
					// Reverse the operations and place it onto a stack to be
					// transformed by the IRBuilder later.
					GetBinaryOps GBO;
					expr->accept(&GBO);
					ExprReverseVisitor RV(std::move(GBO.s));
					expr->accept(&RV);
					inverse.push(RV.get_inv());
					globalStores.push(GV);
				}
			}
		};

	// Driver, runs the reverser pass, and then constructs the inverse function
	struct Hello : public FunctionPass {
		static char ID;
		Hello() : FunctionPass(ID) {}

		bool runOnFunction(Function &F) override {
			if (F.getName() == "foo") {
				errs() << "[+] Constructing expression tree\n";
				GlobalReverser bodg;
				bodg.visit(F);
				errs() << "[+] Constructing function inverse\n";
				auto inverse = constructInverseFunction(F, F.getContext(),
						bodg.inverse, bodg.globalStores);
				errs() << "[+] Inverted function [" << F.getName()
					<< "] to [" << inverse->getName() << "]\n";
				return true;
			}
			return false;
		}
	};
}

char Hello::ID = 0;
static RegisterPass<Hello> X("hello", "Hello World Pass");
