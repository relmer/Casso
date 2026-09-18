#include "Pch.h"
#include "../EhmTestHelper.h"
#include "Cassque/Model/DragPayload.h"
#include "Machines/Apple2/Common/Dos33Volume.h"
#include "Machines/Apple2/Common/ProDosVolume.h"
#include "Machines/Apple2/Common/VolumeImage.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  DragPayloadTests
//
//  What each kind of drag offers, and the names the host side would receive.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (DragPayloadTests)
{
public:

    using Format = DragPayload::Format;
    using Source = DragPayload::SourceKind;
    using Style  = HostFileNaming::Style;



    static FileEntry MakeEntry (const char * name, Byte type, bool isDirectory = false, bool hasAddress = false, Word address = 0)
    {
        FileEntry  entry;

        entry.name           = name;
        entry.type           = type;
        entry.isDirectory    = isDirectory;
        entry.hasLoadAddress = hasAddress;
        entry.loadAddress    = address;
        entry.hasAuxType     = hasAddress;
        entry.auxType        = address;

        return entry;
    }



    static bool Offers (const DragPayload::Plan & plan, Format format)
    {
        return std::find (plan.formats.begin(), plan.formats.end(), format) != plan.formats.end();
    }



    TEST_METHOD (CatalogEntries_OfferThePrivateFormatAndDescriptors_NotHDrop)
    {
        std::vector<FileEntry>  entries = { MakeEntry ("HELLO", ProDosVolume::kTypeBasic),
                                            MakeEntry ("PIC", ProDosVolume::kTypeBinary, false, true, 0x2000) };
        DragPayload::Plan       plan    = DragPayload::Build (Source::CatalogEntries, "C:\\a.po", VolumeKind::ProDos,
                                                              entries, Style::Descriptive, nullptr, {});
        std::string               image;
        VolumeKind                kind  = VolumeKind::Unknown;
        std::vector<std::string>  paths;

        Assert::IsTrue  (Offers (plan, Format::CatalogEntries));
        Assert::IsTrue  (Offers (plan, Format::FileDescriptors));
        Assert::IsTrue  (Offers (plan, Format::FileContents));
        Assert::IsFalse (Offers (plan, Format::HDrop));

        Assert::AreEqual ((size_t) 2, plan.descriptors.size());
        Assert::AreEqual (std::wstring (L"HELLO.Applesoft BASIC.txt"), plan.descriptors[0].relativePath);
        Assert::IsTrue   (plan.descriptors[0].converted);
        Assert::AreEqual (std::wstring (L"PIC.Binary.$2000.bin"), plan.descriptors[1].relativePath);
        Assert::IsFalse  (plan.descriptors[1].converted);
        Assert::AreEqual (std::string ("PIC"), plan.descriptors[1].catalogPath);

        Assert::IsTrue   (DragPayload::DecodeCatalogEntries (plan.privateBytes, image, kind, paths));
        Assert::AreEqual (std::string ("C:\\a.po"), image);
        Assert::IsTrue   (kind == VolumeKind::ProDos);
        Assert::AreEqual ((size_t) 2, paths.size());
        Assert::AreEqual (std::string ("PIC"), paths[1]);
    }



    TEST_METHOD (DiskImagesAndHostFiles_OfferOnlyHDrop)
    {
        DragPayload::Plan  images = DragPayload::Build (Source::DiskImages, "", VolumeKind::Unknown, {},
                                                        Style::Descriptive, nullptr, { L"C:\\a.dsk", L"C:\\b.woz" });
        DragPayload::Plan  files  = DragPayload::Build (Source::HostFiles, "", VolumeKind::Unknown, {},
                                                        Style::Descriptive, nullptr, { L"C:\\notes.txt" });

        Assert::AreEqual ((size_t) 1, images.formats.size());
        Assert::IsTrue   (Offers (images, Format::HDrop));
        Assert::AreEqual ((size_t) 2, images.hostPaths.size());
        Assert::IsTrue   (images.descriptors.empty());

        Assert::IsTrue   (Offers (files, Format::HDrop));
        Assert::IsFalse  (Offers (files, Format::CatalogEntries));
    }



    TEST_METHOD (HostNames_FollowTheNamingRuleAndTheStyle)
    {
        bool  converted = false;

        Assert::AreEqual (std::wstring (L"NOTES.Text.txt"),
                          DragPayload::GetHostName (MakeEntry ("NOTES", Dos33Volume::kTypeText), VolumeKind::Dos33, Style::CiderPress, converted));
        Assert::IsTrue (converted);

        Assert::AreEqual (std::wstring (L"INTPROG.Integer BASIC.txt"),
                          DragPayload::GetHostName (MakeEntry ("INTPROG", Dos33Volume::kTypeInteger), VolumeKind::Dos33, Style::Descriptive, converted));

        Assert::AreEqual (std::wstring (L"PIC#062000"),
                          DragPayload::GetHostName (MakeEntry ("PIC", ProDosVolume::kTypeBinary, false, true, 0x2000), VolumeKind::ProDos, Style::CiderPress, converted));
        Assert::IsFalse (converted);
    }



    TEST_METHOD (ADirectory_YieldsNestedDescriptorsWithRelativePaths)
    {
        std::vector<FileEntry>  entries = { MakeEntry ("SUBDIR", 0x0F, true) };
        DragPayload::Plan       plan;

        plan = DragPayload::Build (Source::CatalogEntries, "C:\\a.po", VolumeKind::ProDos, entries, Style::Descriptive,
                                   [] (const std::string & directoryPath, VolumeListing & outListing)
                                   {
                                       outListing = VolumeListing();

                                       if (directoryPath == "SUBDIR")
                                       {
                                           outListing.entries.push_back (MakeEntry ("INNER", 0x0F, true));
                                           outListing.entries.push_back (MakeEntry ("README", ProDosVolume::kTypeText));
                                       }
                                       else if (directoryPath == "SUBDIR/INNER")
                                       {
                                           outListing.entries.push_back (MakeEntry ("DEEP", ProDosVolume::kTypeBinary, false, true, 0x300));
                                       }

                                       return S_OK;
                                   },
                                   {});

        Assert::AreEqual ((size_t) 4, plan.descriptors.size());
        Assert::AreEqual (std::wstring (L"SUBDIR"),                            plan.descriptors[0].relativePath);
        Assert::IsTrue   (plan.descriptors[0].isDirectory);
        Assert::AreEqual (std::wstring (L"SUBDIR\\INNER"),                     plan.descriptors[1].relativePath);
        Assert::AreEqual (std::wstring (L"SUBDIR\\INNER\\DEEP.Binary.$0300.bin"), plan.descriptors[2].relativePath);
        Assert::AreEqual (std::string ("SUBDIR/INNER/DEEP"),                    plan.descriptors[2].catalogPath);
        Assert::AreEqual (std::wstring (L"SUBDIR\\README.Text.txt"),          plan.descriptors[3].relativePath);
    }



    TEST_METHOD (Folders_LandOnProDosAndNotOnDos33)
    {
        Assert::IsTrue  (DragPayload::CanReceiveFolder (VolumeKind::ProDos));
        Assert::IsFalse (DragPayload::CanReceiveFolder (VolumeKind::Dos33));
    }



    TEST_METHOD (PrivateFormat_RefusesWhatItDidNotWrite)
    {
        std::string               image;
        VolumeKind                kind = VolumeKind::Unknown;
        std::vector<std::string>  paths;

        Assert::IsFalse (DragPayload::DecodeCatalogEntries ("", image, kind, paths));
        Assert::IsFalse (DragPayload::DecodeCatalogEntries ("C:\\a.dsk\nhfs\nX\n", image, kind, paths));
        Assert::IsTrue  (DragPayload::DecodeCatalogEntries ("C:\\a.dsk\ndos33\n", image, kind, paths));
        Assert::IsTrue  (paths.empty());
        Assert::IsTrue  (kind == VolumeKind::Dos33);
    }
};
