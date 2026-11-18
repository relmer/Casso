#include "Pch.h"

#include "../EhmTestHelper.h"
#include "../TestHelpers.h"
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




    //  AssembleShim fails a shim whose resident code extends to the program's
    //  tables at $4000; its startup part, run once, may go past them.
    TEST_METHOD (Shim_Assembles_WithItsResidentCodeBelowTheProgramsTables)
    {
        std::vector<Byte>  bytes;
        std::string        errors;
        HRESULT            hr = LinkProgram::AssembleShim (bytes, errors);

        Assert::IsTrue (SUCCEEDED (hr), TextEncoding::NarrowToWide (errors).c_str());
        Assert::IsTrue (errors.empty());
        Assert::IsFalse (bytes.empty());
        Assert::IsTrue (bytes.size() <= LinkProgram::kShimLimit, L"the shim and its startup part, below $4400");
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


    //  The cassette build, in its debug form too, ends below the shim's copy
    //  of the zero page, as the other builds do.
    TEST_METHOD (TheCassetteProgram_Assembles)
    {
        for (bool faults : { false, true })
        {
            std::vector<Byte>  bytes;
            std::string        errors;
            HRESULT            hr = LinkProgram::AssembleProgram (bytes, errors, LinkChip::Cassette, faults);

            Assert::IsTrue (SUCCEEDED (hr), TextEncoding::NarrowToWide (errors).c_str());
            Assert::IsTrue (LinkProgram::kProgramAddress + bytes.size() <= LinkProgram::kZeroPageSave);
        }
    }




    //  CASSOLINK: the installer at $2000, the driver after it within its
    //  two pages, and the relocation list after the driver.
    TEST_METHOD (TheVirtualDrive_Assembles_WithTheDriverInItsTwoPages)
    {
        std::vector<Byte>  file;
        std::string        errors;
        Byte               control = 0;
        HRESULT            hr      = S_OK;

        Assert::IsTrue (LinkProgram::GetSpeedControl (115200, control));

        hr = LinkProgram::BuildVirtualDrive (control, file, errors);
        Assert::IsTrue (SUCCEEDED (hr), TextEncoding::NarrowToWide (errors).c_str());
        Assert::IsTrue (file.size() > LinkProgram::kDriverLimit);
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

        at = FindReady (bytes, 0x0B);
        Assert::IsTrue (at + 12 <= bytes.size());
        Assert::AreEqual<int> (LinkProgram::kProgramVersion & 0xFF, bytes[at + 5]);
        Assert::AreEqual<int> (LinkProgram::kProgramVersion >> 8,   bytes[at + 6]);
        Assert::AreEqual<int> (LinkProgram::kShimVersion,           bytes[at + 7]);
        Assert::AreEqual<int> (11, bytes[at + 1], L"versions, the drives, the ROM's five identification bytes and the program's CRC, set at startup with the EOR");
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

        at = FindReady (program, 0x0B);
        Assert::IsTrue (at + 12 <= program.size());
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
        Assert::IsFalse (announcement.udp, L"version 1 is TCP");

        datagram[9] = LinkAnnouncement::kVersionUdp;
        Assert::IsTrue (LinkAnnouncement::Parse (datagram, announcement), L"the network shim's version");
        Assert::IsTrue (announcement.udp, L"version 2 is UDP");

        datagram[9] = 3;
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




    //  An Apple keeps the program it has while its version matches the
    //  host's, so a change to the program with no new version never reaches
    //  it. These are the hashes of the builds at kProgramVersion: a program
    //  changed with no new version fails here.
    TEST_METHOD (Program_ChangesOnlyWithANewVersion)
    {
        static constexpr Word      s_kHashedVersion = 104;
        static constexpr uint64_t  s_kAciaHash      = 0x2D4A8FDD739AD56E;
        static constexpr uint64_t  s_kW5100Hash     = 0x28370183098D6DAB;

        std::vector<Byte>  acia;
        std::vector<Byte>  w5100;
        std::string        errors;
        HRESULT            hr        = LinkProgram::AssembleProgram (acia, errors, LinkChip::Acia);
        uint64_t           aciaHash  = 14695981039346656037ull;
        uint64_t           w5100Hash = 14695981039346656037ull;

        Assert::IsTrue (SUCCEEDED (hr), TextEncoding::NarrowToWide (errors).c_str());

        hr = LinkProgram::AssembleProgram (w5100, errors, LinkChip::W5100);
        Assert::IsTrue (SUCCEEDED (hr), TextEncoding::NarrowToWide (errors).c_str());

        for (Byte value : acia)
        {
            aciaHash = (aciaHash ^ value) * 1099511628211ull;
        }

        for (Byte value : w5100)
        {
            w5100Hash = (w5100Hash ^ value) * 1099511628211ull;
        }

        Assert::AreEqual<int> (s_kHashedVersion, LinkProgram::kProgramVersion,
                               std::format (L"A new version has new hashes: set the version, then the hashes to 0x{:016X} and 0x{:016X}.", aciaHash, w5100Hash).c_str());
        Assert::IsTrue (aciaHash == s_kAciaHash && w5100Hash == s_kW5100Hash,
                        std::format (L"The program changed with no new version. Raise LinkProgram::kProgramVersion, then set these to 0x{:016X} and 0x{:016X}.", aciaHash, w5100Hash).c_str());
    }




    //  The SOS build ends below its buffers, with room for its speed's byte,
    //  and its Ready has the SOS flag and no Disk II drives.
    TEST_METHOD (TheSosProgram_AssemblesBelowItsBuffers_WithTheSosFlagInReady)
    {
        std::vector<Byte>  bytes;
        std::string        errors;
        HRESULT            hr = LinkProgram::AssembleProgram (bytes, errors, LinkChip::Sos);
        size_t             at = 0;

        Assert::IsTrue (SUCCEEDED (hr), TextEncoding::NarrowToWide (errors).c_str());
        Assert::IsTrue (LinkProgram::kSosAddress + bytes.size() < LinkProgram::kSosBufferBase);

        at = FindReady (bytes, 0x0B);
        Assert::IsTrue (at + 16 <= bytes.size());
        Assert::AreEqual<int> (LinkProgram::kProgramVersion & 0xFF, bytes[at + 5]);
        Assert::AreEqual<int> (LinkProgram::kProgramVersion >> 8,   bytes[at + 6]);
        Assert::AreEqual<int> (LinkProgram::kSosReadyFlag,          bytes[at + 7]);
        Assert::AreEqual<int> (0,                                   bytes[at + 8], L"no Disk II drives");
    }


    //  The SOS program computes a CRC of itself at startup, and the host
    //  compares it with the CRC of the image it assembles, so the two must be
    //  equal. The
    //  program runs here up to its first SOS call, the BRK that opens the
    //  console, after its CRC is in Ready.
    TEST_METHOD (TheSosProgram_ComputesTheCrcTheHostDoes)
    {
        static constexpr int  kStepLimit = 2000000;
        std::vector<Byte>     bytes;
        std::string           errors;
        TestCpu               cpu;
        size_t                at         = 0;
        int                   steps      = 0;
        Word                  crc        = 0;
        Word                  reported   = 0;

        AssertSucceeded (LinkProgram::AssembleProgram (bytes, errors, LinkChip::Sos));

        cpu.InitForTest (LinkProgram::kSosAddress);

        for (size_t i = 0; i < bytes.size(); i++)
        {
            cpu.Poke ((Word) (LinkProgram::kSosAddress + i), bytes[i]);
        }

        while (cpu.Peek (cpu.RegPC()) != 0x00 && steps < kStepLimit)
        {
            cpu.Step();
            steps++;
        }

        Assert::IsTrue (steps < kStepLimit, L"the program reaches its first SOS call");

        at       = FindReady (bytes, 0x0B);
        crc      = CassoLinkCodec::ComputeCrc (bytes);
        reported = (Word) (cpu.Peek ((Word) (LinkProgram::kSosAddress + at + 14)) | (cpu.Peek ((Word) (LinkProgram::kSosAddress + at + 15)) << 8));

        Assert::AreEqual<int> (crc, reported);
    }


    //  SOS.INTERP: "SOS NTRP", no optional header, the loading address and
    //  the code's length, which covers the program, the speed after it and
    //  the buffers.
    TEST_METHOD (TheSosInterpreter_HasApplesHeader_TheSpeed_AndRoomForItsBuffers)
    {
        static constexpr size_t  kHeader = 14;
        std::vector<Byte>        program;
        std::vector<Byte>        file;
        std::string              errors;
        Byte                     control = 0;
        size_t                   length  = 0;

        Assert::IsTrue (LinkProgram::GetSpeedControl (9600, control));
        AssertSucceeded (LinkProgram::AssembleProgram (program, errors, LinkChip::Sos));
        AssertSucceeded (LinkProgram::BuildSosInterpreter (control, file, errors));

        length = (size_t) (file[12] | (file[13] << 8));

        Assert::AreEqual (std::string ("SOS NTRP"), std::string (file.begin(), file.begin() + 8));
        Assert::AreEqual<int> (0, file[8] | (file[9] << 8), L"no optional header");
        Assert::AreEqual<int> (LinkProgram::kSosAddress, file[10] | (file[11] << 8));
        Assert::AreEqual<size_t> ((size_t) (LinkProgram::kSosEnd - LinkProgram::kSosAddress), length);
        Assert::AreEqual<size_t> (kHeader + length, file.size());
        Assert::IsTrue (std::equal (program.begin(), program.end(), file.begin() + kHeader), L"the program as the host assembles it, so the CRC in its Ready matches");
        Assert::AreEqual<int> (control, file[kHeader + program.size()], L"the speed right after the program");
    }


    //  .RS232 has no rate for the 6551's external clock, the //c's 115,200.
    TEST_METHOD (TheSosInterpreter_FailsForTheExternalClocksRate)
    {
        std::vector<Byte>  file;
        std::string        errors;
        Byte               control = 0;

        Assert::IsTrue (LinkProgram::GetSpeedControl (115200, control));
        Assert::AreEqual (HRESULT_FROM_WIN32 (ERROR_NOT_SUPPORTED), LinkProgram::BuildSosInterpreter (control, file, errors));
        Assert::IsTrue (file.empty());
    }


    //  The SOS build reports the block module's id and version as loaded,
    //  so the host never sends it; the SOS build's block module is its
    //  header, with no requests and a CRC that checks.
    TEST_METHOD (TheSosBlockModule_HasTheBlockModulesIdAndVersion)
    {
        std::vector<Byte>  acia;
        std::vector<Byte>  sos;
        Word               crc = 0;

        AssertSucceeded (LinkProgram::BuildBlockModule (acia, LinkChip::Acia));
        AssertSucceeded (LinkProgram::BuildBlockModule (sos, LinkChip::Sos));

        Assert::AreEqual<int> (LinkProgram::kBlockModuleId,      acia[2]);
        Assert::AreEqual<int> (LinkProgram::kBlockModuleVersion, acia[3]);
        Assert::IsTrue (std::equal (acia.begin(), acia.begin() + 6, sos.begin()), L"the magic, id, version and program version");
        Assert::AreEqual<size_t> (sos.size(), (size_t) (sos[LinkProgram::kModuleLength] | (sos[LinkProgram::kModuleLength + 1] << 8)));
        Assert::AreEqual<int> (0, sos[LinkProgram::kModuleCrcStart], L"no requests");

        crc = CassoLinkCodec::ComputeCrc (std::span<const Byte> (sos).subspan (LinkProgram::kModuleCrcStart));
        Assert::AreEqual<int> (crc, sos[LinkProgram::kModuleCrcOffset] | (sos[LinkProgram::kModuleCrcOffset + 1] << 8));
    }
};
