#include "Pch.h"
#include "EhmTestHelper.h"
#include "FixtureProvider.h"
#include "GuestSession.h"
#include "TestMachine.h"
#include "AppleTextCodec.h"
#include "Devices/Disk/FilePath.h"
#include "Machines/Apple2/Common/ProDosVolume.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  GuestVisibleProDosTextTests
//
//  ProDOS 8's BASIC.SYSTEM on an emulated //e reads a text file in plain
//  ASCII with $0D line ends, as Casso writes it on ProDOS, and one in
//  high-bit text with $8D line ends, as DOS 3.3 writes it. A file the
//  program writes is read back off the disk, for the form BASIC.SYSTEM
//  itself writes.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (GuestVisibleProDosTextTests)
{
public:

    static constexpr const char *  kFixture = "Disks/Merlin-proProdos2.33-b.dsk";
    static constexpr const char *  kText    = "HELLO FROM CASSO\n";
    static constexpr int           kSlot6   = 6;
    static constexpr int           kDrive1  = 0;
    static constexpr Byte          kHighBit = 0x80;

    static void PutText (std::vector<Byte> & ioImage, const char * name, AppleTextConvention convention)
    {
        FilePayload        payload;
        std::vector<Byte>  next;
        size_t             badOffset = 0;



        AssertSucceeded (AppleTextCodec::Encode (kText, convention, payload.bytes, badOffset));
        payload.type = ProDosVolume::kTypeText;

        AssertSucceeded (ProDosVolume (ioImage).Write (FilePath::Parse (name), payload, next));
        ioImage = std::move (next);
    }


    TEST_METHOD (BasicSystem_ReadsPlainAndHighBitText_AndWritesPlainText)
    {
        TestMachine               machine ("Apple2e");
        FixtureProvider           fixtures;
        std::vector<Byte>         image;
        std::vector<std::string>  rows;
        std::vector<Byte>         sectors;
        SectorDecodeReport        report;
        FilePayload               written;
        size_t                    high = 0;
        std::string               bytes;



        AssertSucceeded (fixtures.OpenFixture (kFixture, image));

        PutText (image, "PLAIN", AppleTextConvention::PlainAscii);
        PutText (image, "HIGH",  AppleTextConvention::HighAscii);

        GuestSession::BootToPrompt (machine, image);

        //  Clear the disk's startup program, which the boot left in memory.
        GuestSession::TypeAndCollect (machine, "NEW");
        GuestSession::TypeAndCollect (machine, "10 D$ = CHR$ (4)");
        GuestSession::TypeAndCollect (machine, "20 PRINT D$\"OPEN PLAIN\": PRINT D$\"READ PLAIN\": INPUT A$: PRINT D$\"CLOSE\"");
        GuestSession::TypeAndCollect (machine, "30 PRINT D$\"OPEN HIGH\": PRINT D$\"READ HIGH\": INPUT B$: PRINT D$\"CLOSE\"");
        GuestSession::TypeAndCollect (machine, "40 PRINT D$\"OPEN OUT\": PRINT D$\"WRITE OUT\": PRINT \"WRITTEN BY BASIC\": PRINT D$\"CLOSE\"");
        GuestSession::TypeAndCollect (machine, "50 PRINT \"P=\"A$: PRINT \"H=\"B$");

        rows = GuestSession::TypeAndCollect (machine, "RUN");

        for (const std::string & row : rows)
        {
            Logger::WriteMessage (("screen: " + row + "\n").c_str());
        }

        Assert::IsTrue (GuestSession::AnyRowIs (rows, "P=HELLO FROM CASSO"), L"BASIC.SYSTEM reads plain ASCII with $0D line ends");
        Assert::IsTrue (GuestSession::AnyRowIs (rows, "H=HELLO FROM CASSO"), L"and high-bit text with $8D line ends");

        sectors = GuestSession::DecodeThroughTheDrive (*machine.GetDiskStore().GetImage (kSlot6, kDrive1), report);

        AssertSucceeded (ProDosVolume (sectors).Read (FilePath::Parse ("OUT"), written));

        for (Byte b : written.bytes)
        {
            high  += (b & kHighBit) != 0 ? 1 : 0;
            bytes += std::format ("{:02X} ", b);
        }

        Logger::WriteMessage (("OUT: " + bytes + "\n").c_str());

        Assert::AreEqual ((size_t) 0, high, L"BASIC.SYSTEM writes text with the high bit clear");
        Assert::AreEqual ((int) '\r', (int) written.bytes.back(), L"and $0D line ends");
    }
};
