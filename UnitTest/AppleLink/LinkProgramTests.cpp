#include "Pch.h"

#include "AppleLink/AppleLinkCodec.h"
#include "AppleLink/LinkProgram.h"
#include "Core/TextEncoding.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  LinkProgramTests
//
//  The Apple side's sources assemble on every build, the shim fits where it
//  is loaded, and the Ready packet each one sends has the versions the core
//  compares against.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (LinkProgramTests)
{
public:

    //  The Ready packet in an assembled image: the envelope, then the program
    //  version, the shim version and the drive map.
    static size_t  FindReady (const std::vector<Byte> & image)
    {
        const Byte  header[] = { AppleLinkCodec::kStart, 0x04, 0x00, 0xD2 };
        auto        it       = std::search (image.begin(), image.end(), std::begin (header), std::end (header));

        return (size_t) (it - image.begin());
    }




    TEST_METHOD (Shim_AssemblesBelowTheMonitorVectors)
    {
        std::vector<Byte>  bytes;
        std::string        errors;
        HRESULT            hr = LinkProgram::AssembleShim (bytes, errors);

        Assert::IsTrue (SUCCEEDED (hr), TextEncoding::NarrowToWide (errors).c_str());
        Assert::IsTrue (errors.empty());
        Assert::IsFalse (bytes.empty());
        Assert::IsTrue (LinkProgram::kShimAddress + bytes.size() <= 0x03F0u);
    }




    TEST_METHOD (Program_Assembles)
    {
        std::vector<Byte>  bytes;
        std::string        errors;
        HRESULT            hr = LinkProgram::AssembleProgram (bytes, errors);

        Assert::IsTrue (SUCCEEDED (hr), TextEncoding::NarrowToWide (errors).c_str());
        Assert::IsTrue (errors.empty());
        Assert::IsFalse (bytes.empty());
        Assert::IsTrue (LinkProgram::kProgramAddress + bytes.size() <= 0x4000u);
    }




    TEST_METHOD (Shim_ReadyHasNoProgramAndTheCoreShimVersion)
    {
        std::vector<Byte>  bytes;
        std::string        errors;
        HRESULT            hr  = LinkProgram::AssembleShim (bytes, errors);
        size_t             at  = 0;

        Assert::IsTrue (SUCCEEDED (hr));

        at = FindReady (bytes);
        Assert::IsTrue (at + 10 <= bytes.size());
        Assert::AreEqual<int> (0, bytes[at + 5]);
        Assert::AreEqual<int> (0, bytes[at + 6]);
        Assert::AreEqual<int> (LinkProgram::kShimVersion, bytes[at + 7]);
        Assert::AreEqual<int> (AppleLinkCodec::ComputeEor (std::span<const Byte> (bytes).subspan (at + 5, 4)), bytes[at + 9]);
    }




    TEST_METHOD (Program_ReadyHasTheCoreVersions)
    {
        std::vector<Byte>  bytes;
        std::string        errors;
        HRESULT            hr  = LinkProgram::AssembleProgram (bytes, errors);
        size_t             at  = 0;

        Assert::IsTrue (SUCCEEDED (hr));

        at = FindReady (bytes);
        Assert::IsTrue (at + 10 <= bytes.size());
        Assert::AreEqual<int> (LinkProgram::kProgramVersion & 0xFF, bytes[at + 5]);
        Assert::AreEqual<int> (LinkProgram::kProgramVersion >> 8,   bytes[at + 6]);
        Assert::AreEqual<int> (LinkProgram::kShimVersion,           bytes[at + 7]);
        Assert::AreEqual<int> (AppleLinkCodec::ComputeEor (std::span<const Byte> (bytes).subspan (at + 5, 4)), bytes[at + 9]);
    }
};
