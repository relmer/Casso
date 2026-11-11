#include "Pch.h"

#include "EhmTestHelper.h"
#include "FixtureProvider.h"
#include "KeystrokeInjector.h"
#include "MachineIdle.h"
#include "TestMachine.h"
#include "TextScreenScraper.h"
#include "Core/TextEncoding.h"
#include "Devices/Disk/DiskImageStore.h"
#include "Devices/Disk/FilePath.h"
#include "FileTypes/FileFormatTable.h"
#include "FileTypes/VisiCalcEvaluator.h"
#include "FileTypes/VisiCalcSheet.h"
#include "Machines/Apple2/Common/Dos33Volume.h"
#include "Machines/Apple2/Common/VolumeImage.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  VisiCalcTests
//
//  The sheets of the VisiCalc Home and Office Companion, each loaded and
//  recalculated in VisiCalc 2.08 running in Casso, and the screen's 20 rows
//  compared with what Casso's evaluator draws for the same cells. The disks
//  come from scripts/FetchAsimovSamples.ps1; the test is skipped when they
//  are absent.
//
//  VisiCalc 1.x reads and writes 13-sector disks only, as it predates DOS
//  3.3, so the 16-sector companion disks are loaded in 2.08.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (VisiCalcTests)
{
public:

    static constexpr const char *  kProgram       = "Asimov/productivity/spreadsheet/VisiCalc 2.08.nib";
    static constexpr const char *  kSheets[]      = { "Asimov/productivity/spreadsheet/Visicalc Home and Office Companion disk 1.dsk",
                                                      "Asimov/productivity/spreadsheet/Visicalc Home and Office Companion disk 2.dsk" };
    static constexpr uint64_t      kBootCycles    = 60'000'000ULL;
    static constexpr uint64_t      kLoadCycles    = 60'000'000ULL;
    static constexpr uint64_t      kKeyCycles     = 2'000'000ULL;
    static constexpr size_t        kFirstRow      = 4;
    static constexpr size_t        kShownRows     = 20;
    static constexpr size_t        kLabelWidth    = 3;
    static constexpr size_t        kWindowWidth   = 37;
    static constexpr Byte          kReturn        = 0x0D;

    //  VisiCalc recalculates once as it loads a sheet, and once more for the
    //  ! the test types.
    static constexpr size_t        kPasses        = 2;
    static constexpr int           kGotoAttempts  = 3;

    //  Rows Casso doesn't draw as VisiCalc does yet. DIRECT MAIL CAMPAIGN's
    //  C17 divides by A17 before A17 is loaded, as the file lists cells from
    //  the bottom right up, and the ERROR in VisiCalc lasts through a
    //  recalculation; SURVEY RESULTS' J20 is blank in VisiCalc, where Casso
    //  draws its formula's 0.
    struct Known
    {
        const char *  sheet;
        size_t        row;
    };

    static constexpr Known  kKnown[] = { { "DIRECT MAIL CAMPAIGN", 17 }, { "SURVEY RESULTS", 20 } };

    static bool IsKnown (const std::string & sheet, size_t row)
    {
        return std::any_of (std::begin (kKnown), std::end (kKnown), [&] (const Known & known) { return sheet == known.sheet && row == known.row; });
    }

    //  The scraper returns inverse digits and punctuation 64 above their
    //  normal codes; VisiCalc draws its cursor cell and borders in inverse.
    static std::string Normalize (const std::string & row)
    {
        std::string  text = row;



        for (char & c : text)
        {
            if ((unsigned char) c >= 0x60 && (unsigned char) c <= 0x7F)
            {
                c = (char) (c - 0x40);
            }
        }

        while (!text.empty() && text.back() == ' ')
        {
            text.pop_back();
        }

        return text;
    }

    static void Type (MachineHost & host, const std::string & keys)
    {
        KeystrokeInjector::InjectString (host, keys);
        MachineIdle::RunUntilIdle (host, kKeyCycles);
    }

    //  Each of rows 1 to 20 across the whole columns from A that fit the
    //  window, drawn by Casso as VisiCalc's screen draws it.
    static std::vector<std::string> DrawExpected (const VisiCalcSheet::Sheet & sheet)
    {
        std::vector<VisiCalcSheet::Value>  values  = VisiCalcEvaluator::Evaluate (sheet, kPasses);
        size_t                             shown   = kWindowWidth / std::max<size_t> (sheet.columnWidth, 1) * sheet.columnWidth;
        std::vector<std::string>           rows (kShownRows, std::string (shown, ' '));



        for (size_t at = 0; at < sheet.cells.size(); at++)
        {
            const VisiCalcSheet::Cell &  cell  = sheet.cells[at];
            size_t                       start = cell.column * sheet.columnWidth;
            std::string                  text  = TextEncoding::WideToUtf8 (VisiCalcSheet::Draw (sheet, cell, values[at]));

            if (cell.row < 1 || cell.row > kShownRows || start >= shown)
            {
                continue;
            }

            rows[cell.row - 1].replace (start, std::min (text.size(), shown - start), text.substr (0, shown - start));
        }

        for (std::string & row : rows)
        {
            row = Normalize (row);
        }

        return rows;
    }

    TEST_METHOD (CompanionSheets_MatchVisiCalcAfterARecalculation)
    {
        FixtureProvider    fixtures;
        TestMachine        machine ("Apple2Plus");
        std::vector<Byte>  program;
        HRESULT            hrProgram = fixtures.OpenFixture (kProgram, program);
        size_t             compared  = 0;
        std::string        mismatches;



        if (FAILED (hrProgram))
        {
            Logger::WriteMessage ("SKIPPED: the VisiCalc disks are not on this machine; run scripts/FetchAsimovSamples.ps1\n");
            return;
        }

        machine.PowerCycle();
        AssertSucceeded (machine.GetDiskStore().MountFromBytes (6, 0, "visicalc.nib", DiskFormat::Nib, program));
        machine.GetCpu()->SetPC (0xC600);
        MachineIdle::RunUntilIdle (machine, kBootCycles);

        for (const char * disk : kSheets)
        {
            std::vector<Byte>  sectors;
            VolumeListing      listing;
            HRESULT            hrSheets = fixtures.OpenFixture (disk, sectors);

            if (FAILED (hrSheets))
            {
                Logger::WriteMessage ("SKIPPED: the VisiCalc disks are not on this machine; run scripts/FetchAsimovSamples.ps1\n");
                return;
            }

            Dos33Volume  volume (sectors);

            AssertSucceeded (volume.Enumerate (listing));
            AssertSucceeded (machine.GetDiskStore().MountFromBytes (6, 0, "sheets.dsk", DiskFormat::Dsk, sectors));

            for (const FileEntry & entry : listing.entries)
            {
                FilePayload                payload;
                FileFacts                  facts    = FileFacts::From (entry, VolumeKind::Dos33);
                VisiCalcSheet::Sheet       sheet;
                std::wstring               error;
                std::vector<std::string>   expected;
                std::vector<std::string>   screen;
                HRESULT                    hrParse  = S_OK;

                AssertSucceeded (volume.Read (FilePath::Parse (entry.name), payload));

                hrParse = VisiCalcSheet::Parse (payload.bytes, sheet, error);

                if (!VisiCalcSheet::Matches (facts, payload.bytes) || FAILED (hrParse))
                {
                    continue;
                }

                //  Clear the sheet, load this one, recalculate, and go to A1 so
                //  the window starts there.
                Type (machine, "/CY");
                Type (machine, "/SL");
                KeystrokeInjector::InjectString (machine, entry.name);
                KeystrokeInjector::InjectKey (machine, kReturn);
                MachineIdle::RunUntilIdle (machine, kLoadCycles);
                Type (machine, "!");
                //  The status line starts with the cursor's cell; the goto is
                //  repeated until it is A1, in case the load was running.
                for (int attempt = 0; attempt < kGotoAttempts; attempt++)
                {
                    std::vector<std::string>  top;

                    Type (machine, ">A1");
                    KeystrokeInjector::InjectKey (machine, kReturn);
                    MachineIdle::RunUntilIdle (machine, kLoadCycles);

                    top = TextScreenScraper::Scrape (machine);

                    if (!top.empty() && Normalize (top[0]).starts_with ("A1 "))
                    {
                        break;
                    }
                }

                screen   = TextScreenScraper::Scrape (machine);
                expected = DrawExpected (sheet);

                for (size_t row = 0; row < kShownRows && kFirstRow + row < screen.size(); row++)
                {
                    std::string  line  = screen[kFirstRow + row];
                    std::string  shown = Normalize (line.substr (std::min (kLabelWidth, line.size())));

                    if (shown != expected[row] && !IsKnown (entry.name, row + 1))
                    {
                        mismatches += std::format ("{} row {}:\n  VisiCalc [{}]\n  Casso    [{}]\n", entry.name, row + 1, shown, expected[row]);
                    }
                }

                compared++;
            }
        }

        Logger::WriteMessage (std::format ("Compared {} sheets with VisiCalc 2.08\n", compared).c_str());
        Assert::IsTrue (compared > 0, L"no sheet was compared");
        Assert::IsTrue (mismatches.empty(), TextEncoding::Utf8ToWide (mismatches).c_str());
    }
};
