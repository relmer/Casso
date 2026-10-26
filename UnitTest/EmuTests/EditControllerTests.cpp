#include "Pch.h"

#include "../EhmTestHelper.h"
#include "Devices/Disk/Inspector/DiskAnalyzer.h"
#include "Machines/Apple2/Common/NibblizationLayer.h"
#include "Ui/DiskInspector/EditController.h"
#include "InspectorTrackBuilder.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  EditControllerTests
//
//  Pending sector edits (FR-096 to FR-098, FR-108): typing over hex and
//  text, pasting up to $FF, undo and redo over pending edits, discarding,
//  the checksum each sector gets, the reasons a sector cannot be edited,
//  following a field after re-analysis, and the writes an apply makes.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (EditControllerTests)
{
public:

    //  A DSK with track 5 replaced: sector 3's data checksum broken, and the
    //  first byte of every sector $C1 so the text column has a high bit set.
    static void AnalyzePlanted (DiskAnalysis & out)
    {
        vector<Byte>                                     sectors (NibblizationLayer::kImageByteSize, 0);
        DiskImage                                        image;
        InspectorTrackBuilder                            odd;
        std::array<Byte, DiskFieldFormat::kSectorBytes>  bytes = {};
        int                                              s     = 0;



        AssertSucceeded (NibblizationLayer::NibblizeDsk (sectors, image));

        odd.AppendSync (48);

        for (s = 0; s < 16; s++)
        {
            InspectorTrackBuilder::FillPattern (s, bytes);
            bytes[0] = 0xC1;
            odd.AppendAddressField (DiskFieldKind::Sixteen, 254, 5, static_cast<Byte> (s));
            odd.AppendSync         (6);
            odd.AppendDataField    (DiskFieldKind::Sixteen, bytes, s == 3);
            odd.AppendSync         (20);
        }

        odd.PadToCells (NibblizationLayer::kTrackBitCapacity);
        odd.PackBits (image.GetTrackBitsForWrite (5));
        image.SetTrackBitCount (5, odd.GetCellCount());

        DiskAnalyzer::Analyze (DiskCopy::MakeFromImage (image, 1, "planted.dsk", 0, false), DecodeSettings::MakeStandard(), out);
    }



    static const TrackAnalysis & GetTrack (const DiskAnalysis & analysis)
    {
        return *analysis.tracks[analysis.entries[20].slot];
    }



    static int GetSectorIndex (const TrackAnalysis & track, Byte number)
    {
        int  index = -1;



        for (size_t s = 0; s < track.sectors.size(); s++)
        {
            index = (track.sectors[s].sector == number) ? static_cast<int> (s) : index;
        }

        Assert::IsTrue (index >= 0);

        return index;
    }



    TEST_METHOD (TypingOverwritesHexDigitsAndKeepsTheHighBitOfText)
    {
        DiskAnalysis    analysis;
        EditController  edits;
        std::wstring    reason;
        int             edit = -1;



        AnalyzePlanted (analysis);
        edit = edits.Begin (GetTrack (analysis), 20, GetSectorIndex (GetTrack (analysis), 2), reason);
        Assert::IsTrue (edit >= 0, reason.c_str());

        Assert::IsTrue   (edits.TypeHexDigit (edit, 1, true, L'a'));
        Assert::IsTrue   (edits.TypeHexDigit (edit, 1, false, L'7'));
        Assert::AreEqual (static_cast<Byte> (0xA7), edits.GetEdit (edit).bytes[1]);
        Assert::IsFalse  (edits.TypeHexDigit (edit, 1, true, L'g'), L"not a hex digit");

        Assert::IsTrue   (edits.TypeCharacter (edit, 0, L'b'));
        Assert::AreEqual (static_cast<Byte> (0xE2), edits.GetEdit (edit).bytes[0], L"the high bit of $C1 stays set");

        Assert::AreEqual (2, edits.GetCounts().bytes);
        Assert::AreEqual (1, edits.GetCounts().sectors);
    }



    TEST_METHOD (APasteStopsAtTheLastByteAndReportsTheOverflow)
    {
        DiskAnalysis    analysis;
        EditController  edits;
        std::wstring    reason;
        int             edit     = -1;
        int             overflow = 0;



        AnalyzePlanted (analysis);
        edit = edits.Begin (GetTrack (analysis), 20, GetSectorIndex (GetTrack (analysis), 2), reason);

        Assert::AreEqual (2, edits.Paste (edit, 0xFE, L"01 02 03", false, overflow));
        Assert::AreEqual (1, overflow);
        Assert::AreEqual (static_cast<Byte> (0x02), edits.GetEdit (edit).bytes[0xFF]);

        Assert::AreEqual (0, edits.Paste (edit, 0, L"0G", false, overflow), L"not hex");
        Assert::AreEqual (0, edits.Paste (edit, 0, L"012", false, overflow), L"an odd digit");

        Assert::AreEqual (2, edits.Paste (edit, 0, L"hi", true, overflow));
        Assert::AreEqual (static_cast<Byte> (0xE8), edits.GetEdit (edit).bytes[0], L"pasted text takes the high bit of $C1");
    }



    TEST_METHOD (UndoAndRedoStepThroughPendingChanges)
    {
        DiskAnalysis    analysis;
        EditController  edits;
        std::wstring    reason;
        int             edit  = -1;
        Byte            first = 0;



        AnalyzePlanted (analysis);
        edit  = edits.Begin (GetTrack (analysis), 20, GetSectorIndex (GetTrack (analysis), 2), reason);
        first = edits.GetEdit (edit).bytes[5];

        Assert::IsTrue  (edits.TypeHexDigit (edit, 5, true, L'F'));
        Assert::IsTrue  (edits.TypeHexDigit (edit, 6, true, L'F'));
        Assert::IsTrue  (edits.Undo());
        Assert::IsTrue  (edits.Undo());
        Assert::AreEqual (first, edits.GetEdit (edit).bytes[5]);
        Assert::IsFalse (edits.Undo(), L"nothing left to undo");
        Assert::IsTrue  (edits.Redo());
        Assert::AreEqual (1, edits.GetCounts().bytes);

        edits.DiscardAll();
        Assert::IsTrue  (edits.IsEmpty());
        Assert::IsTrue  (edits.Undo(), L"a discard is an action too");
        Assert::AreEqual (1, edits.GetCounts().bytes);
    }



    TEST_METHOD (DiscardSectorDropsOnlyThatSector)
    {
        DiskAnalysis    analysis;
        EditController  edits;
        std::wstring    reason;
        int             a = -1;
        int             b = -1;



        AnalyzePlanted (analysis);
        a = edits.Begin (GetTrack (analysis), 20, GetSectorIndex (GetTrack (analysis), 2), reason);
        b = edits.Begin (GetTrack (analysis), 20, GetSectorIndex (GetTrack (analysis), 4), reason);
        Assert::IsTrue (edits.TypeHexDigit (a, 0, true, L'0'));
        Assert::IsTrue (edits.TypeHexDigit (b, 0, true, L'0'));

        edits.DiscardSector (a);
        Assert::AreEqual (1, edits.GetCounts().sectors);
        Assert::AreEqual (static_cast<Byte> (4), edits.GetEdits()[0].sectorNumber);
    }



    TEST_METHOD (TheHeaderPredictsTheChecksumResult)
    {
        DiskAnalysis    analysis;
        EditController  edits;
        std::wstring    reason;
        int             good     = -1;
        int             bad      = -1;
        int             overflow = 0;



        AnalyzePlanted (analysis);
        good = edits.Begin (GetTrack (analysis), 20, GetSectorIndex (GetTrack (analysis), 2), reason);
        bad  = edits.Begin (GetTrack (analysis), 20, GetSectorIndex (GetTrack (analysis), 3), reason);
        Assert::IsTrue (bad >= 0, L"a sector whose data checksum fails can be repaired");

        edits.SetChecksumMode (good, ChecksumMode::KeepStored);
        Assert::IsTrue  (EditController::IsChecksumGoodAfter (edits.GetEdit (good), GetTrack (analysis)), L"no change, the stored checksum fits");
        Assert::AreEqual (1, edits.Paste (good, 0xFF, std::format (L"{:02X}", edits.GetEdit (good).bytes[0xFF] ^ 0xFF), false, overflow));
        Assert::IsFalse (EditController::IsChecksumGoodAfter (edits.GetEdit (good), GetTrack (analysis)), L"a changed last byte changes the checksum value, so the stored one fails");

        Assert::IsFalse (EditController::IsChecksumGoodAfter (edits.GetEdit (bad), GetTrack (analysis)) && edits.GetEdit (bad).checksumMode == ChecksumMode::KeepStored);
        Assert::IsTrue  (EditController::IsChecksumGoodAfter (edits.GetEdit (bad), GetTrack (analysis)), L"recomputed by default");
    }



    TEST_METHOD (EachReasonASectorCannotBeEdited)
    {
        DiskAnalysis   analysis;
        TrackAnalysis  track;
        int            s = 0;



        AnalyzePlanted (analysis);
        track = GetTrack (analysis);
        s     = GetSectorIndex (track, 2);
        Assert::AreEqual (std::wstring(), EditController::GetUneditableReason (track, s));

        track.fields[track.sectors[s].dataField].data.invalidNibbles.push_back ({});
        Assert::AreEqual (std::wstring (L"Nibbles outside the translate table"), EditController::GetUneditableReason (track, s));

        track.fields[track.sectors[s].dataField].hasNoise = true;
        Assert::AreEqual (std::wstring (L"Noise inside the data field"), EditController::GetUneditableReason (track, s));

        track.sectors[s].isAddressGood = false;
        Assert::AreEqual (std::wstring (L"Address checksum failed"), EditController::GetUneditableReason (track, s));

        track.sectors[s].isAddressCheck = false;
        Assert::AreEqual (std::wstring (L"Noise inside the data field"), EditController::GetUneditableReason (track, s), L"an address check that is off does not stop an edit");

        track.sectors[s].dataField = -1;
        Assert::AreEqual (std::wstring (L"No data field"), EditController::GetUneditableReason (track, s));

        track = GetTrack (analysis);
        track.context.isDamaged = true;
        Assert::AreEqual (std::wstring (L"Damaged track record"), EditController::GetUneditableReason (track, s));
    }



    TEST_METHOD (AnEditFollowsItsFieldAndIsOutOfDateWhenTheSectorChanged)
    {
        DiskAnalysis         analysis;
        EditController       edits;
        std::wstring         reason;
        vector<PendingEdit>  discarded;
        vector<SectorWrite>  writes;
        int                  edit  = -1;
        int                  slot  = 0;
        int                  s     = 0;



        AnalyzePlanted (analysis);
        slot = analysis.entries[20].slot;
        s    = GetSectorIndex (GetTrack (analysis), 2);
        edit = edits.Begin (GetTrack (analysis), 20, s, reason);
        Assert::IsTrue (edits.TypeHexDigit (edit, 0x10, true, L'9'));

        writes = edits.BuildWrites (analysis);
        Assert::AreEqual (static_cast<size_t> (1), writes.size());
        Assert::AreEqual (slot, writes[0].slot);
        Assert::AreEqual (static_cast<int> (GetTrack (analysis).fields[GetTrack (analysis).sectors[s].dataField].startCell), writes[0].dataFieldCell);

        //  The guest changed byte $20 of the sector since editing began.
        auto  changed = std::make_shared<TrackAnalysis> (GetTrack (analysis));

        changed->fields[changed->sectors[s].dataField].data.bytes[0x20] ^= 0xFF;
        analysis.tracks[slot] = changed;

        edits.Follow (analysis, discarded);
        Assert::IsTrue  (discarded.empty());
        Assert::IsTrue  (edits.GetEdit (edit).isOutOfDate);

        writes = edits.BuildWrites (analysis);
        Assert::AreEqual (changed->fields[changed->sectors[s].dataField].data.bytes[0x20], writes[0].bytes[0x20], L"the guest's byte is written");
        Assert::AreEqual (edits.GetEdit (edit).bytes[0x10], writes[0].bytes[0x10], L"and the user's byte is written");

        //  The sector became noisy: the edit is dropped and listed.
        changed->fields[changed->sectors[s].dataField].hasNoise = true;
        edits.Follow (analysis, discarded);
        Assert::AreEqual (static_cast<size_t> (1), discarded.size());
        Assert::IsTrue   (edits.IsEmpty());
    }


    TEST_METHOD (AWholeSectorIsOneActionForUseBsBytes)
    {
        DiskAnalysis         analysis;
        EditController       edits;
        std::wstring         reason;
        PendingEdit::Sector  bytes = {};
        int                  edit  = -1;



        AnalyzePlanted (analysis);
        edit = edits.Begin (GetTrack (analysis), 20, GetSectorIndex (GetTrack (analysis), 2), reason);
        bytes.fill (0x3C);

        Assert::IsTrue   (edits.SetSector (edit, bytes));
        Assert::IsTrue   (edits.GetEdit (edit).bytes == bytes);
        Assert::IsTrue   (edits.Undo());
        Assert::IsTrue   (edits.GetEdit (edit).bytes == edits.GetEdit (edit).bytesAtStart, L"one undo puts the whole sector back");
        Assert::IsFalse  (edits.SetSector (7, bytes), L"no such edit");
    }


    //  After a reload, an edit stays pending until its record is analyzed
    //  again, and then follows its field.
    TEST_METHOD (AnEditWaitsForItsRecordAfterAReload)
    {
        DiskAnalysis         analysis;
        DiskAnalysis         reloaded;
        EditController       edits;
        std::wstring         reason;
        vector<PendingEdit>  discarded;
        int                  edit  = -1;
        int                  slot  = 0;
        int                  s     = 0;



        AnalyzePlanted (analysis);
        slot = analysis.entries[20].slot;
        s    = GetSectorIndex (GetTrack (analysis), 2);
        edit = edits.Begin (GetTrack (analysis), 20, s, reason);
        Assert::IsTrue (edits.TypeHexDigit (edit, 0x10, true, L'9'));

        reloaded = analysis;
        reloaded.tracks[slot].reset();

        edits.Follow (reloaded, discarded);
        Assert::IsTrue (discarded.empty(), L"the edit waits for the record");
        Assert::IsFalse (edits.IsEmpty());

        edits.Follow (analysis, discarded);
        Assert::IsTrue  (discarded.empty());
        Assert::IsFalse (edits.GetEdit (edit).isOutOfDate, L"the record decodes to the same bytes");
    }
};
