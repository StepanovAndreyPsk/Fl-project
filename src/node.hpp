/*
    A definition of AST node is here
*/
#pragma once

#include <llvm/IR/Value.h>

#include <vector>
#include <string>
#include <iostream>

namespace AST
{
    // None must be unreachable
    enum class DataType
    {
        None,
        String,
        Int,
        Bool
    };

    enum class UnaryOpType
    {
        Minus,
        Neg
    };

    enum class BinaryOpType
    {
        Pow,
        Mult,
        Div,
        Sum,
        Sub,
        Leq,
        Les,
        Geq,
        Gre,
        Eq,
        Neq,
        And,
        Or
    };

    std::string ShowType(DataType type)
    {
        switch (type)
        {
        case DataType::None:
            return "None";
            break;
        case DataType::String:
            return "String";
            break;
        case DataType::Int:
            return "Int";
            break;
        case DataType::Bool:
            return "Bool";
            break;
        default:
            llvm_unreachable("ShowType: unknown DataType!");
            return "Wtf?\n";
            break;
        }
    }

    struct CodeGenContext;

    struct Node
    {
        virtual ~Node() {}
        virtual llvm::Value *CodeGen(CodeGenContext &context)
        {
            return nullptr;
        }
    };

    struct Expression : Node
    {
        DataType type;
        virtual llvm::Value *CodeGen(CodeGenContext &context);
    };

    bool HasType(const Expression &e, DataType type)
    {
        return e.type == type;
    }

    struct Statement : Node
    {
    };

    template <class LiteralType>
    struct Constant : Expression
    {
        LiteralType val;
        Constant(LiteralType val_) : val(val_)
        {
        }
        virtual llvm::Value *CodeGen(CodeGenContext &context);
    };

    // Gonna find value in context
    struct NIdentifier : public Expression
    {
        std::string name;
        NIdentifier(std::string name_) : name(std::move(name_)) {}
        virtual llvm::Value *CodeGen(CodeGenContext &context);
    };

    struct UnaryOp : public Expression
    {
        UnaryOpType op;
        Expression &expr;
        UnaryOp(UnaryOpType op_, Expression &expr_) : op(op_), expr(expr_)
        {
            // checking for type matching
            switch (op)
            {
            case UnaryOpType::Minus:
                if (!HasType(expr, DataType::Int))
                {
                    std::cerr << "UnaryOp Minus, type is " << ShowType(expr.type);
                    // throw ??
                }
                break;
            }
        }
        virtual llvm::Value *CodeGen(CodeGenContext &context);
    };

    struct BinaryOp : public Expression
    {
        BinaryOpType op;
        Expression &lhs;
        Expression &rhs;
        BinaryOp(Expression &lhs_, BinaryOpType op_, Expression &rhs_) : op(op_), lhs(lhs_), rhs(rhs_) {
            // TODO add check
        }
        virtual llvm::Value *CodeGen(CodeGenContext &context);
    };
}
