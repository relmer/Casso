#include "Pch.h"

#include "EhmTestHelper.h"
#include "FixtureProvider.h"
#include "TestMachine.h"
#include "TextScreenScraper.h"
#include "Core/TextEncoding.h"
#include "CassoLink/InMemoryCassoLinkFolder.h"
#include "CassoLink/CassoLinkHost.h"
#include "CassoLink/LinkFault.h"
#include "CassoLink/LinkProgram.h"
#include "Devices/Disk/DiskImageStore.h"
#include "Machines/Apple2/Common/NibblizationLayer.h"
#include "Seams/InProcessByteStream.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkFaultTests
//
//  Faults injected into Casso's client on an emulated //c, running the
//  program built with the Fault command, and how the link recovers: a byte
//  the Apple sends flipped, left out or stalled, a sector not read, read
//  wrong with a CRC that matches, or given a wrong CRC, and a byte the PC
//  sends flipped or left out. A sector is the physical sector, as in
//  CassoLinkSectorTests.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (CassoLinkFaultTests)
{
public:

    static constexpr uint64_t  kFrame       = 17030;
    static constexpr uint64_t  kBudget      = 400'000'000;
    static constexpr Byte      kFastControl = 0x10;     // 115,200 baud
    static constexpr Byte      kTrack       = 17;
    static constexpr size_t    kSector      = 3;

    struct Rig
    {
        TestMachine                           machine { "Apple2c" };
        InMemoryCassoLinkFolder               folder;
        std::shared_ptr<InProcessByteStream>  machineEnd;
        std::shared_ptr<InProcessByteStream>  hostEnd;
        std::unique_ptr<CassoLinkHost>        host;
        std::vector<Byte>                     dos;
        std::vector<Byte>                     prodos;
        std::vector<std::string>              activity;
        std::mutex                            lock;
        LinkFault::Source                     armed = LinkFault::Source::Pc;
        uint64_t                              spent = 0;     // the last Run's cycles

        Rig()
        {
            FixtureProvider       fixtures;
            std::vector<uint8_t>  bytes;
            std::vector<Byte>     shim;
            std::string           errors;
            Word                  at = LinkProgram::kShimAddress;



            AssertSucceeded (fixtures.OpenFixture ("CassoExplorer/dos33.dsk", bytes));
            dos.assign (bytes.begin(), bytes.end());
            AssertSucceeded (fixtures.OpenFixture ("CassoExplorer/prodos.po", bytes));
            prodos.assign (bytes.begin(), bytes.end());

            machine.SetVideoTiming (std::make_unique<VideoTiming>());
            machine.GetCpu()->SetVideoTiming (machine.GetVideoTiming());
            machine.PowerCycle();

            AssertSucceeded (machine.GetDiskStore().MountFromBytes (6, 0, "", DiskFormat::Dsk, dos));
            AssertSucceeded (machine.GetDiskStore().MountFromBytes (6, 1, "", DiskFormat::Po, prodos));

            InProcessByteStream::CreatePair (machineEnd, hostEnd);
            Assert::AreEqual (S_OK, machine.AttachSerialStream (2, machineEnd));

            host = std::make_unique<CassoLinkHost> (hostEnd, folder, [] () { return std::array<Byte, 4> {}; });
            host->SetOperationTimeout (60000);
            host->SetFaultProgram (true);
            host->SetActivitySink ([this] (const std::string & text) { std::lock_guard<std::mutex> guard (lock); activity.push_back (text); });

            Assert::AreEqual (S_OK, LinkProgram::AssembleShim (shim, errors));

            for (Byte b : shim)
            {
                machine.GetCpu()->WriteByte (at++, b);
            }

            machine.GetCpu()->SetPC (LinkProgram::kShimAddress);
        }

        //  Steps the machine while the host runs `operation`.
        HRESULT  Run (std::function<HRESULT()> operation)
        {
            auto  result = std::async (std::launch::async, operation);



            spent = 0;

            while (result.wait_for (std::chrono::seconds (0)) != std::future_status::ready && spent < kBudget)
            {
                machine.RunCycles (kFrame);
                spent += kFrame;
            }

            Assert::IsTrue (spent < kBudget, L"the operation finishes");
            return result.get();
        }

        //  The program with the Fault command loaded, and the line at
        //  115,200 baud.
        void  Connect()
        {
            std::vector<Byte>  program;
            std::string        errors;



            Assert::AreEqual (S_OK, LinkProgram::AssembleProgram (program, errors, LinkChip::Acia, true));
            Assert::AreEqual (S_OK, Run ([this] () { return host->SetSpeed (kFastControl); }));
            Assert::AreEqual<int> (CassoLinkCodec::ComputeCrc (program), host->GetClient().programCrc, L"the program with the Fault command");
        }

        void  Arm (const char * text)
        {
            LinkFault  fault;



            Assert::AreEqual (S_OK, LinkFault::ParseOne (text, fault));
            Assert::AreEqual (S_OK, host->ArmFault (fault));

            armed = fault.source;
        }

        //  The fault armed was applied: the PC's injected, or the Apple's
        //  sent to the Apple.
        void  AssertApplied()
        {
            Assert::IsTrue (HasActivity (armed == LinkFault::Source::Pc ? "Injected a fault" : "Sent a fault to the Apple"), L"the fault was applied");
        }

        std::vector<Byte>  Read (Byte drive, Byte track, std::span<const Byte> sectors, Byte expectedError = CassoLinkHost::kErrorNone)
        {
            std::vector<Byte>  data;
            CassoLinkStatus    status;



            Assert::AreEqual (S_OK, Run ([&] () { return host->ReadSectors (drive, track, sectors, data, status); }));
            Assert::AreEqual<int> (expectedError, status.error);
            return data;
        }

        std::vector<Byte>  Image (int drive)
        {
            std::vector<Byte>  bytes;



            AssertSucceeded (machine.GetDiskStore().GetImage (6, drive)->Serialize (bytes));
            return bytes;
        }

        //  The status screen, after a few frames to draw it.
        std::string  Screen()
        {
            std::string  text;



            for (int frame = 0; frame < 30; frame++)
            {
                machine.RunCycles (kFrame);
            }

            for (const std::string & row : TextScreenScraper::Scrape (machine))
            {
                text += row + "\n";
            }

            return text;
        }

        bool  HasActivity (const char * text)
        {
            std::lock_guard<std::mutex>  guard (lock);



            return std::any_of (activity.begin(), activity.end(), [text] (const std::string & line) { return line.find (text) != std::string::npos; });
        }

        //  Each of track 17's sectors as drive 1's image has it.
        void  AssertTrackMatches (const std::vector<Byte> & data)
        {
            for (int p = 0; p < 16; p++)
            {
                Assert::IsTrue (SameSector (data, (size_t) p, dos, DosOffset (kTrack, p)), std::format (L"sector {}", p).c_str());
            }
        }
    };

    static std::vector<Byte>  AllSectors()
    {
        std::vector<Byte>  sectors (16);



        for (size_t i = 0; i < sectors.size(); i++)
        {
            sectors[i] = (Byte) i;
        }

        return sectors;
    }

    static size_t  DosOffset (int track, int physical)
    {
        return (size_t) (track * 16 + NibblizationLayer::GetDosFileIndexForPhysicalSector (physical)) * 256;
    }

    static bool  SameSector (const std::vector<Byte> & data, size_t index, const std::vector<Byte> & image, size_t offset)
    {
        return std::equal (data.begin() + (ptrdiff_t) (index * 256), data.begin() + (ptrdiff_t) ((index + 1) * 256), image.begin() + (ptrdiff_t) offset);
    }

    static std::vector<Byte>  Pattern (size_t size, Byte seed)
    {
        std::vector<Byte>  bytes (size);



        for (size_t i = 0; i < size; i++)
        {
            bytes[i] = (Byte) (seed + i * 13 + (i >> 8));
        }

        return bytes;
    }



    //  A byte of a sector the Apple sends, flipped: the packet fails its
    //  check, and the track is read again.
    TEST_METHOD (AppleFlip_InASector_IsReadAgain)
    {
        Rig  rig;



        rig.Connect();
        rig.Arm ("apple:flip@1000");

        rig.AssertTrackMatches (rig.Read (1, kTrack, AllSectors()));
        Assert::IsTrue (rig.HasActivity ("damaged"), L"read again");
        rig.AssertApplied();
    }



    TEST_METHOD (AppleDrop_InASector_IsReadAgain)
    {
        Rig  rig;



        rig.Connect();
        rig.Arm ("apple:drop@1000");

        rig.AssertTrackMatches (rig.Read (1, kTrack, AllSectors()));
        Assert::IsTrue (rig.HasActivity ("damaged"), L"read again");
        rig.AssertApplied();
    }



    //  A pause shorter than the host waits is no fault of the link's: the
    //  read takes the second longer, and isn't read again.
    TEST_METHOD (AppleStall_InASector_IsWaitedFor)
    {
        constexpr uint64_t  kSecondOfCycles = 1'000'000;
        Rig                 rig;
        uint64_t            plain           = 0;



        //  The second read's time, with the drive's motor already running.
        rig.Connect();
        rig.AssertTrackMatches (rig.Read (1, kTrack, AllSectors()));
        rig.AssertTrackMatches (rig.Read (1, kTrack, AllSectors()));
        plain = rig.spent;

        rig.Arm ("apple:stall@1000:4");

        rig.AssertTrackMatches (rig.Read (1, kTrack, AllSectors()));
        Assert::IsTrue (rig.spent >= plain + kSecondOfCycles, std::format (L"{} cycles with the stall, {} without", rig.spent, plain).c_str());
        Assert::IsFalse (rig.HasActivity ("damaged"), L"not read again");
        rig.AssertApplied();
    }



    //  The fault is for the request after its #: the first read is whole, and
    //  the second is read again.
    TEST_METHOD (AppleFlip_InTheSecondRequest_DamagesThatReply)
    {
        Rig  rig;



        rig.Connect();
        rig.Arm ("apple:flip@1000#1");

        rig.AssertTrackMatches (rig.Read (1, kTrack, AllSectors()));
        Assert::IsFalse (rig.HasActivity ("damaged"), L"the first request");

        rig.AssertTrackMatches (rig.Read (1, kTrack, AllSectors()));
        Assert::IsTrue (rig.HasActivity ("damaged"), L"the second request");
        rig.AssertApplied();
    }



    //  A sector the Apple reports as not read ends the request with its
    //  error, and the next read of the track finds it.
    TEST_METHOD (AppleUnreadable_EndsTheReadWithItsError_AndTheNextReadFindsIt)
    {
        Rig  rig;



        rig.Connect();
        rig.Arm ("apple:unreadable@T17S3");

        rig.Read (1, kTrack, AllSectors(), CassoLinkHost::kErrorNotFound);
        rig.AssertTrackMatches (rig.Read (1, kTrack, AllSectors()));
        rig.AssertApplied();
    }



    //  A sector read wrong with a CRC that matches passes the line's checks;
    //  only reading it again finds the difference, as verified reads do.
    TEST_METHOD (AppleBadData_PassesTheLine_AndDiffersFromTheNextRead)
    {
        Rig                rig;
        std::vector<Byte>  first;
        std::vector<Byte>  second;
        size_t             differ = 0;



        rig.Connect();
        rig.Arm ("apple:baddata@T17S3");

        first  = rig.Read (1, kTrack, AllSectors());
        second = rig.Read (1, kTrack, AllSectors());

        rig.AssertTrackMatches (second);
        Assert::IsFalse (rig.HasActivity ("damaged"), L"no packet arrived damaged");

        for (size_t i = 0; i < first.size(); i++)
        {
            differ += (first[i] != second[i]) ? 1 : 0;
        }

        Assert::AreEqual ((size_t) 1, differ, L"one byte of sector 3");
        Assert::IsFalse (SameSector (first, kSector, rig.dos, DosOffset (kTrack, (int) kSector)));
        rig.AssertApplied();
    }



    TEST_METHOD (AppleBadCrc_IsTheSectorsAlone)
    {
        Rig                rig;
        std::vector<Byte>  data;
        std::vector<Word>  crcs;
        std::vector<bool>  read;
        CassoLinkStatus    status;
        Word               crc = 0;



        rig.Connect();
        data = rig.Read (1, kTrack, AllSectors());

        rig.Arm ("apple:badcrc@T17S3");
        Assert::AreEqual (S_OK, rig.Run ([&] () { return rig.host->ReadCrcs (1, kTrack, AllSectors(), crcs, read, status); }));

        for (size_t i = 0; i < crcs.size(); i++)
        {
            crc = CassoLinkCodec::ComputeCrc (std::span<const Byte> (data.data() + i * 256, 256));

            Assert::IsTrue (read[i]);
            Assert::AreEqual (i != kSector, crcs[i] == crc, std::format (L"sector {}", i).c_str());
        }

        rig.AssertApplied();
    }



    //  A byte of the sectors the PC writes, flipped: the Apple sends a NAK,
    //  the sector is sent again, and the disk has what was sent.
    TEST_METHOD (PcFlip_InWrittenSectors_IsSentAgain)
    {
        Rig                rig;
        std::vector<Byte>  sectors = AllSectors();
        std::vector<Byte>  data    = Pattern (16 * 256, 0x21);
        std::vector<Byte>  image;
        CassoLinkStatus    status;



        rig.Connect();
        rig.Arm ("pc:flip@2000");

        Assert::AreEqual (S_OK, rig.Run ([&] () { return rig.host->WriteSectors (1, kTrack, sectors, data, status); }));
        Assert::AreEqual<int> (CassoLinkHost::kErrorNone, status.error);

        image = rig.Image (0);

        for (int p = 0; p < 16; p++)
        {
            Assert::IsTrue (SameSector (data, (size_t) p, image, DosOffset (kTrack, p)), std::format (L"sector {}", p).c_str());
        }

        rig.AssertApplied();
    }



    //  A byte of the program the PC loads, flipped: the page is sent again,
    //  and the program's Ready has the CRC of the image sent.
    TEST_METHOD (PcFlip_InTheProgram_IsSentAgain)
    {
        Rig  rig;



        rig.Arm ("pc:flip@700");
        rig.Connect();
        rig.AssertApplied();
    }



    //  A byte of a request left out: the Apple's wait for the rest times out,
    //  it sends Ready again, and the request is sent again.
    TEST_METHOD (PcDrop_InARequest_IsSentAgain)
    {
        Rig  rig;



        rig.Connect();
        rig.Arm ("pc:drop@6");

        rig.AssertTrackMatches (rig.Read (1, kTrack, AllSectors()));
        rig.AssertApplied();
    }



    //  An ERR line in the history marks the last operation's drive: its bar
    //  has FAILED over it until the next operation on the drive.
    TEST_METHOD (AFailedOperation_HasFailedOverItsBar_UntilTheNextOnTheDrive)
    {
        Rig  rig;



        rig.Connect();
        rig.AssertTrackMatches (rig.Read (1, kTrack, AllSectors()));
        Assert::IsTrue (rig.Screen().find ("FAILED") == std::string::npos);

        Assert::AreEqual (S_OK, rig.Run ([&] () { return rig.host->SendLog (12, 34, "ERR READING D1"); }));
        Assert::IsTrue (rig.Screen().find ("FAILED") != std::string::npos, TextEncoding::NarrowToWide (rig.Screen()).c_str());

        Assert::AreEqual (S_OK, rig.Run ([&] () { return rig.host->SendLog (12, 35, "OK READING D2"); }));
        Assert::IsTrue (rig.Screen().find ("FAILED") != std::string::npos, L"an OK line doesn't clear it");

        rig.AssertTrackMatches (rig.Read (1, kTrack, AllSectors()));
        Assert::IsTrue (rig.Screen().find ("FAILED") == std::string::npos, L"the drive's next operation clears it");
    }
};
