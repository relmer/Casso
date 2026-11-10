#include "Pch.h"

#include "FileTypes/VisiCalcEvaluator.h"





////////////////////////////////////////////////////////////////////////////////
//
//  VisiCalcEvaluator::Parse
//
//  The whole of the text has to be one expression.
//
////////////////////////////////////////////////////////////////////////////////

bool VisiCalcEvaluator::Parse (std::string_view formula, Node & outNode)
{
    size_t  at = 0;
    bool    ok = ParseExpression (formula, at, 0, outNode) && at == formula.size();



    if (!ok)
    {
        outNode = Node();
    }

    return ok;
}





////////////////////////////////////////////////////////////////////////////////
//
//  VisiCalcEvaluator::ParseOperator
//
//  Returns 0 where the text has no operator.
//
////////////////////////////////////////////////////////////////////////////////

char VisiCalcEvaluator::ParseOperator (std::string_view text, size_t & ioAt)
{
    char  first  = ioAt < text.size()     ? text[ioAt]     : 0;
    char  second = ioAt + 1 < text.size() ? text[ioAt + 1] : 0;
    char  op     = 0;



    if (first == '<' && second == '=')
    {
        op = 'L';
    }
    else if (first == '>' && second == '=')
    {
        op = 'G';
    }
    else if (first == '<' && second == '>')
    {
        op = 'N';
    }
    else if (first != 0 && std::string_view ("+-*/^<>=").find (first) != std::string_view::npos)
    {
        op = first;
    }

    if (op != 0)
    {
        ioAt += (op == 'L' || op == 'G' || op == 'N') ? 2 : 1;
    }

    return op;
}





////////////////////////////////////////////////////////////////////////////////
//
//  VisiCalcEvaluator::ParseExpression
//
//  Terms joined by operators, each operator applied to everything on its
//  left.
//
////////////////////////////////////////////////////////////////////////////////

