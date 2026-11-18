#include "Pch.h"

#include "../EhmTestHelper.h"
#include "../EmuTests/FakeDiskFileIo.h"
#include "CassoLink/FullSend/FullSendDisk.h"
#include "CassoLink/FullSend/FullSendFolder.h"
#include "CassoLink/Session/CassoLinkDiskTransfer.h"
#include "FullSendTestDisks.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;
using namespace FullSendTestDisks;





////////////////////////////////////////////////////////////////////////////////
//
//  FullSendFolderTests
//
//  FullSendFolder over an in-memory folder: the images it writes, the
//  renamed images it finds, and its manifest.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (FullSendFolderTests)
{
public:

    static constexpr const char * kFolder = "C:\\Box";

    static FullSendRow  MakeRow (int sequence, const std::string & file, const Image & sectors)
    {
        FullSendRow  row;

        row.sequence = sequence;
        row.file     = file;
        Assert::AreEqual (S_OK, FullSendDisk::ComputeHash (sectors, row.hash));

        return row;
    }

    TEST_METHOD (AnImage_BesideAFileWithItsTitle_TakesTheNextNumber)
    {
        FakeDiskFileIo  io;
        FullSendFolder  folder (io, kFolder, "");
        FullSendImage   image;
        std::string     file;

        io.files["C:\\Box\\0001.dsk"] = { 1, 2, 3 };
        image.sectors                 = Other (1);

        Assert::AreEqual (S_OK, folder.WriteImage ("0001", image, true, file));

        Assert::AreEqual (std::string ("0001 (2).dsk"), file);
        Assert::IsTrue (io.files["C:\\Box\\0001.dsk"] == Image ({ 1, 2, 3 }));
        Assert::IsTrue (io.files["C:\\Box\\0001 (2).dsk"] == Other (1));
    }

    TEST_METHOD (ARawCapture_IsAWoz_WithItsReadsBesideIt_AndNoList)
    {
        FakeDiskFileIo  io;
        FullSendFolder  folder (io, kFolder, ".dsk");
        FullSendImage   image;
        std::string     file;

        image.sectors    = Other (1);
        image.unreadable = { { 3, 4 } };
        image.raw        = { 'W', 'O', 'Z', '2', 0xFF, 0x0A, 0x0D, 0x0A };
        image.reads      = { 7, 8, 9 };

        Assert::AreEqual (S_OK, folder.WriteImage ("0002", image, true, file));

        Assert::AreEqual (std::string ("0002.woz"), file);
        Assert::IsTrue (io.files["C:\\Box\\0002.woz"] == image.raw);
        Assert::IsTrue (io.files["C:\\Box\\0002.woz.reads"] == image.reads);
        Assert::IsFalse (io.files.contains (CassoLinkDiskTransfer::GetUnreadableListPath ("C:\\Box\\0002.woz")));
    }

    TEST_METHOD (ACaptureLeftOut_WritesTheSectors_WithTheirList)
    {
        FakeDiskFileIo  io;
        FullSendFolder  folder (io, kFolder, ".dsk");
        FullSendImage   image;
        std::string     file;

        image.sectors    = Other (1);
        image.unreadable = { { 3, 4 } };
        image.raw        = { 'W', 'O', 'Z', '2', 0xFF, 0x0A, 0x0D, 0x0A };
        image.reads      = { 7, 8, 9 };

        Assert::AreEqual (S_OK, folder.WriteImage ("0003", image, false, file));

        Assert::AreEqual (std::string ("0003.dsk"), file);
        Assert::IsTrue (io.files["C:\\Box\\0003.dsk"] == Other (1));
        Assert::IsTrue (io.files.contains (CassoLinkDiskTransfer::GetUnreadableListPath ("C:\\Box\\0003.dsk")));
        Assert::IsFalse (io.files.contains ("C:\\Box\\0003.dsk.reads"));
    }

    TEST_METHOD (AWriteProtectedDisksSectorImage_IsReadOnly)
    {
        FakeDiskFileIo  io;
        FullSendFolder  folder (io, kFolder, ".dsk");
        FullSendImage   image;
        std::string     file;

        image.sectors        = Other (1);
        image.writeProtected = true;

        Assert::AreEqual (S_OK, folder.WriteImage ("0004", image, true, file));

        Assert::IsTrue (io.readOnlyFiles["C:\\Box\\0004.dsk"]);
    }

    TEST_METHOD (ARenamedImage_IsFoundByItsHash_AndItsDuplicatesFollowIt)
    {
        FakeDiskFileIo           io;
        FullSendFolder           folder (io, kFolder, "");
        FullSendRow              first = MakeRow (1, "0001.dsk", Other (1));
        FullSendRow              copy  = MakeRow (2, "0002.dsk", Other (1));
        FullSendFolder::Renames  renames;

        copy.duplicateOf = "0001.dsk";

        folder.GetManifest().Add (first);
        folder.GetManifest().Add (copy);

        io.files["C:\\Box\\0002.dsk"]    = Other (1);
        io.files["C:\\Box\\Renamed.dsk"] = Other (1);

        Assert::AreEqual (S_OK, folder.FindMovedImages ([] (const FullSendRow &) { return true; }, renames));

        Assert::AreEqual (size_t (1), renames.size());
        Assert::AreEqual (std::string ("0001.dsk"),    renames[0].first);
        Assert::AreEqual (std::string ("Renamed.dsk"), renames[0].second);
        Assert::AreEqual (std::string ("Renamed.dsk"), folder.GetManifest().GetRows()[0].file);
        Assert::AreEqual (std::string ("Renamed.dsk"), folder.GetManifest().GetRows()[1].duplicateOf);
        Assert::IsFalse (io.files.contains ("C:\\Box\\manifest.fullsend"), L"the manifest is written by the caller");
        Assert::IsNotNull (folder.FindWithImage ([] (const FullSendRow & row) { return row.sequence == 1; }));
    }

    TEST_METHOD (AManifestThatDoesNotParse_LeavesTheRowsInMemory_AndDamagesTheFolder)
    {
        FakeDiskFileIo  io;
        FullSendFolder  folder (io, kFolder, "");
        HRESULT         hr     = S_OK;

        folder.GetManifest().Add (Row (1, "0001.dsk"));
        Assert::AreEqual (S_OK, folder.WriteManifest());
        Assert::AreEqual (S_OK, folder.Reload());
        Assert::IsFalse (folder.IsDamaged());

        io.files["C:\\Box\\manifest.fullsend"] = { 'x' };

        hr = folder.Reload();

        Assert::IsTrue (FAILED (hr));
        Assert::IsTrue (folder.IsDamaged());
        Assert::AreEqual (size_t (1), folder.GetManifest().GetRows().size());
    }

    TEST_METHOD (AFolder_HasOneLock_InAnyCase)
    {
        Assert::IsTrue (&FullSendFolder::GetLock ("C:\\Box") == &FullSendFolder::GetLock ("c:\\BOX"));
        Assert::IsTrue (&FullSendFolder::GetLock ("C:\\Box") != &FullSendFolder::GetLock ("C:\\Other"));
    }
};
