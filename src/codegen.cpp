// #include "decl.hpp"
#include "codegen.hpp"

#include "node.hpp"
#include <llvm/IR/Value.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Type.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/GlobalValue.h>
#include <llvm/ExecutionEngine/GenericValue.h>
#include <llvm/IR/LegacyPassManager.h>
#include <llvm/ExecutionEngine/ExecutionEngine.h>
#include <llvm/IR/Operator.h>
#include <llvm/Bitcode/BitcodeWriter.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/Support/raw_os_ostream.h>

#include <iostream>
#include <fstream>
#include <vector>
#include <cassert>
#include <unordered_map>

using GlobalStringPool = std::unordered_map<std::string, llvm::Value *>;

namespace codegen {
    CodeGenContext::CodeGenContext() : module(new llvm::Module("main", llvmCtx)) {
    }

    void CodeGenContext::generateCode() {
        std::cout << "Start generating code...\n";
        std::vector<llvm::Type *> argTypes;

        builder = new llvm::IRBuilder<>(llvmCtx);

        llvm::FunctionType *ftype = llvm::FunctionType::get(llvm::Type::getVoidTy(llvmCtx),
                                                            llvm::makeArrayRef(argTypes), false);
        mainFunction = llvm::Function::Create(ftype, llvm::GlobalValue::ExternalLinkage, "main", module);
        basicBlock = llvm::BasicBlock::Create(llvmCtx, "entry", mainFunction);
        builder->SetInsertPoint(basicBlock);

        assert(astBlock);

        std::cout << "AST block size = " << astBlock->statements.size() << '\n';

        astBlock->CodeGen(*this);

        builder->CreateRetVoid();

        llvm::raw_os_ostream out(std::cout);
        module->print(out, nullptr);

        std::cout << "Code generated..\n";
    }

    llvm::GenericValue CodeGenContext::runCode() const {
        std::cout << "Running code\n";
        llvm::ExecutionEngine *ee = llvm::EngineBuilder(std::unique_ptr<llvm::Module>(module)).create();
        ee->finalizeObject();
        std::cout << "After finalize.." << std::endl;
        std::vector<llvm::GenericValue> noargs;
        llvm::GenericValue v = ee->runFunction(mainFunction, noargs);
        std::cout << "After run func.." << std::endl;
        std::cout << "Code was run.\n";
        return {};
    }

    void CodeGenContext::saveCode(bool isBinary) const {
        if (isBinary) {
            std::ofstream bit_file("lol.bc", std::ios_base::binary);
            llvm::raw_os_ostream bit_write(bit_file);
            WriteBitcodeToFile(*module, bit_write);
        } else {
            std::ofstream text_file("lol.ll", std::ios_base::out);
            llvm::raw_os_ostream text_write(text_file);
            module->print(text_write, nullptr);
        }
    }


}

namespace {
    llvm::Type *getType(AST::Identifier *id, llvm::LLVMContext &ctx) {
        if (id->type == AST::DataType::Int) {
            return llvm::Type::getInt32Ty(ctx);
        }
        if (id->type == AST::DataType::Bool) {
            return llvm::Type::getInt1Ty(ctx);
        }
        if (id->type == AST::DataType::String) {
            return llvm::Type::getInt8PtrTy(ctx);
        }
        throw std::runtime_error("[internal error] Unknown data type!");
    }
}

namespace AST {
    llvm::Value *CodeBlock::CodeGen(codegen::CodeGenContext &context) {
        std::cout << "Generating block...\n";
        llvm::Value *last = nullptr;
        int i = 0;
        for (auto &st: statements) {
            std::cout << ++i << " statement:\n";
            last = st->CodeGen(context);
        }
        return context.builder->getInt1(true);
    }

    // expressions
    llvm::Value *ConstantInt::CodeGen(codegen::CodeGenContext &context) {
        std::cout << "Generating constant i32...\n";
        return context.builder->getInt32(val);
    }

    llvm::Value *ConstantBool::CodeGen(codegen::CodeGenContext &context) {
        std::cout << "Generating constant i1...\n";
        return context.builder->getInt1(val);
    }

