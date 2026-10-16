#include "Pch.h"
#include "CLexer.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  CLexerTests
//
//  The runs of C lines: keywords and types, numbers, strings and characters,
//  comments over one line or several, and preprocessor lines.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (CLexerTests)
{
public:

    static std::wstring FormatRuns (const std::wstring & line, bool & ioIsInComment)
    {
        static const wchar_t * const  s_kNames[] = { L"K", L"T", L"N", L"S", L"C", L"P" };
        std::wstring                  text;



        for (const CLexer::Run & run : CLexer::GetRuns (line, ioIsInComment))
        {
            text += std::format (L"{}[{}] ", s_kNames[(int) run.token], line.substr ((size_t) run.start, (size_t) run.length));
        }

        return text;
    }



    static std::wstring FormatRuns (const std::wstring & line)
    {
        bool  isInComment = false;



        return FormatRuns (line, isInComment);
    }



    TEST_METHOD (KeywordsAndTypesAreColoredAndIdentifiersAreNot)
    {
        Assert::AreEqual (std::wstring (L"K[static] T[unsigned] T[char] "), FormatRuns (L"static unsigned char count;"));
        Assert::AreEqual (std::wstring (L"K[if] K[return] "),                FormatRuns (L"    if (done) return result;"));
        Assert::AreEqual (std::wstring (L"T[uint8_t] "),                     FormatRuns (L"uint8_t forward;"), L"a word that starts with a keyword is not one");
        Assert::AreEqual (std::wstring (L""),                                FormatRuns (L"interior = format_it();"));
    }



    TEST_METHOD (NumbersInEachBase)
    {
        Assert::AreEqual (std::wstring (L"N[0x1F] N[42u] N[0777] "), FormatRuns (L"a = 0x1F + 42u + 0777;"));
        Assert::AreEqual (std::wstring (L"N[1.5e-3f] N[.25] "),      FormatRuns (L"x = 1.5e-3f * .25;"), L"an exponent's sign is part of the number");
        Assert::AreEqual (std::wstring (L"N[0xE] N[1] "),                 FormatRuns (L"x = 0xE-1;"), L"a hex E is a digit, not an exponent");
        Assert::AreEqual (std::wstring (L""),                        FormatRuns (L"x2 = y3;"), L"a digit inside a word is part of the word");
    }



    TEST_METHOD (StringsAndCharacters)
    {
        Assert::AreEqual (std::wstring (L"S[\"a \\\"b\\\"\"] S['\\n'] "), FormatRuns (L"puts (\"a \\\"b\\\"\"); c = '\\n';"));
        Assert::AreEqual (std::wstring (L"S[\"// not a comment\"] "),       FormatRuns (L"s = \"// not a comment\";"));
        Assert::AreEqual (std::wstring (L"S[\"open to the end] "),          FormatRuns (L"s = \"open to the end"), L"an unclosed string runs to the line's end");
    }



    TEST_METHOD (CommentsOnOneLine)
    {
        Assert::AreEqual (std::wstring (L"K[return] N[0] C[// done] "), FormatRuns (L"return 0; // done"));
        Assert::AreEqual (std::wstring (L"T[int] C[/* a */] N[1] "),    FormatRuns (L"int /* a */ x = 1;"));
    }



    TEST_METHOD (ABlockCommentRunsOverLines)
    {
        bool  isInComment = false;



        Assert::AreEqual (std::wstring (L"T[int] C[/* starts here] "), FormatRuns (L"int x; /* starts here", isInComment));
        Assert::IsTrue   (isInComment, L"open at the first line's end");

        Assert::AreEqual (std::wstring (L"C[  int inside;] "), FormatRuns (L"  int inside;", isInComment), L"a keyword inside the comment is the comment's");
        Assert::IsTrue   (isInComment);

        Assert::AreEqual (std::wstring (L"C[ends */] K[return] "), FormatRuns (L"ends */ return;", isInComment));
        Assert::IsFalse  (isInComment, L"closed");

        isInComment = true;
        Assert::AreEqual (std::wstring (L"C[#define inside] "), FormatRuns (L"#define inside", isInComment), L"a # inside a comment is no preprocessor line");
    }



    TEST_METHOD (PreprocessorLines)
    {
        Assert::AreEqual (std::wstring (L"P[#include <stdio.h>] "),                FormatRuns (L"#include <stdio.h>"));
        Assert::AreEqual (std::wstring (L"P[# define MAX 10 // ten] "),           FormatRuns (L"  # define MAX 10 // ten"), L"indented, its comment included");
    }



    TEST_METHOD (CFilesAreKnownByExtension)
    {
        Assert::IsTrue  (CLexer::IsCFile (L"src/main.c"));
        Assert::IsTrue  (CLexer::IsCFile (L"C:\\inc\\APPLE2.H"), L"in either case");
        Assert::IsFalse (CLexer::IsCFile (L"main.s"));
        Assert::IsFalse (CLexer::IsCFile (L"main.cpp"));
        Assert::IsFalse (CLexer::IsCFile (L""));
    }
};
