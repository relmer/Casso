#include "Pch.h"

#include "EhmTestHelper.h"
#include "GuestSession.h"
#include "TestMachine.h"
#include "../UnitTest/Inspector/RepairFixtures.h"
#include "CassoExplorer/ExplorerBootWorker.h"
#include "CassoExplorer/FullSendDialog.h"
#include "CassoLink/FullSend/FullSendManifest.h"
#include "Core/TextEncoding.h"
#include "Devices/Disk/Inspector/Repair/DiskRepair.h"
#include "Devices/Disk/Inspector/Repair/FileSystemCheck.h"
#include "Devices/Disk/Inspector/Repair/RepairCopy.h"
#include "Seams/Win32DiskFileIo.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  FullSendFileSystemCheckTests
//
//  The file-system check of each image a Full Send saves (SC-076), on real
//  disks booted on an emulated Apple: the DOS 3.3 System Master with planted
//  bookkeeping damage boots, has the verdict Bad in its manifest row, the
//  window and the activity line, counts as a failed capture, and has a
//  repaired copy with the verdict OK; a copy-protected disk that boots has
//  no standard file system and counts as complete.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (FullSendFileSystemCheckTests)
{
public:

    static constexpr int  kVtocTrack    = 17;
    static constexpr int  kBitmap       = 0x38;
    static constexpr int  kBitmapBytes  = 4;



    static HRESULT  BuildFromFixtures (const std::string & machineId, BootCheck::Built & outBuilt, std::string & outError)
    {
        std::shared_ptr<TestMachine>  machine = std::make_shared<TestMachine> (machineId);



        outError.clear();

        outBuilt.builder = &machine->GetBuilder();
        outBuilt.host    = machine;

        return S_OK;
    }



    static std::filesystem::path  MakeFolder (const wchar_t * what)
    {
        std::filesystem::path  folder = std::filesystem::temp_directory_path() / std::format (L"CassoFsCheck{}_{}", what, ::GetCurrentProcessId());



        std::filesystem::remove_all (folder);
        std::filesystem::create_directories (folder);

        return folder;
    }



    //  A manifest in the folder with one row for the file, as a Full Send
    //  writes it before the image is booted.
    static void  WriteManifest (Win32DiskFileIo & io, const std::filesystem::path & folder, const std::string & file, const char * status)
    {
        FullSendManifest  manifest;
        FullSendRow       row;



        row.sequence    = 1;
        row.file        = file;
        row.fingerprint = std::string (64, 'a');
        row.hash        = std::string (64, 'b');
        row.time        = "2026-11-16T10:00:00";
        row.status      = status;
        manifest.Add (row);

        AssertSucceeded (io.WriteAllBytes (TextEncoding::WideToUtf8 ((folder / FullSendManifest::kFileName).wstring()), manifest.Format()));
    }



    static FullSendRow  ReadRow (Win32DiskFileIo & io, const std::filesystem::path & folder)
    {
        FullSendManifest   manifest;
        std::vector<Byte>  bytes;



        AssertSucceeded (io.ReadAllBytes (TextEncoding::WideToUtf8 ((folder / FullSendManifest::kFileName).wstring()), bytes));
        AssertSucceeded (manifest.Parse (bytes));
        Assert::AreEqual ((size_t) 1, manifest.GetRows().size());

        return manifest.GetRows()[0];
    }



    static ExplorerBootWorker::Result  Boot (Win32DiskFileIo & io, const std::filesystem::path & folder, const std::filesystem::path & image)
    {
        ThumbnailStore      store (folder / L"cache");
        ExplorerBootWorker  worker (store, BuildFromFixtures);



        worker.SetManifestIo (&io);

        return worker.Check (image.wstring(), "Apple //e enhanced");
    }



    //  The catalog track's sectors, which the catalog uses, marked free in the
    //  VTOC's bitmap.
    TEST_METHOD (ADamagedDisk_ThatBoots_IsBad_AndItsRepairedCopyIsOk)
    {
        Win32DiskFileIo             io;
        std::filesystem::path       folder  = MakeFolder (L"Damage");
        std::filesystem::path       image   = folder / L"0001.dsk";
        std::string                 path    = TextEncoding::WideToUtf8 (image.wstring());
        std::vector<Byte>           master  = GuestSession::RequireDos33Master();
        Byte                      * vtoc    = RepairFixtures::GetDosSector (master, kVtocTrack, 0);
        ExplorerBootWorker::Result  result;
        FullSendRow                 row;
        RepairPlan                  plan;
        DiskAnalysis                analysis;
        std::wstring                reason;
        std::string                 written;
        std::string                 line;



        vtoc[kBitmap + kVtocTrack * kBitmapBytes]     = 0xFF;
        vtoc[kBitmap + kVtocTrack * kBitmapBytes + 1] = 0xFF;

        AssertSucceeded (io.WriteAllBytes (path, master));
        WriteManifest (io, folder, "0001.dsk", FullSendManifest::kStatusSent);

        result = Boot (io, folder, image);
        line   = ExplorerBootWorker::Describe (result);
        row    = ReadRow (io, folder);

        Assert::IsTrue   (result.hasReport && result.verdict == BootVerdict::Booted, TextEncoding::NarrowToWide (result.reason).c_str());
        Assert::AreEqual (std::string (FullSendManifest::kCheckBad), result.fsCheck, TextEncoding::NarrowToWide (line).c_str());
        Assert::IsFalse  (result.isComplete, L"A bad file system is a failed capture, so the failure sounds");
        Assert::IsTrue   (line.find (" File system: Bad, ") != std::string::npos, TextEncoding::NarrowToWide (line).c_str());
        Assert::AreEqual (std::string (FullSendManifest::kCheckBad), row.fsCheck, L"The verdict is in the manifest");
        Assert::AreEqual (std::wstring (L"Sent, boots, file system bad"), FullSendPanel::DescribeStatus (row));
        Assert::IsTrue   (FullSendDialog::IsRepairOffered (row));

        //  The row's Repair disk image, as the inspector's repair writes it.
        AssertSucceeded (RepairCopy::PlanFile (io, path, {}, plan, reason));
        Assert::IsTrue  (plan.canRepair, plan.reason.c_str());

        AssertSucceeded (RepairCopy::Write (io, path, "", plan, written, reason));
        AssertSucceeded (RepairCopy::AnalyzeFile (io, written, analysis, reason));
        Assert::IsTrue  (analysis.verdict == FileSystemVerdict::Ok, FileSystemCheck::FormatVerdict (analysis.verdict).c_str());

        std::filesystem::remove_all (folder);
    }



    TEST_METHOD (ACopyProtectedDisk_ThatBoots_HasNoStandardFileSystem_AndIsComplete)
    {
        Win32DiskFileIo             io;
        std::filesystem::path       folder  = MakeFolder (L"Protected");
        std::filesystem::path       image   = folder / L"0001.woz";
        std::vector<Byte>           woz     = GuestSession::FindGameImage (L"Choplifter.woz");
        ExplorerBootWorker::Result  result;
        FullSendRow                 row;
        std::string                 line;



        Assert::IsFalse (woz.empty(), L"Choplifter.woz is in the game images");

        AssertSucceeded (io.WriteAllBytes (TextEncoding::WideToUtf8 (image.wstring()), woz));
        WriteManifest (io, folder, "0001.woz", FullSendManifest::kStatusRawCapture);

        result = Boot (io, folder, image);
        line   = ExplorerBootWorker::Describe (result);
        row    = ReadRow (io, folder);

        Assert::IsTrue   (result.hasReport && result.verdict == BootVerdict::Booted, TextEncoding::NarrowToWide (line).c_str());
        Assert::AreEqual (std::string (FullSendManifest::kCheckNone), result.fsCheck, TextEncoding::NarrowToWide (line).c_str());
        Assert::IsTrue   (result.isComplete, L"No standard file system isn't a failure, so success sounds");
        Assert::IsTrue   (line.ends_with (" No standard file system."), TextEncoding::NarrowToWide (line).c_str());
        Assert::AreEqual (std::string (FullSendManifest::kCheckNone), row.fsCheck);
        Assert::AreEqual (std::wstring (L"Raw capture, boots"), FullSendPanel::DescribeStatus (row), L"The status implies no standard file system");
        Assert::IsFalse  (FullSendDialog::IsRepairOffered (row));

        std::filesystem::remove_all (folder);
    }
};
