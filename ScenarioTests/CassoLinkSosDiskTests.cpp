#include "Pch.h"

#include "EhmTestHelper.h"
#include "FakeDiskFileIo.h"
#include "GuestSession.h"
#include "UiTests/InMemoryFileSystem.h"
#include "CassoExplorer/CassoExplorerActions.h"
#include "CassoExplorer/CassoExplorerBrowser.h"
#include "Machines/Apple2/Common/NibblizationLayer.h"
#include "Machines/Apple2/Common/ProDosSkeleton.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkSosDiskTests
//
//  Make CassoLink SOS disk on Apple's System Demo disk, a stock disk read
//  from the data folder.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (CassoLinkSosDiskTests)
{
public:

    //  The CassoLink SOS disk made from Apple's System Demo disk has its
    //  SOS.KERNEL and SOS.DRIVER and the SOS.INTERP; a disk without SOS
    //  and a speed above the Apple ///'s fail with the reason.
    TEST_METHOD (SaveLinkSosDisk_WritesADiskWithTheUsersSos)
    {
        InMemoryFileSystem     fs;
        FakeDiskFileIo         io;
        CassoExplorerBrowser   browser { fs, io };
        CassoExplorerActions   actions { browser, fs };
        CassoLinkSettings      serial;
        std::vector<Byte>      blank (NibblizationLayer::kImageByteSize, 0);
        std::vector<Byte>      file;
        Byte                   fileType = 0;
        Word                   auxType  = 0;
        ActionsTypes::Outcome  outcome;



        serial.baud = 9600;

        io.files["C:/Disks/Sos.dsk"]  = GuestSession::RequireSosDemoDisk();
        io.stamps["C:/Disks/Sos.dsk"] = FileStamp { io.files["C:/Disks/Sos.dsk"].size(), 1 };

        AssertSucceeded (actions.SaveLinkSosDisk (L"C:/Disks/Sos.dsk", L"C:/Disks/Link.dsk", serial).hr);

        for (const char * name : { "SOS.KERNEL", "SOS.DRIVER", "SOS.INTERP" })
        {
            AssertSucceeded (ProDosReader::ExtractFile (io.files["C:/Disks/Link.dsk"], name, file, fileType, auxType));
        }

        AssertSucceeded (ProDosSkeleton::Write (blank, "EMPTY"));
        io.files["C:/Disks/Empty.dsk"]  = blank;
        io.stamps["C:/Disks/Empty.dsk"] = FileStamp { blank.size(), 1 };

        outcome = actions.SaveLinkSosDisk (L"C:/Disks/Empty.dsk", L"C:/Disks/Link2.dsk", serial);
        Assert::IsFalse (outcome.Succeeded());
        Assert::IsTrue (outcome.message.find (L"SOS loader") != std::wstring::npos, outcome.message.c_str());

        serial.baud = 115200;
        outcome     = actions.SaveLinkSosDisk (L"C:/Disks/Sos.dsk", L"C:/Disks/Link3.dsk", serial);
        Assert::IsTrue (outcome.message.find (L"19,200") != std::wstring::npos, outcome.message.c_str());
    }
};
