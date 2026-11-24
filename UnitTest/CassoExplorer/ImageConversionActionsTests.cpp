#include "Pch.h"

#include "../EhmTestHelper.h"
#include "../EmuTests/FakeDiskFileIo.h"
#include "../UiTests/InMemoryFileSystem.h"
#include "ScratchImages.h"
#include "CassoExplorer/CassoExplorerActions.h"
#include "CassoExplorer/CassoExplorerBrowser.h"
#include "Core/TextEncoding.h"
#include "Devices/Disk/DiskImage.h"
#include "Machines/Apple2/Common/DiskImageConverter.h"
#include "Machines/Apple2/Common/NibbleImageCodec.h"
#include "Machines/Apple2/Common/Nibblizer.h"
#include "Machines/Apple2/Common/VolumeImage.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  ImageConversionActionsTests
//
//  Convert to on a disk image in a folder: the new image beside it under a
//  free name, and the reason when the image can't be converted.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (ImageConversionActionsTests)
{
public:

    struct Host
    {
        InMemoryFileSystem    fs;
        FakeDiskFileIo        io;
        CassoExplorerBrowser  browser { fs, io };
        CassoExplorerActions  actions { browser, fs };

        void Seed (const std::vector<Byte> & bytes, const std::wstring & path)
        {
            std::string  narrow = TextEncoding::WideToNarrow (path);



            AssertSucceeded (fs.WriteAllText (path, std::string (bytes.begin(), bytes.end())));
            io.files[narrow]  = bytes;
            io.stamps[narrow] = FileStamp { bytes.size(), 100 };
        }
    };

    TEST_METHOD (ConvertImage_WritesThePoBesideTheImage)
    {
        Host                           host;
        std::vector<Byte>              dsk = ScratchImages::GetDos33 (false);
        std::vector<Byte>              expected;
        std::wstring                   made;
        std::wstring                   second;
        CassoExplorerActions::Outcome  outcome;



        host.Seed (dsk, L"C:\\Disks\\dos33.dsk");

        outcome = host.actions.ConvertImage (L"C:\\Disks\\dos33.dsk", DiskImageFormat::Po, made);

        Assert::IsTrue   (outcome.Succeeded(), outcome.message.c_str());
        Assert::AreEqual (std::wstring (L"C:\\Disks\\dos33.po"), made);

        VolumeImage::DosLogicalToProDosFile (dsk, expected);
        Assert::IsTrue (host.io.files["C:\\Disks\\dos33.po"] == expected);

        AssertSucceeded (host.fs.WriteAllText (made, "taken"));
        outcome = host.actions.ConvertImage (L"C:\\Disks\\dos33.dsk", DiskImageFormat::Po, second);
        Assert::AreEqual (std::wstring (L"C:\\Disks\\dos33 (2).po"), second);
    }

    TEST_METHOD (ConvertImage_GivesTheReasonADamagedNibFails)
    {
        Host                           host;
        std::vector<Byte>              nib;
        DiskImage                      image;
        std::wstring                   made;
        CassoExplorerActions::Outcome  outcome;



        AssertSucceeded (Nibblizer::NibblizeDsk (ScratchImages::GetDos33 (false), image));
        AssertSucceeded (NibbleImageCodec::Build (image, NibbleImageCodec::kNibTrackSize, nib));
        std::fill (nib.begin(), nib.begin() + (ptrdiff_t) NibbleImageCodec::kNibTrackSize / 2, NibbleImageCodec::kSyncNibble);

        host.Seed (nib, L"C:\\Disks\\game.nib");

        outcome = host.actions.ConvertImage (L"C:\\Disks\\game.nib", DiskImageFormat::Dsk, made);

        Assert::IsFalse (outcome.Succeeded());
        Assert::IsTrue  (outcome.message.find (L"game.nib can't be converted to .dsk. Track 0") != std::wstring::npos, outcome.message.c_str());
        Assert::IsTrue  (made.empty());
        Assert::IsTrue  (host.io.files.count ("C:\\Disks\\game.dsk") == 0);
    }
};
