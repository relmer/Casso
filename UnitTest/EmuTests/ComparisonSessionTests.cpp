#include "Pch.h"

#include "ComparisonTestImages.h"
#include "../EhmTestHelper.h"
#include "Core/UnicodeSymbols.h"
#include "Devices/Disk/Inspector/DiskComparer.h"
#include "Devices/Disk/Inspector/InspectorImageLoader.h"
#include "Ui/DiskInspector/CompareDialog.h"
#include "Ui/DiskInspector/ComparisonSession.h"
#include "Ui/DiskInspector/ComparisonText.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;
using Pairs = ComparisonTestImages;





////////////////////////////////////////////////////////////////////////////////
//
//  FakeComparisonHost
//
//  A host whose drives hold made-up disks: each request is kept, and the
//  test answers it with the drive's copy.
//
////////////////////////////////////////////////////////////////////////////////

class FakeComparisonHost : public IDiskInspectorHost
{
public:
    uint64_t  PostInspectorRequest (const InspectorRequest & request) override
    {
        requests.push_back ({ ++nextId, request });
        return nextId;
    }

    void  TakeInspectorReplies (vector<InspectorReply> & outReplies) override { outReplies.clear(); }
    int   GetDriveCount        () const override                               { return 2; }
    void  OnInspectorClosed    () override                                     {}

    //  Each request so far answered with its drive's disk.
    void  Answer (ComparisonSession & session)
    {
        for (const auto & [id, request] : requests)
        {
            InspectorReply  reply;

            reply.requestId = id;
            reply.drive     = request.drive;
            reply.disk      = drives[request.drive];
            Assert::IsTrue (session.OfferReply (reply), L"the session takes the reply to its own request");
        }

        requests.clear();
    }

    vector<std::pair<uint64_t, InspectorRequest>>    requests;
    std::array<std::shared_ptr<const DiskCopy>, 2>   drives;
    uint64_t                                         nextId = 0;
};