    llvm::Value *ConstantString::CodeGen(codegen::CodeGenContext &context) {
        std::cout << "Generating constant string...\n";

        static GlobalStringPool globs;
        auto iter = globs.find(val);
        if (iter == globs.end()) {
            iter = globs.emplace(val, context.builder->CreateGlobalString(val)).first;
        }

        return iter->second;
    }

    llvm::Value *Identifier::CodeGen(codegen::CodeGenContext &context) {
        if (context.variables.find(name) != context.variables.end()) {
            std::cout << "Extracting " << name << "...\n";
            return context.builder->CreateLoad(context.variables[name]);
        }
        std::cout << "Generating Ident with name \"" << name << "\"...\n";
        llvm::AllocaInst *alloca = context.builder->CreateAlloca(getType(this, context.llvmCtx), nullptr, name);
        context.variables[name] = alloca;
        return alloca;
    }

    // NOTE: type checking done during ast building
    llvm::Value *UnaryOp::CodeGen(codegen::CodeGenContext &context) {
        std::cout << "Generating unary op...\n";
        llvm::Value *expr_v = expr.CodeGen(context);
        switch (op) {
            case AST::UnaryOpType::Minus:
            case AST::UnaryOpType::Neg: {
                return context.builder->CreateNeg(expr_v);
            }
        }
        return nullptr;
    }

    // NOTE: type checking done during ast building
    llvm::Value *BinaryOp::CodeGen(codegen::CodeGenContext &context) {
        std::cout << "Generating binary op...\n";
        llvm::Value *lhs_v = lhs.CodeGen(context);
        llvm::Value *rhs_v = rhs.CodeGen(context);
        assert(lhs_v);
        assert(rhs_v);
        switch (op) {
            case BinaryOpType::Pow: {
                // TODO change, now is right assoc mult
                return context.builder->CreateMul(lhs_v, rhs_v);
            }
                // for ints
            case BinaryOpType::Mult: {
                return context.builder->CreateMul(lhs_v, rhs_v);
            }
            case BinaryOpType::Div: {
                return context.builder->CreateUDiv(lhs_v, rhs_v);
            }
            case BinaryOpType::Sub: {
                return context.builder->CreateSub(lhs_v, rhs_v);
            }
            case BinaryOpType::Leq: {
                return context.builder->CreateICmpSLE(lhs_v, rhs_v);
            }
            case BinaryOpType::Les: {
                return context.builder->CreateICmpSLT(lhs_v, rhs_v);
            }
            case BinaryOpType::Geq: {
                return context.builder->CreateICmpSGE(lhs_v, rhs_v);
            }
            case BinaryOpType::Gre: {
                return context.builder->CreateICmpSGT(lhs_v, rhs_v);
            }

            case BinaryOpType::Sum: {
                if (lhs.type == DataType::String) {
                    throw std::runtime_error("[internal error] Concat is not supported");
                }
                return context.builder->CreateAdd(lhs_v, rhs_v);
            }

                // all
            case BinaryOpType::Eq: {
                if (lhs.type == DataType::Int || lhs.type == DataType::Bool) {
                    return context.builder->CreateICmpEQ(lhs_v, rhs_v);
                } else {
                    auto *casted_l = context.builder->CreatePtrToInt(lhs_v, context.builder->getInt32Ty());
                    auto *casted_r = context.builder->CreatePtrToInt(rhs_v, context.builder->getInt32Ty());
                    return context.builder->CreateICmpEQ(casted_l, casted_r);
                }
            }

            case BinaryOpType::Neq: {
                if (lhs.type == DataType::Int || lhs.type == DataType::Bool) {
                    return context.builder->CreateICmpNE(lhs_v, rhs_v);
                } else {
                    auto *casted_l = context.builder->CreatePtrToInt(lhs_v, context.builder->getInt32Ty());
                    auto *casted_r = context.builder->CreatePtrToInt(rhs_v, context.builder->getInt32Ty());
                    return context.builder->CreateICmpNE(casted_l, casted_r);
                }
            }
                // bool
            case BinaryOpType::And: {
                return context.builder->CreateAnd(lhs_v, rhs_v);
            }
            case BinaryOpType::Or: {
                return context.builder->CreateOr(lhs_v, rhs_v);
            }
        }
        throw std::runtime_error("Unknown bin op!");
    }

