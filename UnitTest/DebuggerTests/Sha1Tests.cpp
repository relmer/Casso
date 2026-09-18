#include "Pch.h"

#include "Sha1.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  Sha1Tests
//
//  The hash a debug file records for each source file, so a file found on disk
//  can be told to be the one the program was built from. The digests are RFC
//  3174's test vectors; the line-ending rule is the debug-file contract's: a
//  file checked out with CRLF and one with LF hash the same.
//
////////////////////////////////////////////////////////////////////////////////

namespace DebuggerTests
{
    TEST_CLASS (Sha1Tests)
    {
    public:

        static std::string HashOf (std::string_view text)
        {
            std::span<const uint8_t>  bytes ((const uint8_t *) text.data(), text.size());



            return Sha1::ToHex (Sha1::Compute (bytes));
        }



        TEST_METHOD (Rfc3174_Abc)
        {
            Assert::AreEqual (std::string ("a9993e364706816aba3e25717850c26c9cd0d89d"), HashOf ("abc"));
        }


        TEST_METHOD (Rfc3174_TwoBlockMessage)
        {
            Assert::AreEqual (std::string ("84983e441c3bd26ebaae4aa1f95129e5e54670f1"),
                              HashOf ("abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq"));
        }


        TEST_METHOD (Rfc3174_OneMillionAs)
        {
            std::string  text (1000000, 'a');



            Assert::AreEqual (std::string ("34aa973cd4c4daa4f61eeb2bdbad27316534016f"), HashOf (text));
        }


        TEST_METHOD (EmptyInput)
        {
            Assert::AreEqual (std::string ("da39a3ee5e6b4b0d3255bfef95601890afd80709"), HashOf (""));
        }


        TEST_METHOD (LengthsAroundTheBlockBoundary)
        {
            //  55, 56 and 64 bytes are where the padding changes shape: 55 fits
            //  the length in the same block, 56 and 64 do not.
            Assert::AreEqual (std::string ("c1c8bbdc22796e28c0e15163d20899b65621d65a"), HashOf (std::string (55, 'a')));
            Assert::AreEqual (std::string ("c2db330f6083854c99d4b5bfb6e8f29f201be699"), HashOf (std::string (56, 'a')));
            Assert::AreEqual (std::string ("0098ba824b5c16427bd7a1122a5a442a25ec644d"), HashOf (std::string (64, 'a')));
        }


        TEST_METHOD (LineEndingsNormalizeToLf)
        {
            Assert::AreEqual (std::string ("a\nb\nc\n"), Sha1::NormalizeLineEndings ("a\r\nb\rc\n"));
            Assert::AreEqual (std::string ("\n\n"),      Sha1::NormalizeLineEndings ("\r\r"),   L"a lone CR is a line end");
            Assert::AreEqual (std::string ("\n"),        Sha1::NormalizeLineEndings ("\r\n"),   L"CR LF is one line end, not two");
        }


        TEST_METHOD (TextHashIgnoresLineEndings)
        {
            Assert::AreEqual (Sha1::ComputeTextHex ("LDA #1\nRTS\n"), Sha1::ComputeTextHex ("LDA #1\r\nRTS\r\n"));
            Assert::AreNotEqual (Sha1::ComputeTextHex ("LDA #1\n"), Sha1::ComputeTextHex ("LDA #2\n"));
        }
    };
}
