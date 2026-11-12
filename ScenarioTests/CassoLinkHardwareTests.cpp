#include "Pch.h"

#include "EhmTestHelper.h"
#include "GuestSession.h"
#include "TestMachine.h"
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

        AssertSucceeded (factory.OpenComPort (TextEncoding::NarrowToWide (port), settings, opened));

        CassoLinkHost  host (std::shared_ptr<IByteStream> (std::move (opened)), files, [] () { return std::array<Byte, 4> {}; });

        std::ofstream  trace (folder / "trace.log");
        int            last  = std::stoi (GetSetting ("CASSO_LINK_LAST", "34"));

        host.SetTraceSink ([&trace] (bool sent, const std::string & text)
        {
            trace << (sent ? "> " : "< ") << text << "\n";
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

    //  The disk inspector's summary and findings for the image, as its Issues
    //  list has them.
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

    static constexpr int       s_kBootSteps  = 6;
    static constexpr uint64_t  s_kBootStep   = 10'000'000ULL;
    static constexpr int       s_kBootSlices = 100;
};
