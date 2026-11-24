#include "Pch.h"

#include "../EhmTestHelper.h"
#include "Devices/Disk/Inspector/DiskAnalyzer.h"
#include "Devices/Disk/Inspector/FileMap/FileMap.h"
#include "Devices/Disk/Inspector/TrackAnalyzer.h"
#include "Machines/Apple2/Common/NibblizationLayer.h"
#include "../Dxui/MockDxuiPainter.h"
#include "../Dxui/MockDxuiTextRenderer.h"
#include "Ui/DiskInspector/FileMapGridView.h"
#include "Ui/DiskInspector/InspectorTip.h"
#include "Ui/DiskInspector/Views/SectorsView.h"
#include "Ui/DiskInspector/SectorRowView.h"
#include "FileMapTestImages.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  InspectorTipTests
//
//  The platter's tooltip (FR-027) and its colors (FR-148): what is under the
//  pointer, a swatch beside each line about a color, every kind in a wide
//  spot, and the Files overlay's role for the sector.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (InspectorTipTests)
{
public:

    static constexpr double  kOneCell      = 1e-6;
    static constexpr int     kEmptyRing    = DiskImage::kQuarterTrackCount - 1;   // empty on a .dsk, beyond the head's reach



    static void AnalyzeDsk (DiskAnalysis & out)
    {
        vector<Byte>  sectors (NibblizationLayer::kImageByteSize, 0);
        DiskImage     image;



        AssertSucceeded (NibblizationLayer::NibblizeDsk (sectors, image));
        DiskAnalyzer::Analyze (DiskCopy::MakeFromImage (image, 7, "test", 0, false), DecodeSettings::MakeStandard(), out);
    }



    //  The analysis is built once for the class. Each test copies it.
    static const DiskAnalysis & GetDsk()
    {
        static DiskAnalysis  s_analysis;
        static bool          s_isBuilt  = false;



        if (!s_isBuilt)
        {
            AnalyzeDsk (s_analysis);
            s_isBuilt = true;
        }

        return s_analysis;
    }



    //  The DOS 3.3 volume with files, analyzed once for the class.
    static const DiskAnalysis & GetDos33()
    {
        static DiskAnalysis  s_analysis;
        static bool          s_isBuilt  = false;



        if (!s_isBuilt)
        {
            FileMapTestImages::Analyze (FileMapTestImages::MakeDos33(), false, s_analysis);
            s_isBuilt = true;
        }

        return s_analysis;
    }



    static std::wstring GetLine (const std::wstring & text, int line)
    {
        size_t  start = 0;
        size_t  end   = 0;



        for (int i = 0; i < line && start != std::wstring::npos; i++)
        {
            start = text.find (L'\n', start);
            start = (start == std::wstring::npos) ? start : start + 1;
        }

        Assert::IsTrue (start != std::wstring::npos, text.c_str());
        end = text.find (L'\n', start);

        return text.substr (start, (end == std::wstring::npos) ? std::wstring::npos : end - start);
    }



    //  The turn at a nibble `offset` nibbles into a physical sector's data
    //  field.
    static double GetDataTurn (const TrackAnalysis & track, int physical, int offset)
    {
        auto  sector = std::find_if (track.sectors.begin(), track.sectors.end(), [&] (const AnalyzedSector & s) { return s.sector == physical; });



        Assert::IsTrue (sector != track.sectors.end() && sector->dataField >= 0);

        return TrackAnalyzer::GetAngle (track, track.framed.nibbles[track.fields[sector->dataField].bodyNibble + offset].startCell);
    }



    TEST_CLASS_INITIALIZE (BuildSharedInput)
    {
        (void) GetDsk();
        (void) GetDos33();
    }



    TEST_METHOD (ThePlatterTipDescribesWhatIsUnderThePointer)
    {
        DiskAnalysis           analysis;
        InspectorViewContext   context;
        const TrackAnalysis *  track = nullptr;
        InspectorTip           tip;
        double                 turn  = 0;



        analysis = GetDsk();
        context.analysis = &analysis;
        track = analysis.tracks[analysis.entries[68].slot].get();
        turn  = TrackAnalyzer::GetAngle (*track, track->framed.nibbles[track->fields[track->sectors[3].dataField].bodyNibble + 20].startCell);
        tip   = InspectorTip::BuildPlatterTip (context, 68, turn, kOneCell, false);

        Assert::IsTrue (tip.GetText().starts_with (L"Track 17 \x00B7 Sector $3\n16 of 16 sectors good\n"), tip.GetText().c_str());
        Assert::IsTrue (tip.GetText().ends_with (L"\nData field"), tip.GetText().c_str());

        tip = InspectorTip::BuildPlatterTip (context, 68, turn, kOneCell, true);
        Assert::IsTrue (tip.GetText().find (L"\nNibble $96 at offset $") != std::wstring::npos, tip.GetText().c_str());

        analysis.entries[kEmptyRing].isBeyondHeadReach = false;   // an empty ring within reach
        tip = InspectorTip::BuildPlatterTip (context, kEmptyRing, 0.5, kOneCell, false);
        Assert::IsTrue (tip.GetText().ends_with (L"\nNothing recorded\nThe drive reads random bits here"), tip.GetText().c_str());
    }



    //  An empty ring beyond the head's reach has no random bits to read: its
    //  tip has the line for a track outside the head's range, and its swatch
    //  is the dimmed empty color the platter draws there.
    TEST_METHOD (AnEmptyRingBeyondTheHeadsReach_IsMarkedAsBeyondIt)
    {
        DiskAnalysis           analysis;
        InspectorViewContext   context;
        InspectorTip           tip;
        uint32_t               dim = 0;



        analysis = GetDsk();
        context.analysis = &analysis;
        dim = context.palette.colors.beyondReach;
        tip = InspectorTip::BuildPlatterTip (context, kEmptyRing, 0.5, kOneCell, false);

        Assert::IsTrue   (tip.GetText().ends_with (L"\nTrack outside the range of Disk II's read/write head"), tip.GetText().c_str());
        Assert::AreEqual (static_cast<size_t> (1), tip.GetSwatches().size());
        Assert::AreEqual (DxuiColor::Mix (context.palette.colors.nothingRecorded, dim | 0xFF000000u, static_cast<float> (dim >> 24) / 255.0f), tip.GetSwatches()[0].argb);
    }



    TEST_METHOD (AFindHitUnderThePointerHasALineWithItsSwatch)
    {
        DiskAnalysis            analysis;
        InspectorViewContext    context;
        FindMarks               marks;
        DiskSearchQuery         query;
        const TrackAnalysis   * track    = nullptr;
        InspectorTip            tip;
        int                     mark     = 0;



        analysis = GetDsk();
        query.text = L"D5 AA 96";
        marks.Build (InspectorSearch::Find (analysis, 68, query), nullptr, query.text);
        context.analysis  = &analysis;
        context.findMarks = &marks;
        track = analysis.tracks[analysis.entries[68].slot].get();
        mark  = track->fields[track->sectors[3].addressField].firstNibble;

        tip = InspectorTip::BuildPlatterTip (context, 68, TrackAnalyzer::GetAngle (*track, track->framed.nibbles[mark + 1].startCell), kOneCell, false);
        Assert::IsTrue   (tip.GetText().ends_with (L"\nA match for the search \"D5 AA 96\""), tip.GetText().c_str());
        Assert::AreEqual (context.palette.colors.findHit, tip.GetSwatches().back().argb);
        Assert::AreEqual (std::wstring (L"A match for the search \"D5 AA 96\""), GetLine (tip.GetText(), tip.GetSwatches().back().line));

        tip = InspectorTip::BuildPlatterTip (context, 68, GetDataTurn (*track, 3, 20), kOneCell, false);
        Assert::IsTrue   (tip.GetText().find (L"A match for") == std::wstring::npos, tip.GetText().c_str());

        tip = InspectorTip::BuildNibbleTip (context, *track, mark + 2, track->framed.nibbles[mark + 2].startCell, 8, false);
        Assert::IsTrue   (tip.GetText().ends_with (L"\nA match for the search \"D5 AA 96\""), tip.GetText().c_str());

        tip = InspectorTip::BuildNibbleTip (context, *track, mark + 3, track->framed.nibbles[mark + 3].startCell, 8, false);
        Assert::IsTrue   (tip.GetText().find (L"A match for") == std::wstring::npos, L"the nibble after the hit");
    }



    TEST_METHOD (EachColorLineHasItsColorsSwatch)
    {
        DiskAnalysis           analysis;
        InspectorViewContext   context;
        const TrackAnalysis *  track = nullptr;
        InspectorTip           tip;



        analysis = GetDsk();
        context.analysis = &analysis;
        track = analysis.tracks[analysis.entries[68].slot].get();
        tip   = InspectorTip::BuildPlatterTip (context, 68, GetDataTurn (*track, 3, 20), kOneCell, false);

        Assert::AreEqual (static_cast<size_t> (1), tip.GetSwatches().size());
        Assert::AreEqual (std::wstring (L"Data field"), GetLine (tip.GetText(), tip.GetSwatches()[0].line));
        Assert::AreEqual (context.palette.GetKindColor (PlatterKind::DataField), tip.GetSwatches()[0].argb);

        analysis.entries[kEmptyRing].isBeyondHeadReach = false;   // an empty ring within reach
        tip = InspectorTip::BuildPlatterTip (context, kEmptyRing, 0.5, kOneCell, false);
        Assert::AreEqual (static_cast<size_t> (1), tip.GetSwatches().size());
        Assert::AreEqual (std::wstring (L"Nothing recorded"), GetLine (tip.GetText(), tip.GetSwatches()[0].line));
        Assert::AreEqual (context.palette.colors.nothingRecorded, tip.GetSwatches()[0].argb);
    }



    TEST_METHOD (AWideSpotListsEveryKindMostCellsFirst)
    {
        DiskAnalysis           analysis;
        InspectorViewContext   context;
        const TrackAnalysis *  track  = nullptr;
        InspectorTip           tip;
        std::wstring           lines;
        double                 turn   = 0;



        analysis = GetDsk();
        context.analysis = &analysis;
        track = analysis.tracks[analysis.entries[68].slot].get();

        //  A tenth of a turn from the middle of sector 3's data field takes
        //  in the gap and the next sector's address field.
        turn = GetDataTurn (*track, 3, 200);
        tip  = InspectorTip::BuildPlatterTip (context, 68, turn + 0.05, 0.1, false);

        for (const DxuiTooltipSwatch & swatch : tip.GetSwatches())
        {
            lines += GetLine (tip.GetText(), swatch.line) + L"|";
        }

        Assert::IsTrue (lines.starts_with (L"Data field|"), lines.c_str());
        Assert::IsTrue (lines.find (L"|Sync|")          != std::wstring::npos, lines.c_str());
        Assert::IsTrue (lines.find (L"|Address field|") != std::wstring::npos, lines.c_str());
    }



    TEST_METHOD (TheNibbleTipGivesTheNibbleAndItsColor)
    {
        DiskAnalysis           analysis;
        InspectorViewContext   context;
        const TrackAnalysis *  track  = nullptr;
        InspectorTip           tip;
        int                    nibble = 0;



        analysis = GetDsk();
        context.analysis = &analysis;
        track  = analysis.tracks[analysis.entries[68].slot].get();
        nibble = track->fields[track->sectors[3].dataField].bodyNibble + 20;
        tip    = InspectorTip::BuildNibbleTip (context, *track, nibble, track->framed.nibbles[nibble].startCell, 8, false);

        Assert::IsTrue   (GetLine (tip.GetText(), 0).starts_with (L"Nibble $96 at offset $"), tip.GetText().c_str());
        Assert::IsTrue   (GetLine (tip.GetText(), 0).ends_with (L" \x00B7 Sector $3"), tip.GetText().c_str());
        Assert::IsTrue   (GetLine (tip.GetText(), 1).ends_with (L": 8 cells"), tip.GetText().c_str());
        Assert::AreEqual (static_cast<size_t> (1), tip.GetSwatches().size());
        Assert::AreEqual (std::wstring (L"Data field"), GetLine (tip.GetText(), tip.GetSwatches()[0].line));
    }



    TEST_METHOD (ThePlatterTipHasTheSectorsRole)
    {
        vector<Byte>           image   = FileMapTestImages::MakeDos33();
        DiskAnalysis           analysis;
        vector<FileMap>        maps;
        InspectorViewContext   context;
        const TrackAnalysis *  track   = nullptr;
        InspectorTip           tip;
        int                    hello   = -1;
        int                    cell    = -1;
        int                    qt      = 0;



        analysis = GetDos33();
        maps = FileMapBuilder::Build (analysis);
        Assert::AreEqual (static_cast<size_t> (1), maps.size());

        for (size_t f = 0; f < maps[0].files.size(); f++)
        {
            hello = (maps[0].files[f].path == L"HELLO") ? static_cast<int> (f) : hello;
        }

        Assert::IsTrue (hello >= 0);

        for (const FilePlace & place : maps[0].files[hello].sectors)
        {
            cell = (cell < 0 && place.role == SectorRole::FileData) ? place.cell : cell;
        }

        context.analysis = &analysis;
        context.fileMap  = &maps[0];
        qt               = maps[0].GetTrack (cell) * DiskImage::kQuarterTracksPerWholeTrack;
        track            = analysis.tracks[analysis.entries[qt].slot].get();

        tip = InspectorTip::BuildPlatterTip (context, qt, GetDataTurn (*track, maps[0].GetPhysical (cell), 20), kOneCell, false);

        Assert::IsTrue   (GetLine (tip.GetText(), tip.GetSwatches().back().line).starts_with (L"Used by HELLO, data sector 1 of 3"), tip.GetText().c_str());
        Assert::AreEqual (context.palette.GetRoleColor (MapRole::FileData), tip.GetSwatches().back().argb);
    }


    TEST_METHOD (TheSectorRowAndSectorDataTipsHaveTheirFindHits)
    {
        DiskAnalysis            analysis;
        InspectorViewModel      model;
        InspectorViewContext    context;
        FindMarks               marks;
        DiskSearchQuery         query;
        vector<SearchHit>       hits;
        SearchHit               hit;
        const TrackAnalysis   * track    = nullptr;
        DxuiTheme               theme    = DxuiTheme::Dark();
        DxuiDpiScaler           scaler;
        MockDxuiPainter         painter;
        MockDxuiTextRenderer    text;
        std::wstring            tip;
        std::wstring            byteTip;
        RECT                    anchor   = {};
        int                     three    = -1;



        analysis = GetDsk();
        track = analysis.tracks[analysis.entries[68].slot].get();

        for (size_t s = 0; s < track->sectors.size(); s++)
        {
            three = (track->sectors[s].sector == 3) ? static_cast<int> (s) : three;
        }

        query.text = L"D5 AA 96";
        hits       = InspectorSearch::Find (analysis, 68, query);

        hit.quarterTrack = 68;
        hit.track        = track;
        hit.sectorIndex  = three;
        hit.offset       = 0x10;
        hit.length       = 2;
        hits.push_back (hit);
        marks.Build (hits, nullptr, L"q");

        model.SetDisk (1);
        model.SetAnalysis (&analysis);
        model.SelectSector (68, three);
        context.analysis  = &analysis;
        context.model     = &model;
        context.hasDisk   = true;
        context.findMarks = &marks;

        SectorRowView   row (context);
        SectorsView  bytes (context);

        row.Layout   ({ 0, 0, 900, 200 }, scaler);
        bytes.Layout ({ 0, 0, 900, 700 }, scaler);
        bytes.Paint  (painter, text, theme);

        for (int y = 0; y < 200 && tip.find (L"Sector $3") == std::wstring::npos; y += 2)
        {
            for (int x = 0; x < 900 && tip.find (L"Sector $3") == std::wstring::npos; x += 2)
            {
                tip.clear();
                (void) row.GetTooltip ({ x, y }, tip, anchor);
            }
        }

        Assert::IsTrue (tip.ends_with (L"\n2 matches for the search \"q\""), tip.c_str());

        for (int y = 0; y < 700 && byteTip.empty(); y += 2)
        {
            for (int x = 0; x < 900 && byteTip.empty(); x += 2)
            {
                (void) bytes.GetTooltip ({ x, y }, byteTip, anchor);
            }
        }

        Assert::IsTrue (byteTip == L"Byte $10\nA match for the search \"q\"" || byteTip == L"Byte $11\nA match for the search \"q\"", byteTip.c_str());
    }


    TEST_METHOD (TheSectorBytesAreTypedInTheHexViewWhileEditing)
    {
        DiskAnalysis           analysis;
        InspectorViewModel     model;
        InspectorViewContext   context;
        EditController         edits;
        const TrackAnalysis  * track  = nullptr;
        DxuiTheme              theme  = DxuiTheme::Dark();
        DxuiDpiScaler          scaler;
        MockDxuiPainter        painter;
        MockDxuiTextRenderer   text;
        DxuiKeyEvent           ev     = {};
        std::wstring           reason;
        int                    edit   = -1;



        analysis = GetDsk();
        track    = analysis.tracks[analysis.entries[68].slot].get();

        model.SetDisk      (1);
        model.SetAnalysis  (&analysis);
        model.SelectSector (68, 0);
        context.analysis = &analysis;
        context.model    = &model;
        context.hasDisk  = true;
        context.edits    = &edits;

        SectorsView  bytes (context);

        bytes.Layout ({ 0, 0, 900, 700 }, scaler);
        bytes.Paint  (painter, text, theme);

        edit = edits.Begin (*track, 68, 0, reason);
        Assert::IsTrue (edit >= 0, reason.c_str());

        model.SelectBytes (4, 1, false);
        ev.kind = DxuiKeyEventKind::Char;
        ev.vk   = L'C';

        Assert::IsFalse (bytes.OnEditKey (ev), L"typing doesn't edit until Edit");

        bytes.SetEditing (true);
        bytes.Paint      (painter, text, theme);

        for (wchar_t ch : { L'C', L'3' })
        {
            ev.vk = ch;
            Assert::IsTrue (bytes.OnEditKey (ev));
        }

        Assert::AreEqual ((int) 0xC3, (int) edits.GetEdit (edit).bytes[4], L"two hex digits set one byte");
        Assert::AreEqual (5, model.GetFirstByte(), L"the caret moves on to the next byte");
    }



    TEST_METHOD (ASectorTipDrawsItsChecksumErrorInTheFailedColor)
    {
        DiskAnalysis                    analysis;
        InspectorViewModel              model;
        InspectorViewContext            context;
        std::shared_ptr<TrackAnalysis>  damaged;
        std::wstring                    tip;
        RECT                            anchor = {};
        POINT                           at     = {};
        vector<uint32_t>                colors;



        analysis = GetDsk();

        model.SetDisk (1);
        model.SetAnalysis (&analysis);
        context.palette  = DiskInspectorPalette::Resolve (DxuiTheme::Dark());
        context.analysis = &analysis;
        context.model    = &model;
        context.hasDisk  = true;
        model.SelectSector (68, 0);

        SectorRowView  row (context);

        row.Layout ({ 0, 0, 900, 200 }, DxuiDpiScaler());

        for (int y = 0; y < 200 && !tip.starts_with (L"Sector $3\n"); y += 2)
        {
            for (int x = 0; x < 900 && !tip.starts_with (L"Sector $3\n"); x += 2)
            {
                tip.clear();
                at = { x, y };
                (void) row.GetTooltip (at, tip, anchor);
            }
        }

        Assert::IsFalse (row.GetTooltipLineColors (at, colors), L"a good sector's lines are in the text color");

        damaged = std::make_shared<TrackAnalysis> (*analysis.tracks[analysis.entries[68].slot]);

        for (AnalyzedSector & sector : damaged->sectors)
        {
            sector.state = (sector.sector == 3) ? SectorState::BadData : sector.state;
        }

        analysis.tracks[analysis.entries[68].slot] = damaged;

        (void) row.GetTooltip (at, tip, anchor);
        Assert::IsTrue   (GetLine (tip, 2) == L"Data field checksum error", tip.c_str());
        Assert::IsTrue   (row.GetTooltipLineColors (at, colors));
        Assert::AreEqual (static_cast<size_t> (3), colors.size());
        Assert::AreEqual (context.palette.GetStateColor (SectorState::BadData), colors[2]);
        Assert::AreNotEqual (0u, colors[2]);
    }


    TEST_METHOD (TheFileMapCellTipHasItsFindHit)
    {
        vector<Byte>               image  = FileMapTestImages::MakeDos33();
        DiskAnalysis               analysis;
        vector<FileMap>            maps;
        InspectorViewContext       context;
        FindMarks                  marks;
        SearchHit                  hit;
        DxuiDpiScaler              scaler;
        vector<DxuiTooltipSwatch>  swatches;
        std::wstring               tip;
        RECT                       rect   = {};
        RECT                       anchor = {};
        int                        cell   = -1;
        int                        half   = 0;
        int                        qt     = 0;



        analysis = GetDos33();
        maps = FileMapBuilder::Build (analysis);
        cell = maps[0].GetCellOf (17, 0, half);
        qt   = 17 * DiskImage::kQuarterTracksPerWholeTrack;

        hit.quarterTrack = qt;
        hit.track        = analysis.tracks[analysis.entries[qt].slot].get();
        hit.offset       = 0;
        hit.length       = 1;

        for (size_t s = 0; s < hit.track->sectors.size(); s++)
        {
            hit.sectorIndex = (hit.track->sectors[s].sector == 0) ? static_cast<int> (s) : hit.sectorIndex;
        }

        marks.Build ({ hit }, &maps[0], L"VTOC");
        Assert::IsTrue (marks.IsCellMarked (cell));

        context.analysis  = &analysis;
        context.findMarks = &marks;

        FileMapGridView  grid (context);

        grid.SetMap (&maps[0]);
        grid.Layout ({ 0, 0, 640, 600 }, scaler);
        Assert::IsTrue (grid.GetCellRect (cell, rect));

        Assert::IsTrue   (grid.GetTooltip ({ (rect.left + rect.right) / 2, (rect.top + rect.bottom) / 2 }, tip, anchor));
        Assert::IsTrue   (tip.ends_with (L"\nA match for the search \"VTOC\""), tip.c_str());
        Assert::IsTrue   (grid.GetTooltipSwatches ({ (rect.left + rect.right) / 2, (rect.top + rect.bottom) / 2 }, swatches));
        Assert::AreEqual (context.palette.colors.findHit, swatches.back().argb);
        Assert::AreEqual (static_cast<int> (std::count (tip.begin(), tip.end(), L'\n')), swatches.back().line);
    }
};