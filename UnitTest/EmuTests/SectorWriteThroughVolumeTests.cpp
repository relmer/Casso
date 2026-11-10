#include "Pch.h"

#include "../EhmTestHelper.h"
#include "CassoExplorer/Model/DiskOperations.h"
#include "Devices/Disk/DiskCommandRunner.h"
#include "Devices/Disk/DiskImage.h"
#include "Devices/Disk/Inspector/FieldLocator.h"
#include "Machines/Apple2/Common/BlankDiskBuilder.h"
#include "Machines/Apple2/Common/NibbleImageCodec.h"
#include "Machines/Apple2/Common/NibblizationLayer.h"
#include "Machines/Apple2/Common/VolumeImage.h"
#include "Machines/Apple2/Common/WozLoader.h"
#include "FakeDiskFileIo.h"
#include "InspectorTrackBuilder.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  SectorWriteThroughVolumeTests
//
//  SC-017 through the path every `disk` command write takes (VolumeImage::Save):
//  only the data fields written change; every address field keeps the disk's
//  volume; every changed track keeps its length, its sync runs and the place
//  of every sector; every other record is unchanged; the standard WOZ layouts
//  are accepted; records out of track order are found through the map; and
//  an image with a record of its own between whole tracks is refused whole.
//
//  The same is checked for the writes made through that path from above:
//  `disk mkdir` and `disk rmdir`, and Casso Explorer's directory and file
//  writes. After a write that fails, at the track map or at any step of the
//  save, the file is byte for byte as it was.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (SectorWriteThroughVolumeTests)
{
public:

    static constexpr Byte  kVolume     = 130;
    static constexpr int   kTracks     = NibblizationLayer::kTrackCount;
    static constexpr int   kSectorSize = NibblizationLayer::kSectorByteSize;



    enum class Layout
    {
        Standard,           //  N-0.25, N and N+0.25 on track N's record
        WithHalfTracks,     //  standard, and N+0.5 on track N's record too
        Casso,              //  N to N+0.75 on track N's record
    };



    static vector<int> GetQuarterTracks (int track, Layout layout)
    {
        vector<int>  qts;



        switch (layout)
        {
            case Layout::Standard:       qts = { 4 * track - 1, 4 * track, 4 * track + 1 };                break;
            case Layout::WithHalfTracks: qts = { 4 * track - 1, 4 * track, 4 * track + 1, 4 * track + 2 }; break;
            case Layout::Casso:          qts = { 4 * track, 4 * track + 1, 4 * track + 2, 4 * track + 3 };  break;
        }

        qts.erase (std::remove (qts.begin(), qts.end(), -1), qts.end());

        return qts;
    }



    //  A 35-track WOZ with volume 130 in every address field. Track t is in
    //  record order[t], so the records need not be in track order.
    static vector<Byte> MakeWoz (Layout layout, bool isFlux, const vector<int> & order)
    {
        vector<WozSyntheticTrack>  tracks (kTracks);
        vector<Byte>               bytes;
        int                        t      = 0;



        for (t = 0; t < kTracks; t++)
        {
            InspectorTrackBuilder  builder;
            WozSyntheticTrack &    record = tracks[order[t]];

            builder.AppendStandardTrack (DiskFieldKind::Sixteen, kVolume, static_cast<Byte> (t), InspectorTrackBuilder::FillPattern);

            if (isFlux)
            {
                record.data   = builder.MakeFluxCopy()->fluxBytes;
                record.isFlux = true;
            }
            else
            {
                builder.PackBits (record.data);
                record.bitCount = builder.GetCellCount();
            }

            record.quarterTracks = GetQuarterTracks (t, layout);
        }

        AssertSucceeded (WozLoader::BuildSyntheticV21 (tracks, bytes));

        return bytes;
    }



    static vector<int> InOrder()
    {
        vector<int>  order (kTracks);
        int          t     = 0;



        for (t = 0; t < kTracks; t++)
        {
            order[t] = t;
        }

        return order;
    }



    static void FrameRecord (const DiskImage & image, int slot, FramedTrack & track, vector<LocatedField> & fields)
    {
        LatchFramer::Frame   (*TrackCopy::MakeFromImage (image, slot), track);
        FieldLocator::Locate (track, FieldMarks::MakeStandard(), fields);
    }



    //  Writes one byte of DOS logical sector 3 on the given track through the
    //  disk command's path, and checks everything SC-017 asks of the result.
    static void AssertWriteKeepsTheTrack (const vector<Byte> & woz, int track)
    {
        vector<Byte>          sectors;
        vector<Byte>          edited;
        vector<Byte>          written;
        vector<Byte>          readBack;
        std::string           refusal;
        SectorDecodeReport    report;
        DiskImage             before;
        DiskImage             after;
        FramedTrack           framedBefore;
        FramedTrack           framedAfter;
        vector<LocatedField>  fieldsBefore;
        vector<LocatedField>  fieldsAfter;
        size_t                at         = 0;
        int                   slot       = 0;
        int                   other      = 0;
        size_t                i          = 0;



        AssertSucceeded (VolumeImage::Load (woz, "t.woz", sectors, report));

        at          = (static_cast<size_t> (track) * NibblizationLayer::kSectorsPerTrack + 3) * kSectorSize + 0x40;
        edited      = sectors;
        edited[at] ^= 0x5A;

        AssertSucceeded (VolumeImage::Save (woz, "t.woz", edited, written, refusal), L"the write is accepted");
        Assert::AreEqual (std::string(), refusal);

        AssertSucceeded (VolumeImage::Load (written, "t.woz", readBack, report));
        Assert::IsTrue (readBack == edited, L"the volume reads back exactly as written");

        AssertSucceeded (WozLoader::Load (woz, before));
        AssertSucceeded (WozLoader::Load (written, after));

        slot = before.ResolveWholeTrack (track);
        Assert::AreEqual (slot, after.ResolveWholeTrack (track), L"the track stays in its record");

        for (other = 0; other < before.GetTrackCount(); other++)
        {
            if (other == slot)
            {
                continue;
            }

            Assert::IsTrue (before.GetTrackBits (other) == after.GetTrackBits (other)
                            && before.GetFluxTrack (other).GetBytes() == after.GetFluxTrack (other).GetBytes(),
                            std::format (L"record {} is unchanged", other).c_str());
        }

        FrameRecord (before, slot, framedBefore, fieldsBefore);
        FrameRecord (after,  slot, framedAfter,  fieldsAfter);

        Assert::AreEqual (framedBefore.cellCount, framedAfter.cellCount, L"the track keeps its length");
        Assert::AreEqual (framedBefore.nibbles.size(), framedAfter.nibbles.size());
        Assert::AreEqual (fieldsBefore.size(), fieldsAfter.size());

        for (i = 0; i < framedBefore.nibbles.size(); i++)
        {
            Assert::AreEqual (framedBefore.nibbles[i].startCell, framedAfter.nibbles[i].startCell, L"no nibble moves");
        }

        for (const LocatedField & f : fieldsAfter)
        {
            if (f.role == FieldRole::Address)
            {
                Assert::AreEqual (static_cast<int> (kVolume), static_cast<int> (f.volume), L"every address field keeps the volume");
            }
        }
    }



    TEST_METHOD (TheStandardLayoutIsWritten)
    {
        AssertWriteKeepsTheTrack (MakeWoz (Layout::Standard, false, InOrder()), 17);
    }



    TEST_METHOD (ALayoutWithHalfTracksOnTheirOwnRecordIsWritten)
    {
        AssertWriteKeepsTheTrack (MakeWoz (Layout::WithHalfTracks, false, InOrder()), 20);
    }



    TEST_METHOD (CassosOwnLayoutIsWritten)
    {
        AssertWriteKeepsTheTrack (MakeWoz (Layout::Casso, false, InOrder()), 3);
    }



    TEST_METHOD (AFluxDiskIsWritten)
    {
        AssertWriteKeepsTheTrack (MakeWoz (Layout::Standard, true, InOrder()), 17);
    }



    TEST_METHOD (RecordsOutOfTrackOrderAreFoundThroughTheMap)
    {
        vector<int>  order = InOrder();



        std::reverse (order.begin(), order.end());

        AssertWriteKeepsTheTrack (MakeWoz (Layout::Standard, false, order), 13);
    }



    TEST_METHOD (ARecordOfItsOwnBetweenTracksRefusesEveryWrite)
    {
        vector<WozSyntheticTrack>  tracks;
        vector<Byte>               woz;
        vector<Byte>               sectors;
        vector<Byte>               written;
        std::string                refusal;
        SectorDecodeReport         report;
        InspectorTrackBuilder      halfTrack;
        WozSyntheticTrack          extra;
        HRESULT                    hr       = S_OK;
        int                        t        = 0;



        //  The standard layout, plus quarter track 2.5 on a record of its own.
        for (t = 0; t < kTracks; t++)
        {
            InspectorTrackBuilder  builder;
            WozSyntheticTrack      record;

            builder.AppendStandardTrack (DiskFieldKind::Sixteen, kVolume, static_cast<Byte> (t), InspectorTrackBuilder::FillPattern);
            builder.PackBits (record.data);
            record.bitCount      = builder.GetCellCount();
            record.quarterTracks = GetQuarterTracks (t, Layout::Standard);
            tracks.push_back (record);
        }

        halfTrack.AppendStandardTrack (DiskFieldKind::Sixteen, kVolume, 2, InspectorTrackBuilder::FillPattern);
        halfTrack.PackBits (extra.data);
        extra.bitCount      = halfTrack.GetCellCount();
        extra.quarterTracks = { 10 };
        tracks.push_back (extra);

        AssertSucceeded (WozLoader::BuildSyntheticV21 (tracks, woz));
        AssertSucceeded (VolumeImage::Load (woz, "t.woz", sectors, report));

        sectors[0x8000] ^= 1;

        hr = VolumeImage::Save (woz, "t.woz", sectors, written, refusal);

        Assert::AreEqual (HRESULT_FROM_WIN32 (ERROR_ACCESS_DENIED), hr);
        Assert::IsTrue (written.empty(), L"the file is left as it was");
        Assert::IsTrue (refusal.find ("quarter track 2.5 plays track record 35") != std::string::npos);
    }



    TEST_METHOD (ANibbleImageChangesOnlyTheWrittenBytes)
    {
        BlankDiskSpec       spec;
        vector<Byte>        nib;
        vector<Byte>        sectors;
        vector<Byte>        written;
        std::string         refusal;
        SectorDecodeReport  report;
        size_t              differ  = 0;
        size_t              firstAt = SIZE_MAX;
        size_t              lastAt  = 0;
        size_t              i       = 0;



        spec.format   = DiskFormat::Nib;
        spec.contents = BlankDiskContents::Dos33;

        AssertSucceeded (BlankDiskBuilder::Build (spec, BootPayload(), nib));
        AssertSucceeded (VolumeImage::Load (nib, "t.nib", sectors, report));

        sectors[(static_cast<size_t> (9) * NibblizationLayer::kSectorsPerTrack + 4) * kSectorSize + 7] ^= 0xFF;

        AssertSucceeded (VolumeImage::Save (nib, "t.nib", sectors, written, refusal));
        Assert::AreEqual (nib.size(), written.size());

        for (i = 0; i < nib.size(); i++)
        {
            if (nib[i] != written[i])
            {
                differ++;
                firstAt = std::min (firstAt, i);
                lastAt  = i;
            }
        }

        Assert::IsTrue (differ > 0 && differ <= DiskFieldFormat::kBodyLength62 + 1, L"only data field nibbles change");
        Assert::IsTrue (lastAt - firstAt <= DiskFieldFormat::kBodyLength62, L"all within one data field");
        Assert::AreEqual (static_cast<size_t> (9), firstAt / NibbleImageCodec::kNibTrackSize, L"on track 9's block");
    }


    using Command = CommandLineOptions::DiskOptions::Command;

    static constexpr const char *  kWozPath = "C:\\disks\\sc017.woz";



    //  MakeWoz's disk with a blank ProDOS volume written to it.
    static vector<Byte> MakeProDosWoz (Layout layout)
    {
        BlankDiskSpec       spec;
        vector<Byte>        blank;
        vector<Byte>        sectors;
        vector<Byte>        woz     = MakeWoz (layout, false, InOrder());
        vector<Byte>        written;
        std::string         refusal;
        SectorDecodeReport  report;



        spec.format   = DiskFormat::Po;
        spec.contents = BlankDiskContents::ProDos;

        AssertSucceeded (BlankDiskBuilder::Build (spec, BootPayload(), blank));
        AssertSucceeded (VolumeImage::Load (blank, "t.po", sectors, report));
        AssertSucceeded (VolumeImage::Save (woz, "t.woz", sectors, written, refusal));

        return written;
    }



    //  The same tracks in the standard layout, and a copy of track 2 as a
    //  separate record on quarter track 2.5.
    static vector<Byte> AddRecordBetweenTracks (const vector<Byte> & woz)
    {
        DiskImage                  image;
        vector<WozSyntheticTrack>  tracks;
        vector<Byte>               bytes;
        WozSyntheticTrack          extra;
        int                        t      = 0;



        AssertSucceeded (WozLoader::Load (woz, image));

        for (t = 0; t < kTracks; t++)
        {
            WozSyntheticTrack  record;
            int                slot   = image.ResolveWholeTrack (t);

            record.data          = image.GetTrackBits (slot);
            record.bitCount      = image.GetTrackBitCount (slot);
            record.quarterTracks = GetQuarterTracks (t, Layout::Standard);
            tracks.push_back (record);
        }

        extra               = tracks[2];
        extra.quarterTracks = { 10 };
        tracks.push_back (extra);

        AssertSucceeded (WozLoader::BuildSyntheticV21 (tracks, bytes));

        return bytes;
    }



    static void Seed (FakeDiskFileIo & io, const vector<Byte> & woz)
    {
        io.files[kWozPath]  = woz;
        io.stamps[kWozPath] = FileStamp { woz.size(), 100 };
    }



    static DiskCommandResult RunDiskCommand (FakeDiskFileIo & io, Command command, const char * path)
    {
        DiskCommandRunner   runner (io);
        CommandLineOptions  options;



        options.subcommand     = CommandLineOptions::Subcommand::Disk;
        options.disk.command   = command;
        options.disk.imagePath = kWozPath;
        options.disk.path      = path;
        options.disk.recurse   = true;
        options.disk.yes       = true;

        return runner.Run (options);
    }



    static FilePayload MakePayload()
    {
        FilePayload  payload;



        payload.bytes.assign (300, 0xC3);
        payload.type           = 0x06;
        payload.loadAddress    = 0x2000;
        payload.hasLoadAddress = true;

        return payload;
    }



    //  Every record a write changed has its length, the place of every
    //  nibble and the disk's volume in every address field, and at least one
    //  record changed.
    static void AssertChangedRecordsAreLaidOutTheSame (const vector<Byte> & wozBefore, const vector<Byte> & wozAfter, const wchar_t * what)
    {
        DiskImage  before;
        DiskImage  after;
        int        changed = 0;
        int        slot    = 0;
        size_t     i       = 0;



        AssertSucceeded (WozLoader::Load (wozBefore, before));
        AssertSucceeded (WozLoader::Load (wozAfter,  after));

        Assert::AreEqual (before.GetTrackCount(), after.GetTrackCount(), what);

        for (slot = 0; slot < before.GetTrackCount(); slot++)
        {
            FramedTrack           framedBefore;
            FramedTrack           framedAfter;
            vector<LocatedField>  fieldsBefore;
            vector<LocatedField>  fieldsAfter;

            if (before.GetTrackBits (slot) == after.GetTrackBits (slot))
            {
                continue;
            }

            changed++;

            FrameRecord (before, slot, framedBefore, fieldsBefore);
            FrameRecord (after,  slot, framedAfter,  fieldsAfter);

            Assert::AreEqual (framedBefore.cellCount, framedAfter.cellCount, what);
            Assert::AreEqual (framedBefore.nibbles.size(), framedAfter.nibbles.size(), what);
            Assert::AreEqual (fieldsBefore.size(), fieldsAfter.size(), what);

            for (i = 0; i < framedBefore.nibbles.size(); i++)
            {
                Assert::AreEqual (framedBefore.nibbles[i].startCell, framedAfter.nibbles[i].startCell, what);
            }

            for (const LocatedField & f : fieldsAfter)
            {
                if (f.role == FieldRole::Address)
                {
                    Assert::AreEqual (static_cast<int> (kVolume), static_cast<int> (f.volume), what);
                }
            }
        }

        Assert::IsTrue (changed > 0, what);
    }



    TEST_METHOD (TracksAreLaidOutTheSameAfterDiskMkdirAndRmdir)
    {
        for (Layout layout : { Layout::Standard, Layout::WithHalfTracks, Layout::Casso })
        {
            FakeDiskFileIo     io;
            vector<Byte>       blank   = MakeProDosWoz (layout);
            vector<Byte>       made;
            DiskCommandResult  result;



            Seed (io, blank);

            result = RunDiskCommand (io, Command::Mkdir, "CASSO/DEEP");
            Assert::AreEqual (DiskCommandResult::kClean, result.exitStatus, L"mkdir");

            made = io.files[kWozPath];
            AssertChangedRecordsAreLaidOutTheSame (blank, made, L"mkdir");

            result = RunDiskCommand (io, Command::List, "CASSO");
            Assert::IsTrue (result.output.find ("DEEP") != std::string::npos, L"the directory reads back");

            result = RunDiskCommand (io, Command::Rmdir, "CASSO");
            Assert::AreEqual (DiskCommandResult::kClean, result.exitStatus, L"rmdir");
            AssertChangedRecordsAreLaidOutTheSame (made, io.files[kWozPath], L"rmdir");

            result = RunDiskCommand (io, Command::List, "");
            Assert::IsTrue (result.output.find ("CASSO") == std::string::npos, L"the directory is gone");
            Assert::IsTrue (io.HasNoTemporaryFiles());
        }
    }



    TEST_METHOD (TracksAreLaidOutTheSameAfterExplorerDirectoryAndFileWrites)
    {
        FakeDiskFileIo  io;
        DiskOperations  ops (io);
        vector<Byte>    blank    = MakeProDosWoz (Layout::Standard);
        vector<Byte>    made;
        vector<Byte>    filled;
        FilePayload     readBack;



        Seed (io, blank);

        Assert::IsTrue (ops.Mkdir (kWozPath, "CASSO").Succeeded(), L"mkdir");
        made = io.files[kWozPath];
        AssertChangedRecordsAreLaidOutTheSame (blank, made, L"mkdir");

        Assert::IsTrue (ops.WritePayload (kWozPath, "DATA", MakePayload()).Succeeded(), L"file write");
        filled = io.files[kWozPath];
        AssertChangedRecordsAreLaidOutTheSame (made, filled, L"file write");

        Assert::IsTrue (ops.Read (kWozPath, "DATA", readBack).Succeeded());
        Assert::IsTrue (readBack.bytes == MakePayload().bytes, L"the file reads back");

        Assert::IsTrue (ops.Rmdir (kWozPath, "CASSO", true).Succeeded(), L"rmdir");
        AssertChangedRecordsAreLaidOutTheSame (filled, io.files[kWozPath], L"rmdir");
        Assert::IsTrue (io.HasNoTemporaryFiles());
    }



    TEST_METHOD (EveryDirectoryAndFileWriteFailsOnARecordBetweenTracks)
    {
        FakeDiskFileIo     io;
        DiskOperations     ops (io);
        vector<Byte>       woz    = AddRecordBetweenTracks (MakeProDosWoz (Layout::Standard));
        DiskCommandResult  result;



        Seed (io, woz);

        result = RunDiskCommand (io, Command::Mkdir, "CASSO");
        Assert::AreNotEqual (DiskCommandResult::kClean, result.exitStatus);
        Assert::IsTrue (result.diagnostics.find ("quarter track 2.5 plays track record 35") != std::string::npos);

        Assert::IsFalse (ops.Mkdir (kWozPath, "CASSO").Succeeded());
        Assert::IsFalse (ops.WritePayload (kWozPath, "DATA", MakePayload()).Succeeded());

        Assert::IsTrue   (io.files[kWozPath] == woz, L"the file is byte for byte as it was");
        Assert::AreEqual (0, io.writeCount);
        Assert::IsTrue   (io.HasNoTemporaryFiles());
    }



    TEST_METHOD (TheFileIsAsItWasAfterASaveThatFailsAtAnyStep)
    {
        struct Case
        {
            const wchar_t  * name;
            void          (* arm) (FakeDiskFileIo & io);
        };

        const Case  cases[] =
        {
            { L"write",   [] (FakeDiskFileIo & io) { io.failNextWrite   = true; } },
            { L"flush",   [] (FakeDiskFileIo & io) { io.failNextFlush   = true; } },
            { L"replace", [] (FakeDiskFileIo & io) { io.failNextReplace = true; } },
        };

        vector<Byte>  woz = MakeProDosWoz (Layout::Standard);



        for (const Case & c : cases)
        {
            FakeDiskFileIo     io;
            DiskOperations     ops (io);
            DiskCommandResult  result;

            Seed (io, woz);

            c.arm (io);
            result = RunDiskCommand (io, Command::Mkdir, "CASSO");
            Assert::AreNotEqual (DiskCommandResult::kClean, result.exitStatus, c.name);
            Assert::IsTrue (io.files[kWozPath] == woz, c.name);
            Assert::IsTrue (io.HasNoTemporaryFiles(), c.name);

            c.arm (io);
            Assert::IsFalse (ops.WritePayload (kWozPath, "DATA", MakePayload()).Succeeded(), c.name);
            Assert::IsTrue  (io.files[kWozPath] == woz, c.name);
            Assert::IsTrue  (io.HasNoTemporaryFiles(), c.name);
        }
    }
};
