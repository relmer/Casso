#include "Pch.h"

#include "../EhmTestHelper.h"
#include "CassoLink/CassoLinkCodec.h"
#include "CassoLink/LinkProgram.h"
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
    static size_t  FindReady (const std::vector<Byte> & image, Byte length = 0x04)
    {
        const Byte  header[] = { CassoLinkCodec::kStart, length, 0x00, 0xD2 };
        auto        it       = std::search (image.begin(), image.end(), std::begin (header), std::end (header));

        return (size_t) (it - image.begin());
    }




    TEST_METHOD (Shim_AssemblesBelowTheProgramsTables)
    {
        std::vector<Byte>  bytes;
        std::string        errors;
        HRESULT            hr = LinkProgram::AssembleShim (bytes, errors);

        Assert::IsTrue (SUCCEEDED (hr), TextEncoding::NarrowToWide (errors).c_str());
        Assert::IsTrue (errors.empty());
        Assert::IsFalse (bytes.empty());
        Assert::IsTrue (LinkProgram::kShimAddress + bytes.size() <= 0x4000u);
    }




    TEST_METHOD (Program_Assembles)
    {
        std::vector<Byte>  bytes;
        std::string        errors;
        HRESULT            hr = LinkProgram::AssembleProgram (bytes, errors);

        Assert::IsTrue (SUCCEEDED (hr), TextEncoding::NarrowToWide (errors).c_str());
        Assert::IsTrue (errors.empty());
        Assert::IsFalse (bytes.empty());
        Assert::IsTrue (LinkProgram::kProgramAddress + bytes.size() <= LinkProgram::kShimAddress, L"the program ends below the shim");

        //  The version the shim draws, at $0803.
        Assert::AreEqual<int> (LinkProgram::kProgramVersion & 0xFF, bytes[3]);
        Assert::AreEqual<int> (LinkProgram::kProgramVersion >> 8,   bytes[4]);
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
        Assert::AreEqual<int> (CassoLinkCodec::ComputeEor (std::span<const Byte> (bytes).subspan (at + 5, 4)), bytes[at + 9]);
    }




    TEST_METHOD (Program_ReadyHasTheCoreVersions)
    {
        std::vector<Byte>  bytes;
        std::string        errors;
        HRESULT            hr  = LinkProgram::AssembleProgram (bytes, errors);
        size_t             at  = 0;

        Assert::IsTrue (SUCCEEDED (hr));

        at = FindReady (bytes, 0x09);
        Assert::IsTrue (at + 10 <= bytes.size());
        Assert::AreEqual<int> (LinkProgram::kProgramVersion & 0xFF, bytes[at + 5]);
        Assert::AreEqual<int> (LinkProgram::kProgramVersion >> 8,   bytes[at + 6]);
        Assert::AreEqual<int> (LinkProgram::kShimVersion,           bytes[at + 7]);
        Assert::AreEqual<int> (9, bytes[at + 1], L"versions, the drives, and the ROM's five identification bytes, set at startup with the EOR");
    }



    TEST_METHOD (BuildShim_WritesTheSpeedAndItsText)
    {
        std::vector<Byte>  assembled;
        std::vector<Byte>  fast;
        std::string        errors;
        Byte               control = 0;

        AssertSucceeded (LinkProgram::AssembleShim (assembled, errors));
        AssertSucceeded (LinkProgram::BuildShim (115200, fast));

        Assert::IsTrue (LinkProgram::GetSpeedControl (9600, control));
        Assert::AreEqual<int> (control, assembled[LinkProgram::kSpeedOffset], L"as assembled, 9,600 baud");
        Assert::AreEqual (std::string ("9600  "), std::string (assembled.begin() + LinkProgram::kSpeedTextOffset, assembled.begin() + LinkProgram::kSpeedTextOffset + LinkProgram::kSpeedTextLength));

        Assert::IsTrue (LinkProgram::GetSpeedControl (115200, control));
        Assert::AreEqual<int> (control, fast[LinkProgram::kSpeedOffset]);
        Assert::AreEqual (std::string ("115200"), std::string (fast.begin() + LinkProgram::kSpeedTextOffset, fast.begin() + LinkProgram::kSpeedTextOffset + LinkProgram::kSpeedTextLength));
        Assert::AreEqual (HRESULT (E_INVALIDARG), LinkProgram::BuildShim (300, fast), L"a speed the //c's port does not have");
    }



    TEST_METHOD (BuildShim_WritesTheSlot_AndRefusesOneNoCardHas)
    {
        std::vector<Byte>  shim;
        LinkShimOptions    options;

        AssertSucceeded (LinkProgram::BuildShim (115200, shim));
        Assert::AreEqual<int> (2, shim[LinkProgram::kSlotOffset], L"the //c's port 2, as assembled");

        options.slot = 5;
        AssertSucceeded (LinkProgram::BuildShim (options, shim));
        Assert::AreEqual<int> (5, shim[LinkProgram::kSlotOffset]);

        options.slot = 0;
        Assert::AreEqual (HRESULT (E_INVALIDARG), LinkProgram::BuildShim (options, shim));
        options.slot = 8;
        Assert::AreEqual (HRESULT (E_INVALIDARG), LinkProgram::BuildShim (options, shim));
    }



    TEST_METHOD (BootDisk_LoadsTheShimForItsSlot)
    {
        std::vector<Byte>  disk;
        std::vector<Byte>  shim;
        LinkShimOptions    options;
        bool               found = false;

        options.slot = 4;
        AssertSucceeded (LinkProgram::BuildBootDisk (options, disk));
        AssertSucceeded (LinkProgram::BuildShim (options, shim));

        found = std::search (disk.begin(), disk.end(), shim.begin(), shim.begin() + 0x40) != disk.end();
        Assert::IsTrue (found, L"the shim's header, with slot 4, is on the disk");
    }
};
