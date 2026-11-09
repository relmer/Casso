#include "Pch.h"

#include "EhmTestHelper.h"
#include "TestMachine.h"
#include "TextScreenScraper.h"
#include "CassoLink/InMemoryCassoLinkFolder.h"
#include "EmuTests/FakeDiskFileIo.h"
#include "CassoLink/CassoLinkSession.h"
#include "CassoLink/LinkProgram.h"
#include "Devices/RamDevice.h"
#include "Devices/Disk/DiskImageStore.h"
#include "Machines/Apple2/Apple2e/Apple2eMmu.h"
#include "Machines/Apple2/Apple2e/Apple2eSoftSwitchBank.h"
#include "Machines/Apple2/Common/Disk2Controller.h"
#include "Machines/Apple2/Common/NibblizationLayer.h"
#include "Machines/Apple2/Common/PascalVolume.h"
#include "Machines/Apple2/Common/SectorDecodeReport.h"
#include "Machines/Apple2/Common/VolumeImage.h"
#include "Seams/IByteStreamFactory.h"
#include "Seams/InProcessByteStream.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkKrunchTests
//
//  The block map a krunch draws over the program's history, on an emulated
//  //c with an Apple Pascal disk in drive 1. The krunch itself is covered by
//  the unit tests in KrunchProgramTests.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (CassoLinkKrunchTests)
{
public:

    static constexpr uint64_t  kFrame       = 17030;
    static constexpr uint64_t  kBudget      = 4'000'000'000;

    //  The block map, by its rows' numbers on the text page.
    static constexpr int       kRowHeading     = 14;
    static constexpr int       kRowMap         = 15;
    static constexpr int       kMapRows        = 7;
    static constexpr int       kRowLegend      = 22;
    static constexpr int       kMapColumns     = 40;
    static constexpr int       kMapCells       = kMapRows * kMapColumns;
    static constexpr int       kHistoryRows    = 8;
    static constexpr size_t    kLegendColumn80 = 15;
    static constexpr Byte      kNormalSpace    = 0xA0;
    static constexpr Byte      kNormalBit      = 0x80;
    static constexpr Byte      kInverseSpace   = 0x20;
    static constexpr Byte      kInverseMask    = 0x3F;
    static constexpr Byte      kFlashFirst     = 0x40;
    static constexpr Byte      kFlashR         = 0x52;
    static constexpr Byte      kFlashW         = 0x57;
    static constexpr size_t    kEntryBytes     = 26;
    static constexpr size_t    kVolumeBlocks   = 14;
    static constexpr size_t    kFileCount      = 16;
    static constexpr size_t    kNameLength     = 6;

    //  The one line to the machine, as the session's TCP connection.
    struct Factory : IByteStreamFactory
    {
        std::shared_ptr<InProcessByteStream>  pcEnd;

        struct Handle : IByteStream
        {
            std::shared_ptr<InProcessByteStream>  end;

            explicit Handle (std::shared_ptr<InProcessByteStream> e) : end (std::move (e)) {}

            HRESULT  Write            (const Byte * data, size_t count) override                                   { return end->Write (data, count); }
            HRESULT  Read             (Byte * buffer, size_t capacity, DWORD timeoutMs, size_t & outRead) override { return end->Read (buffer, capacity, timeoutMs, outRead); }
            size_t   GetReadableCount() override                                                                  { return end->GetReadableCount(); }
            bool     IsClosed() const override                                                                    { return end->IsClosed(); }
            void     Close() override                                                                             { end->Close(); }
        };

        HRESULT  ListComPorts (std::vector<std::wstring> & outPorts) override                                      { outPorts.clear(); return S_OK; }
        HRESULT  OpenComPort  (const std::wstring &, const ComSettings &, std::unique_ptr<IByteStream> &) override { return HRESULT_FROM_WIN32 (ERROR_FILE_NOT_FOUND); }
        HRESULT  ListenTcp    (uint16_t, LinkTransport, std::unique_ptr<IByteStream> &) override                  { return HRESULT_FROM_WIN32 (ERROR_NOT_SUPPORTED); }

        HRESULT  ConnectTcp (const std::string &, uint16_t, std::unique_ptr<IByteStream> & outStream) override
        {
            outStream = std::make_unique<Handle> (pcEnd);
            return S_OK;
        }
    };

    //  The operations done, and the highest progress a krunch reported.
    struct Sink : ICassoLinkSessionSink
    {
        std::mutex                                    lock;
        std::vector<std::shared_ptr<LinkOperation>>   done;
        std::atomic<int>                              progress { 0 };

        void  OnStateChanged (LinkState) override {}

        void  OnProgress (const std::shared_ptr<LinkOperation> & operation) override
        {
            if (operation->kind == LinkOperationKind::Krunch)
            {
                progress = (std::max) (progress.load(), operation->progress.load());
            }
        }

        void  OnDone (const std::shared_ptr<LinkOperation> & operation) override
        {
            std::lock_guard<std::mutex>  guard (lock);

            done.push_back (operation);
        }

        bool  IsDone (const std::shared_ptr<LinkOperation> & operation)
        {
            std::lock_guard<std::mutex>  guard (lock);

            return std::find (done.begin(), done.end(), operation) != done.end();
        }
    };

    //  An emulated //c running the shim at 115,200 baud, with `disk` in drive 1.
    static void  StartMachine (TestMachine & machine, const std::vector<Byte> & disk, std::shared_ptr<InProcessByteStream> & machineEnd)
    {
        std::vector<Byte>  shim;
        Word               at = LinkProgram::kShimAddress;

        machine.SetVideoTiming (std::make_unique<VideoTiming>());
        machine.GetCpu()->SetVideoTiming (machine.GetVideoTiming());
        machine.PowerCycle();

        AssertSucceeded (machine.GetDiskStore().MountFromBytes (6, 0, "", DiskFormat::Dsk, disk));
        Assert::AreEqual (S_OK, machine.AttachSerialStream (2, machineEnd));
        Assert::AreEqual (S_OK, LinkProgram::BuildShim (115200, shim));

        for (Byte b : shim)
        {
            machine.GetCpu()->WriteByte (at++, b);
        }

        machine.GetCpu()->SetPC (LinkProgram::kShimAddress);
    }

    //  Drive 1's disk, as DOS-logical sectors.
    static std::vector<Byte>  ReadDrive1 (TestMachine & machine)
    {
        std::vector<Byte>   file;
        std::vector<Byte>   sectors;
        SectorDecodeReport  report;

        AssertSucceeded (machine.GetDiskStore().GetImage (6, 0)->Serialize (file));
        AssertSucceeded (VolumeImage::Load (file, "d.dsk", sectors, report));
        return sectors;
    }

    //  A session on the machine, as Casso Explorer runs one.
    struct Rig
    {
        TestMachine                           machine { "Apple2c" };
        std::shared_ptr<InProcessByteStream>  machineEnd;
        Factory                               factory;
        InMemoryCassoLinkFolder               folder;
        FakeDiskFileIo                        fileIo;
        Sink                                  sink;
        std::unique_ptr<CassoLinkSession>     session;

        explicit Rig (const std::vector<Byte> & disk, bool use40 = true)
        {
            CassoLinkSettings  settings;

            InProcessByteStream::CreatePair (machineEnd, factory.pcEnd);
            StartMachine (machine, disk, machineEnd);

            session = std::make_unique<CassoLinkSession> (factory, folder, fileIo, sink);

            settings.name      = "//c";
            settings.transport = LinkTransport::Emulated;
            settings.address   = "127.0.0.1:6502";
            settings.use40     = use40;

            AssertSucceeded (session->Start (settings));
        }

        ~Rig()
        {
            session.reset();
        }

        //  A row of the text page as text, with its trailing spaces cut.
        std::string  Row (Word address)
        {
            std::string  text;

            for (int i = 0; i < 40; i++)
            {
                text += (char) (machine.GetCpu()->ReadByte ((Word) (address + i)) & 0x7F);
            }

            return text.substr (0, text.find_last_not_of (' ') + 1);
        }

        //  Steps the machine until `until` is true, and calls `eachFrame`
        //  after each frame. The budget runs out only on a failure.
        void  Step (const std::function<bool()> & until, const std::function<void()> & eachFrame = nullptr)
        {
            uint64_t  spent = 0;

            while (!until() && spent < kBudget)
            {
                machine.RunCycles (kFrame);
                spent += kFrame;

                if (eachFrame)
                {
                    eachFrame();
                }
            }

            Assert::IsTrue (until(), L"the condition waited for is met");
        }

        std::shared_ptr<LinkOperation>  Finish (std::shared_ptr<LinkOperation> operation, const std::function<void()> & eachFrame = nullptr)
        {
            Step ([&] () { return sink.IsDone (operation); }, eachFrame);
            return operation;
        }

        std::vector<Byte>  Sectors() { return ReadDrive1 (machine); }
    };

    //  A file's bytes: `blocks` blocks less 100 bytes, each a function of
    //  the file and its offset, so no two files and no two blocks match.
    static FilePayload  MakeFile (int file, int blocks)
    {
        FilePayload  payload;

        payload.bytes.resize ((size_t) blocks * PascalVolume::kBlockBytes - 100);

        for (size_t j = 0; j < payload.bytes.size(); j++)
        {
            payload.bytes[j] = (Byte) (file * 7 + j * 3 + (j >> 9));
        }

        return payload;
    }

    static std::string  Name (int file)
    {
        return std::format ("F{:02}.DATA", file);
    }

    //  A volume of the files of `sizes`, in blocks, written in order, then
    //  those in `deleted` removed, so the free space is in pieces.
    static std::vector<Byte>  BuildDisk (std::span<const int> sizes, std::span<const int> deleted)
    {
        std::vector<Byte>  image (NibblizationLayer::kImageByteSize, 0);
        std::vector<Byte>  next;

        PascalVolume::WriteBlank ("KRUNCH", 0, image);

        for (size_t i = 0; i < sizes.size(); i++)
        {
            AssertSucceeded (PascalVolume (image).Write (FilePath::Parse (Name ((int) i)), MakeFile ((int) i, sizes[i]), next));
            image = next;
        }

        for (int i : deleted)
        {
            AssertSucceeded (PascalVolume (image).Delete (FilePath::Parse (Name (i)), next));
            image = next;
        }

        return image;
    }

    //  The free space in pieces, between files that move over part of the
    //  blocks they were in, and one of 20 blocks that crosses tracks at both
    //  places.
    static std::vector<Byte>  BuildSmallDisk()
    {
        static constexpr int  kSizes[]   = { 3, 1, 4, 2, 20, 1, 3, 5, 2, 6 };
        static constexpr int  kDeleted[] = { 1, 3, 5, 8 };

        return BuildDisk (kSizes, kDeleted);
    }

    //  Whether every file starts where the file ahead of it ends.
    static bool  IsKrunched (const std::vector<Byte> & sectors)
    {
        std::vector<PascalKrunchMove>  moves;
        std::vector<Byte>              directory;

        AssertSucceeded (PascalVolume (sectors).PlanKrunch (moves, directory));
        return moves.empty();
    }

    //  Row `row` of the text page as its screen codes: 40, or in 80 columns
    //  80, each auxiliary memory byte ahead of the main memory byte at the
    //  same offset, as the video reads them.
    static std::vector<Byte>  ReadRawRow (TestMachine & machine, int row, bool wide)
    {
        const Byte         * mainRam = machine.GetRefs().mainRamDev->GetData();
        const Byte         * auxRam  = machine.GetMmu()->GetAuxBuffer();
        Word                 base    = TextScreenScraper::GetRowBaseAddress (TextScreenScraper::kTextPage1, row);
        std::vector<Byte>    bytes;

        for (int i = 0; i < kMapColumns; i++)
        {
            if (wide)
            {
                bytes.push_back (auxRam[base + i]);
            }

            bytes.push_back (mainRam[base + i]);
        }

        return bytes;
    }

    //  Row `row` as text, with its trailing spaces cut.
    static std::string  ReadTextRow (TestMachine & machine, int row, bool wide)
    {
        std::string  text;

        for (Byte code : ReadRawRow (machine, row, wide))
        {
            text += TextScreenScraper::Glyph (code);
        }

        return text.substr (0, text.find_last_not_of (' ') + 1);
    }

    //  The history's rows as text.
    static std::vector<std::string>  ReadHistory (TestMachine & machine, bool wide)
    {
        std::vector<std::string>  rows;

        for (int i = 0; i < kHistoryRows; i++)
        {
            rows.push_back (ReadTextRow (machine, kRowMap + i, wide));
        }

        return rows;
    }

    //  The block map's screen codes, a cell at a time from block 0: one a
    //  cell, or in 80 columns two, the left column's first.
    static std::vector<Byte>  ReadMap (TestMachine & machine, bool wide)
    {
        std::vector<Byte>  map;

        for (int row = 0; row < kMapRows; row++)
        {
            std::vector<Byte>  raw = ReadRawRow (machine, kRowMap + row, wide);

            map.insert (map.end(), raw.begin(), raw.end());
        }

        return map;
    }

    //  Block `block`'s cell in a map ReadMap read: its screen code, or in 80
    //  columns its left column's.
    static Byte  GetCell (const std::vector<Byte> & map, int block, bool wide)
    {
        return map[(size_t) block * (wide ? 2 : 1)];
    }

    //  Whether every cell is one the map draws: in 80 columns the same code
    //  twice, or a name's character with a space of its kind after it. A
    //  cell's columns are written one after the other, so one cell can be
    //  read between them.
    static bool  AreCellsWhole (const std::vector<Byte> & map, bool wide)
    {
        int  broken = 0;

        for (size_t i = 0; wide && i < map.size(); i += 2)
        {
            Byte  left  = map[i];
            Byte  right = map[i + 1];
            bool  whole = right == left || (left < kFlashFirst && right == kInverseSpace) || (left >= kNormalBit && right == kNormalSpace);

            broken += whole ? 0 : 1;
        }

        return broken <= 1;
    }

    //  A name's character as the map has it: in capitals, and ? for a
    //  control character.
    static char  FoldNameChar (Byte c)
    {
        constexpr Byte  kLowerFirst = 0x60;
        constexpr Byte  kUpperMask  = 0x5F;

        c = (Byte) (c & 0x7F);
        c = c >= kLowerFirst ? (Byte) (c & kUpperMask) : c;
        return c < ' ' ? '?' : (char) c;
    }

    //  The cells a block of a file has: its name's character `c`, or a space
    //  past the name, done in inverse or to move in normal characters with #
    //  for a space; in 80 columns the right column too.
    static void  PutFileCell (std::vector<Byte> & map, int block, char c, bool moving, bool wide)
    {
        size_t  width = wide ? 2 : 1;
        Byte    left  = moving ? (Byte) (c == ' ' ? '#' | kNormalBit : c | kNormalBit) : (Byte) (c & kInverseMask);
        Byte    right = c == ' ' ? left : moving ? kNormalSpace : kInverseSpace;

        map[block * width] = left;

        if (wide)
        {
            map[block * width + 1] = right;
        }
    }

    //  The map a krunch of `disk` starts with, from its directory, as ReadMap
    //  reads it: the boot blocks, the directory and each file that stays
    //  done, each file that moves to move, each file's name across its
    //  blocks, the rest free, and blank past the volume's blocks.
    static std::vector<Byte>  MakeFirstMap (const std::vector<Byte> & disk, bool wide)
    {
        std::vector<PascalKrunchMove>  moves;
        std::vector<Byte>              directory;
        std::vector<Byte>              map;
        size_t                         width  = wide ? 2 : 1;
        int                            volume = 0;
        int                            files  = 0;

        AssertSucceeded (PascalVolume (disk).PlanKrunch (moves, directory));

        map.assign (kMapCells * width, (Byte) ('.' | kNormalBit));

        volume = directory[kVolumeBlocks] | directory[kVolumeBlocks + 1] << 8;
        files  = directory[kFileCount]    | directory[kFileCount + 1] << 8;

        std::fill (map.begin() + (ptrdiff_t) ((size_t) (std::min) (volume, kMapCells) * width), map.end(), kNormalSpace);
        std::fill (map.begin(), map.begin() + (ptrdiff_t) (PascalVolume::kFirstDataBlock * width), kInverseSpace);

        for (size_t slot = 1; slot <= (size_t) files; slot++)
        {
            size_t  entry  = slot * kEntryBytes;
            int     first  = directory[entry]     | directory[entry + 1] << 8;
            int     next   = directory[entry + 2] | directory[entry + 3] << 8;
            size_t  length = directory[entry + kNameLength];
            bool    moving =std::any_of (moves.begin(), moves.end(), [&] (const PascalKrunchMove & m) { return m.from == first; });

            for (int block = first; block < next && block < kMapCells; block++)
            {
                size_t  at = (size_t) (block - first);

                PutFileCell (map, block, at < length ? FoldNameChar (directory[entry + kNameLength + 1 + at]) : ' ', moving, wide);
            }
        }

        return map;
    }

    //  File `slot`'s name in the directory PlanKrunch returns.
    static std::string  GetEntryName (const std::vector<Byte> & directory, int slot)
    {
        size_t  entry = (size_t) slot * kEntryBytes;

        return std::string (directory.begin() + (ptrdiff_t) (entry + kNameLength + 1),
                            directory.begin() + (ptrdiff_t) (entry + kNameLength + 1 + directory[entry + kNameLength]));
    }

    //  The legend's row as the program draws it: each cell as the map draws
    //  it, then its text, and in 80 columns the cells doubled, centered.
    static std::vector<Byte>  MakeLegend (bool wide)
    {
        std::vector<Byte>  bytes (wide ? 2 * kMapColumns : kMapColumns, kNormalSpace);
        size_t             at    = wide ? kLegendColumn80 : 0;
        std::string_view   gap   = wide ? "   " : "  ";

        auto  put  = [&] (std::string_view text) { for (char c : text) { bytes[at++] = (Byte) (c | kNormalBit); } };
        auto  cell = [&] (Byte code)             { bytes[at++] = code; if (wide) { bytes[at++] = code; } };

        cell (kInverseSpace);
        put  (" DONE");
        put  (gap);
        cell ((Byte) ('#' | kNormalBit));
        put  (" TO GO");
        put  (gap);
        cell ((Byte) ('.' | kNormalBit));
        put  (" FREE");
        put  (gap);
        cell (kFlashR);
        put  (" READ");
        put  (gap);
        cell (kFlashW);
        put  (" WRITE");
        return bytes;
    }

    //  The Apple's screen during a krunch: the history just before the map
    //  was drawn, the map in each frame it was drawn, with its heading, and
    //  the history once the krunch's line is in it.
    struct MapRecord
    {
        std::vector<std::string>        before;
        std::vector<std::vector<Byte>>  maps;
        std::vector<std::string>        after;
        std::vector<Byte>               disk;
        bool                            wide = false;
    };

    static void  KrunchWithMap (const std::vector<Byte> & disk, bool wide, MapRecord & record)
    {
        constexpr int                   kSteadyFrames = 30;
        Rig                             rig (disk, !wide);
        std::vector<Byte>               legend        = MakeLegend (wide);
        std::shared_ptr<LinkOperation>  krunch;
        int                             steady        = 0;

        record.disk = disk;
        record.wide = wide;

        AssertSucceeded (rig.Finish (rig.session->Enumerate (1, FilePath()))->result);
        Assert::AreEqual (wide, rig.machine.GetRefs().iieSoftSwitches->Is80ColMode(), L"the Apple is in the settings' columns");

        krunch = rig.Finish (rig.session->Krunch (1), [&]
        {
            std::string  heading = ReadTextRow (rig.machine, kRowHeading, wide);

            if (record.maps.empty() && heading == "HISTORY:")
            {
                record.before = ReadHistory (rig.machine, wide);
            }

            //  A map is added only when whole: its legend is drawn last, and
            //  its heading is drawn over first when the history is drawn again.
            if (heading == "BLOCKS:" && ReadRawRow (rig.machine, kRowLegend, wide) == legend)
            {
                record.maps.push_back (ReadMap (rig.machine, wide));
            }
        });

        AssertSucceeded (krunch->result);
        Assert::IsTrue (IsKrunched (rig.Sectors()));

        //  The history once its krunch line is in it and it is the same for
        //  a while, so no line is read partway through its scroll.
        rig.Step ([&] ()
        {
            std::vector<std::string>  rows    = ReadHistory (rig.machine, wide);
            bool                      logged  = std::any_of (rows.begin(), rows.end(), [] (const std::string & row) { return row.find ("KRUNCHED") != std::string::npos; });

            steady        = logged && rows == record.after ? steady + 1 : 0;
            record.after  = rows;
            return steady >= kSteadyFrames;
        });

        Assert::AreEqual (std::string ("HISTORY:"), ReadTextRow (rig.machine, kRowHeading, wide), L"the history's heading is back");
    }

    //  A map's rows in the test's log, as text.
    static void  LogMap (const char * label, const std::vector<Byte> & map, bool wide)
    {
        size_t  width = (size_t) kMapColumns * (wide ? 2 : 1);

        for (size_t row = 0; row < (size_t) kMapRows; row++)
        {
            std::string  text;

            for (size_t i = 0; i < width; i++)
            {
                text += TextScreenScraper::Glyph (map[row * width + i]);
            }

            Logger::WriteMessage (std::format ("{} |{}|", label, text).c_str());
        }
    }

    //  The checks of a MapRecord, the same in 40 and 80 columns.
    static void  CheckMapRecord (const MapRecord & record)
    {
        std::vector<PascalKrunchMove>  moves;
        std::vector<Byte>              directory;
        std::vector<Byte>              first;
        std::vector<Byte>              expected;
        std::string                    name;
        bool                           wide   = record.wide;
        size_t                         newest = 0;
        bool                           read   = false;
        bool                           wrote  = false;
        bool                           done   = false;
        bool                           freed  = false;
        int                            cover  = 0;
        int                            filler = -1;

        AssertSucceeded (PascalVolume (record.disk).PlanKrunch (moves, directory));
        Assert::IsFalse (moves.empty());
        Assert::IsFalse (record.maps.empty(), L"the map was drawn, with its heading and legend");

        //  The first move overlaps its old blocks, so its first old block is
        //  in its new place.
        cover = moves[0].from;
        name  = GetEntryName (directory, moves[0].slot);

        Assert::IsTrue (moves[0].to < cover && cover < moves[0].to + moves[0].blocks);
        Assert::IsFalse (name.empty());

        //  An old block past a moved file's name and its new place, drawn as #
        //  and then as free.
        for (const PascalKrunchMove & move : moves)
        {
            int     last   = move.from + move.blocks - 1;
            size_t  length = GetEntryName (directory, move.slot).size();

            filler = filler < 0 && last >= move.to + move.blocks && (size_t) (move.blocks - 1) >= length ? last : filler;
        }

        Assert::IsTrue (filler >= 0);

        first    = record.maps.front();
        expected = MakeFirstMap (record.disk, wide);

        LogMap ("map", first, wide);

        if (first != expected)
        {
            LogMap ("expected", expected, wide);
        }

        Assert::IsTrue (first == expected, L"the map before the first move");
        Assert::AreEqual<int> (FoldNameChar ((Byte) name[0]) | kNormalBit, GetCell (first, cover, wide), L"the name's first character, to move");
        Assert::AreEqual<int> ('#' | kNormalBit, GetCell (first, filler, wide));

        for (const std::vector<Byte> & map : record.maps)
        {
            read  = read  || std::find (map.begin(), map.end(), kFlashR) != map.end();
            wrote = wrote || std::find (map.begin(), map.end(), kFlashW) != map.end();
            done  = done  || GetCell (map, cover, wide) < kFlashFirst;
            freed = freed || GetCell (map, filler, wide) == ('.' | kNormalBit);

            Assert::IsTrue (AreCellsWhole (map, wide), L"every cell is drawn as one of the map's");
        }

        Assert::IsTrue (read,  L"a part's blocks are drawn as read");
        Assert::IsTrue (wrote, L"and as written");
        Assert::IsTrue (done,  L"a block to move is drawn as done once the part over it is written");
        Assert::IsTrue (freed, L"an old block past the new place is drawn as free once its entry is written");

        //  The moved file's name is at its new place, in inverse, once its
        //  parts are written.
        for (size_t i = 0; i < name.size() && i < (size_t) moves[0].blocks; i++)
        {
            Assert::AreEqual<int> (FoldNameChar ((Byte) name[i]) & kInverseMask, GetCell (record.maps.back(), moves[0].to + (int) i, wide), L"the name at the new place");
        }

        //  The history is drawn again as it was, below the lines the host
        //  logged after the krunch.
        Assert::IsTrue (std::any_of (record.before.begin(), record.before.end(), [] (const std::string & row) { return !row.empty(); }), L"the history had lines");

        while (record.after[newest].find ("KRUNCHED") == std::string::npos)
        {
            newest++;
        }

        for (size_t i = newest + 1; i < record.after.size(); i++)
        {
            Assert::AreEqual (record.before[i - newest - 1], record.after[i], L"the history's line is as it was");
        }
    }



    //  During a krunch the history's rows have a map of the disk's blocks,
    //  with a legend, and the history is drawn again when it ends.
    TEST_METHOD (Krunch_DrawsABlockMapOverTheHistory)
    {
        MapRecord  record;



        KrunchWithMap (BuildSmallDisk(), false, record);
        CheckMapRecord (record);
    }



    //  In 80 columns the map is the same, each cell two columns wide.
    TEST_METHOD (Krunch_DrawsTheSameBlockMapIn80Columns)
    {
        MapRecord  record;



        KrunchWithMap (BuildSmallDisk(), true, record);
        CheckMapRecord (record);
    }
};
