#include "Pch.h"
#include "../EhmTestHelper.h"
#include "FakeDiskFileIo.h"
#include "Devices/Disk/Containers/DiskCopy42Container.h"
#include "Devices/Disk/Containers/TwoImgContainer.h"
#include "Devices/Disk/FilePath.h"
#include "Devices/Disk/Inspector/Blocks/BlockInspectorModel.h"
#include "Machines/Apple2/Common/HfsWriter.h"
#include "Machines/Apple2/Common/ProDosSkeleton.h"
#include "Machines/Apple2/Common/ProDosVolume.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  BlockInspectorModelTests
//
//  Block mode over image files in a fake file system: which files open in
//  it, stepping and following links with Back and Forward, highlighting a
//  file's blocks, the 2IMG and Disk Copy 4.2 headers, an Apple Partition
//  Map's bands and volumes, and edits written in place.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (BlockInspectorModelTests)
{
public:

    static constexpr size_t   kBlockBytes  = 512;
    static constexpr size_t   k800KBlocks  = 1600;
    static constexpr size_t   kSeedBytes   = 300;
    static constexpr Byte     kFill        = 0x5A;
    static constexpr int64_t  kTime        = 1'700'000'000;
    static constexpr Word     kLoadAddress = 0x2000;

    static vector<Byte> MakeProDos()
    {
        vector<Byte>  buffer (k800KBlocks * kBlockBytes, 0);
        vector<Byte>  next;
        FilePayload   payload;



        AssertSucceeded (ProDosSkeleton::Write (buffer, "BLOCKS"));

        payload.bytes.assign (kSeedBytes, kFill);
        payload.type           = ProDosVolume::kTypeBinary;
        payload.loadAddress    = kLoadAddress;
        payload.hasLoadAddress = true;

        AssertSucceeded (ProDosVolume (buffer).Write (FilePath::Parse ("SEED"), payload, next));

        return next;
    }


    static void Seed (FakeDiskFileIo & io, const std::string & path, const vector<Byte> & bytes)
    {
        io.files[path]  = bytes;
        io.stamps[path] = { bytes.size(), kTime };
    }


    static void OpenModel (FakeDiskFileIo & io, const std::string & path, BlockInspectorModel & model)
    {
        std::unique_ptr<BlockImageFile>  file;
        std::wstring                     error;



        AssertSucceeded (BlockImageFile::Open (io, path, file, error));
        AssertSucceeded (model.Open (std::move (file)));
    }


    static const BlockField * FindField (const vector<BlockField> & fields, const std::wstring & label)
    {
        for (const BlockField & field : fields)
        {
            if (field.label == label)
            {
                return &field;
            }
        }

        return nullptr;
    }


    static vector<BlockField> GetFields (BlockInspectorModel & model, uint32_t block)
    {
        BlockInspectorModel::Block  bytes = {};
        vector<BlockField>          fields;



        AssertSucceeded (model.ReadBlock (block, bytes));
        model.GetFields (block, bytes, fields);

        return fields;
    }


    TEST_METHOD (IsBlockImage_OpensLargeImagesAndLeavesFloppiesToTheTrackInspector)
    {
        FakeDiskFileIo  io;



        Seed (io, "big.po",    vector<Byte> (k800KBlocks * kBlockBytes));
        Seed (io, "small.po",  vector<Byte> (280 * kBlockBytes));
        Seed (io, "big.dsk",   vector<Byte> (k800KBlocks * kBlockBytes));

        Assert::IsTrue  (BlockImageFile::IsBlockImage (io, "big.po"));
        Assert::IsFalse (BlockImageFile::IsBlockImage (io, "small.po"));
        Assert::IsFalse (BlockImageFile::IsBlockImage (io, "big.dsk"));
    }


    TEST_METHOD (FollowLink_StepsToTheBlockAndBackAndForwardReturn)
    {
        FakeDiskFileIo       io;
        BlockInspectorModel  model;



        Seed (io, "vol.po", MakeProDos());
        OpenModel (io, "vol.po", model);

        Assert::AreEqual ((uint32_t) k800KBlocks, model.GetBlockCount());
        Assert::IsTrue (model.GetRole (2) == SectorRole::VtocOrKeyBlock);

        model.SelectBlock (2);

        Assert::IsTrue (model.TryFollowLink (2), L"the next-block pointer links to block 3");
        Assert::AreEqual ((uint32_t) 3, model.GetBlock());
        Assert::IsFalse (model.TryFollowLink (0x30), L"an entry's name links nowhere");

        Assert::IsTrue (model.GoBack());
        Assert::AreEqual ((uint32_t) 2, model.GetBlock());
        Assert::IsTrue (model.GoForward());
        Assert::AreEqual ((uint32_t) 3, model.GetBlock());

        model.StepBlock (-10);
        Assert::AreEqual ((uint32_t) 0, model.GetBlock());
        model.StepBlock (100'000);
        Assert::AreEqual ((uint32_t) k800KBlocks - 1, model.GetBlock());
    }


    TEST_METHOD (HighlightFileAt_MarksOnlyThatFilesBlocks)
    {
        FakeDiskFileIo       io;
        BlockInspectorModel  model;
        uint32_t             data  = 0;
        uint32_t             inner = 0;



        Seed (io, "vol.po", MakeProDos());
        OpenModel (io, "vol.po", model);

        for (data = 0; data < model.GetBlockCount() && model.GetOwners (data) != L"SEED"; data++)
        {
        }

        Assert::IsNotNull (model.FindMap (data, inner));
        Assert::IsTrue (model.HighlightFileAt (data));
        Assert::AreEqual (std::wstring (L"SEED"), model.GetHighlightedPath());
        Assert::IsTrue (model.IsHighlighted (data));
        Assert::IsFalse (model.IsHighlighted (2));
        Assert::IsFalse (model.HighlightFileAt (2), L"no file uses the volume directory");
    }


    TEST_METHOD (ApplyEdits_WritesTheBlockInPlaceAndUndoWorksBefore)
    {
        FakeDiskFileIo       io;
        BlockInspectorModel  model;
        std::wstring         error;
        constexpr size_t     kAt = 100;



        Seed (io, "vol.po", MakeProDos());
        OpenModel (io, "vol.po", model);

        model.SelectBlock (300);

        Assert::IsTrue (model.TypeHexDigit ((int) kAt, true, L'A'));
        Assert::IsTrue (model.TypeHexDigit ((int) kAt, false, L'5'));
        Assert::IsTrue (model.GetEdits().IsChanged (300, (int) kAt));
        Assert::AreEqual (1, model.GetEdits().CountChangedBytes());

        Assert::IsTrue (model.Undo());
        Assert::IsTrue (model.Redo());

        AssertSucceeded (model.ApplyEdits (error));

        Assert::AreEqual ((Byte) 0xA5, io.files["vol.po"][300 * kBlockBytes + kAt]);
        Assert::IsTrue (model.GetEdits().IsEmpty());
    }


    TEST_METHOD (ApplyEdits_WritesNothingWhenTheFileChanged)
    {
        FakeDiskFileIo       io;
        BlockInspectorModel  model;
        std::wstring         error;



        Seed (io, "vol.po", MakeProDos());
        OpenModel (io, "vol.po", model);

        model.SelectBlock (300);
        Assert::IsTrue (model.TypeCharacter (0, L'Q'));

        io.mutateStampOnNextStat = true;

        AssertFailed (model.ApplyEdits (error));

        Assert::IsFalse (error.empty());
        Assert::AreEqual ((Byte) 0, io.files["vol.po"][300 * kBlockBytes]);
        Assert::IsFalse (model.GetEdits().IsEmpty(), L"the edits stay pending");
    }


    TEST_METHOD (TwoImg_HeaderIsAnnotatedAndBlocksAreThePayloads)
    {
        FakeDiskFileIo       io;
        BlockInspectorModel  model;
        vector<Byte>         volume = MakeProDos();
        vector<Byte>         file;
        TwoImgHeader         header;
        vector<BlockField>   fields;



        header.blockCount = (uint32_t) k800KBlocks;
        TwoImgContainer::Build (header, volume, file);

        Seed (io, "vol.2mg", file);
        Assert::IsTrue (BlockImageFile::IsBlockImage (io, "vol.2mg"));

        OpenModel (io, "vol.2mg", model);

        Assert::IsTrue (model.GetFile().GetKind() == BlockContainerKind::TwoImg);
        Assert::IsTrue (model.GetRole (2) == SectorRole::VtocOrKeyBlock);

        model.GetFile().AnnotateHeader (fields);

        Assert::IsNotNull (FindField (fields, L"2IMG: blocks"));
        Assert::AreEqual (std::wstring (L"$00000640 (1600)"), FindField (fields, L"2IMG: blocks")->value);
        Assert::AreEqual (std::wstring (L"\"2IMG\""), FindField (fields, L"2IMG: magic")->value);
    }


    TEST_METHOD (DiskCopy_ChecksumsAreCheckedAndTheDataSumIsWrittenAfterAnEdit)
    {
        FakeDiskFileIo       io;
        BlockInspectorModel  model;
        vector<Byte>         volume = MakeProDos();
        vector<Byte>         file;
        DiskCopy42Header     header;
        vector<BlockField>   fields;
        std::wstring         error;



        header.name = "Blocks";
        header.tags.assign (k800KBlocks * DiskCopy42Container::kTagBytesPerBlock, 0x11);
        DiskCopy42Container::Build (header, volume, file);

        Seed (io, "vol.dc42", file);
        OpenModel (io, "vol.dc42", model);

        model.GetFile().AnnotateHeader (fields);

        Assert::IsTrue (FindField (fields, L"Disk Copy: data checksum")->value.ends_with (L", matches"));
        Assert::IsTrue (FindField (fields, L"Disk Copy: tag checksum")->value.ends_with (L", matches"));
        Assert::AreEqual (std::wstring (L"\"Blocks\""), FindField (fields, L"Disk Copy: disk name")->value);

        model.SelectBlock (400);
        Assert::IsTrue (model.TypeHexDigit (0, true, L'F'));
        AssertSucceeded (model.ApplyEdits (error));

        model.GetFile().AnnotateHeader (fields);

        Assert::IsTrue (FindField (fields, L"Disk Copy: data checksum")->value.ends_with (L", matches"));
        Assert::AreEqual ((Byte) 0xF0, io.files["vol.dc42"][84 + 400 * kBlockBytes]);
    }


    TEST_METHOD (PartitionMap_HasBandsAndAVolumeOverTheOuterBlocks)
    {
        constexpr uint32_t  kFirst  = 64;
        constexpr uint32_t  kBlocks = kFirst + (uint32_t) k800KBlocks;

        FakeDiskFileIo       io;
        BlockInspectorModel  model;
        vector<Byte>         disk ((size_t) kBlocks * kBlockBytes, 0);
        vector<Byte>         hfs;
        vector<BlockField>   fields;
        const BlockField   * first = nullptr;

        auto  putLong = [&] (size_t at, uint32_t value) { disk[at] = (Byte) (value >> 24); disk[at + 1] = (Byte) (value >> 16); disk[at + 2] = (Byte) (value >> 8); disk[at + 3] = (Byte) value; };
        auto  putText = [&] (size_t at, const char * text) { std::copy (text, text + strlen (text), disk.begin() + (ptrdiff_t) at); };



        disk[0] = 'E'; disk[1] = 'R'; disk[2] = 0x02; disk[3] = 0x00;
        putLong (4, kBlocks);

        for (uint32_t entry = 1; entry <= 2; entry++)
        {
            size_t  at = entry * kBlockBytes;

            disk[at] = 'P'; disk[at + 1] = 'M';
            putLong (at + 4, 2);
        }

        putLong (kBlockBytes + 8, 1);
        putLong (kBlockBytes + 12, 63);
        putText (kBlockBytes + 16, "Apple");
        putText (kBlockBytes + 48, "Apple_partition_map");

        putLong (2 * kBlockBytes + 8, kFirst);
        putLong (2 * kBlockBytes + 12, (uint32_t) k800KBlocks);
        putText (2 * kBlockBytes + 16, "Untitled");
        putText (2 * kBlockBytes + 48, "Apple_HFS");

        AssertSucceeded (HfsWriter::Format (hfs, k800KBlocks, "Untitled"));
        std::copy (hfs.begin(), hfs.end(), disk.begin() + (ptrdiff_t) kFirst * kBlockBytes);

        Seed (io, "disk.hd", disk);
        OpenModel (io, "disk.hd", model);

        Assert::AreEqual ((size_t) 3, model.GetBands().size());
        Assert::AreEqual (std::wstring (L"Untitled (Apple_HFS)"), model.GetBands()[2].label);
        Assert::AreEqual (0, model.GetBands()[2].volume);
        Assert::IsTrue (model.GetRole (1) == SectorRole::VtocOrKeyBlock);
        Assert::IsTrue (model.GetRole (kFirst + 2) == SectorRole::VtocOrKeyBlock);

        fields = GetFields (model, 2);
        first  = FindField (fields, L"Partition map: first block");

        Assert::IsNotNull (first);
        Assert::AreEqual ((int64_t) kFirst, first->link);

        fields = GetFields (model, kFirst + 2);

        Assert::IsTrue (FindField (fields, L"Catalog file, extent 1 start")->link > (int64_t) kFirst, L"a link inside the partition is in the image's block numbers");
    }
};
