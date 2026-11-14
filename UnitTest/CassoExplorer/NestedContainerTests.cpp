#include "Pch.h"
#include "../EhmTestHelper.h"
#include "../EmuTests/FakeDiskFileIo.h"
#include "../EmuTests/FixtureProvider.h"
#include "../UiTests/InMemoryFileSystem.h"
#include "CassoExplorer/CassoExplorerBrowser.h"
#include "Devices/Disk/Archives/ContainerDiskFileIo.h"
#include "Machines/Apple2/Common/IVolume.h"
#include "Machines/Apple2/Common/VolumeImage.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  NestedContainerTests
//
//  A file on a disk image that is itself an archive or an image, opened in
//  place in the browser through a container path.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (NestedContainerTests)
{
public:

    static int FindRow (const CassoExplorerBrowser & browser, const wchar_t * name)
    {
        const std::vector<CatalogRow> &  rows = browser.GetRows();
        int                              row  = -1;

        for (size_t i = 0; i < rows.size(); i++)
        {
            if (row < 0 && rows[i].name == name)
            {
                row = (int) i;
            }
        }

        Assert::IsTrue (row >= 0, name);

        return row;
    }


    //  A ShrinkIt archive on a ProDOS image, with no extension, opens in
    //  place, as an archive in a ZIP file does, and a text file doesn't.
    TEST_METHOD (OpenRow_AnArchiveOnAnImage_OpensInPlace)
    {
        InMemoryFileSystem   fs;
        FakeDiskFileIo       io;
        ContainerDiskFileIo  nested  { io };
        CassoExplorerBrowser browser { fs, nested };
        FixtureProvider      fixtures;
        std::vector<Byte>    image;
        std::vector<Byte>    archive;
        std::vector<Byte>    sectors;
        std::vector<Byte>    edited;
        std::vector<Byte>    saved;
        std::string          reason;
        SectorDecodeReport   report;
        FilePayload          payload;
        Location             target;
        int                  row     = 0;



        AssertSucceeded (fixtures.OpenFixture ("CassoExplorer/prodos.po", image));
        AssertSucceeded (fixtures.OpenFixture ("CiderPress/nufx/gshk-empty-forks.shk", archive));
        AssertSucceeded (VolumeImage::Load (image, "prodos.po", sectors, report));

        payload.bytes   = archive;
        payload.type    = 0xE0;
        payload.auxType = 0x8002;
        AssertSucceeded (VolumeImage::CreateVolume (VolumeKind::ProDos, sectors)->Write (FilePath::Parse ("NOTES"), payload, edited));
        AssertSucceeded (VolumeImage::Save (image, "prodos.po", edited, saved, reason));

        AssertSucceeded (fs.WriteAllText (L"C:\\Disks\\p.po", std::string (saved.begin(), saved.end())));
        io.files["C:\\Disks\\p.po"]  = saved;
        io.stamps["C:\\Disks\\p.po"] = FileStamp { saved.size(), 100 };

        browser.NavigateToLocation (Location::MakeDiskImage (L"C:\\Disks\\p.po"));
        row = FindRow (browser, L"NOTES");

        Assert::IsTrue   (browser.TryGetRowLocation (row, target));
        Assert::AreEqual (std::wstring (L"C:\\Disks\\p.po|NOTES"), target.path);
        Assert::IsTrue   (browser.OpenRow (row));
        Assert::IsTrue   (FindRow (browser, L"d0") >= 0, L"the archive's records are listed");
    }
};