////////////////////////////////////////////////////////////////////////////////
//
//  ComparisonSessionTests
//
//  The comparison as the window runs it (FR-117 to FR-121): B read from a
//  drive and analyzed in the background, the two compared once both are
//  ready, the options changing only the list, A and B swapping places, and
//  the words the views show. An image file loads the way a drive mounts it.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (ComparisonSessionTests)
{
public:

    static constexpr int  kChanged    = 9;
    static constexpr int  kDeadlineMs = 30000;

    static std::shared_ptr<const DiskCopy> MakeCopy (const vector<Byte> & woz, const std::string & name)
    {
        std::shared_ptr<const DiskCopy>  copy;
        std::wstring                     reason;



        AssertSucceeded (InspectorImageLoader::LoadBytes (woz, name, false, copy, reason));
        Assert::IsNotNull (copy.get());

        return copy;
    }



    //  Ticks until the session has a result, failing past the deadline.
    static void WaitForResult (ComparisonSession & session, const DiskAnalysis & a)
    {
        vector<LoadedDisk>  loads;
        ULONGLONG           start = GetTickCount64();



        while (!session.HasResult() || session.IsBusy())
        {
            Assert::IsTrue (GetTickCount64() - start < kDeadlineMs, L"the comparison finishes");
            session.Tick (a, true, loads);
            std::this_thread::sleep_for (std::chrono::milliseconds (5));
        }
    }



    TEST_METHOD (DriveBIsReadAnalyzedAndComparedWithA)
    {
        Pairs::TrackSpec    changed;
        FakeComparisonHost  host;
        ComparisonSession   session;
        DiskAnalysis        a;



        changed.changedSector = 5;
        Pairs::AnalyzeDisk (-1, {}, a);
        host.drives[1] = MakeCopy (Pairs::BuildWoz (kChanged, changed), "b.woz");

        session.Begin ({ ComparisonSourceKind::DriveNow, 0 }, { ComparisonSourceKind::DriveNow, 1 }, false, DecodeSettings::MakeStandard(), host);
        Assert::AreEqual (size_t (1), host.requests.size(), L"only B is read; A is the window's");
        Assert::AreEqual (1, host.requests[0].second.drive);
        Assert::IsTrue (session.IsBusy());

        host.Answer (session);
        WaitForResult (session, a);

        Assert::AreEqual (static_cast<int> (TrackVerdict::SectorsDiffer), static_cast<int> (session.GetResult().tracks[kChanged * 4].verdict));
        Assert::AreEqual (size_t (1), session.GetListed().size(), L"the one planted sector");
        Assert::AreEqual (static_cast<int> (DifferenceKind::SectorBytes), static_cast<int> (session.GetListed()[0].kind));
    }



    TEST_METHOD (OptionsChangeOnlyTheListNotTheVerdict)
    {
        Pairs::TrackSpec    changed;
        FakeComparisonHost  host;
        ComparisonSession   session;
        ComparisonOptions   options;
        DiskAnalysis        a;



        changed.volume = 17;
        Pairs::AnalyzeDisk (-1, {}, a);
        host.drives[1] = MakeCopy (Pairs::BuildWoz (kChanged, changed), "b.woz");

        session.Begin ({ ComparisonSourceKind::DriveNow, 0 }, { ComparisonSourceKind::DriveNow, 1 }, false, DecodeSettings::MakeStandard(), host);
        host.Answer (session);
        WaitForResult (session, a);

        Assert::AreEqual (size_t (1), session.GetListed().size(), L"the volume number is listed");

        options.isIgnoringVolumes = true;
        session.SetOptions (options);

        Assert::AreEqual (size_t (0), session.GetListed().size(), L"Ignore volume numbers leaves it out");
        Assert::AreNotEqual (static_cast<int> (TrackVerdict::Identical), static_cast<int> (session.GetResult().tracks[kChanged * 4].verdict),
                             L"the verdict stays");
    }



    TEST_METHOD (ADriveWithNoDiskSaysSo)
    {
        FakeComparisonHost  host;
        ComparisonSession   session;
        DiskAnalysis        a;
        vector<LoadedDisk>  loads;
        ULONGLONG           start = GetTickCount64();



        Pairs::AnalyzeDisk (-1, {}, a);

        session.Begin ({ ComparisonSourceKind::DriveNow, 0 }, { ComparisonSourceKind::DriveNow, 1 }, false, DecodeSettings::MakeStandard(), host);
        host.Answer (session);

        while (session.GetBError().empty())
        {
            Assert::IsTrue (GetTickCount64() - start < kDeadlineMs, L"the reason arrives");
            session.Tick (a, true, loads);
        }

        Assert::AreEqual (std::wstring (L"There is no disk in drive 2."), session.GetBError());
        Assert::IsFalse (session.IsBusy(), L"a side that cannot be read is not shown as comparing");
    }



    TEST_METHOD (SwapExchangesTheSidesAndComparesAgain)
    {
        Pairs::TrackSpec                    changed;
        FakeComparisonHost                  host;
        ComparisonSession                   session;
        DiskAnalysis                        a;
        std::unique_ptr<AnalysisScheduler>  scheduler = std::make_unique<AnalysisScheduler> (nullptr);
        ComparisonSource                    sourceA   = { ComparisonSourceKind::DriveNow, 0 };
        uint64_t                            mediaA    = 0;



        changed.isPresent = false;
        Pairs::AnalyzeDisk (-1, {}, a);
        mediaA         = a.mediaId;
        host.drives[1] = MakeCopy (Pairs::BuildWoz (kChanged, changed), "b.woz");

        session.Begin (sourceA, { ComparisonSourceKind::DriveNow, 1 }, false, DecodeSettings::MakeStandard(), host);
        host.Answer (session);
        WaitForResult (session, a);
        Assert::AreEqual (static_cast<int> (TrackVerdict::OnlyInA), static_cast<int> (session.GetResult().tracks[kChanged * 4].verdict));

        session.Swap (a, scheduler, sourceA);

        Assert::AreEqual (1, sourceA.drive, L"A is now drive 2's disk");
        Assert::AreEqual (0, session.GetSourceB().drive);
        Assert::AreEqual (mediaA, session.GetB().mediaId, L"B is the disk A was");

        WaitForResult (session, a);
        Assert::AreEqual (static_cast<int> (TrackVerdict::OnlyInB), static_cast<int> (session.GetResult().tracks[kChanged * 4].verdict));
    }



    TEST_METHOD (EndDropsTheComparison)
    {
        FakeComparisonHost  host;
        ComparisonSession   session;
        DiskAnalysis        a;



        Pairs::AnalyzeDisk (-1, {}, a);
        host.drives[1] = MakeCopy (Pairs::BuildWoz (-1, {}), "b.woz");

        session.Begin ({ ComparisonSourceKind::DriveNow, 0 }, { ComparisonSourceKind::DriveNow, 1 }, false, DecodeSettings::MakeStandard(), host);
        host.Answer (session);
        WaitForResult (session, a);
        session.End();

        Assert::IsFalse (session.IsComparing());
        Assert::IsFalse (session.HasResult());
        Assert::IsNull  (session.GetB().copy.get());
    }



    TEST_METHOD (ImageBytesLoadAsADriveMountsThem)
    {
        std::shared_ptr<const DiskCopy>  copy;
        std::wstring                     reason;
        vector<Byte>                     shortDsk (1000, 0);
        HRESULT                          hr       = S_OK;



        AssertSucceeded (InspectorImageLoader::LoadBytes (Pairs::BuildWoz (-1, {}), "C:\\disks\\good.woz", true, copy, reason));
        Assert::IsNotNull (copy.get());
        Assert::IsTrue (copy->mediaId >= InspectorImageLoader::kFirstMediaId, L"apart from the drives' ids");
        Assert::IsTrue (copy->isReadOnly);

        hr = InspectorImageLoader::LoadBytes (shortDsk, "C:\\disks\\short.dsk", false, copy, reason);
        Assert::IsTrue (FAILED (hr));
        Assert::IsNull (copy.get());
        Assert::IsTrue (reason.starts_with (L"short.dsk "), reason.c_str());

        hr = InspectorImageLoader::LoadBytes (shortDsk, "C:\\disks\\notes.txt", false, copy, reason);
        Assert::IsTrue (FAILED (hr));
        Assert::AreEqual (std::wstring (L"notes.txt is not a disk image Casso opens."), reason);
    }



    TEST_METHOD (VerdictsGiveWhatQualifiesThem)
    {
        TrackComparison  t;



        t.verdict        = TrackVerdict::SameCells;
        t.rotationCells  = 12800;
        t.cellCount      = 51200;
        t.isTimingDiffer = true;
        Assert::AreEqual (std::format (L"Same cells, rotated 90{}, flux timing differs", s_kpszDegree), ComparisonText::FormatVerdict (t));

        t                 = TrackComparison();
        t.verdict         = TrackVerdict::SameNibbles;
        t.rotationNibbles = 40;
        t.lengthChange    = -1200;
        Assert::AreEqual (std::wstring (L"Same nibbles, rotated 40 nibbles, B 1,200 cells shorter"), ComparisonText::FormatVerdict (t));

        t               = TrackComparison();
        t.verdict       = TrackVerdict::SectorsDiffer;
        t.sectorsDiffer = 3;
        Assert::AreEqual (std::wstring (L"Sectors differ (3)"), ComparisonText::FormatVerdict (t));

        t         = TrackComparison();
        t.verdict = TrackVerdict::NotCompared;
        t.reason  = L"B's record is damaged";
        Assert::AreEqual (std::wstring (L"Not compared: B's record is damaged"), ComparisonText::FormatVerdict (t));
    }



    TEST_METHOD (ResultCountsEachComparedTrackOnce)
    {
        DiskComparison  c;



        c.tracks.resize (8);
        c.tracks[0].verdict = TrackVerdict::Identical;
        c.tracks[1].verdict = TrackVerdict::StandardLayout;
        c.tracks[2].verdict = TrackVerdict::Identical;
        c.tracks[3].verdict = TrackVerdict::SameSectorData;
        c.tracks[4].verdict = TrackVerdict::OnlyInB;
        c.tracks[5].verdict = TrackVerdict::NothingRecorded;

        Assert::AreEqual (std::format (L"2 identical {0} 1 differ {0} 1 only in B", s_kpszMiddleDot), ComparisonText::FormatResult (c));
    }



    TEST_METHOD (RowsFilterByGroupAndSort)
    {
        vector<Difference>  listed;
        vector<TableRow>    rows;



        listed.push_back ({ DifferenceKind::SectorBytes, 40, 3 });
        listed.back().count       = 2;
        listed.back().firstOffset = 0x40;
        listed.push_back ({ DifferenceKind::TrackOnlyInB, 8 });
        listed.push_back ({ DifferenceKind::FileOnlyInA });
        listed.back().path = L"HELLO";

        rows = ComparisonText::BuildRows (listed, ComparisonText::GetAllGroups(), -1, false);
        Assert::AreEqual (size_t (3), rows.size());
        Assert::AreEqual (std::wstring (L"2 bytes differ, the first at $40"), rows[0].cells[4]);
        Assert::AreEqual (std::wstring (L"HELLO"), rows[2].cells[4]);
        Assert::AreEqual (2, rows[2].finding, L"a row refers to its difference");

        rows = ComparisonText::BuildRows (listed, 1u << ComparisonText::kGroupSectors, -1, false);
        Assert::AreEqual (size_t (1), rows.size(), L"only the sectors group");

        rows = ComparisonText::BuildRows (listed, ComparisonText::GetAllGroups(), 0, false);
        Assert::AreEqual (1, rows[0].finding, L"track 2 sorts before track 10");
    }



    TEST_METHOD (TracksTabGainsAComparisonColumn)
    {
        DiskComparison        c;
        vector<std::wstring>  columns = { L"Track", L"Recorded" };
        vector<TableRow>      rows (2);



        c.tracks.resize (2);
        c.tracks[1].verdict  = TrackVerdict::OnlyInA;
        rows[0].quarterTrack = 0;
        rows[0].cells        = { L"0", L"Bits" };
        rows[1].quarterTrack = 1;
        rows[1].cells        = { L"0.25", L"Bits" };

        ComparisonText::AddVerdictColumn (columns, rows, c, false);

        Assert::AreEqual (std::wstring (L"Comparison"), columns[1]);
        Assert::AreEqual (std::wstring (L"Nothing recorded"), rows[0].cells[1]);
        Assert::AreEqual (std::wstring (L"Only in A"), rows[1].cells[1]);
    }



    TEST_METHOD (SectorsPairByNumberOnARotatedTrack)
    {
        Pairs::TrackSpec       rotated;
        DiskAnalysis           a;
        DiskAnalysis           b;
        const TrackAnalysis *  trackA = nullptr;
        const TrackAnalysis *  trackB = nullptr;
        int                    paired = -1;



        rotated.rotation = 3000;
        Pairs::AnalyzeDisk (-1, {}, a);
        Pairs::AnalyzeDisk (kChanged, rotated, b);
        trackA = a.tracks[a.entries[kChanged * 4].slot].get();
        trackB = b.tracks[b.entries[kChanged * 4].slot].get();

        for (int i = 0; i < static_cast<int> (trackA->sectors.size()); i++)
        {
            paired = DiskComparer::FindPairedSector (*trackA, i, *trackB);

            Assert::IsTrue (paired >= 0, std::to_wstring (i).c_str());
            Assert::AreEqual (trackA->sectors[i].sector, trackB->sectors[paired].sector);
        }

        Assert::AreNotEqual (trackA->sectors[0].sector, trackB->sectors[0].sector, L"the rotation moved the first sector");
        Assert::AreEqual (-1, DiskComparer::FindPairedSector (*trackA, -1, *trackB));
    }



    TEST_METHOD (ANibbleInAHunkGivesBsNibble)
    {
        vector<Difference>  hunks;
        int                 nibbleB = 0;



        hunks.push_back ({ DifferenceKind::Nibbles, 0 });
        hunks.back().firstNibbleA = 100;
        hunks.back().nibbleCountA = 4;
        hunks.back().firstNibbleB = 210;
        hunks.back().nibbleCountB = 2;

        Assert::IsTrue  (ComparisonText::FindInHunk (hunks, 101, nibbleB));
        Assert::AreEqual (211, nibbleB);
        Assert::IsTrue  (ComparisonText::FindInHunk (hunks, 103, nibbleB));
        Assert::AreEqual (-1, nibbleB, L"past B's side of the hunk");
        Assert::IsFalse (ComparisonText::FindInHunk (hunks, 104, nibbleB));
    }


    TEST_METHOD (CompareDialogListsEachDrivesSourcesThenAFile)
    {
        vector<ComparisonSource>  sources = CompareDialog::BuildSources (2);



        Assert::AreEqual (size_t (7), sources.size());
        Assert::AreEqual (std::wstring (L"Drive 1 now"),          ComparisonText::FormatSource (sources[0]));
        Assert::AreEqual (std::wstring (L"Drive 1 as inserted"),  ComparisonText::FormatSource (sources[1]));
        Assert::AreEqual (std::wstring (L"Drive 1's file"),       ComparisonText::FormatSource (sources[2]));
        Assert::AreEqual (std::wstring (L"Drive 2 now"),          ComparisonText::FormatSource (sources[3]));
        Assert::AreEqual (static_cast<int> (ComparisonSourceKind::ImageFile), static_cast<int> (sources[6].kind));
    }
};
