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

    // Defaulted, so every caller that predates dialect selection keeps the
    // precedence it has always had and only a dialect that says otherwise
    // changes.
    OperatorBinding                                  binding = OperatorBinding::ByPrecedence;
};





////////////////////////////////////////////////////////////////////////////////
//
//  ExprResult
//
////////////////////////////////////////////////////////////////////////////////

// Members carry defaults so a plain `ExprResult er;` is a well-defined
// "failed, no value, no reason" rather than garbage. Callers that declare the
// result up front and fill it in a branch (the single-exit shape) would
// otherwise leave it uninitialized on the path that never evaluates, which
// C26495 flags -- and which would read as a spurious success if the bool
// happened to land non-zero.
struct ExprResult
{
    bool        success       = false;
    int32_t     value         = 0;
    std::string error;
    bool        hasUnresolved = false;   // failed due to undefined symbol
};





////////////////////////////////////////////////////////////////////////////////
//
//  ExpressionEvaluator
//
//  Evaluates an assembler expression against a symbol table and a program
//  counter.
//
//  A pure STATIC entry point taking its context by parameter, so evaluation
//  carries no state between calls and can be tested by passing a string and a
//  symbol map.
//
//  Precedence is expressed as a TABLE -- one row per binary operator, naming
//  its token, its binding level, and how it folds its operands. That replaced
//  ten hand-written parse levels that were identical apart from their token
//  set and which function they called next. Precedence used to live only in
//  that call order, so it could not be read without tracing all ten; now it is
//  a column.
//
//  The tokenizer is declared here and defined in the .cpp because it is a few
//  hundred lines of pure implementation detail with no business in a header.
//  TokType cannot follow it: a nested enumeration has to be complete where the
//  class is defined.
//
//  An unresolved symbol is reported distinctly from an expression error, since
//  the caller needs to tell a legitimate forward reference in pass 1 from a
//  genuine mistake.
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

    static bool  TryApplyLogOr  (int32_t a, int32_t b, int32_t & o, std::string & e);
    static bool  TryApplyLogAnd (int32_t a, int32_t b, int32_t & o, std::string & e);
    static bool  TryApplyBitOr  (int32_t a, int32_t b, int32_t & o, std::string & e);
    static bool  TryApplyBitXor (int32_t a, int32_t b, int32_t & o, std::string & e);
    static bool  TryApplyBitAnd (int32_t a, int32_t b, int32_t & o, std::string & e);
    static bool  TryApplyEq     (int32_t a, int32_t b, int32_t & o, std::string & e);
    static bool  TryApplyNe     (int32_t a, int32_t b, int32_t & o, std::string & e);
    static bool  TryApplyLt     (int32_t a, int32_t b, int32_t & o, std::string & e);
    static bool  TryApplyGt     (int32_t a, int32_t b, int32_t & o, std::string & e);
    static bool  TryApplyLe     (int32_t a, int32_t b, int32_t & o, std::string & e);
    static bool  TryApplyGe     (int32_t a, int32_t b, int32_t & o, std::string & e);
    static bool  TryApplyShl    (int32_t a, int32_t b, int32_t & o, std::string & e);
    static bool  TryApplyShr    (int32_t a, int32_t b, int32_t & o, std::string & e);
    static bool  TryApplyAdd    (int32_t a, int32_t b, int32_t & o, std::string & e);
    static bool  TryApplySub    (int32_t a, int32_t b, int32_t & o, std::string & e);
    static bool  TryApplyMul    (int32_t a, int32_t b, int32_t & o, std::string & e);
    static bool  TryApplyDiv    (int32_t a, int32_t b, int32_t & o, std::string & e);
    static bool  TryApplyMod    (int32_t a, int32_t b, int32_t & o, std::string & e);

    static const BinaryOp *  FindBinaryOp (TokType token);

    // Punctuation tables, consulted in this order so the lexer is
    // longest-match: "<=" must not read as "<" then "=".
    struct Digraph
    {
        char     lead;
        char     follow;
        TokType  type;
    };

    struct Monograph
    {
        char     lead;
        TokType  type;
    };

    static const Digraph    s_kDigraphs[11];
    static const Monograph  s_kMonographs[16];

    static const Digraph *    FindDigraph   (char lead, char follow);
    static const Monograph *  FindMonograph (char lead);

    // Prefix operators, same shape as BinaryOp. None of them can fail --
    // there is no unary equivalent of divide-by-zero -- so unlike
    // BinaryOp::Apply there is no error out-parameter and no bool.
    struct UnaryOp
    {
        TokType    token;
        int32_t (* Apply) (int32_t value);
    };

    static const UnaryOp  s_kUnaryOps[8];

    static int32_t  ApplyNegate    (int32_t v);
    static int32_t  ApplyIdentity  (int32_t v);
    static int32_t  ApplyBitNot    (int32_t v);
    static int32_t  ApplyLogNot    (int32_t v);
    static int32_t  ApplyIncrement (int32_t v);
    static int32_t  ApplyDecrement (int32_t v);
    static int32_t  ApplyLowByte   (int32_t v);
    static int32_t  ApplyHighByte  (int32_t v);

    static const UnaryOp *  FindUnaryOp (TokType token);

    // Precedence climbing. `minLevel` is the loosest operator this call is
    // allowed to absorb; recursing at level+1 makes every operator here
    // left-associative, which is what the old ladder's loops did.
    static bool         TryParseBinary     (Tokenizer & tok, const ExprContext & ctx, int minLevel, int32_t & result, std::string & error);
    static bool         TryParseUnary      (Tokenizer & tok, const ExprContext & ctx, int32_t & result, std::string & error);
    static bool         TryParsePrimary    (Tokenizer & tok, const ExprContext & ctx, int32_t & result, std::string & error);

    static std::string  ToUpperIdent    (const std::string & s);
};