    // statements
    llvm::Value *Skip::CodeGen(codegen::CodeGenContext &context) {
        return context.builder->getInt1(true);
    }

    llvm::Value *VarDecl::CodeGen(codegen::CodeGenContext &context) {
        std::cout << "Generating declaration for " << ident->name << "...\n";
        ident->CodeGen(context);

        VarAssign va(ident, expr);
        return va.CodeGen(context);
    }

    llvm::Value *VarAssign::CodeGen(codegen::CodeGenContext &context) {
        std::cout << "Generating assignment for " << ident->name << "...\n";
        auto iter = context.variables.find(ident->name);
        if (iter == context.variables.end()) {
            throw std::runtime_error("[internal error] Variable is not in scope");
        }
        llvm::Value *var = iter->second;
        llvm::Value *expr_val = expr.CodeGen(context);

        return context.builder->CreateStore(expr_val, var, false);
    }

    llvm::Value *IfStatement::CodeGen(codegen::CodeGenContext &context) {
        llvm::Value *cond_v = expr.CodeGen(context);
        cond_v = context.builder->CreateICmpEQ(
                cond_v, context.builder->getInt1(true), "ifcond");

        llvm::BasicBlock *thenBB = llvm::BasicBlock::Create(context.llvmCtx, "then", context.mainFunction);
        llvm::BasicBlock *elseBB = llvm::BasicBlock::Create(context.llvmCtx, "else");
        llvm::BasicBlock *mergeBB = llvm::BasicBlock::Create(context.llvmCtx, "merge");

        context.builder->CreateCondBr(cond_v, thenBB, elseBB);

        context.builder->SetInsertPoint(thenBB);
        llvm::Value *thenGen = on_if.CodeGen(context);

        context.builder->CreateBr(mergeBB);
        thenBB = context.builder->GetInsertBlock();

        context.mainFunction->getBasicBlockList().push_back(elseBB);
        context.builder->SetInsertPoint(elseBB);

        llvm::Value *elseGen = nullptr;

        if (on_else) {
            elseGen = on_else->CodeGen(context);
        } else {
            elseGen = AST::Skip{}.CodeGen(context);
        }
        std::cout << "B4 assert: " << intptr_t(elseGen) << std::endl;
        assert(elseGen);
        context.builder->CreateBr(mergeBB);
        elseBB = context.builder->GetInsertBlock();

        context.mainFunction->getBasicBlockList().push_back(mergeBB);
        context.builder->SetInsertPoint(mergeBB);
        llvm::PHINode *PN = context.builder->CreatePHI(context.builder->getInt1Ty(), 2, "iftmp");

        PN->addIncoming(thenGen, thenBB);
        PN->addIncoming(elseGen, elseBB);
        return PN;
    }

    llvm::Value *WhileLoop::CodeGen(codegen::CodeGenContext &context) {
        llvm::BasicBlock *loopBB = llvm::BasicBlock::Create(context.llvmCtx, "loop", context.mainFunction);
        context.builder->CreateBr(loopBB);
        context.builder->SetInsertPoint(loopBB);

        auto *body_v = code_block.CodeGen(context);
        assert(body_v);

        auto *end_cond_v = expr.CodeGen(context);
        assert(end_cond_v);

        end_cond_v = context.builder->CreateICmpNE(
                end_cond_v, context.builder->getInt1(false), "loopcond");

        assert(end_cond_v);

        llvm::BasicBlock *afterBB =
                llvm::BasicBlock::Create(context.llvmCtx, "afterloop", context.mainFunction);

        context.builder->CreateCondBr(end_cond_v, loopBB, afterBB);

        context.builder->SetInsertPoint(afterBB);
        return Skip{}.CodeGen(context);
    }

}
