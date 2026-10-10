#include "Pch.h"

#include "../EhmTestHelper.h"
#include "Devices/Disk/Inspector/DiskAnalyzer.h"
#include "Devices/Disk/Inspector/InspectorSearch.h"
#include "Devices/Disk/Inspector/TrackAnalyzer.h"
#include "Machines/Apple2/Common/WozLoader.h"
#include "InspectorTrackBuilder.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  InspectorSearchTests
//
//  Find and Go to (FR-055, FR-056) on made-up tracks: nibble patterns with ?
//  and +, "Any bit offset", hex and text in sector data, no matches, and
//  each kind of Go to target read in the base the inspector shows it in.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (InspectorSearchTests)
{
public:

    static constexpr int  kTextSector = 5;
    static constexpr int  kTextOffset = 0x20;

    //  Sector 5 holds "HELLO" in high-bit ASCII at $20; the rest the pattern.
    static void FillWithText (int sector, std::array<Byte, DiskFieldFormat::kSectorBytes> & bytes)
    {
        InspectorTrackBuilder::FillPattern (sector, bytes);

        for (size_t i = 0; sector == kTextSector && i < 5; i++)
        {
            bytes[kTextOffset + i] = static_cast<Byte> ("HELLO"[i] | 0x80);
        }
    }



    static void AnalyzeTrack (TrackAnalysis & out)
    {
        InspectorTrackBuilder  builder;
        TrackContext           context;



        builder.AppendStandardTrack (DiskFieldKind::Sixteen, 254, 0, FillWithText);
        TrackAnalyzer::Analyze (*builder.MakeBitCopy(), context, DecodeSettings::MakeStandard(), out);
    }



    static void AnalyzeDisk (DiskAnalysis & out)
    {
        vector<Byte>  woz;
        DiskImage     image;



        AssertSucceeded (InspectorTrackBuilder::MakeStandardWoz (254, false, false, woz));
        AssertSucceeded (WozLoader::Load (woz, image));
        DiskAnalyzer::Analyze (DiskCopy::MakeFromImage (image, 1, "test", 0, false), DecodeSettings::MakeStandard(), out);
    }



    static vector<SearchHit> FindOnTrack (const TrackAnalysis & track, SearchKind kind, const std::wstring & text, bool isAnyBit = false)
    {
        vector<SearchItem>  items;
        vector<SearchHit>   hits;
        SearchQuery         query;



        query.kind           = kind;
        query.text           = text;
        query.isAnyBitOffset = isAnyBit;

        if (InspectorSearch::ParsePattern (text, kind, items))
        {
            InspectorSearch::FindInTrack (track, 0, items, query, hits);
        }

        return hits;
    }



    TEST_METHOD (PatternsTakeWildcardsAndExtraZeros)
    {
        vector<SearchItem>  items;



        Assert::IsTrue  (InspectorSearch::ParsePattern (L"D5 AA 96", SearchKind::Nibbles, items));
        Assert::AreEqual (static_cast<size_t> (3), items.size());
        Assert::IsTrue  (InspectorSearch::ParsePattern (L"D? FF+", SearchKind::Nibbles, items));
        Assert::AreEqual (static_cast<Byte> (0xF0), items[0].mask);
        Assert::IsTrue  (items[1].needsExtraZeros);
        Assert::IsFalse (InspectorSearch::ParsePattern (L"FF+", SearchKind::SectorHex, items), L"+ belongs to nibble patterns only");
        Assert::IsFalse (InspectorSearch::ParsePattern (L"D5 A", SearchKind::Nibbles, items), L"a lone digit");
        Assert::IsFalse (InspectorSearch::ParsePattern (L"G5", SearchKind::Nibbles, items));
        Assert::IsFalse (InspectorSearch::ParsePattern (L"", SearchKind::SectorText, items));
    }



    TEST_METHOD (NibblePatternsFindEveryPrologue)
    {
        TrackAnalysis  track;



        AnalyzeTrack (track);

        Assert::AreEqual (static_cast<size_t> (16), FindOnTrack (track, SearchKind::Nibbles, L"D5 AA 96").size());
        Assert::AreEqual (static_cast<size_t> (16), FindOnTrack (track, SearchKind::Nibbles, L"D5 AA AD").size());
        Assert::AreEqual (static_cast<size_t> (32), FindOnTrack (track, SearchKind::Nibbles, L"D5 AA ??").size(), L"?? matches either");
        Assert::IsTrue  (FindOnTrack (track, SearchKind::Nibbles, L"FF+ FF+").size() > 0, L"10-cell sync has extra zeros");
        Assert::AreEqual (static_cast<size_t> (0), FindOnTrack (track, SearchKind::Nibbles, L"D5+").size(), L"a prologue has none");
        Assert::AreEqual (static_cast<size_t> (0), FindOnTrack (track, SearchKind::Nibbles, L"12 34").size(), L"no matches");
    }



    TEST_METHOD (AnyBitOffsetFindsPatternsInsideFramedNibbles)
    {
        TrackAnalysis      track;
        vector<SearchHit>  hits;
        uint32_t           inside  = 0;
        bool               isFound = false;



        AnalyzeTrack (track);

        //  D5 is 11010101; from its second cell the latch would read 1 and
        //  the next seven cells, 1010101 and the first cell of AA: AB.
        inside = track.framed.nibbles[track.fields[track.sectors[0].addressField].firstNibble].startCell + 1;
        hits   = FindOnTrack (track, SearchKind::Nibbles, L"AB", true);

        for (const SearchHit & hit : hits)
        {
            isFound = isFound || (!hit.isAligned && hit.cell == inside);
        }

        Assert::IsTrue (isFound);
    }



    TEST_METHOD (SectorDataIsSearchedAsHexAndAsText)
    {
        TrackAnalysis      track;
        vector<SearchHit>  hits;



        AnalyzeTrack (track);
        hits = FindOnTrack (track, SearchKind::SectorText, L"hello");

        Assert::AreEqual (static_cast<size_t> (1), hits.size(), L"text matches without regard to case or the high bit");
        Assert::AreEqual (kTextSector, static_cast<int> (track.sectors[hits[0].sectorIndex].sector));
        Assert::AreEqual (kTextOffset, hits[0].offset);

        hits = FindOnTrack (track, SearchKind::SectorHex, L"C8 C5 C? CC");

        Assert::AreEqual (static_cast<size_t> (1), hits.size());
        Assert::AreEqual (kTextOffset, hits[0].offset);
    }



    TEST_METHOD (GoToReadsEachKindInItsBase)
    {
        DiskAnalysis  analysis;
        GoToTarget    target;
        std::wstring  error;



        AnalyzeDisk (analysis);

        Assert::IsTrue  (InspectorGoTo::Resolve (analysis, 0, GoToKind::Track, L"17.25", target, error));
        Assert::AreEqual (69, target.quarterTrack);

        Assert::IsTrue  (InspectorGoTo::Resolve (analysis, 8, GoToKind::PhysicalSector, L"$A", target, error));
        Assert::AreEqual (0xA, static_cast<int> (analysis.tracks[analysis.entries[8].slot]->sectors[target.sectorIndex].sector));

        Assert::IsTrue  (InspectorGoTo::Resolve (analysis, 8, GoToKind::Dos33Sector, L"E", target, error));
        Assert::AreEqual (0xE, analysis.tracks[analysis.entries[8].slot]->sectors[target.sectorIndex].dos33Logical);

        Assert::IsTrue  (InspectorGoTo::Resolve (analysis, 0, GoToKind::ProDosBlock, L"21", target, error));
        Assert::AreEqual (8, target.quarterTrack, L"block 21 is on track 2");
        Assert::AreEqual (21, analysis.tracks[analysis.entries[8].slot]->sectors[target.sectorIndex].prodosBlock);

        Assert::IsTrue  (InspectorGoTo::Resolve (analysis, 0, GoToKind::NibbleOffset, L"1A3", target, error));
        Assert::AreEqual (0x1A3, target.firstNibble);

        Assert::IsTrue  (InspectorGoTo::Resolve (analysis, 0, GoToKind::Cell, L"3,164", target, error));
        Assert::IsTrue  (target.firstNibble >= 0);

        Assert::IsFalse (InspectorGoTo::Resolve (analysis, 0, GoToKind::PhysicalSector, L"1F", target, error));
        Assert::IsFalse (error.empty(), L"says why");
        Assert::IsFalse (InspectorGoTo::Resolve (analysis, 0, GoToKind::Track, L"41", target, error));
    }
};
