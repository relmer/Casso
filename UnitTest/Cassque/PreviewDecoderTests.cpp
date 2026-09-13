#include "Pch.h"
#include "../EhmTestHelper.h"
#include "../EmuTests/FixtureProvider.h"
#include "Cassque/Model/PicturePreview.h"
#include "Cassque/Model/PreviewDecoder.h"
#include "Core/MemoryBus.h"
#include "Core/MemoryDevice.h"
#include "Devices/Disk/FilePath.h"
#include "Machines/Apple2/Common/Dos33Volume.h"
#include "Machines/Apple2/Common/ProDosVolume.h"
#include "Machines/Apple2/Common/VolumeImage.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  FailingMemoryDevice
//
//  Covers the whole address space and fails the test on any read, so the
//  picture path is proven never to touch the bus it is handed.
//
////////////////////////////////////////////////////////////////////////////////

class FailingMemoryDevice : public MemoryDevice
{
public:
    Byte  Read     (Word address) override            { UNREFERENCED_PARAMETER (address); Assert::Fail (L"the picture path read the bus"); return 0; }
    void  Write    (Word address, Byte value) override { UNREFERENCED_PARAMETER (address); UNREFERENCED_PARAMETER (value); }
    Word  GetStart() const override                   { return 0x0000; }
    Word  GetEnd()   const override                   { return 0xFFFF; }
    void  Reset()    override                         {}
};





