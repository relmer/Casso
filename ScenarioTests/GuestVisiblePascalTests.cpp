#include "Pch.h"

#include "EhmTestHelper.h"
#include "FakeDiskFileIo.h"
#include "FixtureProvider.h"
#include "GuestSession.h"
#include "KeystrokeInjector.h"
#include "TestMachine.h"
#include "TextScreenScraper.h"
#include "CassoExplorer/CassoExplorerActions.h"
#include "CassoExplorer/Model/DiskOperations.h"
#include "Core/TextEncoding.h"
#include "Machines/Apple2/Common/VolumeImage.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  GuestVisiblePascalTests
//
//  A text file from the PC put on Apple Pascal's APPLE1: disk as Casso
//  Explorer's put plans it, and opened in Apple Pascal's editor on an
//  enhanced //e (SC-048).
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (GuestVisiblePascalTests)
{
public:

    //  casso.txt goes in as CASSO.TEXT, a Pascal textfile, and the editor
    //  opens it with each of its lines.
    TEST_METHOD (ApplePascalsEditorOpensATextFilePutFromThePc)
    {
        std::vector<Byte>  disk = MakeDiskWithText();
        TestMachine        machine ("Apple2eEnhanced", TestMachine::Video::Timed);



        GuestSession::MountAndBoot (machine, disk);

        TypeAndWait (machine, "",        "Command: E(dit");
        TypeAndWait (machine, "E",       "No workfile is present");
        TypeAndWait (machine, "CASSO\r", ">Edit:");

        for (const char * line : kLines)
        {
            TypeAndWait (machine, "", line);
        }
    }

private:

    static constexpr const char *  kLines[] = { "PROGRAM CASSO;", "BEGIN", "  WRITELN ('FROM THE PC')", "END." };

    //  The pause after each key, how often the screen is read, and the
    //  cycles run before a missing prompt fails the test.
    static constexpr uint64_t  kKeyGapCycles  = 60'000;
    static constexpr uint64_t  kPollCycles    = 250'000;
    static constexpr uint64_t  kCeilingCycles = 60'000'000;

    //  The keys typed, then the machine run until a row contains `prompt`.
    static void TypeAndWait (MachineHost & machine, const char * keys, const char * prompt)
    {
        uint64_t     spent = 0;
        bool         found = false;
        std::string  rows;



        for (const char * c = keys; *c != '\0'; c++)
        {
            KeystrokeInjector::InjectKey (machine, static_cast<Byte> (*c));
            machine.RunCycles (kKeyGapCycles);
        }

        while (!found && spent < kCeilingCycles)
        {
            spent += machine.RunCycles (kPollCycles);
            found  = GuestSession::AnyRowContains (TextScreenScraper::Scrape (machine), prompt);
        }

        for (const std::string & row : TextScreenScraper::Scrape (machine))
        {
            rows += GuestSession::TrimTrailingBlanks (row) + " | ";
        }

        Assert::IsTrue (found, TextEncoding::NarrowToWide (std::format ("no row contains \"{}\" after \"{}\": {}", prompt, keys, rows)).c_str());
    }

    //  APPLE1: with casso.txt put as Casso Explorer's put writes it, in the
    //  disk's sector order.
    static std::vector<Byte> MakeDiskWithText()
    {
        static constexpr const char *  kImage = "C:\\Disks\\apple1.po";
        static constexpr const char *  kHost  = "C:\\In\\casso.txt";

        FixtureProvider                fixtures;
        FakeDiskFileIo                 io;
        DiskOperations                 ops (io);
        std::vector<Byte>              bytes;
        std::vector<Byte>              sectors;
        std::string                    host;
        SectorDecodeReport             report;
        CassoExplorerActions::PutPlan  plan;
        DiskOperations::Result         result;



        for (const char * line : kLines)
        {
            host += std::string (line) + "\r\n";
        }

        AssertSucceeded (fixtures.OpenFixture ("CiderPress/pascal/Apple Pascal1.po", bytes));

        io.files[kImage]  = bytes;
        io.stamps[kImage] = FileStamp { bytes.size(), 1 };
        io.files[kHost].assign (host.begin(), host.end());
        io.stamps[kHost]  = FileStamp { host.size(), 1 };

        plan = CassoExplorerActions::PlanPut (L"casso.txt", io.files[kHost], VolumeKind::Pascal);
        Assert::AreEqual (std::string ("CASSO.TEXT"), plan.catalogName, L".TEXT on the catalog name");

        result = plan.usePayload ? ops.WritePayload (kImage, plan.catalogName, plan.payload)
                                 : ops.Put (kImage, kHost, plan.catalogName, plan.typeName, plan.hasLoadAddress, plan.loadAddress, plan.encoding);
        AssertSucceeded (result.hr, TextEncoding::NarrowToWide (result.message).c_str());

        AssertSucceeded (VolumeImage::Load (io.files[kImage], "apple1.po", sectors, report));

        return sectors;
    }
};
