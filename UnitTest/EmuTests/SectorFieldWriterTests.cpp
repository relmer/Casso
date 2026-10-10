#include "Pch.h"

#include "Devices/Disk/DiskImage.h"
#include "Devices/Disk/SectorFieldWriter.h"
#include "Machines/Apple2/Common/NibblizationLayer.h"
#include "InspectorTrackBuilder.h"
#include "FluxTestImages.h"
#include "Devices/Disk/SectorWrite.h"
#include "HResultAssert.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  SectorFieldWriterTests
//
//  The shared sector writer changes a data field's body and checksum nibbles
//  in place and nothing else: not the volume, the sync runs, the track's
//  length, any other field, or on a flux track any transition outside the
//  field. Records are found through the map, a write that cannot be made
//  changes nothing, and the Strict policy writes only tracks that decode
//  completely.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (SectorFieldWriterTests)
{
public:

    using SectorBytes = std::array<Byte, DiskFieldFormat::kSectorBytes>;

    static constexpr Byte  kVolume = 130;



    //  Puts a builder's cells in a slot, as a bit track or a flux track, and
    //  maps the given quarter tracks to it.
    static void InstallTrack (DiskImage & image, int slot, const InspectorTrackBuilder & builder, bool isFlux, std::initializer_list<int> quarterTracks)
    {
        vector<Byte>  packed;



        image.EnsureTrackSlots (slot + 1);

        if (isFlux)
        {
            image.SetFluxTrack (slot, builder.MakeFluxCopy()->fluxBytes);
        }
        else
        {
            builder.PackBits (packed);
            image.ResizeTrack (slot, builder.GetCellCount());
            image.GetTrackBitsForWrite (slot) = packed;
        }

        for (int qt : quarterTracks)
        {
            image.SetQuarterTrackSlot (qt, slot);
        }
    }



    static void MakeStandard (InspectorTrackBuilder & builder, int track)
    {
        builder.AppendStandardTrack (DiskFieldKind::Sixteen, kVolume, static_cast<Byte> (track), InspectorTrackBuilder::FillPattern);
    }



    static void MakeImage (DiskImage & image, bool isFlux)
    {
        InspectorTrackBuilder  builder;



        image.ClearQuarterTrackMap();
        MakeStandard (builder, 17);
        InstallTrack (image, 0, builder, isFlux, { 67, 68, 69 });
        image.ClearDirty();
    }



    static SectorWrite MakeWrite (int track, int sector, Byte fill)
    {
        SectorWrite  write;



        write.track  = track;
        write.sector = static_cast<Byte> (sector);
        write.bytes.fill (fill);

        return write;
    }



    static vector<LocatedField> Locate (const DiskImage & image, int slot, FramedTrack & track)
    {
        vector<LocatedField>  fields;



        LatchFramer::Frame   (*TrackCopy::MakeFromImage (image, slot), track);
        FieldLocator::Locate (track, FieldMarks::MakeStandard(), fields);

        return fields;
    }



    static const LocatedField * FindData (const vector<LocatedField> & fields, int sector)
    {
        const LocatedField *  found = nullptr;



        for (const LocatedField & f : fields)
        {
            if (f.role == FieldRole::Address && f.sector == sector && f.pairedField >= 0)
            {
                found = &fields[f.pairedField];
            }
        }

        return found;
    }



    //  Every cell outside [first, last) the same in both. Cell ranges may wrap.
    static void AssertCellsOutsideSame (const FramedTrack & before, const FramedTrack & after, uint32_t first, uint32_t last)
    {
        uint32_t  k      = 0;
        uint32_t  n      = before.cellCount;
        bool      inside = false;



        Assert::AreEqual (n, after.cellCount, L"the track keeps its length");

        for (k = 0; k < n; k++)
        {
            inside = (first <= last) ? (k >= first && k < last) : (k >= first || k < last);

            if (!inside)
            {
                Assert::AreEqual (static_cast<int> (before.cells[k]), static_cast<int> (after.cells[k]), std::format (L"cell {}", k).c_str());
            }
        }
    }



    static void AssertWrittenInPlace (bool isFlux)
    {
        DiskImage                 image;
        FramedTrack               before;
        FramedTrack               after;
        vector<LocatedField>      fieldsBefore;
        vector<LocatedField>      fieldsAfter;
        vector<SectorWriteError>  errors;
        SectorWrite               write = MakeWrite (17, 5, 0x42);
        const LocatedField *      data  = nullptr;
        uint32_t                  first = 0;
        uint32_t                  last  = 0;



        MakeImage (image, isFlux);
        fieldsBefore = Locate (image, 0, before);
        data         = FindData (fieldsBefore, 5);
        first        = before.nibbles[data->bodyNibble].startCell;
        last         = before.nibbles[(data->bodyNibble + DiskFieldFormat::kBodyLength62) % before.nibbles.size()].startCell + LatchFramer::kNibbleCells;

        AssertSucceeded (SectorFieldWriter::Write (image, std::span (&write, 1), SectorWritePolicy::Strict, errors));

        fieldsAfter = Locate (image, 0, after);

        Assert::IsTrue (FindData (fieldsAfter, 5)->data.bytes == write.bytes);
        Assert::IsTrue (FindData (fieldsAfter, 5)->data.isChecksumGood);
        AssertCellsOutsideSame (before, after, first, last);

        for (const LocatedField & f : fieldsAfter)
        {
            if (f.role == FieldRole::Address)
            {
                Assert::AreEqual (static_cast<int> (kVolume), static_cast<int> (f.volume), L"every address field keeps the volume");
            }
        }

        Assert::IsTrue (image.IsTrackDirty (0));
        Assert::IsTrue (image.IsTrackChangedOnlyByWriter (0));
    }



    TEST_METHOD (ABitTrackChangesOnlyTheDataField)
    {
        AssertWrittenInPlace (false);
    }



    TEST_METHOD (AFluxTrackChangesOnlyTheDataField)
    {
        AssertWrittenInPlace (true);
    }



    TEST_METHOD (AFluxTrackKeepsEveryTransitionOutsideTheFieldAndItsTurn)
    {
        DiskImage                 image;
        FramedTrack               before;
        FramedTrack               after;
        vector<LocatedField>      fields;
        vector<SectorWriteError>  errors;
        SectorWrite               write = MakeWrite (17, 9, 0xA5);
        uint32_t                  first = 0;
        uint32_t                  last  = 0;
        double                    start = 0;
        double                    end   = 0;
        double                    t     = 0;
        size_t                    i     = 0;



        MakeImage (image, true);
        fields = Locate (image, 0, before);
        first  = before.nibbles[FindData (fields, 9)->bodyNibble].startCell;
        last   = first + (DiskFieldFormat::kBodyLength62 + 1) * LatchFramer::kNibbleCells;

        for (i = 0; i < first; i++)
        {
            start += before.cellTicks[i];
        }

        for (i = 0; i < last; i++)
        {
            end += before.cellTicks[i];
        }

        AssertSucceeded (SectorFieldWriter::Write (image, std::span (&write, 1), SectorWritePolicy::Strict, errors));
        Locate (image, 0, after);

        Assert::AreEqual (before.turnTicks, after.turnTicks, 0.5, L"one turn takes the same time");

        for (i = 0; i < before.transitionTicks.size(); i++)
        {
            t = static_cast<double> (before.transitionTicks[i]);

            if (t <= start || t > end)
            {
                Assert::IsTrue (std::find (after.transitionTicks.begin(), after.transitionTicks.end(), before.transitionTicks[i]) != after.transitionTicks.end(),
                                std::format (L"transition at tick {} is still there", before.transitionTicks[i]).c_str());
            }
        }
    }



    TEST_METHOD (AFluxFieldKeepsItsRecordedCellTimes)
    {
        //  The track is written 4% slow; a write at the drive's own cell would
        //  shorten the field, so its cells must keep their slow times.
        static constexpr double  kSlowCell     = FluxTestImages::kNominalCellTicks * 1.04;
        static constexpr double  kTickRounding = 0.6;
        static constexpr size_t  kFieldCells   = DiskFieldFormat::kBodyLength62 * LatchFramer::kNibbleCells;



        InspectorTrackBuilder     builder;
        DiskImage                 image;
        FramedTrack               before;
        FramedTrack               after;
        vector<LocatedField>      fields;
        vector<SectorWriteError>  errors;
        vector<Byte>              packed;
        SectorWrite               write = MakeWrite (17, 2, 0x00);
        uint32_t                  first = 0;
        size_t                    i     = 0;
        double                    total = 0;



        MakeStandard (builder, 17);
        builder.PackBits (packed);

        image.ClearQuarterTrackMap();
        image.SetFluxTrack (0, FluxTestImages::BitsToFlux (packed, builder.GetCellCount(), { { 0, FluxTestImages::kNominalCellTicks * 1.04 } }));
        image.SetQuarterTrackSlot (68, 0);

        fields = Locate (image, 0, before);
        first  = before.nibbles[FindData (fields, 2)->bodyNibble].startCell;

        AssertSucceeded (SectorFieldWriter::Write (image, std::span (&write, 1), SectorWritePolicy::Strict, errors));
        Locate (image, 0, after);

        //  Transitions sit on whole 125 ns ticks, so one cell's time can be off
        //  by up to half a tick either way; the field as a whole cannot.
        for (i = first; i < first + kFieldCells; i++)
        {
            Assert::AreEqual (kSlowCell, after.cellTicks[i], kTickRounding, std::format (L"cell {}", i).c_str());
            total += after.cellTicks[i];
        }

        Assert::AreEqual (kSlowCell, total / kFieldCells, 0.01, L"the field keeps its slow speed, not the drive's own cell");
    }



    TEST_METHOD (TheRecordIsFoundThroughTheMap)
    {
        //  Track 13 lives in record 14, and record 13 holds track 14: slot N is
        //  not track N.
        InspectorTrackBuilder     track13;
        InspectorTrackBuilder     track14;
        DiskImage                 image;
        vector<Byte>              before14;
        vector<SectorWriteError>  errors;
        vector<Byte>              sectors;
        SectorWrite               write = MakeWrite (13, 0, 0x77);



        MakeStandard (track13, 13);
        MakeStandard (track14, 14);

        image.ClearQuarterTrackMap();
        InstallTrack (image, 14, track13, false, { 52 });
        InstallTrack (image, 13, track14, false, { 56 });
        before14 = image.GetTrackBits (13);

        AssertSucceeded (SectorFieldWriter::Write (image, std::span (&write, 1), SectorWritePolicy::Strict, errors));

        Assert::IsTrue (image.GetTrackBits (13) == before14, L"the record holding track 14 is untouched");
        Assert::IsTrue (image.IsTrackDirty (14));
        Assert::IsFalse (image.IsTrackDirty (13));
    }



    TEST_METHOD (OnlyTheFluxRecordChangesWhereBothMapsClaimTheQuarterTrack)
    {
        InspectorTrackBuilder     builder;
        DiskImage                 image;
        vector<Byte>              bitsBefore;
        vector<SectorWriteError>  errors;
        SectorWrite               write = MakeWrite (17, 3, 0x11);



        MakeStandard (builder, 17);

        image.ClearQuarterTrackMap();
        InstallTrack (image, 0, builder, false, { 68 });
        InstallTrack (image, 1, builder, true,  { 68 });
        bitsBefore = image.GetTrackBits (0);

        AssertSucceeded (SectorFieldWriter::Write (image, std::span (&write, 1), SectorWritePolicy::Strict, errors));

        Assert::IsTrue (image.GetTrackBits (0) == bitsBefore);
        Assert::IsFalse (image.IsTrackDirty (0));
        Assert::IsTrue (image.IsTrackDirty (1));
    }



    TEST_METHOD (StrictRefusesATrackThatDoesNotDecodeCompletely)
    {
        InspectorTrackBuilder     builder;
        DiskImage                 image;
        vector<Byte>              bitsBefore;
        vector<SectorWriteError>  errors;
        SectorBytes               bytes = {};
        SectorWrite               write = MakeWrite (17, 3, 0x11);
        HRESULT                   hr    = S_OK;
        int                       s     = 0;



        builder.AppendSync (48);

        for (s = 0; s < 16; s++)
        {
            InspectorTrackBuilder::FillPattern (s, bytes);
            builder.AppendAddressField (DiskFieldKind::Sixteen, kVolume, 17, static_cast<Byte> (s));
            builder.AppendSync         (6);
            builder.AppendDataField    (DiskFieldKind::Sixteen, bytes, s == 9);
            builder.AppendSync         (20);
        }

        image.ClearQuarterTrackMap();
        InstallTrack (image, 0, builder, false, { 68 });
        bitsBefore = image.GetTrackBits (0);

        hr = SectorFieldWriter::Write (image, std::span (&write, 1), SectorWritePolicy::Strict, errors);

        Assert::AreEqual (HRESULT_FROM_WIN32 (ERROR_ACCESS_DENIED), hr);
        Assert::AreEqual (1, static_cast<int> (errors.size()));
        Assert::IsTrue (errors[0].reason == SectorWriteFailure::TrackIncomplete);
        Assert::IsTrue (image.GetTrackBits (0) == bitsBefore);
        Assert::IsFalse (image.IsTrackDirty (0));
        Assert::IsTrue (SectorFieldWriter::Describe (errors[0]).find ("track 17") != std::string::npos);
    }



    TEST_METHOD (TheEditorRepairsASectorWhoseChecksumFails)
    {
        InspectorTrackBuilder     builder;
        DiskImage                 image;
        FramedTrack               track;
        vector<LocatedField>      fields;
        vector<SectorWriteError>  errors;
        SectorBytes               bytes = {};
        SectorWrite               write;



        builder.AppendSync         (48);
        builder.AppendAddressField (DiskFieldKind::Sixteen, kVolume, 17, 4);
        builder.AppendSync         (6);
        builder.AppendDataField    (DiskFieldKind::Sixteen, bytes, true);
        builder.AppendSync         (48);

        image.ClearQuarterTrackMap();
        InstallTrack (image, 0, builder, false, { 68 });

        fields = Locate (image, 0, track);
        Assert::IsFalse (FindData (fields, 4)->data.isChecksumGood);

        write.slot          = 0;
        write.dataFieldCell = static_cast<int> (FindData (fields, 4)->startCell);
        write.sector        = 4;
        write.bytes.fill (0x3C);

        AssertSucceeded (SectorFieldWriter::Write (image, std::span (&write, 1), SectorWritePolicy::Editor, errors));

        fields = Locate (image, 0, track);
        Assert::IsTrue (FindData (fields, 4)->data.isChecksumGood);
        Assert::IsTrue (FindData (fields, 4)->data.bytes == write.bytes);
    }



    TEST_METHOD (KeepingTheStoredChecksumLeavesItsNibble)
    {
        //  Byte $FF from $00 to $FC with the stored checksum kept: the field
        //  then reads bad, and its checksum nibble is unchanged.
        DiskImage                 image;
        FramedTrack               before;
        FramedTrack               after;
        vector<LocatedField>      fields;
        vector<SectorWriteError>  errors;
        SectorWrite               write;
        const LocatedField *      data  = nullptr;
        Byte                      nib   = 0;



        MakeImage (image, false);
        fields = Locate (image, 0, before);
        data   = FindData (fields, 6);
        nib    = before.nibbles[data->bodyNibble + DiskFieldFormat::kBodyLength62].value;

        write.slot          = 0;
        write.dataFieldCell = static_cast<int> (data->startCell);
        write.sector        = 6;
        write.bytes         = data->data.bytes;
        write.bytes[0xFF]   = static_cast<Byte> (write.bytes[0xFF] ^ 0xFC);
        write.checksumMode  = ChecksumMode::KeepStored;

        AssertSucceeded (SectorFieldWriter::Write (image, std::span (&write, 1), SectorWritePolicy::Editor, errors));

        fields = Locate (image, 0, after);
        data   = FindData (fields, 6);

        Assert::AreEqual (static_cast<int> (nib), static_cast<int> (after.nibbles[data->bodyNibble + DiskFieldFormat::kBodyLength62].value));
        Assert::IsFalse (data->data.isChecksumGood);
        Assert::IsTrue (data->data.bytes == write.bytes);
    }



    TEST_METHOD (OfTwoSectorsWithOneNumberOnlyTheChosenOneChanges)
    {
        InspectorTrackBuilder     builder;
        DiskImage                 image;
        FramedTrack               track;
        vector<LocatedField>      fields;
        vector<SectorWriteError>  errors;
        SectorBytes               bytes = {};
        SectorWrite               write;
        vector<int>               datas;
        int                       copy  = 0;



        builder.AppendSync (48);

        for (copy = 0; copy < 2; copy++)
        {
            bytes.fill (static_cast<Byte> (0x10 + copy));
            builder.AppendAddressField (DiskFieldKind::Sixteen, kVolume, 17, 5);
            builder.AppendSync         (6);
            builder.AppendDataField    (DiskFieldKind::Sixteen, bytes);
            builder.AppendSync         (48);
        }

        image.ClearQuarterTrackMap();
        InstallTrack (image, 0, builder, false, { 68 });

        fields = Locate (image, 0, track);

        for (const LocatedField & f : fields)
        {
            if (f.role == FieldRole::Data)
            {
                datas.push_back (static_cast<int> (&f - fields.data()));
            }
        }

        write.slot          = 0;
        write.dataFieldCell = static_cast<int> (fields[datas[1]].startCell);
        write.sector        = 5;
        write.bytes.fill (0xEE);

        AssertSucceeded (SectorFieldWriter::Write (image, std::span (&write, 1), SectorWritePolicy::Editor, errors));

        fields = Locate (image, 0, track);
        Assert::AreEqual (static_cast<int> (0x10), static_cast<int> (fields[datas[0]].data.bytes[0]), L"the first sector $5 is untouched");
        Assert::AreEqual (static_cast<int> (0xEE), static_cast<int> (fields[datas[1]].data.bytes[0]));
    }



    TEST_METHOD (ExtraZeroCellsAfterFieldNibblesAreKept)
    {
        InspectorTrackBuilder     builder;
        DiskImage                 image;
        FramedTrack               before;
        FramedTrack               after;
        vector<LocatedField>      fields;
        vector<SectorWriteError>  errors;
        SectorBytes               bytes    = {};
        vector<Byte>              body;
        Byte                      checksum = 0;
        SectorWrite               write;
        int                       i        = 0;



        //  A data field with one extra zero cell after every fourth body
        //  nibble, as timing-bit protections write.
        builder.AppendSync         (48);
        builder.AppendAddressField (DiskFieldKind::Sixteen, kVolume, 17, 1);
        builder.AppendSync         (6);
        DiskFieldFormat::EncodeData (DiskFieldKind::Sixteen, bytes, body, checksum);
        builder.AppendNibble (0xD5);
        builder.AppendNibble (0xAA);
        builder.AppendNibble (0xAD);

        for (i = 0; i < static_cast<int> (body.size()); i++)
        {
            builder.AppendNibble (body[i], (i % 4 == 3) ? 1 : 0);
        }

        builder.AppendNibble (checksum);
        builder.AppendNibble (0xDE);
        builder.AppendNibble (0xAA);
        builder.AppendNibble (0xEB);
        builder.AppendSync   (48);

        image.ClearQuarterTrackMap();
        InstallTrack (image, 0, builder, false, { 68 });
        fields = Locate (image, 0, before);

        write.slot          = 0;
        write.dataFieldCell = static_cast<int> (FindData (fields, 1)->startCell);
        write.sector        = 1;
        write.bytes.fill (0x81);

        AssertSucceeded (SectorFieldWriter::Write (image, std::span (&write, 1), SectorWritePolicy::Editor, errors));
        Locate (image, 0, after);

        Assert::AreEqual (before.nibbles.size(), after.nibbles.size());

        for (i = 0; i < static_cast<int> (before.nibbles.size()); i++)
        {
            Assert::AreEqual (before.nibbles[i].startCell, after.nibbles[i].startCell);
            Assert::AreEqual (static_cast<int> (before.nibbles[i].extraZeroCells), static_cast<int> (after.nibbles[i].extraZeroCells));
        }
    }



    TEST_METHOD (AFailedWriteChangesNothing)
    {
        DiskImage                 image;
        vector<Byte>              bitsBefore;
        vector<SectorWriteError>  errors;
        SectorWrite               writes[2] = { MakeWrite (17, 3, 0x11), MakeWrite (18, 3, 0x22) };
        HRESULT                   hr        = S_OK;



        MakeImage (image, false);
        bitsBefore = image.GetTrackBits (0);

        hr = SectorFieldWriter::Write (image, writes, SectorWritePolicy::Strict, errors);

        Assert::IsTrue (FAILED (hr));
        Assert::IsTrue (errors.size() == 1 && errors[0].reason == SectorWriteFailure::NoRecord, L"track 18 has no record");
        Assert::IsTrue (image.GetTrackBits (0) == bitsBefore, L"track 17's write was not made either");
        Assert::IsFalse (image.IsDirty());
    }



    TEST_METHOD (AThirteenSectorFieldIsWrittenAndDecodes)
    {
        InspectorTrackBuilder     builder;
        DiskImage                 image;
        FramedTrack               track;
        vector<LocatedField>      fields;
        vector<SectorWriteError>  errors;
        SectorWrite               write;



        builder.AppendStandardTrack (DiskFieldKind::Thirteen, kVolume, 2, InspectorTrackBuilder::FillPattern);

        image.ClearQuarterTrackMap();
        InstallTrack (image, 0, builder, false, { 8 });
        fields = Locate (image, 0, track);

        write.slot          = 0;
        write.dataFieldCell = static_cast<int> (FindData (fields, 7)->startCell);
        write.sector        = 7;
        InspectorTrackBuilder::FillPattern (99, write.bytes);

        AssertSucceeded (SectorFieldWriter::Write (image, std::span (&write, 1), SectorWritePolicy::Editor, errors));

        fields = Locate (image, 0, track);
        Assert::IsTrue (FindData (fields, 7)->kind == DiskFieldKind::Thirteen);
        Assert::IsTrue (FindData (fields, 7)->data.isChecksumGood);
        Assert::IsTrue (FindData (fields, 7)->data.bytes == write.bytes);
    }
};
