#include "Pch.h"
#include "EhmTestHelper.h"
#include "GuestSession.h"
#include "KeystrokeInjector.h"
#include "TestMachine.h"
#include "Devices/Disk/Inspector/DiskAnalyzer.h"
#include "Machines/Apple2/Common/Disk2Controller.h"
#include "Ui/DiskInspector/SectorAccess.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  GuestVisibleDiskAccessTests
//
//  The sectors the drive marks as read and written while DOS 3.3 runs
//  CATALOG, SAVE and RUN, against the sectors in DOS's RWTS calls
//  (FR-161). The test steps the CPU one instruction at a time and takes
//  each call's track, sector and command from the IOB at RWTS's entry.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (GuestVisibleDiskAccessTests)
{
public:

    static constexpr int       kSlot          = 6;
    static constexpr int       kDrive         = 0;
    static constexpr Word      kRwtsEntry     = 0xBD00;
    static constexpr Byte      kIobTrack      = 0x04;
    static constexpr Byte      kIobSector     = 0x05;
    static constexpr Byte      kIobCommand    = 0x0C;
    static constexpr Byte      kCommandRead   = 0x01;
    static constexpr Byte      kCommandWrite  = 0x02;
    static constexpr uint64_t  kCommandCycles = 20'000'000ULL;
    static constexpr uint64_t  kSettleCycles  = 0;

    static constexpr const char *  kImagePath = "access.dsk";

    using SectorKey = std::pair<int, int>;



    //  Types a line, then runs the guest one instruction at a time until
    //  the line has had its cycles, adding each RWTS read and write to the
    //  traced sets as a whole track and DOS 3.3 sector.
    static void RunTraced (MachineHost & machine, const std::string & line, std::set<SectorKey> & outReads, std::set<SectorKey> & outWrites)
    {
        EmuCpu &           cpu    = *machine.GetCpu();
        uint64_t           cycles = 0;
        std::vector<Byte>  iob;



        //  The first key after a stepped stretch is lost, so a bare RETURN
        //  goes first.
        KeystrokeInjector::InjectLine (machine, "", kSettleCycles);
        KeystrokeInjector::InjectLine (machine, line, kSettleCycles);

        while (cycles < kCommandCycles)
        {
            if (cpu.GetPC() == kRwtsEntry)
            {
                iob = GuestSession::GuestBytesAt (machine, static_cast<Word> ((cpu.GetA() << 8) | cpu.GetY()), kIobCommand + 1);

                if (iob[kIobCommand] == kCommandRead || iob[kIobCommand] == kCommandWrite)
                {
                    (iob[kIobCommand] == kCommandRead ? outReads : outWrites).insert ({ iob[kIobTrack], iob[kIobSector] });
                }
            }

            cycles += std::max<uint64_t> (machine.StepOne(), 1);
        }
    }



    //  The sectors whose count went up between the two logs, by whole track
    //  and DOS 3.3 sector, as the inspector counts them on the disk now.
    static std::set<SectorKey> GetMarked (const DiskAnalysis & analysis, const DiskAccessLog::Spans & before, const DiskAccessLog::Spans & after, bool isWrite)
    {
        std::set<SectorKey>  marked;



        for (int track = 0; track * DiskImage::kQuarterTracksPerWholeTrack < DiskImage::kQuarterTrackCount; track++)
        {
            int  qt   = track * DiskImage::kQuarterTracksPerWholeTrack;
            int  slot = analysis.entries[static_cast<size_t> (qt)].slot;

            if (slot < 0)
            {
                Assert::IsTrue (after[static_cast<size_t> (qt)].empty(), L"no access on a track with nothing recorded");
                continue;
            }

            const TrackAnalysis &  analyzed = *analysis.tracks[static_cast<size_t> (slot)];
            vector<uint32_t>       was      = isWrite ? SectorAccess::CountWrites (analyzed, before[static_cast<size_t> (qt)]) : SectorAccess::CountReads (analyzed, before[static_cast<size_t> (qt)]);
            vector<uint32_t>       now      = isWrite ? SectorAccess::CountWrites (analyzed, after[static_cast<size_t> (qt)])  : SectorAccess::CountReads (analyzed, after[static_cast<size_t> (qt)]);

            for (size_t s = 0; s < analyzed.sectors.size(); s++)
            {
                if (now[s] > was[s])
                {
                    marked.insert ({ track, analyzed.sectors[s].dos33Logical });
                }
            }
        }

        return marked;
    }



    static std::wstring Describe (const std::set<SectorKey> & sectors)
    {
        std::wstring  text;



        for (const SectorKey & key : sectors)
        {
            text += std::format (L"T{}S{} ", key.first, key.second);
        }

        return text;
    }



    TEST_METHOD (CatalogSaveAndRunMarkTheSectorsInRwtsCalls)
    {
        TestMachine            machine ("Apple2e");
        std::vector<Byte>      master  = GuestSession::RequireDos33Master();
        std::set<SectorKey>    tracedReads;
        std::set<SectorKey>    tracedWrites;
        DiskAccessLog::Spans   readsBefore;
        DiskAccessLog::Spans   writesBefore;
        DiskAnalysis           analysis;



        GuestSession::BootToPrompt (machine, master);

        const Disk2Controller &  controller = *machine.GetRefs().diskController;

        readsBefore  = controller.GetAccessLog (kDrive).GetReads();
        writesBefore = controller.GetAccessLog (kDrive).GetWrites();

        RunTraced (machine, "CATALOG",      tracedReads, tracedWrites);
        KeystrokeInjector::InjectLine (machine, "",                  kSettleCycles);
        KeystrokeInjector::InjectLine (machine, "10 PRINT \"FR-161\"", GuestSession::kLineCycles);
        RunTraced (machine, "SAVE NEWPROG", tracedReads, tracedWrites);
        RunTraced (machine, "RUN NEWPROG",  tracedReads, tracedWrites);

        Assert::IsFalse (tracedReads.empty(),  L"DOS read some sectors");
        Assert::IsFalse (tracedWrites.empty(), L"and wrote some");

        DiskAnalyzer::Analyze (DiskCopy::MakeFromImage (*machine.GetDiskStore().GetImage (kSlot, kDrive), 1, kImagePath, 0, false), DecodeSettings::MakeStandard(), analysis);

        std::set<SectorKey>  markedReads  = GetMarked (analysis, readsBefore,  controller.GetAccessLog (kDrive).GetReads(),  false);
        std::set<SectorKey>  markedWrites = GetMarked (analysis, writesBefore, controller.GetAccessLog (kDrive).GetWrites(), true);

        Assert::AreEqual (Describe (tracedReads),  Describe (markedReads),  L"the sectors marked read are the ones RWTS read");
        Assert::AreEqual (Describe (tracedWrites), Describe (markedWrites), L"the sectors marked written are the ones RWTS wrote");
    }
};
