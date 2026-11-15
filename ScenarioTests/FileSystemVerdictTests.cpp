#include "Pch.h"

#include "GuestSession.h"
#include "Core/TextEncoding.h"
#include "Devices/Disk/Inspector/FindingFormatter.h"
#include "Devices/Disk/Inspector/Repair/DiskRepair.h"
#include "Devices/Disk/Inspector/Repair/FileSystemCheck.h"
#include "Devices/Disk/Inspector/Repair/RepairCopy.h"
#include "Seams/Win32DiskFileIo.h"
#include "../UnitTest/Inspector/RepairFixtures.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  FileSystemVerdictTests
//
//  The file-system verdict and Repair on real files (SC-045): each kind of
//  planted bookkeeping damage repaired, through the copy written beside the
//  image in a temporary folder, to the image before the damage; and on the
//  test collection, standard and copy-protected disks, no copy-protected
//  disk or disk of another file system Bad and no standard disk without
//  damage other than OK. A collection with fewer than 30 disks present
//  fails rather than checking fewer.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (FileSystemVerdictTests)
{
public:

    static constexpr size_t  kCollectionMin = 30;



    struct Disk
    {
        std::wstring  name;
        vector<Byte>  bytes;
        bool          isStandard = false;
    };



    static std::filesystem::path MakeFolder (const wchar_t * what)
    {
        std::filesystem::path  folder = std::filesystem::temp_directory_path() / std::format (L"CassoVerdict{}_{}", what, ::GetCurrentProcessId());



        std::filesystem::remove_all (folder);
        std::filesystem::create_directories (folder);

        return folder;
    }



    static std::wstring Describe (const DiskAnalysis & analysis)
    {
        std::wstring  text = FileSystemCheck::FormatVerdict (analysis.verdict);



        for (const Finding & finding : analysis.findings)
        {
            if (finding.category == FindingCategory::FileSystem && finding.weight != FindingWeight::Note)
            {
                text += L"; " + FindingFormatter::Format (finding);
            }
        }

        return text;
    }



    static bool IsImageName (const std::string & path)
    {
        std::string  extension = std::filesystem::path (path).extension().string();



        return extension == ".dsk" || extension == ".do" || extension == ".po" || extension == ".woz" || extension == ".nib";
    }



    TEST_METHOD (EveryPlantedKindIsRepairedToTheImageBeforeTheDamage)
    {
        Win32DiskFileIo        io;
        std::filesystem::path  folder   = MakeFolder (L"Repair");
        int                    repaired = 0;
        int                    declined = 0;



        for (const RepairFixture & fixture : RepairFixtures::MakeAll())
        {
            std::filesystem::path  image   = folder / (fixture.name + (fixture.isProDosOrder ? L".po" : L".dsk"));
            std::string            path    = TextEncoding::WideToUtf8 (image.wstring());
            RepairPlan             plan;
            std::string            written;
            std::wstring           reason;
            vector<Byte>           copy;
            HRESULT                hr      = S_OK;

            hr = io.WriteAllBytes (path, fixture.damaged);
            Assert::IsTrue (SUCCEEDED (hr), fixture.name.c_str());

            hr = RepairCopy::PlanFile (io, path, {}, plan, reason);
            Assert::IsTrue (SUCCEEDED (hr), (fixture.name + L": " + reason).c_str());
            Assert::AreEqual (fixture.canRepair, plan.canRepair, (fixture.name + L": " + plan.reason).c_str());

            if (!fixture.canRepair)
            {
                declined++;
                continue;
            }

            hr = RepairCopy::Write (io, path, "", plan, written, reason);
            Assert::IsTrue (SUCCEEDED (hr), (fixture.name + L": " + reason).c_str());

            hr = io.ReadAllBytes (written, copy);
            Assert::IsTrue (SUCCEEDED (hr));
            Assert::IsTrue (copy == fixture.before, (fixture.name + L": the copy is the image before the damage").c_str());

            hr = io.ReadAllBytes (path, copy);
            Assert::IsTrue (SUCCEEDED (hr));
            Assert::IsTrue (copy == fixture.damaged, (fixture.name + L": the image isn't changed").c_str());
            repaired++;
        }

        Assert::AreEqual (7, repaired, L"every kind that can be repaired was");
        Assert::AreEqual (2, declined, L"and the cross-link and the chain off the disk weren't");

        std::filesystem::remove_all (folder);
    }



    TEST_METHOD (TheCollectionHasNoProtectedDiskBadAndEveryStandardDiskOk)
    {
        static constexpr const wchar_t *  kGames[] =
        {
            L"00_Bandits.woz", L"AppleStellarInvaders.woz", L"Archon.woz", L"Blank Disk.woz", L"Choplifter.woz", L"HardHatMack.woz", L"Karateka.woz",
            L"LodeRunner.woz", L"RockysBoots.woz", L"Space Quarks (1981)(Br\x00F8" L"derbund)(II-II+)[48K].woz", L"The Print Shop Color side A.woz",
            L"The Print Shop Color side B.woz", L"Where in the World is Carmen Sandiego side A.woz", L"Where in the World is Carmen Sandiego side B.woz",
        };

        static constexpr const wchar_t *  kStandardGames[] = { L"Blank Disk.do", L"Bootable DOS scratch.dsk", L"ProDOS test and data.po" };
        static constexpr const char *     kTestDisks[]     = { "wizardry-played.dsk", "wizardry1-scenario.dsk", "wizardry2-scenario.dsk", "wizardry3-scenario.dsk" };
        static constexpr const char *     kOtherDisks[]    = { "gutenberg-jr.dsk", "zork1.dsk" };
        static constexpr const char *     kRepoDisks[]     = { "Disks/Casso/JoyportTest.dsk", "Disks/Casso/JoystickTest.dsk", "Disks/Casso/JoystickTest4.dsk",
                                                               "Disks/Network/contiki-apple2-3.dsk", "Disks/Network/ip65.dsk" };



        vector<Disk>   disks;
        std::wstring   missing;
        std::wstring   wrong;
        int            standard = 0;
        size_t         present  = 0;
        int            none     = 0;



        disks.push_back ({ L"DOS 3.3 System Master", GuestSession::RequireDos33Master(),     true });
        disks.push_back ({ L"ProDOS Users Disk",     GuestSession::RequireProDosUsersDisk(), true });
        disks.push_back ({ L"Kermit-65",             GuestSession::RequireKermitDisk(),      true });
        disks.push_back ({ L"SOS System Utilities",  GuestSession::RequireSosUtilitiesDisk(), false });

        for (const char * path : kRepoDisks)
        {
            disks.push_back ({ TextEncoding::Utf8ToWide (path), GuestSession::RequireRepoImage (path), true });
        }

        for (const char * name : kTestDisks)
        {
            disks.push_back ({ TextEncoding::Utf8ToWide (name), GuestSession::FindTestDisk (name), true });
        }

        for (const char * name : kOtherDisks)
        {
            disks.push_back ({ TextEncoding::Utf8ToWide (name), GuestSession::FindTestDisk (name), false });
        }

        for (const wchar_t * name : kStandardGames)
        {
            disks.push_back ({ name, GuestSession::FindGameImage (name), true });
        }

        for (const wchar_t * name : kGames)
        {
            disks.push_back ({ name, GuestSession::FindGameImage (name), false });
        }

        for (const Disk & disk : disks)
        {
            DiskAnalysis  analysis;
            std::wstring  reason;
            std::string   path    = TextEncoding::WideToUtf8 (std::filesystem::path (disk.name).filename().wstring());
            HRESULT       hr      = S_OK;
            bool          isRight = false;

            if (disk.bytes.empty())
            {
                missing += disk.name + L"\n";
                continue;
            }

            path = IsImageName (path) ? path : path + ".dsk";
            hr   = FileSystemCheck::Analyze (disk.bytes, path, analysis, reason);
            Assert::IsTrue (SUCCEEDED (hr), (disk.name + L": " + reason).c_str());

            isRight   = disk.isStandard ? analysis.verdict == FileSystemVerdict::Ok : analysis.verdict != FileSystemVerdict::Bad;
            standard += disk.isStandard ? 1 : 0;
            present  += 1;
            none     += (analysis.verdict == FileSystemVerdict::None) ? 1 : 0;

            if (!isRight)
            {
                wrong += disk.name + L": " + Describe (analysis) + L"\n";
            }
        }

        Assert::IsTrue (present >= kCollectionMin,
                        (L"Fewer than 30 disks of the collection are present. Missing:\n" + missing).c_str());
        Assert::IsTrue (standard > 0 && static_cast<size_t> (standard) < present, L"the collection has standard disks and others");
        Assert::IsTrue (none > 0, L"a copy-protected disk has no standard file system");
        Assert::IsTrue (wrong.empty(), (L"Verdicts that are wrong:\n" + wrong).c_str());
    }
};
