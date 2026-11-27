#include "Pch.h"

#include "../EmuTests/FakeDiskFileIo.h"
#include "CassoExplorer/ExplorerInspectorHost.h"
#include "CassoExplorer/Model/FilesystemChoices.h"
#include "Devices/Disk/Inspector/DiskAnalysis.h"
#include "Devices/Disk/Inspector/FileMap/FileMap.h"
#include "Machines/Apple2/Common/VolumeImage.h"
#include "Ui/DiskInspector/IDiskInspectorHost.h"
#include "Ui/DiskInspector/InspectorFilesystemOffer.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

using Offer = InspectorFilesystemOffer::Offer;





////////////////////////////////////////////////////////////////////////////////
//
//  InspectorFilesystemOfferTests
//
//  Read as CP/M in the inspector: offered only where no standard file system
//  is found and the host stores a choice, Detect file system once the disk is
//  read as CP/M, and the choice set in the host and the analysis.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (InspectorFilesystemOfferTests)
{
public:

    class FakeHost : public NullDiskInspectorHost
    {
    public:
        bool  CanChooseFilesystem() const override { return canChoose; }
        bool  IsReadAsCpm() const override { return isCpm; }
        void  SetReadAsCpm        (bool value) override { isCpm = value; sets++; }

        bool  canChoose = true;
        bool  isCpm     = false;
        int   sets      = 0;
    };



    static DiskAnalysis  MakeAnalysis (NotMappedReason reason)
    {
        DiskAnalysis  analysis;

        analysis.fileMaps.emplace_back();
        analysis.fileMaps[0].notMapped = reason;

        return analysis;
    }



    TEST_METHOD (ReadAsCpmIsOfferedWhenNoFileSystemIsFound)
    {
        FakeHost      host;
        DiskAnalysis  analysis = MakeAnalysis (NotMappedReason::NoFileSystem);

        Assert::IsTrue (InspectorFilesystemOffer::GetOffer (analysis, host) == Offer::ReadAsCpm);
        Assert::AreEqual (std::wstring (L"No standard file system was found. If this is a CP/M disk with a damaged directory, try Read as CP/M."),
                          InspectorFilesystemOffer::GetNote (Offer::ReadAsCpm));
        Assert::AreEqual (std::wstring (L"Read as CP/M"), InspectorFilesystemOffer::GetButtonLabel (Offer::ReadAsCpm));
    }



    TEST_METHOD (NothingIsOfferedForAMappedOrThirteenSectorDisk)
    {
        FakeHost  host;

        Assert::IsTrue (InspectorFilesystemOffer::GetOffer (MakeAnalysis (NotMappedReason::None),           host) == Offer::None);
        Assert::IsTrue (InspectorFilesystemOffer::GetOffer (MakeAnalysis (NotMappedReason::ThirteenSector), host) == Offer::None);
        Assert::IsTrue (InspectorFilesystemOffer::GetOffer (DiskAnalysis(),                                 host) == Offer::None, L"an analysis with no map yet");
    }



    TEST_METHOD (NothingIsOfferedWithoutAHostThatStoresAChoice)
    {
        FakeHost  host;

        host.canChoose = false;

        Assert::IsTrue (InspectorFilesystemOffer::GetOffer (MakeAnalysis (NotMappedReason::NoFileSystem), host) == Offer::None);
    }



    TEST_METHOD (ReadAsCpmSetsTheChoiceAndDetectClearsIt)
    {
        FakeHost      host;
        DiskAnalysis  analysis = MakeAnalysis (NotMappedReason::NoFileSystem);

        InspectorFilesystemOffer::Accept (InspectorFilesystemOffer::GetOffer (analysis, host), host, analysis);

        Assert::IsTrue (host.isCpm);
        Assert::IsTrue (analysis.isCpmChosen);
        Assert::IsTrue (InspectorFilesystemOffer::GetOffer (analysis, host) == Offer::DetectFilesystem);
        Assert::AreEqual (std::wstring (L"Detect file system"), InspectorFilesystemOffer::GetButtonLabel (Offer::DetectFilesystem));

        InspectorFilesystemOffer::Accept (Offer::DetectFilesystem, host, analysis);

        Assert::IsFalse (host.isCpm);
        Assert::IsFalse (analysis.isCpmChosen);

        InspectorFilesystemOffer::Accept (Offer::None, host, analysis);
        Assert::AreEqual (2, host.sets, L"no offer changes nothing");
    }



    TEST_METHOD (ExplorersHostStoresTheChoiceForTheImage)
    {
        FakeDiskFileIo         fileIo;
        FilesystemChoices      choices;
        ExplorerInspectorHost  host (L"C:\\Disks\\cpm.dsk", fileIo);

        Assert::IsFalse (host.CanChooseFilesystem(), L"no choices before SetFilesystemChoices");

        host.SetFilesystemChoices (&choices);
        host.SetReadAsCpm (true);

        Assert::IsTrue (host.CanChooseFilesystem());
        Assert::IsTrue (host.IsReadAsCpm());
        Assert::IsTrue (choices.GetChoice (L"C:\\Disks\\cpm.dsk") == VolumeKind::Cpm);

        host.SetReadAsCpm (false);

        Assert::IsFalse (host.IsReadAsCpm());
        Assert::IsTrue  (choices.GetChoice (L"C:\\Disks\\cpm.dsk") == VolumeKind::Unknown);
    }
};
