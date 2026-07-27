#pragma once

#include "AssemblerTypes.h"





////////////////////////////////////////////////////////////////////////////////
//
//  ExprContext
//
////////////////////////////////////////////////////////////////////////////////

struct ExprContext
{
    const std::unordered_map<std::string, int32_t> * symbols;
    int32_t                                          currentPC;
};





////////////////////////////////////////////////////////////////////////////////
//
//  ExprResult
//
////////////////////////////////////////////////////////////////////////////////

struct ExprResult
{
    bool        success;
    int32_t     value;
    std::string error;
    bool        hasUnresolved;   // failed due to undefined symbol
};





////////////////////////////////////////////////////////////////////////////////
//
//  ExpressionEvaluator
//
////////////////////////////////////////////////////////////////////////////////

class ExpressionEvaluator
{
public:
    static ExprResult Evaluate (const std::string & expr, const ExprContext & ctx);

private:
    // Token kind. A nested enumeration cannot be defined out-of-line, so
    // unlike Token and Tokenizer below this one has to live here in full.
    enum class TokType
    {
        Number, Ident,
        Plus, Minus, Star, Slash, Percent,
        Amp, Pipe, Caret, Tilde, Bang,
        AmpAmp, PipePipe,
        LShift, RShift,
        Lt, Le, Gt, Ge, Eq, Ne,
        LParen, RParen, LBracket, RBracket,
        PlusPlus, MinusMinus,
        End, Error
    };

    // Declared here, defined in the .cpp -- the tokenizer is ~370 lines of
    // pure implementation detail and has no business in a header.
    struct Token;
    class  Tokenizer;

    // Recursive-descent ladder, loosest binding first.
    static bool         ParseLogOr      (Tokenizer & tok, const ExprContext & ctx, int32_t & result, std::string & error);
    static bool         ParseLogAnd     (Tokenizer & tok, const ExprContext & ctx, int32_t & result, std::string & error);
    static bool         ParseBitOr      (Tokenizer & tok, const ExprContext & ctx, int32_t & result, std::string & error);
    static bool         ParseBitXor     (Tokenizer & tok, const ExprContext & ctx, int32_t & result, std::string & error);
    static bool         ParseBitAnd     (Tokenizer & tok, const ExprContext & ctx, int32_t & result, std::string & error);
    static bool         ParseEquality   (Tokenizer & tok, const ExprContext & ctx, int32_t & result, std::string & error);
    static bool         ParseComparison (Tokenizer & tok, const ExprContext & ctx, int32_t & result, std::string & error);
    static bool         ParseShift      (Tokenizer & tok, const ExprContext & ctx, int32_t & result, std::string & error);
    static bool         ParseAddSub     (Tokenizer & tok, const ExprContext & ctx, int32_t & result, std::string & error);
    static bool         ParseMulDiv     (Tokenizer & tok, const ExprContext & ctx, int32_t & result, std::string & error);
    static bool         ParseUnary      (Tokenizer & tok, const ExprContext & ctx, int32_t & result, std::string & error);
    static bool         ParsePrimary    (Tokenizer & tok, const ExprContext & ctx, int32_t & result, std::string & error);

    static std::string  ToUpperIdent    (const std::string & s);
};
