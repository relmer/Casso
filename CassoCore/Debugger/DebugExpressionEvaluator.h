#pragma once

#include "Debugger/IDebugExpressionContext.h"





////////////////////////////////////////////////////////////////////////////////
//
//  ExpressionOperator
//
////////////////////////////////////////////////////////////////////////////////

enum class ExpressionOperator
{
    Add,
    Subtract,
    Multiply,
    Divide,
    Modulo,
    And,
    Or,
    Xor,
    Less,
    Greater,
    LessOrEqual,
    GreaterOrEqual,
    Equal,
    NotEqual,
    LogicalAnd,
    LogicalOr,
    Negate,
    Identity,
    LogicalNot,
    Complement,
    LowByte,
    HighByte,
    Dereference,
    OpenParen,
};





////////////////////////////////////////////////////////////////////////////////
//
//  ExpressionToken
//
////////////////////////////////////////////////////////////////////////////////

struct ExpressionToken
{
    enum class Kind
    {
        Number,
        Register,
        Symbol,
        Operator,
    };

    Kind                kind  = Kind::Number;
    int32_t             value = 0;
    std::string         name;
    ExpressionOperator  op    = ExpressionOperator::Add;
};





////////////////////////////////////////////////////////////////////////////////
//
//  Expression
//
//  A parsed expression in postfix order. Registers, memory and symbols are
//  read when it is evaluated, so a breakpoint condition parsed once sees the
//  machine as it is at each evaluation.
//
////////////////////////////////////////////////////////////////////////////////

struct Expression
{
    std::string                   text;
    std::vector<ExpressionToken>  postfix;
};





////////////////////////////////////////////////////////////////////////////////
//
//  EvaluationMode
//
//  Check evaluates an expression to find what it reads, not for its value: a
//  division by zero gives 0 there, since the divisor comes from a machine that
//  will have changed by the time the expression is used.
//
////////////////////////////////////////////////////////////////////////////////

enum class EvaluationMode
{
    Evaluate,
    Check,
};





////////////////////////////////////////////////////////////////////////////////
//
//  DebugExpressionEvaluator
//
////////////////////////////////////////////////////////////////////////////////

class DebugExpressionEvaluator
{
public:
    static HRESULT  Parse            (const std::string             & text,
                                      Expression                    & expression,
                                      std::string                   & error,
                                      NumberSyntax                    syntax = NumberSyntax::AppleWin);
    static HRESULT  Evaluate         (const Expression              & expression,
                                      const IDebugExpressionContext & context,
                                      int32_t                       & value,
                                      std::string                   & error,
                                      EvaluationMode                  mode = EvaluationMode::Evaluate);
    static HRESULT  ParseAndEvaluate (const std::string             & text,
                                      const IDebugExpressionContext & context,
                                      int32_t                       & value,
                                      std::string                   & error);

private:
    struct OperatorSpelling
    {
        const char          * spelling;
        ExpressionOperator    op;
        int                   precedence;
    };

    using OperatorStack = std::vector<ExpressionOperator>;

    static const OperatorSpelling  s_kBinaryOperators[18];
    static const OperatorSpelling  s_kUnaryOperators[7];

    static bool  TryParseOperandPosition  (const std::string & text, size_t & pos, NumberSyntax syntax, bool & expectOperand, OperatorStack & stack, Expression & expression, std::string & error);
    static bool  TryParseOperatorPosition (const std::string & text, size_t & pos, bool & expectOperand, OperatorStack & stack, Expression & expression, std::string & error);
    static bool  TryReadOperand           (const std::string & text, size_t & pos, NumberSyntax syntax, ExpressionToken & token, std::string & error);
    static bool  TryReadOperator          (const std::string & text, size_t & pos, std::span<const OperatorSpelling> table, ExpressionOperator & op);
    static bool  TryParseNumber           (const std::string & digits, int base, int32_t & value);
    static bool  TryReadWinDbgRegister    (const std::string & text, size_t & pos, ExpressionToken & token, std::string & error);
    static bool  IsRegisterName           (const std::string & upperName);
    static bool  IsUnary                  (ExpressionOperator op);
    static int   GetPrecedence            (ExpressionOperator op);
    static void  EmitOperator             (ExpressionOperator op, Expression & expression);
    static bool  TryApplyToken            (const ExpressionToken & token, const IDebugExpressionContext & context, EvaluationMode mode, std::vector<int32_t> & values, std::string & error);
    static bool  TryResolveOperand        (const ExpressionToken & token, const IDebugExpressionContext & context, int32_t & value, std::string & error);
    static bool  TryApplyUnary            (ExpressionOperator op, int32_t operand, const IDebugExpressionContext & context, int32_t & result, std::string & error);
    static bool  TryApplyBinary           (ExpressionOperator op, int32_t lhs, int32_t rhs, EvaluationMode mode, int32_t & result, std::string & error);
};