////////////////////////////////////////////////////////////////////////////////
//
//  PreviewDecoderTests
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (PreviewDecoderTests)
{
public:

    using Kind = PreviewContent::Kind;
    using Mode = PicturePreview::Mode;



    static void LoadDos33 (vector<Byte> & outSectors, VolumeListing & outListing)
    {
        FixtureProvider     fixtures;
        vector<Byte>        bytes;
        SectorDecodeReport  report;

        AssertSucceeded (fixtures.OpenFixture ("Cassque/dos33.dsk", bytes));
        AssertSucceeded (VolumeImage::Load (bytes, "Cassque/dos33.dsk", outSectors, report));

        {
            Dos33Volume  volume (outSectors);

            AssertSucceeded (volume.Enumerate (outListing));
        }
    }



    static const FileEntry & FindEntry (const VolumeListing & listing, const char * name)
    {
        for (const FileEntry & entry : listing.entries)
        {
            if (entry.name == name)
            {
                return entry;
            }
        }

        Assert::Fail (L"entry not found");

        return listing.entries[0];
    }



    static void RenderDos33 (const char * name, bool disassemble, PreviewContent & outContent)
    {
        vector<Byte>         sectors;
        VolumeListing        listing;
        FilePayload          payload;
        MemoryBus            bus;
        FailingMemoryDevice  device;

        LoadDos33 (sectors, listing);

        {
            Dos33Volume  volume (sectors);

            AssertSucceeded (volume.Read (FilePath::Parse (name), payload));
        }

        bus.AddDevice (&device);

        AssertSucceeded (PreviewDecoder::Render (FindEntry (listing, name), VolumeKind::Dos33, payload,
                                                 disassemble, bus, outContent));
    }



    TEST_METHOD (Kind_FollowsTheTypeAndTheGraphicsRule)
    {
        PreviewContent  content;

        RenderDos33 ("HELLO", false, content);
        Assert::IsTrue (content.kind == Kind::Listing);
        Assert::AreEqual (std::wstring (L"10  PRINT \"HELLO, CASSQUE\""), content.lines[0]);

        RenderDos33 ("INTPROG", false, content);
        Assert::IsTrue (content.kind == Kind::Listing);
        Assert::AreEqual (std::wstring (L"15 DSP I"), content.lines[1]);

        RenderDos33 ("NOTES", false, content);
        Assert::IsTrue (content.kind == Kind::Text);
        Assert::AreEqual (std::wstring (L"CASSQUE TEST NOTES"), content.lines[0]);

        RenderDos33 ("PICTURE", false, content);
        Assert::IsTrue (content.kind == Kind::Picture);
        Assert::AreEqual (PicturePreview::kWidth,  content.width);
        Assert::AreEqual (PicturePreview::kHeight, content.height);

        RenderDos33 ("LORES", false, content);
        Assert::IsTrue (content.kind == Kind::Picture);

        RenderDos33 ("DHIRES", false, content);
        Assert::IsTrue (content.kind == Kind::Picture);

        RenderDos33 ("ODD", false, content);
        Assert::IsTrue (content.kind == Kind::Hex);
    }



    TEST_METHOD (GraphicsRule_EveryPairAndANearMiss)
    {
        Assert::IsTrue (PicturePreview::Choose (0x2000, 8192)   == Mode::HiRes);
        Assert::IsTrue (PicturePreview::Choose (0x4000, 8192)   == Mode::HiRes);
        Assert::IsTrue (PicturePreview::Choose (0x2000, 0x1FF8) == Mode::HiRes);
        Assert::IsTrue (PicturePreview::Choose (0x4000, 0x1FF8) == Mode::HiRes);
        Assert::IsTrue (PicturePreview::Choose (0x2000, 16384)  == Mode::DoubleHiRes);
        Assert::IsTrue (PicturePreview::Choose (0x0400, 1024)   == Mode::LoRes);
        Assert::IsTrue (PicturePreview::Choose (0x0800, 1024)   == Mode::LoRes);

        //  Off by a byte or an address is not a picture.
        Assert::IsTrue (PicturePreview::Choose (0x2000, 8191)   == Mode::None);
        Assert::IsTrue (PicturePreview::Choose (0x2001, 8192)   == Mode::None);
        Assert::IsTrue (PicturePreview::Choose (0x4000, 16384)  == Mode::None);
        Assert::IsTrue (PicturePreview::Choose (0x0803, 1024)   == Mode::None);
    }



    TEST_METHOD (Picture_RendersSomethingAndNeverReadsTheBus)
    {
        PreviewContent  content;
        bool            lit     = false;

        //  The failing device is attached inside RenderDos33; reaching here
        //  at all proves the bus was never read.
        RenderDos33 ("PICTURE", false, content);

        Assert::AreEqual ((size_t) PicturePreview::kWidth * PicturePreview::kHeight, content.bgra.size());

        for (uint32_t pixel : content.bgra)
        {
            lit = lit || (pixel & 0x00FFFFFF) != 0;
        }

        Assert::IsTrue (lit, L"a pattern of random bytes lights some pixels");
    }



    TEST_METHOD (Hex_CarriesTheBytesAndTheAddressTheyStartAt)
    {
        PreviewContent  content;

        RenderDos33 ("ODD", false, content);

        //  The bytes are passed unchanged, for a view that draws only the
        //  visible rows, rather than rendered into lines of text here.
        Assert::AreEqual ((size_t) 777,  content.bytes.size());
        Assert::AreEqual ((int) 0x0803, (int) content.origin,
            L"addressed from the file's load address");
        Assert::IsTrue   (content.lines.empty(),
            L"and no lines are rendered for it");
    }



    TEST_METHOD (Hex_DisassemblyToggleRendersMnemonicRows)
    {
        PreviewContent  content;

        RenderDos33 ("ODD", true, content);

        Assert::IsTrue (content.kind == Kind::Hex);
        Assert::IsTrue (!content.lines.empty());
        Assert::IsTrue (content.lines[0].rfind (L"0803  ", 0) == 0, L"the first line sits at the load address");
    }



    TEST_METHOD (Catalog_RowsForAnImage)
    {
        vector<Byte>    sectors;
        VolumeListing   listing;
        PreviewContent  content;

        LoadDos33 (sectors, listing);

        PreviewDecoder::RenderCatalog (listing, VolumeKind::Dos33, content);

        Assert::IsTrue   (content.kind == Kind::Catalog);
        Assert::AreEqual ((size_t) 7, content.rows.size());
        Assert::AreEqual (std::wstring (L"HELLO"), content.rows[0].name);
    }



    TEST_METHOD (Error_ADamagedProgramIsRefusedNotShownInPart)
    {
        vector<Byte>         sectors;
        VolumeListing        listing;
        FilePayload          applesoft;
        FilePayload          integer;
        PreviewContent       content;
        MemoryBus            bus;

        LoadDos33 (sectors, listing);

        {
            Dos33Volume  volume (sectors);

            AssertSucceeded (volume.Read (FilePath::Parse ("HELLO"), applesoft));
            AssertSucceeded (volume.Read (FilePath::Parse ("INTPROG"), integer));
        }

        //  Break the first line's link, which the detokenizer checks.
        applesoft.bytes[0] = 0x55;
        applesoft.bytes[1] = 0x55;

        AssertSucceeded (PreviewDecoder::Render (FindEntry (listing, "HELLO"), VolumeKind::Dos33, applesoft,
                                                 false, bus, content));

        Assert::IsTrue  (content.kind == Kind::Error);
        Assert::IsTrue  (content.lines.empty());
        Assert::IsFalse (content.message.empty());

        //  Cut the Integer program inside its last line.
        integer.bytes.resize (integer.bytes.size() - 2);

        AssertSucceeded (PreviewDecoder::Render (FindEntry (listing, "INTPROG"), VolumeKind::Dos33, integer,
                                                 false, bus, content));

        Assert::IsTrue   (content.kind == Kind::Error);
        Assert::AreEqual (integer.bytes.size() - 3, content.offset, L"the offset of the line that was cut");
    }
};
