#include "Pch.h"
#include "Core/TextEncoding.h"
#include "Machines/Apple2/Common/AppleFileName.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  AppleFileNameTests
//
//  Text that reaches the screen from a narrow string. Widening each char by
//  an iterator copy sign-extends a byte above 127 into U+FF80..U+FFFF, so a
//  host name such as "Brøderbund" loses its letter, and an Apple name is
//  Apple's character set, which no host code page describes.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (AppleFileNameTests)
{
public:
    TEST_METHOD (HostText_KeepsItsNonAsciiLetter)
    {
        std::string   narrow = "Br\xF8" "derbund";
        std::wstring  wide   = TextEncoding::NarrowToWide (narrow, 1252);

        Assert::AreEqual (std::wstring (L"Br\x00F8" L"derbund"), wide);    }


    TEST_METHOD (AppleName_ClearsTheHighBit)
    {
        Assert::AreEqual (std::wstring (L"HELLO"), AppleFileName::ToDisplay ("\xC8\xC5\xCC\xCC\xCF"));
    }


    TEST_METHOD (AppleName_ShowsControlCharactersAsPictures)
    {
        //  A control-G with and without the high bit, a NUL, and a DEL.
        std::string  name ("A\x87" "B\x07" "C", 5);

        name += '\0';
        name += '\x7F';

        Assert::AreEqual (std::wstring (L"A\x2407" L"B\x2407" L"C\x2400\x2421"), AppleFileName::ToDisplay (name));
    }
};
