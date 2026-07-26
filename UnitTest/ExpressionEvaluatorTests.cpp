#include "Pch.h"

#include "ExpressionEvaluator.h"
#include "TestHelpers.h"





using namespace Microsoft::VisualStudio::CppUnitTestFramework;





namespace ExpressionEvaluatorTests
{
    static ExprContext MakeCtx (int32_t pc = 0)
    {
        ExprContext ctx = {};
        ctx.symbols   = nullptr;
        ctx.currentPC = pc;
        return ctx;
    }



    static ExprContext MakeCtx (const std::unordered_map<std::string, int32_t> & syms, int32_t pc = 0)
    {
        ExprContext ctx = {};
        ctx.symbols   = &syms;
        ctx.currentPC = pc;
        return ctx;
    }





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  ArithmeticTests
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (ArithmeticTests)
    {
    public:

        TEST_METHOD (Addition)         { auto r = ExpressionEvaluator::Evaluate ("3+5",     MakeCtx()); Assert::IsTrue (r.success); Assert::AreEqual (8,   r.value); }
        TEST_METHOD (Subtraction)      { auto r = ExpressionEvaluator::Evaluate ("10-3",    MakeCtx()); Assert::IsTrue (r.success); Assert::AreEqual (7,   r.value); }
        TEST_METHOD (Multiplication)   { auto r = ExpressionEvaluator::Evaluate ("3*4",     MakeCtx()); Assert::IsTrue (r.success); Assert::AreEqual (12,  r.value); }
        TEST_METHOD (Division)         { auto r = ExpressionEvaluator::Evaluate ("15/3",    MakeCtx()); Assert::IsTrue (r.success); Assert::AreEqual (5,   r.value); }
        TEST_METHOD (Modulo)           { auto r = ExpressionEvaluator::Evaluate ("17%5",    MakeCtx()); Assert::IsTrue (r.success); Assert::AreEqual (2,   r.value); }
        TEST_METHOD (Negation)         { auto r = ExpressionEvaluator::Evaluate ("-1&$ff",  MakeCtx()); Assert::IsTrue (r.success); Assert::AreEqual (255, r.value); }
        TEST_METHOD (UnaryPlus)        { auto r = ExpressionEvaluator::Evaluate ("+5",      MakeCtx()); Assert::IsTrue (r.success); Assert::AreEqual (5,   r.value); }
        TEST_METHOD (DivisionByZero)   { auto r = ExpressionEvaluator::Evaluate ("5/0",     MakeCtx()); Assert::IsFalse (r.success); }
    };





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  BitwiseTests
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (BitwiseTests)
    {
    public:

        TEST_METHOD (And)       { auto r = ExpressionEvaluator::Evaluate ("$FF&$0F",      MakeCtx()); Assert::IsTrue (r.success); Assert::AreEqual (0x0F,  r.value); }
        TEST_METHOD (Or)        { auto r = ExpressionEvaluator::Evaluate ("$80|$01",      MakeCtx()); Assert::IsTrue (r.success); Assert::AreEqual (0x81,  r.value); }
        TEST_METHOD (Xor)       { auto r = ExpressionEvaluator::Evaluate ("$FF^$0F",      MakeCtx()); Assert::IsTrue (r.success); Assert::AreEqual (0xF0,  r.value); }
        TEST_METHOD (Not)       { auto r = ExpressionEvaluator::Evaluate ("~$0F&$FF",     MakeCtx()); Assert::IsTrue (r.success); Assert::AreEqual (0xF0,  r.value); }
        TEST_METHOD (ShiftLeft) { auto r = ExpressionEvaluator::Evaluate ("1<<4",         MakeCtx()); Assert::IsTrue (r.success); Assert::AreEqual (0x10,  r.value); }
        TEST_METHOD (ShiftRight){ auto r = ExpressionEvaluator::Evaluate ("$80>>4",       MakeCtx()); Assert::IsTrue (r.success); Assert::AreEqual (0x08,  r.value); }
    };





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  LogicalTests
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (LogicalTests)
    {
    public:

        TEST_METHOD (LogicalNotZero)    { auto r = ExpressionEvaluator::Evaluate ("!0",    MakeCtx()); Assert::IsTrue (r.success); Assert::AreEqual (1, r.value); }
        TEST_METHOD (LogicalNotNonzero) { auto r = ExpressionEvaluator::Evaluate ("!5",    MakeCtx()); Assert::IsTrue (r.success); Assert::AreEqual (0, r.value); }
        TEST_METHOD (LogicalAndTT)      { auto r = ExpressionEvaluator::Evaluate ("1&&1",  MakeCtx()); Assert::IsTrue (r.success); Assert::AreEqual (1, r.value); }
        TEST_METHOD (LogicalAndTF)      { auto r = ExpressionEvaluator::Evaluate ("1&&0",  MakeCtx()); Assert::IsTrue (r.success); Assert::AreEqual (0, r.value); }
        TEST_METHOD (LogicalOrTF)       { auto r = ExpressionEvaluator::Evaluate ("0||1",  MakeCtx()); Assert::IsTrue (r.success); Assert::AreEqual (1, r.value); }
        TEST_METHOD (LogicalOrFF)       { auto r = ExpressionEvaluator::Evaluate ("0||0",  MakeCtx()); Assert::IsTrue (r.success); Assert::AreEqual (0, r.value); }
    };





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  ComparisonTests
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (ComparisonTests)
    {
    public:

        TEST_METHOD (EqualTrue)     { auto r = ExpressionEvaluator::Evaluate ("5=5",   MakeCtx()); Assert::IsTrue (r.success); Assert::AreEqual (1, r.value); }
        TEST_METHOD (EqualFalse)    { auto r = ExpressionEvaluator::Evaluate ("5=6",   MakeCtx()); Assert::IsTrue (r.success); Assert::AreEqual (0, r.value); }
        TEST_METHOD (NotEqualTrue)  { auto r = ExpressionEvaluator::Evaluate ("5!=6",  MakeCtx()); Assert::IsTrue (r.success); Assert::AreEqual (1, r.value); }
        TEST_METHOD (NotEqualFalse) { auto r = ExpressionEvaluator::Evaluate ("5!=5",  MakeCtx()); Assert::IsTrue (r.success); Assert::AreEqual (0, r.value); }
        TEST_METHOD (LessThan)      { auto r = ExpressionEvaluator::Evaluate ("3<5",   MakeCtx()); Assert::IsTrue (r.success); Assert::AreEqual (1, r.value); }
        TEST_METHOD (GreaterThan)   { auto r = ExpressionEvaluator::Evaluate ("5>3",   MakeCtx()); Assert::IsTrue (r.success); Assert::AreEqual (1, r.value); }
        TEST_METHOD (LessEqual)     { auto r = ExpressionEvaluator::Evaluate ("3<=3",  MakeCtx()); Assert::IsTrue (r.success); Assert::AreEqual (1, r.value); }
        TEST_METHOD (GreaterEqual)  { auto r = ExpressionEvaluator::Evaluate ("3>=3",  MakeCtx()); Assert::IsTrue (r.success); Assert::AreEqual (1, r.value); }
    };





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  PrecedenceTests
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (PrecedenceTests)
    {
    public:

        TEST_METHOD (MulBeforeAdd)  { auto r = ExpressionEvaluator::Evaluate ("2+3*4",         MakeCtx()); Assert::IsTrue (r.success); Assert::AreEqual (14, r.value); }
        TEST_METHOD (Parens)        { auto r = ExpressionEvaluator::Evaluate ("(2+3)*4",       MakeCtx()); Assert::IsTrue (r.success); Assert::AreEqual (20, r.value); }
        TEST_METHOD (Brackets)      { auto r = ExpressionEvaluator::Evaluate ("[2+3]*4",       MakeCtx()); Assert::IsTrue (r.success); Assert::AreEqual (20, r.value); }
        TEST_METHOD (Complex)       { auto r = ExpressionEvaluator::Evaluate ("($FF&~$0F)|($80>>4)", MakeCtx()); Assert::IsTrue (r.success); Assert::AreEqual (0xF8, r.value); }
        TEST_METHOD (Nested)        { auto r = ExpressionEvaluator::Evaluate ("((1+2)*(3+4))&$FF",   MakeCtx()); Assert::IsTrue (r.success); Assert::AreEqual (21, r.value); }
    };





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  NumberFormatTests
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (NumberFormatTests)
    {
    public:

        TEST_METHOD (Hex)       { auto r = ExpressionEvaluator::Evaluate ("$FF",          MakeCtx()); Assert::IsTrue (r.success); Assert::AreEqual (255,  r.value); }
        TEST_METHOD (Binary)    { auto r = ExpressionEvaluator::Evaluate ("%11110000",    MakeCtx()); Assert::IsTrue (r.success); Assert::AreEqual (0xF0, r.value); }
        TEST_METHOD (Decimal)   { auto r = ExpressionEvaluator::Evaluate ("255",          MakeCtx()); Assert::IsTrue (r.success); Assert::AreEqual (255,  r.value); }
        TEST_METHOD (CharConst) { auto r = ExpressionEvaluator::Evaluate ("'A'",          MakeCtx()); Assert::IsTrue (r.success); Assert::AreEqual (0x41, r.value); }
        TEST_METHOD (CharMath)  { auto r = ExpressionEvaluator::Evaluate ("'Z'-'A'",      MakeCtx()); Assert::IsTrue (r.success); Assert::AreEqual (25,   r.value); }
    };





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  CurrentPCTests
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (CurrentPCTests)
    {
    public:

        TEST_METHOD (StarAsPC)       { auto r = ExpressionEvaluator::Evaluate ("*",     MakeCtx (0x8000)); Assert::IsTrue (r.success); Assert::AreEqual (0x8000, r.value); }
        TEST_METHOD (StarPlusOffset) { auto r = ExpressionEvaluator::Evaluate ("*+5",   MakeCtx (0x100));  Assert::IsTrue (r.success); Assert::AreEqual (0x105,  r.value); }
        TEST_METHOD (BareDollarAsPC) { auto r = ExpressionEvaluator::Evaluate ("$",     MakeCtx (0x400));  Assert::IsTrue (r.success); Assert::AreEqual (0x400,  r.value); }
        TEST_METHOD (MulNotPC)       { auto r = ExpressionEvaluator::Evaluate ("2*3",   MakeCtx (0x8000)); Assert::IsTrue (r.success); Assert::AreEqual (6,      r.value); }
        TEST_METHOD (FiveTimesStar)  { auto r = ExpressionEvaluator::Evaluate ("5**",   MakeCtx (0x10));   Assert::IsTrue (r.success); Assert::AreEqual (0x50,   r.value); }
    };





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  LoHiTests
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (LoHiTests)
    {
    public:

        TEST_METHOD (LoKeyword)    { auto r = ExpressionEvaluator::Evaluate ("lo $1234", MakeCtx()); Assert::IsTrue (r.success); Assert::AreEqual (0x34, r.value); }
        TEST_METHOD (HiKeyword)    { auto r = ExpressionEvaluator::Evaluate ("hi $1234", MakeCtx()); Assert::IsTrue (r.success); Assert::AreEqual (0x12, r.value); }
        TEST_METHOD (LoWithParens) { auto r = ExpressionEvaluator::Evaluate ("lo($1234)",MakeCtx()); Assert::IsTrue (r.success); Assert::AreEqual (0x34, r.value); }
        TEST_METHOD (HiWithParens) { auto r = ExpressionEvaluator::Evaluate ("hi($1234)",MakeCtx()); Assert::IsTrue (r.success); Assert::AreEqual (0x12, r.value); }
        TEST_METHOD (AngleLo)      { auto r = ExpressionEvaluator::Evaluate ("<$1234",   MakeCtx()); Assert::IsTrue (r.success); Assert::AreEqual (0x34, r.value); }
        TEST_METHOD (AngleHi)      { auto r = ExpressionEvaluator::Evaluate (">$1234",   MakeCtx()); Assert::IsTrue (r.success); Assert::AreEqual (0x12, r.value); }
    };





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  SymbolTests
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (SymbolTests)
    {
    public:

        TEST_METHOD (SymbolLookup)
        {
            std::unordered_map<std::string, int32_t> syms = { { "val", 0x42 } };
            auto r = ExpressionEvaluator::Evaluate ("val", MakeCtx (syms));
            Assert::IsTrue (r.success);
            Assert::AreEqual (0x42, r.value);
        }

        TEST_METHOD (SymbolArithmetic)
        {
            std::unordered_map<std::string, int32_t> syms = { { "base", 0x2000 } };
            auto r = ExpressionEvaluator::Evaluate ("base+3", MakeCtx (syms));
            Assert::IsTrue (r.success);
            Assert::AreEqual (0x2003, r.value);
        }

        TEST_METHOD (UndefinedSymbol)
        {
            auto r = ExpressionEvaluator::Evaluate ("unknown", MakeCtx());
            Assert::IsFalse (r.success);
            Assert::IsTrue (r.hasUnresolved);
        }
    };





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  IncrementDecrementTests
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (IncrementDecrementTests)
    {
    public:

        TEST_METHOD (PrefixIncrement)
        {
            std::unordered_map<std::string, int32_t> syms = { { "val", 5 } };
            auto r = ExpressionEvaluator::Evaluate ("++val", MakeCtx (syms));
            Assert::IsTrue (r.success);
            Assert::AreEqual (6, r.value);
        }

        TEST_METHOD (PrefixDecrement)
        {
            std::unordered_map<std::string, int32_t> syms = { { "val", 5 } };
            auto r = ExpressionEvaluator::Evaluate ("--val", MakeCtx (syms));
            Assert::IsTrue (r.success);
            Assert::AreEqual (4, r.value);
        }
    };





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  ErrorTests
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (ErrorTests)
    {
    public:

        TEST_METHOD (EmptyExpr)      { auto r = ExpressionEvaluator::Evaluate ("",    MakeCtx()); Assert::IsFalse (r.success); }
        TEST_METHOD (WhitespaceOnly) { auto r = ExpressionEvaluator::Evaluate ("   ", MakeCtx()); Assert::IsFalse (r.success); }
        TEST_METHOD (UnmatchedParen) { auto r = ExpressionEvaluator::Evaluate ("(5+3", MakeCtx()); Assert::IsFalse (r.success); }
        TEST_METHOD (TrailingGarbage){ auto r = ExpressionEvaluator::Evaluate ("5 6",  MakeCtx()); Assert::IsFalse (r.success); }
    };





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  ExtendedNumberFormatTests
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (ExtendedNumberFormatTests)
    {
    public:

        TEST_METHOD (OctalAt)          { auto r = ExpressionEvaluator::Evaluate ("@17",       MakeCtx()); Assert::IsTrue (r.success); Assert::AreEqual (15,  r.value); }
        TEST_METHOD (OctalAtZero)      { auto r = ExpressionEvaluator::Evaluate ("@0",        MakeCtx()); Assert::IsTrue (r.success); Assert::AreEqual (0,   r.value); }
        TEST_METHOD (OctalAt377)       { auto r = ExpressionEvaluator::Evaluate ("@377",      MakeCtx()); Assert::IsTrue (r.success); Assert::AreEqual (255, r.value); }

        TEST_METHOD (CStyleHex)        { auto r = ExpressionEvaluator::Evaluate ("0xFF",      MakeCtx()); Assert::IsTrue (r.success); Assert::AreEqual (255, r.value); }
        TEST_METHOD (CStyleHexLower)   { auto r = ExpressionEvaluator::Evaluate ("0xff",      MakeCtx()); Assert::IsTrue (r.success); Assert::AreEqual (255, r.value); }

        TEST_METHOD (CStyleBinary)     { auto r = ExpressionEvaluator::Evaluate ("0b10101010", MakeCtx()); Assert::IsTrue (r.success); Assert::AreEqual (170, r.value); }
        TEST_METHOD (CStyleBinarySmall){ auto r = ExpressionEvaluator::Evaluate ("0b101",      MakeCtx()); Assert::IsTrue (r.success); Assert::AreEqual (5,   r.value); }

        TEST_METHOD (BaseHashHex)      { auto r = ExpressionEvaluator::Evaluate ("16#FF",     MakeCtx()); Assert::IsTrue (r.success); Assert::AreEqual (255, r.value); }
        TEST_METHOD (BaseHashOctal)    { auto r = ExpressionEvaluator::Evaluate ("8#17",      MakeCtx()); Assert::IsTrue (r.success); Assert::AreEqual (15,  r.value); }
        TEST_METHOD (BaseHashBinary)   { auto r = ExpressionEvaluator::Evaluate ("2#10101010", MakeCtx()); Assert::IsTrue (r.success); Assert::AreEqual (170, r.value); }
    };
}
