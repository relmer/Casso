#pragma once

#include "Pch.h"

#include "FileTypes/VisiCalcSheet.h"





////////////////////////////////////////////////////////////////////////////////
//
//  VisiCalcEvaluator
//
//  A VisiCalc sheet's values, computed as the 1982 VisiCalc User's Guide
//  sets out. An expression is evaluated from left to right with no operator
//  ranking; parentheses group. An entry is evaluated once as the file is
//  read, in the file's order, and then the sheet is recalculated: one pass
//  down the columns or along the rows, each entry computed from the values
//  the other entries have at that moment. A forward or circular reference
//  therefore has the value VisiCalc draws, not a converged one.
//
////////////////////////////////////////////////////////////////////////////////

class VisiCalcEvaluator
{
public:
    using Value = VisiCalcSheet::Value;

    struct Node
    {
        enum class Kind { Invalid, Number, Cell, Range, Negate, Binary, Call };

        Kind               kind       = Kind::Invalid;
        double             number     = 0;
        size_t             column     = 0;
        size_t             row        = 0;
        size_t             lastColumn = 0;
        size_t             lastRow    = 0;
        char               op         = 0;      // + - * / ^ < > =, L for <=, G for >=, N for <>
        std::string        function;            // without the @
        std::vector<Node>  children;
    };

    static bool                Parse    (std::string_view formula, Node & outNode);
    static std::vector<Value>  Evaluate (const VisiCalcSheet::Sheet & sheet, size_t recalculations = 1);

private:
    using Grid = std::vector<Value>;

    struct MathFunction
    {
        const char  * name;
        double     (* compute) (double);
    };

    static constexpr size_t  kGridColumns = 64;
    static constexpr size_t  kGridRows    = 256;
    static constexpr int     kMaxDepth    = 48;

    static bool    ParseExpression (std::string_view text, size_t & ioAt, int depth, Node & outNode);
    static bool    ParseTerm       (std::string_view text, size_t & ioAt, int depth, Node & outNode);
    static bool    ParseNumber     (std::string_view text, size_t & ioAt, Node & outNode);
    static bool    ParseReference  (std::string_view text, size_t & ioAt, Node & outNode);
    static bool    ParseCall       (std::string_view text, size_t & ioAt, int depth, Node & outNode);
    static char    ParseOperator   (std::string_view text, size_t & ioAt);

    static size_t  GetIndex        (size_t column, size_t row)     { return row * kGridColumns + column; }
    static Value   MakeNumber      (double number);
    static Value   MakeBoolean     (bool boolean);
    static Value   MakeKind        (Value::Kind kind);
    static Value   ToScalar        (const Value & value);
    static bool    FindFault       (const std::vector<Value> & values, Value & outFault);

    static Value   Compute         (const Node & node, const Grid & grid);
    static Value   Apply           (char op, const Value & left, const Value & right);
    static Value   Call            (const Node & node, const Grid & grid);
    static Value   CallList        (const std::string & name, const std::vector<Value> & list, bool & outKnown);
    static Value   Lookup          (const Value & key, const Node & range, const Grid & grid);
    static void    Flatten         (const Node & node, size_t first, const Grid & grid, std::vector<Value> & outValues);
};
