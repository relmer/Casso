#include "Pch.h"

#include "../EhmTestHelper.h"
#include "CassoLink/CassoLinkCodec.h"
#include "CassoLink/LinkAnnouncement.h"
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




    //  The host sends the program for the chip a Ready indicates, so a W5100
    //  build's Ready differs from a 6551 build's.
    TEST_METHOD (W5100Builds_SetTheReadyFlagInTheShimVersion)
    {
        std::vector<Byte>  shim;
        std::vector<Byte>  program;
        std::string        errors;
        size_t             at = 0;

        AssertSucceeded (LinkProgram::AssembleShim (shim, errors, LinkChip::W5100));
        AssertSucceeded (LinkProgram::AssembleProgram (program, errors, LinkChip::W5100));

        at = FindReady (shim);
        Assert::IsTrue (at + 10 <= shim.size());
        Assert::AreEqual<int> (LinkProgram::kShimVersion | LinkProgram::kW5100ReadyFlag, shim[at + 7]);
        Assert::AreEqual<int> (CassoLinkCodec::ComputeEor (std::span<const Byte> (shim).subspan (at + 5, 4)), shim[at + 9]);

        at = FindReady (program, 0x09);
        Assert::IsTrue (at + 10 <= program.size());
        Assert::AreEqual<int> (LinkProgram::kShimVersion | LinkProgram::kW5100ReadyFlag, program[at + 7]);
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



    TEST_METHOD (Universal_AssemblesBelowTheShimImages)
    {
        std::vector<Byte>  bytes;
        std::string        errors;
        HRESULT            hr = LinkProgram::AssembleUniversal (bytes, errors);

        Assert::IsTrue (SUCCEEDED (hr), std::format (L"{} bytes: {}", bytes.size(), TextEncoding::NarrowToWide (errors)).c_str());
        Assert::IsTrue (bytes.size() <= LinkProgram::kUniversalLimit);
        Assert::IsTrue (LinkProgram::kUniversalAddress + LinkProgram::kUniversalLimit <= LinkProgram::kUniversalAcia);
    }


    //  The universal disk has both shims, each with the header the host
    //  wrote: the 6551's speed, and the W5100's network.
    TEST_METHOD (UniversalBootDisk_HasBothShims)
    {
        std::vector<Byte>  disk;
        std::vector<Byte>  acia;
        std::vector<Byte>  w5100;
        LinkShimOptions    options;
        bool               hasAcia  = false;
        bool               hasW5100 = false;

        options.baud            = 19200;
        options.network.address = { 192, 168, 1, 77 };
        options.network.mac     = { 0x00, 0x08, 0xDC, 0x01, 0x02, 0x03 };
        options.network.port    = 6504;

        AssertSucceeded (LinkProgram::BuildUniversalBootDisk (options, disk));

        options.slot = 2;
        AssertSucceeded (LinkProgram::BuildShim (options, acia));
        options.chip = LinkChip::W5100;
        options.slot = 3;
        AssertSucceeded (LinkProgram::BuildShim (options, w5100));

        hasAcia  = std::search (disk.begin(), disk.end(), acia.begin(),  acia.begin()  + 0x40) != disk.end();
        hasW5100 = std::search (disk.begin(), disk.end(), w5100.begin(), w5100.begin() + 0x40) != disk.end();

        Assert::IsTrue (hasAcia,  L"the 6551 shim's header, at 19,200 baud");
        Assert::IsTrue (hasW5100, L"the W5100 shim's header, with its network");
    }


    TEST_METHOD (AnAnnouncement_IsParsed_AndAnythingElseIsNot)
    {
        std::vector<Byte>  datagram = { 'C', 'A', 'S', 'S', 'O', 'L', 'I', 'N', 'K', 1,
                                        192, 168, 1, 77, 0x19, 0x66, 0x00, 0x08, 0xDC, 0x01, 0x02, 0x03 };
        LinkAnnouncement   announcement;

        Assert::IsTrue (LinkAnnouncement::Parse (datagram, announcement));
        Assert::AreEqual (std::string ("192.168.1.77:6502"), announcement.GetAddressText());
        Assert::IsTrue (announcement.mac == std::array<Byte, 6> { 0x00, 0x08, 0xDC, 0x01, 0x02, 0x03 });

        datagram[9] = 2;
        Assert::IsFalse (LinkAnnouncement::Parse (datagram, announcement), L"another version");

        datagram[9] = 1;
        datagram[0] = 'X';
        Assert::IsFalse (LinkAnnouncement::Parse (datagram, announcement), L"not CASSOLINK");

        datagram[0] = 'C';
        datagram.pop_back();
        Assert::IsFalse (LinkAnnouncement::Parse (datagram, announcement), L"short");
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
