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
#include <map>
#include <set>
#include <stack>
#include <vector>

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
	// emit dependencies on instruction
	void handleDeps(IRBuilder<> &builder, std::map<Value *, std::vector<Value *> > &deps, Instruction &I) {
		Value *v = dyn_cast<Value>(I);
		for(User::op_iterator i = I->op_begin(), e = I->op_end();  i != e; ++i) {
			deps[v].push_back(*i);
		}
	}
}

// Handles the reversal of the code
class GlobalInverter :
	public InstVisitor<GlobalInverter> {
		IRBuilder<> builder;
		std::map<Value *, std::vector<Value *> > bucket;
		std::map<Value *, Value *> oldToNew;

		public:
		Function *f;   // orig func
		Function *inf; // inverted func
		Function::arg_iterator fi, fe, gi, ge;

		// lookup for old to new operands
		Value *lookup(Value *k) {
			if (isa<Constant>(k)) return k;
			std::map<Value *, Value *>::iterator it;

			it = oldToNew.find(k);
			if (it == oldToNew.end()) return 0;

			return it->second;
		}


		GlobalInverter(Function *func, Function *infunc, BasicBlock *entry)
			: builder(entry) {
			f = func;
			inf = infunc;
			for (fi = f->arg_begin(), fe = f->arg_end(), gi = inf->arg_begin(),
					ge = inf->arg_end(); fi != fe; ++fi, ++gi) {
				oldToNew[dyn_cast<Value>(fi)] = dyn_cast<Value>(gi);
			}
		}

		void visitStoreInst(StoreInst &SI) {
			handleDeps(builder, bucket, SI);
		}

		void topoify(Value *it, std::map<std::vector<Value *> > deps, std::set<Value *> ready) {
			for(auto const &dep : deps) {
				for(int j = 0; j < dep.second.size(); ++j) {
					if(it == dep.second[j]) return;
				}
			}
			ready.insert(it);
		}

		std::vector<Value *> topoSort(std::map<Value *, std::vector<Value *> > bucket) {
			std::vector<Value *> ret;
			std::set<Value *> ready;
			// populate ready set with nodes with no incoming edges
			for(int i = 0; i < bucket.size(); ++i) {
				topoify(bucket[i], bucket, &ready);
			}
			return ret;
		}

		void finalize() {
			topoSort(bucket);
			builder.CreateRetVoid();
		}
	};

// Driver, runs the reverser pass, and then constructs the inverse function
struct Hello : public FunctionPass {
	static char ID;
	Hello() : FunctionPass(ID) {}

	bool runOnFunction(Function &F) override {
		if (F.getName() == "foo") {
			auto mod = F.getEntryBlock().getModule();
			auto foo_inverse = cast<Function>(
					mod->getOrInsertFunction("foo_inverse", Type::getVoidTy(F.getContext()), NULL));
			auto entry = BasicBlock::Create(foo_inverse->getContext(), "entry", foo_inverse);
			GlobalInverter inverter(&F, foo_inverse, entry);
			inverter.visit(F);
			inverter.finalize();
			return true;
		}
		return false;
	}
};

char Hello::ID = 0;
static RegisterPass<Hello> X("hello", "Hello World Pass");
