#include "Pch.h"

#include "EhmTestHelper.h"
#include "GuestSession.h"
#include "TestMachine.h"
#include "CassoLink/CassoLinkCodec.h"
#include "CassoLink/CassoLinkHost.h"
#include "Core/TextEncoding.h"
#include "Devices/Disk/DiskImage.h"
#include "Devices/Disk/Inspector/DiskAnalyzer.h"
#include "Devices/Disk/Inspector/FindingFormatter.h"
#include "Machines/Apple2/Common/WozLoader.h"
#include "CassoLink/InMemoryCassoLinkFolder.h"
#include "CassoLink/LinkProgram.h"
#include "CassoLink/ProgramRig.h"
#include "CassoLink/Session/CassoLinkCapture.h"
#include "Machines/Apple2/Common/Disk2Controller.h"
#include "Machines/Apple2/Common/SectorDecodeReport.h"
#include "Machines/Apple2/Common/VolumeImage.h"
#include "Seams/ByteStreamSettings.h"
#include "Seams/Win32ByteStreamFactory.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkHardwareTests
//
//  A real Apple on a serial port, with the link program running and no
//  other program on the port: run only when CASSO_LINK_PORT is set, as
//  COM3, and pass with nothing done otherwise. CASSO_LINK_DRIVE is the drive,
//  2 when unset, CASSO_LINK_LAST the last track, 34 when unset, and
//  CASSO_LINK_OUT the folder the files go to, with every packet in trace.log. The host
//  loads this build's program when the Apple runs another version.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (CassoLinkHardwareTests)
{
public:

    static std::string  GetSetting (const char * name, const char * fallback)
    {
        char         * value  = nullptr;
        size_t         length = 0;
        std::string    text   = fallback;



        if (_dupenv_s (&value, &length, name) == 0 && value != nullptr)
        {
            text = value;
            free (value);
        }

        return text;
    }

    static void  WriteFile (const std::filesystem::path & path, std::span<const Byte> bytes)
    {
        std::ofstream  file (path, std::ios::binary);



        file.write ((const char *) bytes.data(), (std::streamsize) bytes.size());
    }

    //  The port opened, and an Idle sent on it: the program sends Ready only
    //  after a packet from the PC arrives.
    static void  OpenPort (Win32ByteStreamFactory & factory, const std::string & port, const ComSettings & settings, std::unique_ptr<IByteStream> & outStream)
    {
        std::vector<Byte>  idle;



        AssertSucceeded (factory.OpenComPort (TextEncoding::NarrowToWide (port), settings, outStream));
        AssertSucceeded (CassoLinkCodec::Encode (CassoLinkCodec::kIdle, {}, idle));
        AssertSucceeded (outStream->Write (idle.data(), idle.size()));
    }

    //  The disk captured into capture.woz, its reads into capture.reads, and
    //  each read's progress and the totals into capture.log as they come.
    TEST_METHOD (RealApple_CapturesTheDisk)
    {
        std::string                    port    = GetSetting ("CASSO_LINK_PORT", "");
        int                            drive   = std::stoi (GetSetting ("CASSO_LINK_DRIVE", "2"));
        std::filesystem::path          folder  = GetSetting ("CASSO_LINK_OUT", ".");
        Win32ByteStreamFactory         factory;
        ComSettings                    settings;
        std::unique_ptr<IByteStream>   opened;
        InMemoryCassoLinkFolder        files;
        std::vector<Byte>              module;
        std::vector<Byte>              reads;
        CassoLinkCapture::Result       result;
        std::ofstream                  log;



        if (port.empty())
        {
            Logger::WriteMessage ("CASSO_LINK_PORT isn't set; no Apple to capture from.\n");
            return;
        }

        log.open (folder / "capture.log");

        OpenPort (factory, port, settings, opened);

        CassoLinkHost  host (std::shared_ptr<IByteStream> (std::move (opened)), files, [] () { return std::array<Byte, 4> {}; });

        std::ofstream  trace (folder / "trace.log");
        int            last  = std::stoi (GetSetting ("CASSO_LINK_LAST", "34"));

        ULONGLONG      began = GetTickCount64();

        host.SetTraceSink ([&trace, began] (bool sent, const std::string & text)
        {
            trace << std::format ("{:8} ", GetTickCount64() - began) << (sent ? "> " : "< ") << text << "\n";
            trace.flush();
        });

        AssertSucceeded (host.BuildCaptureModule (module));
        AssertSucceeded (CassoLinkCapture::Capture (host, (Byte) drive, module, "2026-11-11T00:00:00Z", [&log] (const CassoLinkCapture::Progress & where)
        {
            log << std::format ("pass {} position {} read {} of {}: {}%, {:.0f} s left\n", where.pass, where.position, where.read, where.reads, where.percent, where.secondsLeft);
            log.flush();
        }, result, last));

        CassoLinkCapture::SerializeReads (result.positions, reads);
        WriteFile (folder / "capture.woz",   result.woz);
        WriteFile (folder / "capture.reads", reads);

        log << std::format ("{} reads, disk {:.1f} s, line {:.1f} s, {} bytes sent for {} stored, {} of {} between read\n",
                            result.reads, result.diskMs / 1000.0, result.lineMs / 1000.0, result.sentBytes, result.storedBytes,
                            result.betweenRead, result.betweenTotal);
        log << std::format ("{} streams, {} weak, {} unstable\n", result.streams.size(), result.weak.size(), result.unstable.size());
        log << std::format ("revolution {:.0f} cycles\n", result.revolutionCycles);
    }

    //  The positions of CASSO_READ_POSITIONS, as 120,124, each read
    //  CassoLinkCapture::kWholeReads times without waiting on a marker, into
    //  positions.reads and positions.woz in CASSO_LINK_OUT: run only when
    //  both CASSO_LINK_PORT and CASSO_READ_POSITIONS are set.
    TEST_METHOD (RealApple_ReadsThePositions)
    {
        std::string                              port   = GetSetting ("CASSO_LINK_PORT", "");
        std::string                              list   = GetSetting ("CASSO_READ_POSITIONS", "");
        int                                      drive  = std::stoi (GetSetting ("CASSO_LINK_DRIVE", "1"));
        std::filesystem::path                    folder = GetSetting ("CASSO_LINK_OUT", ".");
        Win32ByteStreamFactory                   factory;
        ComSettings                              settings;
        std::unique_ptr<IByteStream>             opened;
        InMemoryCassoLinkFolder                  files;
        std::vector<Byte>                        module;
        std::vector<CassoLinkCapture::Position>  positions;
        std::vector<Byte>                        reads;
        CassoLinkCapture::Result                 result;
        int                                      last   = 0;



        if (port.empty() || list.empty())
        {
            Logger::WriteMessage ("CASSO_LINK_PORT or CASSO_READ_POSITIONS isn't set; no positions to read.\n");
            return;
        }

        OpenPort (factory, port, settings, opened);

        CassoLinkHost  host (std::shared_ptr<IByteStream> (std::move (opened)), files, [] () { return std::array<Byte, 4> {}; });

        AssertSucceeded (host.BuildCaptureModule (module));

        for (size_t at = 0; at < list.size(); )
        {
            size_t                      comma = std::min (list.find (',', at), list.size());
            CassoLinkCapture::Position  position;

            position.position = std::stoi (list.substr (at, comma - at));
            last              = std::max (last, position.position);
            at                = comma + 1;

            for (Byte delay : CassoLinkCapture::kWholeDelays)
            {
                std::vector<Byte>  bytes;
                CassoLinkStatus    status;
                CaptureTiming      timing;

                AssertSucceeded (host.EnsureModule (module));
                AssertSucceeded (host.CaptureQuarterTrack ((Byte) drive, (Byte) position.position, (Byte) (delay | (positions.empty() && position.reads.empty() ? CassoLinkHost::kRecalibrate : 0)), 0, 0, CassoLinkHost::kNoSync, {}, bytes, status, timing));

                position.reads.push_back (bytes);
                position.delays.push_back (delay);
            }

            positions.push_back (std::move (position));
        }

        CassoLinkCapture::SerializeReads (positions, reads);
        WriteFile (folder / "positions.reads", reads);

        AssertSucceeded (CassoLinkCapture::BuildWoz (positions, last, "2026-11-12T00:00:00Z", (Byte) drive, result));
        WriteFile (folder / "positions.woz", result.woz);
    }

    //  A real capture's reads, from CASSO_CAPTURE_READS, built into a WOZ
    //  again as rebuilt.woz in CASSO_LINK_OUT, with each whole track's
    //  sectors decoded: run only when CASSO_CAPTURE_READS is set.
    TEST_METHOD (RealReads_BuildAWoz)
    {
        std::string                              path   = GetSetting ("CASSO_CAPTURE_READS", "");
        std::filesystem::path                    folder = GetSetting ("CASSO_LINK_OUT", ".");
        std::vector<Byte>                        file;
        std::vector<CassoLinkCapture::Position>  positions;
        CassoLinkCapture::Result                 result;
        std::vector<Byte>                        sectors;
        SectorDecodeReport                       report;
        std::string                              log;
        int                                      last     = 0;
        int                                      standard = 0;



        if (path.empty())
        {
            Logger::WriteMessage ("CASSO_CAPTURE_READS isn't set; no reads to build.\n");
            return;
        }

        {
            std::ifstream  in (path, std::ios::binary);

            file.assign (std::istreambuf_iterator<char> (in), std::istreambuf_iterator<char>());
        }

        AssertSucceeded (CassoLinkCapture::ParseReads (file, positions));

        for (const CassoLinkCapture::Position & position : positions)
        {
            if (position.position % 4 == 0)
            {
                last      = std::max (last, position.position);
                standard += CassoLinkCapture::IsStandardTrack (position) ? 1 : 0;
            }
        }

        AssertSucceeded (CassoLinkCapture::BuildWoz (positions, last, "2026-11-11T00:00:00Z", 2, result));
        WriteFile (folder / "rebuilt.woz", result.woz);

        AssertSucceeded (VolumeImage::Load (result.woz, "rebuilt.woz", sectors, report));

        log = std::format ("{} standard whole tracks of {}; {} streams, {} weak, {} unstable; {} sectors unrecovered\nweak:",
                           standard, last / 4 + 1, result.streams.size(), result.weak.size(), result.unstable.size(), report.GetUnrecoveredCount());

        for (int position : result.weak)
        {
            log += std::format (" {}", position);
        }

        log += "\nunstable:";

        for (int position : result.unstable)
        {
            log += std::format (" {}", position);
        }

        Logger::WriteMessage ((log + "\n").c_str());

        LogFindings (result.woz);
        LogBoot ("the WOZ", result.woz, sectors, true);
        LogBoot ("its sectors alone", result.woz, sectors, false);
    }

    //  An emulated //c booted from the image, or from its sectors as a plain
    //  image, with the text screen and a hash of each hi-res page logged
    //  every s_kBootStep cycles, so a protected disk's two boots can be
    //  compared.
    static void  LogBoot (const char * what, const std::vector<Byte> & woz, const std::vector<Byte> & sectors, bool fromWoz)
    {
        TestMachine  machine ("Apple2c");
        std::string  log    = std::format ("Booting {}:\n", what);



        if (fromWoz)
        {
            GuestSession::MountWoz (machine, woz, sectors);
        }
        else
        {
            GuestSession::Mount (machine, sectors);
        }

        for (int step = 1; step <= s_kBootSteps; step++)
        {
            std::vector<std::string>  rows;
            uint32_t                  hash[2] = { 2166136261u, 2166136261u };

            std::set<int>  heads;

            for (int slice = 0; slice < s_kBootSlices; slice++)
            {
                machine.RunCycles (s_kBootStep / s_kBootSlices);

                if (machine.GetRefs().diskController != nullptr)
                {
                    heads.insert (machine.GetRefs().diskController->GetQuarterTrack());
                }
            }

            log += "  head at quarter tracks:";

            for (int head : heads)
            {
                log += std::format (" {}", head);
            }

            log += "\n";
            GuestSession::CollectRows (machine, rows);

            for (int page = 0; page < 2; page++)
            {
                for (Word at = 0; at < 0x2000; at++)
                {
                    hash[page] = (hash[page] ^ machine.GetMemoryBus().ReadByte ((Word) (0x2000 * (page + 1) + at))) * 16777619u;
                }
            }

            log += std::format ("  after {} M cycles, PC ${:04X}, hi-res {:08X} {:08X}\n", step * s_kBootStep / 1000000, machine.GetCpu()->GetPC(), hash[0], hash[1]);

            for (const std::string & row : rows)
            {
                if (row.find_first_not_of (' ') != std::string::npos)
                {
                    log += "    |" + row + "|\n";
                }
            }
        }

        Logger::WriteMessage (log.c_str());

        {
            std::vector<Byte>  pages;

            for (Word at = 0x2000; at < 0x6000; at++)
            {
                pages.push_back (machine.GetMemoryBus().ReadByte (at));
            }

            WriteFile (std::filesystem::path (GetSetting ("CASSO_LINK_OUT", ".")) / (fromWoz ? "boot_woz.bin" : "boot_sectors.bin"), pages);
        }
    }

    //  A WOZ from CASSO_CAPTURE_WOZ mounted in an emulated //c and captured
    //  over the link, as a real Apple's disk is, into emulated.woz in
    //  CASSO_LINK_OUT, then booted: run only when CASSO_CAPTURE_WOZ is set.
    TEST_METHOD (AnEmulatedWoz_IsCapturedAndBoots)
    {
        std::string                path   = GetSetting ("CASSO_CAPTURE_WOZ", "");
        std::filesystem::path      folder = GetSetting ("CASSO_LINK_OUT", ".");
        std::vector<Byte>          source;
        std::vector<Byte>          module;
        std::vector<Byte>          sectors;
        SectorDecodeReport         report;
        CassoLinkCapture::Result   result;



        if (path.empty())
        {
            Logger::WriteMessage ("CASSO_CAPTURE_WOZ isn't set; no image to capture.\n");
            return;
        }

        {
            std::ifstream  in (path, std::ios::binary);

            source.assign (std::istreambuf_iterator<char> (in), std::istreambuf_iterator<char>());
        }

        ProgramRig  rig (std::vector<Byte> (143360, 0));

        AssertSucceeded (rig.machine.GetDiskStore().MountFromBytes (ProgramRig::kSlot, 0, "", DiskFormat::Woz, source));
        AssertSucceeded (LinkProgram::BuildCaptureModule (module));
        AssertSucceeded (CassoLinkCapture::Capture (*rig.host, 1, module, "2026-11-11T00:00:00Z", {}, result));
        WriteFile (folder / "emulated.woz", result.woz);

        Logger::WriteMessage (std::format ("{} reads, synchronized {}\n", result.reads, result.synchronized).c_str());

        AssertSucceeded (VolumeImage::Load (result.woz, "emulated.woz", sectors, report));
        LogBoot ("the emulated capture", result.woz, sectors, true);
    }

    //  The time from a marker to a read that waited on it, measured without a
    //  step: each track of CASSO_SYNC_TRACKS is read, its sector 0 address
    //  field is the marker, and the track is read again waiting on it there.
    //  The share of a revolution from the marker to the read is computed from
    //  the marker's place in that read, a revolution after it passed. On the real
    //  Apple when CASSO_LINK_PORT is set, and on an emulated //c with the
    //  WOZ from CASSO_CAPTURE_WOZ otherwise; run only when CASSO_SYNC_TRACKS
    //  is set.
    TEST_METHOD (SyncDelay_IsMeasuredOnEachTrack)
    {
        std::string                    tracks = GetSetting ("CASSO_SYNC_TRACKS", "");
        std::string                    port   = GetSetting ("CASSO_LINK_PORT", "");
        int                            drive  = std::stoi (GetSetting ("CASSO_LINK_DRIVE", "1"));
        Win32ByteStreamFactory         factory;
        ComSettings                    settings;
        std::unique_ptr<IByteStream>   opened;
        InMemoryCassoLinkFolder        files;
        std::ofstream                  trace;
        std::unique_ptr<CassoLinkHost> real;
        std::unique_ptr<ProgramRig>    rig;
        CassoLinkHost                * host   = nullptr;
        std::vector<Byte>              module;
        std::string                    log;



        if (tracks.empty())
        {
            Logger::WriteMessage ("CASSO_SYNC_TRACKS isn't set; no tracks to measure.\n");
            return;
        }

        if (!port.empty())
        {
            OpenPort (factory, port, settings, opened);
            real = std::make_unique<CassoLinkHost> (std::shared_ptr<IByteStream> (std::move (opened)), files, [] () { return std::array<Byte, 4> {}; });
            host = real.get();

            trace.open (std::filesystem::path (GetSetting ("CASSO_LINK_OUT", ".")) / "trace.log");
            host->SetTraceSink ([&trace] (bool sent, const std::string & text)
            {
                trace << (sent ? "> " : "< ") << text << "\n";
                trace.flush();
            });
        }
        else
        {
            std::vector<Byte>  source;
            std::ifstream      in (GetSetting ("CASSO_CAPTURE_WOZ", ""), std::ios::binary);

            source.assign (std::istreambuf_iterator<char> (in), std::istreambuf_iterator<char>());
            rig = std::make_unique<ProgramRig> (std::vector<Byte> (143360, 0));
            AssertSucceeded (rig->machine.GetDiskStore().MountFromBytes (ProgramRig::kSlot, 0, "", DiskFormat::Woz, source));
            host  = rig->host.get();
            drive = 1;
        }

        AssertSucceeded (host->BuildCaptureModule (module));

        for (size_t at = 0; at < tracks.size(); )
        {
            size_t             comma    = std::min (tracks.find (',', at), tracks.size());
            int                track    = std::stoi (tracks.substr (at, comma - at));
            Byte               position = (Byte) (track * 4);
            std::vector<Byte>  first;
            std::vector<Byte>  marker;
            size_t             found    = SIZE_MAX;
            size_t             again    = SIZE_MAX;
            CassoLinkStatus    status;
            CaptureTiming      timing;

            at = comma + 1;

            AssertSucceeded (host->EnsureModule (module));
            AssertSucceeded (host->CaptureQuarterTrack ((Byte) drive, position, 0, 0, 0, CassoLinkHost::kNoSync, {}, first, status, timing));

            found = FindSectorZero (first, 0);
            again = found == SIZE_MAX ? SIZE_MAX : FindSectorZero (first, found + 1);

            if (again == SIZE_MAX)
            {
                log += std::format ("track {}: sector 0's address field doesn't repeat\n", track);
                continue;
            }

            //  A byte the read marked has its top bit clear; the latch has it set.
            std::transform (first.begin() + (std::ptrdiff_t) found, first.begin() + (std::ptrdiff_t) found + s_kMarkerBytes, std::back_inserter (marker), [] (Byte value) { return (Byte) (value | 0x80); });
            log += std::format ("track {}: a revolution is {} bytes\n", track, again - found);

            for (int read = 0; read < s_kSyncReads; read++)
            {
                std::vector<Byte>  synced;
                size_t             passed = SIZE_MAX;

                AssertSucceeded (host->EnsureModule (module));
                AssertSucceeded (host->CaptureQuarterTrack ((Byte) drive, position, 0, 0, 0, position, marker, synced, status, timing));

                passed = status.error == 0 ? FindSectorZero (synced, 0) : SIZE_MAX;

                if (passed == SIZE_MAX)
                {
                    log += std::format ("  read {}: error {:02X}, no marker\n", read, status.error);
                    continue;
                }

                log += std::format ("  read {}: the marker ends {} bytes before the read starts, {:.4f} of a revolution\n", read,
                                    again - found - passed - s_kMarkerBytes,
                                    (double) (again - found - passed - s_kMarkerBytes) / (double) (again - found));
            }
        }

        Logger::WriteMessage (log.c_str());
    }

    //  The bytes from a step to the next track's sector 0, as a protection
    //  that checks track alignment counts them: for each track of
    //  CASSO_STEP_TRACKS, the address field of sector CASSO_STEP_MARKER, 0
    //  when unset, is the marker, and CASSO_STEP_READS reads of the track
    //  CASSO_STEP_BY on, 1 when unset, wait on it, each the bytes before that
    //  track's sector 0 address field. On the real Apple
    //  when CASSO_LINK_PORT is set, and on an emulated //c with the WOZ from
    //  CASSO_CAPTURE_WOZ otherwise; run only when CASSO_STEP_TRACKS is set.
    TEST_METHOD (StepToSectorZero_IsCounted)
    {
        std::string                    tracks = GetSetting ("CASSO_STEP_TRACKS", "");
        std::string                    port   = GetSetting ("CASSO_LINK_PORT", "");
        int                            drive  = std::stoi (GetSetting ("CASSO_LINK_DRIVE", "1"));
        int                            reads  = std::stoi (GetSetting ("CASSO_STEP_READS", "20"));
        int                            from   = std::stoi (GetSetting ("CASSO_STEP_MARKER", "0"));
        int                            step   = std::stoi (GetSetting ("CASSO_STEP_BY", "1"));
        Win32ByteStreamFactory         factory;
        ComSettings                    settings;
        std::unique_ptr<IByteStream>   opened;
        InMemoryCassoLinkFolder        files;
        std::unique_ptr<CassoLinkHost> real;
        std::unique_ptr<ProgramRig>    rig;
        CassoLinkHost                * host   = nullptr;
        std::vector<Byte>              module;
        std::string                    log;



        if (tracks.empty())
        {
            Logger::WriteMessage ("CASSO_STEP_TRACKS isn't set; no steps to count.\n");
            return;
        }

        if (!port.empty())
        {
            OpenPort (factory, port, settings, opened);
            real = std::make_unique<CassoLinkHost> (std::shared_ptr<IByteStream> (std::move (opened)), files, [] () { return std::array<Byte, 4> {}; });
            host = real.get();
        }
        else
        {
            std::vector<Byte>  source;
            std::ifstream      in (GetSetting ("CASSO_CAPTURE_WOZ", ""), std::ios::binary);

            source.assign (std::istreambuf_iterator<char> (in), std::istreambuf_iterator<char>());
            rig = std::make_unique<ProgramRig> (std::vector<Byte> (143360, 0));
            AssertSucceeded (rig->machine.GetDiskStore().MountFromBytes (ProgramRig::kSlot, 0, "", DiskFormat::Woz, source));
            host  = rig->host.get();
            drive = 1;
        }

        AssertSucceeded (host->BuildCaptureModule (module));

        for (size_t at = 0; at < tracks.size(); )
        {
            size_t             comma  = std::min (tracks.find (',', at), tracks.size());
            int                track  = std::stoi (tracks.substr (at, comma - at));
            std::vector<Byte>  first;
            std::vector<Byte>  marker;
            std::vector<int>   counts;
            size_t             found  = SIZE_MAX;
            CassoLinkStatus    status;
            CaptureTiming      timing;

            at = comma + 1;

            AssertSucceeded (host->EnsureModule (module));
            AssertSucceeded (host->CaptureQuarterTrack ((Byte) drive, (Byte) (track * 4), CassoLinkHost::kRecalibrate, 0, 0, CassoLinkHost::kNoSync, {}, first, status, timing));

            found = FindSector (first, 0, from);
            Assert::IsTrue (found != SIZE_MAX, L"The track has the marker's sector");

            std::transform (first.begin() + (std::ptrdiff_t) found, first.begin() + (std::ptrdiff_t) found + s_kMarkerBytes, std::back_inserter (marker), [] (Byte value) { return (Byte) (value | 0x80); });

            for (int read = 0; read < reads; read++)
            {
                std::vector<Byte>  bytes;

                AssertSucceeded (host->EnsureModule (module));
                AssertSucceeded (host->CaptureQuarterTrack ((Byte) drive, (Byte) ((track + step) * 4), 0, 0, 0, (Byte) (track * 4), marker, bytes, status, timing));

                found = status.error == 0 ? FindSectorZero (bytes, 0) : SIZE_MAX;
                counts.push_back (found == SIZE_MAX ? -1 : (int) found);
            }

            log += std::format ("track {} sector {} to track {} sector 0:", track, from, track + step);

            for (int count : counts)
            {
                log += std::format (" {}", count);
            }

            log += "\n";
        }

        Logger::WriteMessage (log.c_str());
    }

    //  The first address field of sector 0 at or after `from`, SIZE_MAX for
    //  none, with each byte's mark ignored.
    static size_t  FindSectorZero (const std::vector<Byte> & bytes, size_t from)
    {
        return FindSector (bytes, from, 0);
    }

    //  The first address field of `sector` at or after `from`, SIZE_MAX for
    //  none, with each byte's mark ignored.
    static size_t  FindSector (const std::vector<Byte> & bytes, size_t from, int sector)
    {
        auto  get = [&bytes] (size_t at) { return (Byte) (bytes[at] | 0x80); };



        for (size_t at = from; at + s_kMarkerBytes <= bytes.size(); at++)
        {
            bool  prolog = get (at) == 0xD5 && get (at + 1) == 0xAA && get (at + 2) == 0x96;

            if (prolog && (((get (at + 7) << 1) | 1) & get (at + 8)) == sector)
            {
                return at;
            }
        }

        return SIZE_MAX;
    }

    //  An image from CASSO_BOOT_IMAGE, a WOZ or a DOS-ordered sector image,
    //  booted as LogBoot boots one: run only when CASSO_BOOT_IMAGE is set.
    TEST_METHOD (AnImage_Boots)
    {
        std::string        path = GetSetting ("CASSO_BOOT_IMAGE", "");
        std::vector<Byte>  sectors;



        if (path.empty())
        {
            Logger::WriteMessage ("CASSO_BOOT_IMAGE isn't set; no image to boot.\n");
            return;
        }

        {
            std::ifstream  in (path, std::ios::binary);

            sectors.assign (std::istreambuf_iterator<char> (in), std::istreambuf_iterator<char>());
        }

        if (path.ends_with (".woz"))
        {
            std::vector<Byte>   woz = std::move (sectors);
            SectorDecodeReport  report;

            AssertSucceeded (VolumeImage::Load (woz, "image.woz", sectors, report));
            LogBoot (path.c_str(), woz, sectors, true);
        }
        else
        {
            LogBoot (path.c_str(), {}, sectors, false);
        }
    }

    //  The disk inspector's summary and findings for the image, as its Issues    //  list has them.
    static void  LogFindings (const std::vector<Byte> & woz)
    {
        DiskImage     image;
        DiskAnalysis  analysis;
        std::wstring  log;



        AssertSucceeded (WozLoader::Load (woz, image));
        DiskAnalyzer::Analyze (DiskCopy::MakeFromImage (image, 1, "rebuilt.woz", woz.size(), false), DecodeSettings::MakeStandard(), analysis);

        log = std::format (L"Inspector: {} tracks with data, {} of {} sectors good, {} bad\n", analysis.summary.tracksWithData,
                           analysis.summary.sectorsGood, analysis.summary.sectorsFound, analysis.summary.badSectors);

        for (const Finding & finding : analysis.findings)
        {
            log += std::format (L"  {:6.2f}  {}\n", finding.quarterTrack / 4.0, FindingFormatter::Format (finding));
        }

        Logger::WriteMessage (log.c_str());
    }

    static constexpr int       s_kBootSteps   = 6;
    static constexpr uint64_t  s_kBootStep    = 10'000'000ULL;
    static constexpr int       s_kBootSlices  = 100;
    static constexpr size_t    s_kMarkerBytes = 11;
    static constexpr int       s_kSyncReads   = 3;
};