bool VisiCalcEvaluator::ParseExpression (std::string_view text, size_t & ioAt, int depth, Node & outNode)
{
    Node  left;
    char  op = 0;



    if (depth > kMaxDepth || !ParseTerm (text, ioAt, depth, left))
    {
        return false;
    }

    while ((op = ParseOperator (text, ioAt)) != 0)
    {
        Node  right;
        Node  joined;

        if (!ParseTerm (text, ioAt, depth, right))
        {
            return false;
        }

        joined.kind = Node::Kind::Binary;
        joined.op   = op;
        joined.children.push_back (std::move (left));
        joined.children.push_back (std::move (right));

        left = std::move (joined);
    }

    outNode = std::move (left);

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  VisiCalcEvaluator::ParseTerm
//
//  A number, a coordinate, a function or an expression in parentheses,
//  after any signs.
//
////////////////////////////////////////////////////////////////////////////////

bool VisiCalcEvaluator::ParseTerm (std::string_view text, size_t & ioAt, int depth, Node & outNode)
{
    bool  negative = false;
    bool  ok       = false;
    char  first    = 0;



    while (ioAt < text.size() && (text[ioAt] == '+' || text[ioAt] == '-'))
    {
        negative = negative != (text[ioAt] == '-');
        ioAt++;
    }

    first = ioAt < text.size() ? text[ioAt] : 0;

    if (first == '(')
    {
        ioAt++;
        ok = ParseExpression (text, ioAt, depth + 1, outNode) && ioAt < text.size() && text[ioAt] == ')';

        if (ok)
        {
            ioAt++;
        }
    }
    else if (first == '@')
    {
        ok = ParseCall (text, ioAt, depth + 1, outNode);
    }
    else if ((first >= '0' && first <= '9') || first == '.')
    {
        ok = ParseNumber (text, ioAt, outNode);
    }
    else
    {
        ok = ParseReference (text, ioAt, outNode);
    }

    if (ok && negative)
    {
        Node  negated;

        negated.kind = Node::Kind::Negate;
        negated.children.push_back (std::move (outNode));

        outNode = std::move (negated);
    }

    return ok;
}





////////////////////////////////////////////////////////////////////////////////
//
//  VisiCalcEvaluator::ParseNumber
//
//  Digits with an optional point, then an optional E and exponent.
//
////////////////////////////////////////////////////////////////////////////////

bool VisiCalcEvaluator::ParseNumber (std::string_view text, size_t & ioAt, Node & outNode)
{
    size_t                  end      = ioAt;
    size_t                  digits   = 0;
    size_t                  exponent = 0;
    bool                    point    = false;
    std::from_chars_result  result   = {};



    while (end < text.size() && ((text[end] >= '0' && text[end] <= '9') || (text[end] == '.' && !point)))
    {
        point   = point || text[end] == '.';
        digits += text[end] != '.';
        end++;
    }

    if (digits == 0)
    {
        return false;
    }

    if (end < text.size() && text[end] == 'E')
    {
        exponent = end + 1;

        if (exponent < text.size() && (text[exponent] == '+' || text[exponent] == '-'))
        {
            exponent++;
        }

        if (exponent < text.size() && text[exponent] >= '0' && text[exponent] <= '9')
        {
            while (exponent < text.size() && text[exponent] >= '0' && text[exponent] <= '9')
            {
                exponent++;
            }

            end = exponent;
        }
    }

    //  from_chars fails on a plus sign after the E, so the text is copied
    //  without one.
    {
        std::string  number (text.substr (ioAt, end - ioAt));
        size_t       plus = number.find ('+');

        if (plus != std::string::npos)
        {
            number.erase (plus, 1);
        }

        result = std::from_chars (number.data(), number.data() + number.size(), outNode.number);
    }

    if (result.ec != std::errc())
    {
        return false;
    }

    outNode.kind = Node::Kind::Number;
    ioAt         = end;

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  VisiCalcEvaluator::ParseReference
//
//  A coordinate, or two joined by dots for a range.
//
////////////////////////////////////////////////////////////////////////////////

bool VisiCalcEvaluator::ParseReference (std::string_view text, size_t & ioAt, Node & outNode)
{
    size_t  length = 0;
    size_t  dots   = ioAt;



    if (!VisiCalcSheet::ParseCoordinate (text.substr (ioAt), outNode.column, outNode.row, length))
    {
        return false;
    }

    outNode.kind  = Node::Kind::Cell;
    ioAt         += length;
    dots          = ioAt;

    while (dots < text.size() && text[dots] == '.')
    {
        dots++;
    }

    if (dots == ioAt)
    {
        return true;
    }

    if (!VisiCalcSheet::ParseCoordinate (text.substr (dots), outNode.lastColumn, outNode.lastRow, length))
    {
        return false;
    }

    if (outNode.lastColumn < outNode.column)
    {
        std::swap (outNode.column, outNode.lastColumn);
    }

    if (outNode.lastRow < outNode.row)
    {
        std::swap (outNode.row, outNode.lastRow);
    }

    outNode.kind = Node::Kind::Range;
    ioAt         = dots + length;

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  VisiCalcEvaluator::ParseCall
//
//  An @, the function's letters and digits, and its arguments in
//  parentheses where it takes any.
//
////////////////////////////////////////////////////////////////////////////////

bool VisiCalcEvaluator::ParseCall (std::string_view text, size_t & ioAt, int depth, Node & outNode)
{
    ioAt++;

    while (ioAt < text.size() && ((text[ioAt] >= 'A' && text[ioAt] <= 'Z') || (text[ioAt] >= '0' && text[ioAt] <= '9')))
    {
        outNode.function += text[ioAt];
        ioAt++;
    }

    outNode.kind = Node::Kind::Call;

    if (outNode.function.empty())
    {
        return false;
    }

    if (ioAt >= text.size() || text[ioAt] != '(')
    {
        return true;
    }

    do
    {
        Node  argument;

        ioAt++;

        if (!ParseExpression (text, ioAt, depth + 1, argument))
        {
            return false;
        }

        outNode.children.push_back (std::move (argument));
    }
    while (ioAt < text.size() && text[ioAt] == ',');

    if (ioAt >= text.size() || text[ioAt] != ')')
    {
        return false;
    }

    ioAt++;

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  VisiCalcEvaluator::MakeNumber
//
//  A result outside what a number can be is ERROR.
//
////////////////////////////////////////////////////////////////////////////////

VisiCalcEvaluator::Value VisiCalcEvaluator::MakeNumber (double number)
{
    Value  value;



    value.kind   = std::isfinite (number) ? Value::Kind::Number : Value::Kind::Error;
    value.number = std::isfinite (number) ? number : 0;

    return value;
}





////////////////////////////////////////////////////////////////////////////////
//
//  VisiCalcEvaluator::MakeBoolean
//
////////////////////////////////////////////////////////////////////////////////

VisiCalcEvaluator::Value VisiCalcEvaluator::MakeBoolean (bool boolean)
{
    Value  value;



    value.kind   = Value::Kind::Boolean;
    value.number = boolean ? 1 : 0;

    return value;
}





////////////////////////////////////////////////////////////////////////////////
//
//  VisiCalcEvaluator::MakeKind
//
////////////////////////////////////////////////////////////////////////////////

VisiCalcEvaluator::Value VisiCalcEvaluator::MakeKind (Value::Kind kind)
{
    Value  value;



    value.kind = kind;

    return value;
}





////////////////////////////////////////////////////////////////////////////////
//
//  VisiCalcEvaluator::ToScalar
//
//  A blank entry and a label are both zero in arithmetic.
//
////////////////////////////////////////////////////////////////////////////////

VisiCalcEvaluator::Value VisiCalcEvaluator::ToScalar (const Value & value)
{
    bool  zero = value.kind == Value::Kind::Blank || value.kind == Value::Kind::Label;



    return zero ? MakeNumber (0) : value;
}





////////////////////////////////////////////////////////////////////////////////
//
//  VisiCalcEvaluator::FindFault
//
//  ERROR anywhere in the values, else NA anywhere in them.
//
////////////////////////////////////////////////////////////////////////////////

bool VisiCalcEvaluator::FindFault (const std::vector<Value> & values, Value & outFault)
{
    bool  error        = false;
    bool  notAvailable = false;



    for (const Value & value : values)
    {
        error        = error || value.kind == Value::Kind::Error;
        notAvailable = notAvailable || value.kind == Value::Kind::NotAvailable;
    }

    if (error || notAvailable)
    {
        outFault = MakeKind (error ? Value::Kind::Error : Value::Kind::NotAvailable);
    }

    return error || notAvailable;
}





////////////////////////////////////////////////////////////////////////////////
//
//  VisiCalcEvaluator::Apply
//
//  Arithmetic takes numbers and a comparison returns a truth value; a
//  truth value in either is ERROR, as is a division by zero.
//
////////////////////////////////////////////////////////////////////////////////

VisiCalcEvaluator::Value VisiCalcEvaluator::Apply (char op, const Value & left, const Value & right)
{
    Value   fault;
    double  a = left.number;
    double  b = right.number;



    if (FindFault ({ left, right }, fault))
    {
        return fault;
    }

    if (left.kind != Value::Kind::Number || right.kind != Value::Kind::Number || (op == '/' && b == 0))
    {
        return MakeKind (Value::Kind::Error);
    }

    switch (op)
    {
        case '+':  return MakeNumber (a + b);
        case '-':  return MakeNumber (a - b);
        case '*':  return MakeNumber (a * b);
        case '/':  return MakeNumber (a / b);
        case '^':  return MakeNumber (std::pow (a, b));
        case '<':  return MakeBoolean (a < b);
        case '>':  return MakeBoolean (a > b);
        case '=':  return MakeBoolean (a == b);
        case 'L':  return MakeBoolean (a <= b);
        case 'G':  return MakeBoolean (a >= b);
        case 'N':  return MakeBoolean (a != b);
        default:   return MakeKind (Value::Kind::Error);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  VisiCalcEvaluator::Flatten
//
//  The arguments from `first` on as one list: a range's entries a column
//  at a time, blank entries left out, and a label as zero.
//
////////////////////////////////////////////////////////////////////////////////

void VisiCalcEvaluator::Flatten (const Node & node, size_t first, const Grid & grid, std::vector<Value> & outValues)
{
    for (size_t at = first; at < node.children.size(); at++)
    {
        const Node &  argument = node.children[at];

        if (argument.kind != Node::Kind::Range)
        {
            outValues.push_back (Compute (argument, grid));
            continue;
        }

        for (size_t column = argument.column; column <= argument.lastColumn; column++)
        {
            for (size_t row = argument.row; row <= argument.lastRow; row++)
            {
                const Value &  value = grid[GetIndex (column, row)];

                if (value.kind != Value::Kind::Blank)
                {
                    outValues.push_back (ToScalar (value));
                }
            }
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  VisiCalcEvaluator::Lookup
//
//  The range is a column or a row of ascending values. The result is the
//  entry beside the last value not above the key: to the right of a
//  column, below a row. NA where the first value is already above it.
//
////////////////////////////////////////////////////////////////////////////////

VisiCalcEvaluator::Value VisiCalcEvaluator::Lookup (const Value & key, const Node & range, const Grid & grid)
{
    bool    down   = range.column == range.lastColumn;
    size_t  count  = down ? (range.lastRow - range.row + 1) : (range.lastColumn - range.column + 1);
    Value   result = MakeKind (Value::Kind::NotAvailable);



    if (key.kind != Value::Kind::Number)
    {
        return (key.kind == Value::Kind::NotAvailable) ? key : MakeKind (Value::Kind::Error);
    }

    for (size_t step = 0; step < count; step++)
    {
        size_t  column = down ? range.column : range.column + step;
        size_t  row    = down ? range.row + step : range.row;
        Value   value  = ToScalar (grid[GetIndex (column, row)]);

        if (value.kind != Value::Kind::Number || value.number > key.number)
        {
            break;
        }

        result = ToScalar (grid[GetIndex (down ? column + 1 : column, down ? row : row + 1)]);
    }

    return result;
}





////////////////////////////////////////////////////////////////////////////////
//
//  VisiCalcEvaluator::CallList
//
//  The functions of a whole list. An empty list's sum and count are zero,
//  and its average divides by zero.
//
////////////////////////////////////////////////////////////////////////////////

VisiCalcEvaluator::Value VisiCalcEvaluator::CallList (const std::string & name, const std::vector<Value> & list, bool & outKnown)
{
    bool    logical = name == "AND" || name == "OR";
    Value   fault;
    double  sum     = 0;
    double  low     = 0;
    double  high    = 0;
    bool    all     = true;
    bool    any     = false;



    outKnown = logical || name == "SUM" || name == "MIN" || name == "MAX" || name == "COUNT" || name == "AVERAGE";

    if (!outKnown)
    {
        return MakeKind (Value::Kind::Error);
    }

    if (name == "COUNT")
    {
        return MakeNumber ((double) list.size());
    }

    if (FindFault (list, fault))
    {
        return fault;
    }

    for (size_t at = 0; at < list.size(); at++)
    {
        const Value &  value = list[at];

        if (value.kind != (logical ? Value::Kind::Boolean : Value::Kind::Number))
        {
            return MakeKind (Value::Kind::Error);
        }

        sum  += value.number;
        low   = (at == 0) ? value.number : std::min (low, value.number);
        high  = (at == 0) ? value.number : std::max (high, value.number);
        all   = all && value.number != 0;
        any   = any || value.number != 0;
    }

    if (logical)
    {
        return list.empty() ? MakeKind (Value::Kind::Error) : MakeBoolean (name == "AND" ? all : any);
    }

    if (name == "AVERAGE")
    {
        return list.empty() ? MakeKind (Value::Kind::Error) : MakeNumber (sum / (double) list.size());
    }

    return MakeNumber (name == "SUM" ? sum : name == "MIN" ? low : high);
}





////////////////////////////////////////////////////////////////////////////////
//
//  VisiCalcEvaluator::Call
//
//  @INT drops the fraction. The angles are in radians. A function the
//  guide doesn't list is ERROR, as is one given the wrong arguments.
//
////////////////////////////////////////////////////////////////////////////////

VisiCalcEvaluator::Value VisiCalcEvaluator::Call (const Node & node, const Grid & grid)
{
    static constexpr double        kPi         = 3.14159265358979323846;
    static constexpr MathFunction  kFunctions[] =
    {
        { "ABS",   [] (double x) { return std::fabs (x);  } },
        { "INT",   [] (double x) { return std::trunc (x); } },
        { "EXP",   [] (double x) { return std::exp (x);   } },
        { "SQRT",  [] (double x) { return std::sqrt (x);  } },
        { "LN",    [] (double x) { return std::log (x);   } },
        { "LOG10", [] (double x) { return std::log10 (x); } },
        { "SIN",   [] (double x) { return std::sin (x);   } },
        { "COS",   [] (double x) { return std::cos (x);   } },
        { "TAN",   [] (double x) { return std::tan (x);   } },
        { "ASIN",  [] (double x) { return std::asin (x);  } },
        { "ACOS",  [] (double x) { return std::acos (x);  } },
        { "ATAN",  [] (double x) { return std::atan (x);  } },
    };

    const std::string &  name   = node.function;
    size_t               count  = node.children.size();
    std::vector<Value>   list;
    Value                first;
    Value                fault;
    Value                result;
    bool                 known  = false;
    double               factor = 1;
    double               sum    = 0;



    if (count == 0)
    {
        return (name == "PI")    ? MakeNumber (kPi)
             : (name == "NA")    ? MakeKind (Value::Kind::NotAvailable)
             : (name == "TRUE")  ? MakeBoolean (true)
             : (name == "FALSE") ? MakeBoolean (false)
             :                     MakeKind (Value::Kind::Error);
    }

    if (name == "LOOKUP")
    {
        return (count == 2 && node.children[1].kind == Node::Kind::Range) ? Lookup (Compute (node.children[0], grid), node.children[1], grid)
                                                                          : MakeKind (Value::Kind::Error);
    }

    Flatten (node, 0, grid, list);

    result = CallList (name, list, known);

    if (known)
    {
        return result;
    }

    //  The rest take one value as their first argument, and a range isn't
    //  one.
    if (node.children[0].kind == Node::Kind::Range)
    {
        return MakeKind (Value::Kind::Error);
    }

    first = list[0];
    list.erase (list.begin());

    if (name == "ISNA" || name == "ISERROR")
    {
        return (count == 1) ? MakeBoolean (first.kind == (name == "ISNA" ? Value::Kind::NotAvailable : Value::Kind::Error))
                            : MakeKind (Value::Kind::Error);
    }

    if (first.kind == Value::Kind::Error || first.kind == Value::Kind::NotAvailable)
    {
        return first;
    }

    if (name == "NOT")
    {
        return (count == 1 && first.kind == Value::Kind::Boolean) ? MakeBoolean (first.number == 0) : MakeKind (Value::Kind::Error);
    }

    if (name == "IF")
    {
        return (count == 3 && first.kind == Value::Kind::Boolean) ? Compute (node.children[first.number != 0 ? 1 : 2], grid)
                                                                  : MakeKind (Value::Kind::Error);
    }

    if (first.kind != Value::Kind::Number)
    {
        return MakeKind (Value::Kind::Error);
    }

    if (name == "CHOOSE")
    {
        bool  inside = first.number >= 1 && first.number < (double) list.size() + 1;

        return inside ? list[(size_t) first.number - 1] : MakeKind (Value::Kind::NotAvailable);
    }

    if (name == "NPV")
    {
        if (FindFault (list, fault))
        {
            return fault;
        }

        for (const Value & value : list)
        {
            if (value.kind != Value::Kind::Number)
            {
                return MakeKind (Value::Kind::Error);
            }

            factor *= 1 + first.number;
            sum    += (factor == 0) ? std::numeric_limits<double>::infinity() : value.number / factor;
        }

        return MakeNumber (sum);
    }

    for (const MathFunction & function : kFunctions)
    {
        if (name == function.name)
        {
            return (count == 1) ? MakeNumber (function.compute (first.number)) : MakeKind (Value::Kind::Error);
        }
    }

    return MakeKind (Value::Kind::Error);
}





////////////////////////////////////////////////////////////////////////////////
//
//  VisiCalcEvaluator::Compute
//
//  A range is a value only inside a function's arguments.
//
////////////////////////////////////////////////////////////////////////////////

VisiCalcEvaluator::Value VisiCalcEvaluator::Compute (const Node & node, const Grid & grid)
{
    Value  value;



    switch (node.kind)
    {
        case Node::Kind::Number:
            return MakeNumber (node.number);

        case Node::Kind::Cell:
            return ToScalar (grid[GetIndex (node.column, node.row)]);

        case Node::Kind::Negate:
            value = Compute (node.children[0], grid);

            return (value.kind == Value::Kind::Number)  ? MakeNumber (-value.number)
                 : (value.kind == Value::Kind::Boolean) ? MakeKind (Value::Kind::Error)
                 :                                        value;

        case Node::Kind::Binary:
            return Apply (node.op, Compute (node.children[0], grid), Compute (node.children[1], grid));

        case Node::Kind::Call:
            return (node.function == "ERROR") ? MakeKind (Value::Kind::Error) : Call (node, grid);

        case Node::Kind::Range:
        case Node::Kind::Invalid:
        default:
            return MakeKind (Value::Kind::Error);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  VisiCalcEvaluator::Evaluate
//
//  Returns a value for each of the sheet's cells, in their order. An entry
//  that isn't an expression is ERROR.
//
////////////////////////////////////////////////////////////////////////////////

std::vector<VisiCalcEvaluator::Value> VisiCalcEvaluator::Evaluate (const VisiCalcSheet::Sheet & sheet, size_t recalculations)
{
    size_t               count = sheet.cells.size();
    Grid                 grid (kGridColumns * kGridRows);
    std::vector<Node>    trees (count);
    std::vector<bool>    computed (count);
    std::vector<size_t>  order (count);
    std::vector<Value>   values (count);



    for (size_t at = 0; at < count; at++)
    {
        const VisiCalcSheet::Cell &  cell  = sheet.cells[at];
        Value &                      value = grid[GetIndex (cell.column, cell.row)];

        order[at] = at;

        if (cell.label || cell.repeat)
        {
            value = MakeKind (Value::Kind::Label);
        }
        else if (cell.contents.empty())
        {
            value = MakeKind (Value::Kind::Blank);
        }
        else
        {
            computed[at] = true;

            Parse (cell.contents, trees[at]);

            value = Compute (trees[at], grid);
        }
    }

    std::stable_sort (order.begin(), order.end(), [&sheet] (size_t left, size_t right)
    {
        const VisiCalcSheet::Cell &  a = sheet.cells[left];
        const VisiCalcSheet::Cell &  b = sheet.cells[right];

        return sheet.byColumns ? std::tie (a.column, a.row) < std::tie (b.column, b.row)
                               : std::tie (a.row, a.column) < std::tie (b.row, b.column);
    });

    for (size_t pass = 0; pass < recalculations; pass++)
    {
        for (size_t at : order)
        {
            if (computed[at])
            {
                grid[GetIndex (sheet.cells[at].column, sheet.cells[at].row)] = Compute (trees[at], grid);
            }
        }
    }

    for (size_t at = 0; at < count; at++)
    {
        values[at] = grid[GetIndex (sheet.cells[at].column, sheet.cells[at].row)];
    }

    return values;
}
