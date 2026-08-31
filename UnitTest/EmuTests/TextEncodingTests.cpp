#include "Pch.h"
#include "Core/TextEncoding.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  TextEncodingTests
//
//  The boundary where a byte string stops being data and becomes something a
//  person reads.
//
//  IT EXISTS BECAUSE OF A REAL FILENAME. `Space Quarks (1981)(Broderbund)` --
//  with the Danish o-slash in the publisher's name -- came back from its own
//  error message as `Br?derbund`. The bytes were correct CP-1252 all the way
//  through; the console was set to UTF-8 and read the single byte $F8 as a
//  broken sequence. A user cannot paste that message back into a command line,
//  which is most of what an error naming a file is for.
//
//  EVERY LITERAL BELOW IS WRITTEN IN HEX ESCAPES, on purpose. The source files
//  are CP-1252 and the /utf-8 switch is deliberately not used, so a test that
//  typed the character directly would be asserting about the compiler's opinion
//  of the source encoding rather than about the conversion. The escapes are
//  split -- "\xF8" "d" rather than "\xF8d" -- because a hex escape in C++ is
//  greedy and would otherwise swallow the following digit.
//
////////////////////////////////////////////////////////////////////////////////




TEST_CLASS (TextEncodingTests)
{
public:

    static constexpr unsigned  kWindows1252 = 1252;
    static constexpr unsigned  kUtf8        = 65001;

    static std::wstring Widen (const std::string & text)
    {
        return std::wstring (text.begin(), text.end());
    }

    TEST_METHOD (Convert_TurnsTheCp1252NameIntoUtf8_RatherThanLeavingAStrayByte)
    {
        std::string  cp1252 = "Space Quarks (1981)(Br\xF8" "derbund)";
        std::string  utf8   = TextEncoding::Convert (cp1252, kWindows1252, kUtf8);

        //  U+00F8 is two bytes in UTF-8, so the result is one byte longer and
        //  the pair sits exactly where the single byte was.
        Assert::AreEqual (cp1252.size() + 1, utf8.size(),
                          L"the o-slash becomes a two-byte sequence");
        Assert::AreEqual (std::string ("Space Quarks (1981)(Br\xC3\xB8" "derbund)"), utf8,
                          L"and the rest of the name is untouched");
    }

    TEST_METHOD (Convert_ComesBackAgain_SoNothingIsLostOnTheWayOut)
    {
        std::string  cp1252 = "Br\xF8" "derbund";
        std::string  utf8   = TextEncoding::Convert (cp1252, kWindows1252, kUtf8);
        std::string  back   = TextEncoding::Convert (utf8,   kUtf8, kWindows1252);

        Assert::AreEqual (cp1252, back, L"the conversion is reversible for a name it can represent");
    }

    TEST_METHOD (Convert_LeavesAsciiExactlyAsItWas)
    {
        std::string  path = "C:\\disks\\casso-rocks.dsk: does not have a DOS or ProDOS file system";

        Assert::AreEqual (path, TextEncoding::Convert (path, kWindows1252, kUtf8),
                          L"the overwhelmingly common case must cost nothing");
    }

    TEST_METHOD (Convert_BetweenIdenticalCodePages_ReturnsTheInput)
    {
        //  Not an optimization so much as a guarantee: a console already set to
        //  the process's own code page must not have its bytes rewritten, or a
        //  name that was correct would be put through a lossy round trip for no
        //  reason at all.
        std::string  text = "\xF8\xFE\xDF";

        Assert::AreEqual (text, TextEncoding::Convert (text, kWindows1252, kWindows1252));
        Assert::AreEqual (std::string(), TextEncoding::Convert ("", kWindows1252, kUtf8));
    }

    TEST_METHOD (Convert_SubstitutesWhatTheTargetCannotSpell_RatherThanDroppingIt)
    {
        //  A Greek letter has no CP-1252 spelling. `?` is what the console would
        //  have drawn anyway, and a name silently missing a letter is harder to
        //  recognize than one carrying a question mark.
        std::string  utf8   = "delta \xCE\x94 here";
        std::string  cp1252 = TextEncoding::Convert (utf8, kUtf8, kWindows1252);

        Assert::AreEqual (std::string ("delta ? here"), cp1252,
                          (L"got: " + Widen (cp1252)).c_str());
    }

    TEST_METHOD (Convert_IntoUtf8_ProducesOutput_RatherThanFailingOverASubstitute)
    {
        //  WideCharToMultiByte REFUSES outright when a default character is
        //  supplied for UTF-8, and returns nothing at all. A single call shape
        //  that named one unconditionally would turn every conversion for a
        //  UTF-8 console into an empty string -- which reads exactly like a
        //  message the program never produced.
        std::string  utf8 = TextEncoding::Convert ("plain ascii", kWindows1252, kUtf8);

        Assert::AreEqual (std::string ("plain ascii"), utf8,
                          L"converting into UTF-8 must not come back empty");
    }

    TEST_METHOD (NarrowToConsole_IsTheIdentity_WhenTheTwoCodePagesAgree)
    {
        std::string  text = "nothing to convert";

        //  Whatever this machine's console is set to, plain ASCII survives it.
        Assert::AreEqual (text, TextEncoding::NarrowToConsole (text));

        Assert::IsTrue (TextEncoding::GetConsoleCodePage() != 0,
                        L"a code page is always returned, console or not");
        Assert::IsTrue (TextEncoding::GetNarrowCodePage() != 0,
                        L"and so is the process's own");
    }
};
