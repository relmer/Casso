#include "Pch.h"
#include "../EhmTestHelper.h"
#include "FileTypes/VisiCalcEvaluator.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  VisiCalcEvaluatorTests
//
//  Sheets built a cell at a time and evaluated. Column A has the values
//  1, 2, 3 and 4 in rows 1 to 4 and a label in row 5; row 6 is blank.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (VisiCalcEvaluatorTests)
{
public:
    using Value = VisiCalcSheet::Value;

    static void Add (VisiCalcSheet::Sheet & sheet, const char * coordinate, const std::string & contents, bool label = false)
    {
        VisiCalcSheet::Cell  cell;
        size_t               length = 0;



        Assert::IsTrue (VisiCalcSheet::ParseCoordinate (coordinate, cell.column, cell.row, length));

        cell.label    = label;
        cell.contents = contents;

        sheet.cells.push_back (cell);
    }


    static Value Compute (const std::string & formula)
    {
        VisiCalcSheet::Sheet  sheet;



        Add (sheet, "A1", "1");
        Add (sheet, "A2", "2");
        Add (sheet, "A3", "3");
        Add (sheet, "A4", "4");
        Add (sheet, "A5", "TOTAL", true);
        Add (sheet, "B1", "10");
        Add (sheet, "B2", "20");
        Add (sheet, "B3", "30");
        Add (sheet, "B4", "40");
        Add (sheet, "C1", "@NA");
        Add (sheet, "C2", "@ERROR");
        Add (sheet, "D9", formula);

        return VisiCalcEvaluator::Evaluate (sheet).back();
    }


    static void AssertNumber (double correct, const std::string & formula)
    {
        Value         value   = Compute (formula);
        std::wstring  message = std::wstring (formula.begin(), formula.end());



        Assert::IsTrue   (value.kind == Value::Kind::Number, message.c_str());
        Assert::AreEqual (correct, value.number, 1e-9, message.c_str());
    }


    static void AssertKind (Value::Kind correct, const std::string & formula)
    {
        std::wstring  message = std::wstring (formula.begin(), formula.end());



        Assert::IsTrue (Compute (formula).kind == correct, message.c_str());
    }


    static void AssertTruth (bool correct, const std::string & formula)
    {
        Value         value   = Compute (formula);
        std::wstring  message = std::wstring (formula.begin(), formula.end());



        Assert::IsTrue   (value.kind == Value::Kind::Boolean, message.c_str());
        Assert::AreEqual (correct, value.number != 0, message.c_str());
    }


    TEST_METHOD (Operators_ApplyFromLeftToRight)
    {
        AssertNumber (20,    "2+3*4");
        AssertNumber (14,    "2+(3*4)");
        AssertNumber (-1,    "-A1");
        AssertNumber (3,     "+A1+A2");
        AssertNumber (1.5,   "A3/A2");
        AssertNumber (64,    "A2^A3^2");
        AssertNumber (2,     "--2");
        AssertNumber (-6,    "2*-3");
        AssertNumber (1500,  "1.5E3");
        AssertNumber (0.015, "1.5E-2");
        AssertNumber (0.5,   ".5");
        AssertNumber (0,     "+A5");
        AssertNumber (0,     "+A6");
    }


    TEST_METHOD (Comparisons_ReturnATruthValue)
    {
        AssertTruth (true,  "A1<A2");
        AssertTruth (false, "A1>A2");
        AssertTruth (true,  "A1+1=A2");
        AssertTruth (true,  "A1<=1");
        AssertTruth (false, "A1>=2");
        AssertTruth (true,  "A1<>A2");
        AssertKind  (Value::Kind::Error, "(A1<A2)+1");
        AssertKind  (Value::Kind::Error, "-@TRUE");
    }


    TEST_METHOD (Faults_ComeFromBadArithmeticAndBadText)
    {
        AssertKind (Value::Kind::Error,        "1/0");
        AssertKind (Value::Kind::Error,        "1/A6");
        AssertKind (Value::Kind::Error,        "2+");
        AssertKind (Value::Kind::Error,        "(2+3");
        AssertKind (Value::Kind::Error,        "@NOSUCH(1)");
        AssertKind (Value::Kind::Error,        "A1...A4");
        AssertKind (Value::Kind::Error,        "1E300*1E300");
        AssertKind (Value::Kind::NotAvailable, "+C1+1");
        AssertKind (Value::Kind::Error,        "+C2+1");
        AssertKind (Value::Kind::Error,        "+C1+C2");
    }


    TEST_METHOD (ListFunctions_TakeRangesAndValues)
    {
        AssertNumber (10,   "@SUM(A1...A4)");
        AssertNumber (110,  "@SUM(A1...B4)");
        AssertNumber (15,   "@SUM(A1...A4,5)");
        AssertNumber (10,   "@SUM(A1.A6)");
        AssertNumber (0,    "@MIN(A1...A5)");
        AssertNumber (1,    "@MIN(A1...A4)");
        AssertNumber (40,   "@MAX(A1...B4)");
        AssertNumber (5,    "@COUNT(A1...A6)");
        AssertNumber (2.5,  "@AVERAGE(A1...A4)");
        AssertNumber (2,    "@AVERAGE(A1...A5)");
        AssertKind   (Value::Kind::Error,        "@AVERAGE(A6...A7)");
        AssertKind   (Value::Kind::NotAvailable, "@SUM(A1...C1)");
        AssertKind   (Value::Kind::Error,        "@SUM(A1...C2)");
    }


    TEST_METHOD (Npv_DiscountsEachValueByItsPlace)
    {
        AssertNumber (100 / 1.1 + 100 / 1.21, "@NPV(.1,100,100)");
        AssertNumber (10,                     "@NPV(0,A1...A4)");
    }


    TEST_METHOD (Lookup_ReturnsTheEntryBesideTheLastValueNotAboveTheKey)
    {
        AssertNumber (20, "@LOOKUP(2,A1...A4)");
        AssertNumber (20, "@LOOKUP(2.9,A1...A4)");
        AssertNumber (40, "@LOOKUP(99,A1...A4)");
        AssertNumber (2,  "@LOOKUP(1,A1...B1)");
        AssertKind   (Value::Kind::NotAvailable, "@LOOKUP(0,A1...A4)");
        AssertKind   (Value::Kind::Error,        "@LOOKUP(2,5)");
    }


    TEST_METHOD (MathFunctions_ReturnTheirValues)
    {
        AssertNumber (3,                "@ABS(-3)");
        AssertNumber (2,                "@INT(2.9)");
        AssertNumber (-2,               "@INT(-2.9)");
        AssertNumber (std::exp (1.0),   "@EXP(1)");
        AssertNumber (3,                "@SQRT(9)");
        AssertNumber (1,                "@LN(@EXP(1))");
        AssertNumber (2,                "@LOG10(100)");
        AssertNumber (3.14159265358979, "@PI");
        AssertNumber (1,                "@SIN(@PI/2)");
        AssertNumber (-1,               "@COS(@PI)");
        AssertNumber (1,                "@TAN(@PI/4)");
        AssertNumber (3.14159265358979, "@ASIN(1)*2");
        AssertNumber (3.14159265358979, "@ACOS(-1)");
        AssertNumber (3.14159265358979, "@ATAN(1)*4");
        AssertKind   (Value::Kind::Error, "@SQRT(-1)");
        AssertKind   (Value::Kind::Error, "@LN(0)");
        AssertKind   (Value::Kind::Error, "@LOG10(-1)");
        AssertKind   (Value::Kind::Error, "@ASIN(2)");
        AssertKind   (Value::Kind::Error, "@ACOS(2)");
        AssertKind   (Value::Kind::Error, "@ABS(1,2)");
        AssertKind   (Value::Kind::NotAvailable, "@ABS(C1)");
    }


    TEST_METHOD (LogicFunctions_TakeTruthValues)
    {
        AssertTruth  (true,  "@TRUE");
        AssertTruth  (false, "@FALSE");
        AssertTruth  (false, "@NOT(@TRUE)");
        AssertTruth  (true,  "@AND(A1<A2,A2<A3)");
        AssertTruth  (false, "@AND(A1<A2,A2>A3)");
        AssertTruth  (true,  "@OR(A1>A2,A2<A3)");
        AssertTruth  (false, "@OR(A1>A2,A2>A3)");
        AssertNumber (7,     "@IF(A1<A2,7,8)");
        AssertNumber (8,     "@IF(A1>A2,7,8)");
        AssertNumber (7,     "@IF(@TRUE,7,1/0)");
        AssertKind   (Value::Kind::Error,        "@IF(1,7,8)");
        AssertKind   (Value::Kind::Error,        "@NOT(1)");
        AssertKind   (Value::Kind::Error,        "@AND(1,2)");
        AssertKind   (Value::Kind::NotAvailable, "@IF(C1,7,8)");
        AssertTruth  (true,  "@ISNA(C1)");
        AssertTruth  (false, "@ISNA(C2)");
        AssertTruth  (true,  "@ISERROR(C2)");
        AssertTruth  (true,  "@ISERROR(1/0)");
        AssertTruth  (false, "@ISERROR(A1)");
    }


    TEST_METHOD (Choose_ReturnsTheValueAtThePlace)
    {
        AssertNumber (20, "@CHOOSE(2,10,20,30)");
        AssertNumber (3,  "@CHOOSE(3.7,A1...A4)");
        AssertKind   (Value::Kind::NotAvailable, "@CHOOSE(0,10,20)");
        AssertKind   (Value::Kind::NotAvailable, "@CHOOSE(3,10,20)");
    }


    TEST_METHOD (Evaluate_ACircularReferenceAdvancesOnceEachRecalculation)
    {
        VisiCalcSheet::Sheet  sheet;



        //  Read in this order, A1 is 1 and B1 is 2; the pass makes A1 3 and
        //  then B1 4.
        Add (sheet, "A1", "+B1+1");
        Add (sheet, "B1", "+A1+1");

        Assert::AreEqual (3.0, VisiCalcEvaluator::Evaluate (sheet)[0].number);
        Assert::AreEqual (4.0, VisiCalcEvaluator::Evaluate (sheet)[1].number);
        Assert::AreEqual (5.0, VisiCalcEvaluator::Evaluate (sheet, 2)[0].number);
        Assert::AreEqual (6.0, VisiCalcEvaluator::Evaluate (sheet, 2)[1].number);
    }


    TEST_METHOD (Evaluate_FollowsTheSheetOrder)
    {
        VisiCalcSheet::Sheet  sheet;



        //  B1 reads A2 and A2 reads A3, and the file has them in that order,
        //  so both are zero once the file is read. Down the columns A2 is
        //  computed ahead of B1; along the rows B1 is computed first.
        Add (sheet, "B1", "+A2");
        Add (sheet, "A2", "+A3");
        Add (sheet, "A3", "5");

        sheet.byColumns = true;
        Assert::AreEqual (5.0, VisiCalcEvaluator::Evaluate (sheet)[0].number);

        sheet.byColumns = false;
        Assert::AreEqual (0.0, VisiCalcEvaluator::Evaluate (sheet)[0].number);
        Assert::AreEqual (5.0, VisiCalcEvaluator::Evaluate (sheet, 2)[0].number);
    }


    TEST_METHOD (Parse_BuildsATreeThatLeansLeft)
    {
        VisiCalcEvaluator::Node  node;



        Assert::IsTrue (VisiCalcEvaluator::Parse ("1+2*3", node));
        Assert::IsTrue (node.kind == VisiCalcEvaluator::Node::Kind::Binary);
        Assert::AreEqual ('*', node.op);
        Assert::AreEqual ('+', node.children[0].op);
        Assert::AreEqual (3.0, node.children[1].number);

        Assert::IsFalse (VisiCalcEvaluator::Parse ("1+", node));
        Assert::IsTrue  (node.kind == VisiCalcEvaluator::Node::Kind::Invalid);
    }
};
