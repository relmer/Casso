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
    // One row per binary operator: the token that introduces it, the level it
    // binds at (higher binds tighter), and how it folds its two operands.
    //
    // This replaces ten hand-written ParseX levels -- ParseLogOr down through
    // ParseMulDiv -- that were identical apart from their token set and which
    // function they called for the next level up. Precedence used to live only
    // in that call order, so it could not be read without tracing all ten.
    struct BinaryOp
    {
        TokType  token;
        int      level;
        // `error` is set (and false returned) only by the operators that can
        // fail: divide and modulo, on a zero divisor.
        bool  (* Apply) (int32_t lhs, int32_t rhs, int32_t & out, std::string & error);
    };

    // The loosest-binding level in the table; where a whole expression starts.
    static constexpr int  s_kLoosestBinaryLevel = 1;

    static const BinaryOp   s_kBinaryOps[18];

    static bool  ApplyLogOr  (int32_t a, int32_t b, int32_t & o, std::string & e);
    static bool  ApplyLogAnd (int32_t a, int32_t b, int32_t & o, std::string & e);
    static bool  ApplyBitOr  (int32_t a, int32_t b, int32_t & o, std::string & e);
    static bool  ApplyBitXor (int32_t a, int32_t b, int32_t & o, std::string & e);
    static bool  ApplyBitAnd (int32_t a, int32_t b, int32_t & o, std::string & e);
    static bool  ApplyEq     (int32_t a, int32_t b, int32_t & o, std::string & e);
    static bool  ApplyNe     (int32_t a, int32_t b, int32_t & o, std::string & e);
    static bool  ApplyLt     (int32_t a, int32_t b, int32_t & o, std::string & e);
    static bool  ApplyGt     (int32_t a, int32_t b, int32_t & o, std::string & e);
    static bool  ApplyLe     (int32_t a, int32_t b, int32_t & o, std::string & e);
    static bool  ApplyGe     (int32_t a, int32_t b, int32_t & o, std::string & e);
    static bool  ApplyShl    (int32_t a, int32_t b, int32_t & o, std::string & e);
    static bool  ApplyShr    (int32_t a, int32_t b, int32_t & o, std::string & e);
    static bool  ApplyAdd    (int32_t a, int32_t b, int32_t & o, std::string & e);
    static bool  ApplySub    (int32_t a, int32_t b, int32_t & o, std::string & e);
    static bool  ApplyMul    (int32_t a, int32_t b, int32_t & o, std::string & e);
    static bool  ApplyDiv    (int32_t a, int32_t b, int32_t & o, std::string & e);
    static bool  ApplyMod    (int32_t a, int32_t b, int32_t & o, std::string & e);

    static const BinaryOp *  FindBinaryOp (TokType token);

    // Precedence climbing. `minLevel` is the loosest operator this call is
    // allowed to absorb; recursing at level+1 makes every operator here
    // left-associative, which is what the old ladder's loops did.
    static bool         ParseBinary     (Tokenizer & tok, const ExprContext & ctx, int minLevel, int32_t & result, std::string & error);
    static bool         ParseUnary      (Tokenizer & tok, const ExprContext & ctx, int32_t & result, std::string & error);
    static bool         ParsePrimary    (Tokenizer & tok, const ExprContext & ctx, int32_t & result, std::string & error);

    static std::string  ToUpperIdent    (const std::string & s);
};
