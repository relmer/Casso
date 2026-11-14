#include "Pch.h"
#include "../EhmTestHelper.h"
#include "Devices/Disk/DiskImageSession.h"
#include "../EmuTests/FixtureProvider.h"
#include "CassoExplorer/CrtPictureRenderer.h"
#include "CassoExplorer/Model/PicturePreview.h"
#include "CassoExplorer/Model/PreviewDecoder.h"
#include "CassoExplorer/Model/PreviewText.h"
#include "Core/MemoryBus.h"
#include "Core/MemoryDevice.h"
#include "Devices/Printer/PngCodec.h"
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

        AssertSucceeded (fixtures.OpenFixture ("CassoExplorer/dos33.dsk", bytes));
        AssertSucceeded (VolumeImage::Load (bytes, "CassoExplorer/dos33.dsk", outSectors, report));

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



    static void RenderDos33 (const char * name, PreviewDecoder::HexForm hexForm, PreviewContent & outContent)
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
                                                 hexForm, bus, outContent));
    }



    TEST_METHOD (Kind_FollowsTheTypeAndTheGraphicsRule)
    {
        PreviewContent  content;

        RenderDos33 ("HELLO", PreviewDecoder::HexForm::Auto, content);
        Assert::IsTrue (content.kind == Kind::Listing);
        Assert::AreEqual (std::wstring (L"10  PRINT \"HELLO, CASSQUE\""), content.lines[0]);

        RenderDos33 ("INTPROG", PreviewDecoder::HexForm::Auto, content);
        Assert::IsTrue (content.kind == Kind::Listing);
        Assert::AreEqual (std::wstring (L"15 DSP I"), content.lines[1]);

        //  Text is lines, the high bit cleared.
        RenderDos33 ("NOTES", PreviewDecoder::HexForm::Auto, content);
        Assert::IsTrue (content.kind == Kind::Text);
        Assert::IsTrue (!content.lines.empty() && content.lines[0].starts_with (L"C"),
            L"The first line is the file's, without the high bit Apple text uses");

        RenderDos33 ("PICTURE", PreviewDecoder::HexForm::Auto, content);
        Assert::IsTrue (content.kind == Kind::Picture);
        Assert::AreEqual (PicturePreview::kWidth,  content.width);
        Assert::AreEqual (PicturePreview::kHeight, content.height);

        RenderDos33 ("LORES", PreviewDecoder::HexForm::Auto, content);
        Assert::IsTrue (content.kind == Kind::Picture);

        RenderDos33 ("DHIRES", PreviewDecoder::HexForm::Auto, content);
        Assert::IsTrue (content.kind == Kind::Picture);

        RenderDos33 ("ODD", PreviewDecoder::HexForm::Auto, content);
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



    //  A monochrome monitor draws hi-res as its bits, then in its own color,
    //  as Casso's screen does; the color monitor draws the colors.
    TEST_METHOD (Picture_OnAMonochromeMonitor_IsItsBitsInThatColor)
    {
        std::vector<Byte>      bytes (8192, 0);
        std::vector<uint32_t>  color;
        std::vector<uint32_t>  green;
        std::vector<uint32_t>  amber;
        MemoryBus              bus;
        int                    width  = 0;
        int                    height = 0;
        bool                   tinted = true;
        bool                   lit    = false;

        //  Alternate dots on the first row: violet on a color monitor.
        bytes[0] = 0x55;
        bytes[1] = 0x2A;

        AssertSucceeded (PicturePreview::Render (Mode::HiRes, bytes, 0x2000, bus, color, width, height, PicturePreview::Monitor::Color));
        AssertSucceeded (PicturePreview::Render (Mode::HiRes, bytes, 0x2000, bus, green, width, height, PicturePreview::Monitor::Green));
        AssertSucceeded (PicturePreview::Render (Mode::HiRes, bytes, 0x2000, bus, amber, width, height, PicturePreview::Monitor::Amber));

        for (uint32_t pixel : green)
        {
            tinted = tinted && ((pixel >> 16) & 0xFF) == 0 && (pixel & 0xFF) == 0;
            lit    = lit || ((pixel >> 8) & 0xFF) != 0;
        }

        Assert::IsTrue  (tinted, L"Green has no red and no blue");
        Assert::IsTrue  (lit,    L"and the bits are lit");
        Assert::IsTrue  (((color[0] >> 16) & 0xFF) != 0 || (color[0] & 0xFF) != 0 || ((color[1] >> 16) & 0xFF) != 0, L"The color monitor has the violet");
        Assert::AreEqual (0u, amber[0] & 0xFF, L"Amber has no blue");
    }



    //  The custom monitor draws the bits in the color picked for it, each
    //  pixel scaled by its brightness, and its effects start from white's.
    TEST_METHOD (Picture_OnTheCustomMonitor_IsItsBitsInItsColor)
    {
        std::vector<Byte>      bytes (8192, 0);
        std::vector<uint32_t>  white;
        std::vector<uint32_t>  custom;
        MemoryBus              bus;
        int                    width   = 0;
        int                    height  = 0;
        bool                   matched = true;
        bool                   lit     = false;

        bytes[0] = 0x55;
        bytes[1] = 0x2A;

        AssertSucceeded (PicturePreview::Render (Mode::HiRes, bytes, 0x2000, bus, white,  width, height, PicturePreview::Monitor::White));
        AssertSucceeded (PicturePreview::Render (Mode::HiRes, bytes, 0x2000, bus, custom, width, height, PicturePreview::Monitor::Custom, 0xFF8040FF));

        for (size_t i = 0; i < white.size(); i++)
        {
            uint32_t  level = (white[i] >> 8) & 0xFF;

            matched = matched && ((custom[i] >> 16) & 0xFF) == level * 0x80 / 0xFF
                              && ((custom[i] >>  8) & 0xFF) == level * 0x40 / 0xFF
                              && ( custom[i]        & 0xFF) == level;
            lit     = lit || level != 0;
        }

        Assert::IsTrue (lit,     L"The bits are lit");
        Assert::IsTrue (matched, L"each in the picked color, by the white monitor's brightness");
        Assert::AreEqual ((size_t) PicturePreview::Monitor::White, PicturePreview::GetPresetIndex (PicturePreview::Monitor::Custom));
    }



    //  A picture drawn through the CRT pass is given a black margin as wide
    //  as its bloom and color bleed sample past it, in drawn pixels, and none
    //  with both off.
    TEST_METHOD (CrtPicture_MarginCoversTheGlow)
    {
        CrtParams  params;

        Assert::AreEqual (0, CrtPictureRenderer::GetGlowMargin (params, 1.0f, 1.0f), L"No glow, no margin");

        params.bloomStrength = 0.5f;
        params.bloomRadius   = 2.0f;
        Assert::AreEqual (9, CrtPictureRenderer::GetGlowMargin (params, 1.0f, 1.0f), L"Four steps of two pixels, and one");

        params.colorBleedWidth = 1.5f;
        Assert::AreEqual (11, CrtPictureRenderer::GetGlowMargin (params, 1.0f, 1.0f), L"and the bleed's two pixels across");
        Assert::AreEqual (21, CrtPictureRenderer::GetGlowMargin (params, 2.0f, 2.0f), L"twice as wide at twice the size");

        params.bloomStrength = 0.0f;
        Assert::AreEqual (3, CrtPictureRenderer::GetGlowMargin (params, 1.0f, 1.0f), L"The bleed alone, with the bloom off");
    }



    TEST_METHOD (DoubleHiRes_NamedA2fm_IsMonochrome)
    {
        std::vector<Byte>      bytes (16384, 0);
        std::vector<uint32_t>  bgra;
        MemoryBus              bus;
        int                    width  = 0;
        int                    height = 0;

        Assert::IsTrue (PicturePreview::Choose (0x2000, 16384, "TITLE.A2FM") == Mode::DoubleHiResMono, L"The A2FM name is the monochrome form");
        Assert::IsTrue (PicturePreview::Choose (0x2000, 16384, "title.a2fm") == Mode::DoubleHiResMono, L"in either case");
        Assert::IsTrue (PicturePreview::Choose (0x2000, 16384, "TITLE.A2FC") == Mode::DoubleHiRes,     L"and A2FC, or no suffix, is color");

        //  The first row's first auxiliary byte lights its low bit, the first
        //  dot; the first main byte lights its low bit, the eighth.
        bytes[0]    = 0x01;
        bytes[8192] = 0x01;

        AssertSucceeded (PicturePreview::Render (Mode::DoubleHiResMono, bytes, 0x2000, bus, bgra, width, height));

        Assert::AreEqual (0xFFFFFFFFu, bgra[0],  L"Auxiliary first");
        Assert::AreEqual (0xFF000000u, bgra[1]);
        Assert::AreEqual (0xFFFFFFFFu, bgra[7],  L"then main, seven dots to a byte");
        Assert::AreEqual (0xFFFFFFFFu, bgra[(size_t) PicturePreview::kWidth], L"each row drawn twice");
    }



    //  A picture named neither .A2FC nor .A2FM is told by its dots: color's
    //  four-dot cells repeat every four, and monochrome art does not.
    static std::vector<Byte> MakeDoubleHiRes (bool color)
    {
        std::vector<Byte>  bytes (16384, 0);
        uint32_t           seed  = 12345;

        for (int y = 0; y < 192; y++)
        {
            size_t  row = (size_t) (((y & 7) << 10) | (((y >> 3) & 7) << 7)) + (size_t) ((y >> 6) * 40);
            int     dot = 0;

            for (int col = 0; col < 40; col++)
            {
                for (size_t bank : { (size_t) 0, (size_t) 8192 })
                {
                    Byte  value = 0;

                    for (int bit = 0; bit < 7; bit++, dot++)
                    {
                        seed = seed * 1103515245u + 12345u;

                        //  Color: the nibble 0011 across every cell; mono: noise.
                        int  lit = color ? ((dot % 4) < 2 ? 1 : 0) : (int) ((seed >> 16) & 1);

                        value |= (Byte) (lit << bit);
                    }

                    bytes[row + (size_t) col + bank] = value;
                }
            }
        }

        return bytes;
    }


    TEST_METHOD (DoubleHiRes_UnnamedIsToldByItsPeriodFourRing)
    {
        std::vector<Byte>  color = MakeDoubleHiRes (true);
        std::vector<Byte>  mono  = MakeDoubleHiRes (false);

        Assert::IsTrue (PicturePreview::MeasurePeriodFour (color) >= PicturePreview::kColorPeriodicity, L"Color's cells ring at period four");
        Assert::IsTrue (PicturePreview::MeasurePeriodFour (mono)  <  PicturePreview::kColorPeriodicity, L"and monochrome art does not");

        Assert::IsTrue (PicturePreview::Choose (0x2000, 16384, "PICTURE", color) == Mode::DoubleHiRes);
        Assert::IsTrue (PicturePreview::Choose (0x2000, 16384, "PICTURE", mono)  == Mode::DoubleHiResMono);
        Assert::IsTrue (PicturePreview::Choose (0x2000, 16384, "PICTURE.A2FC", mono) == Mode::DoubleHiRes, L"and the name, when it says, wins");
    }



    TEST_METHOD (Picture_RendersSomethingAndNeverReadsTheBus)
    {
        PreviewContent  content;
        bool            lit     = false;

        //  The failing device is attached inside RenderDos33; reaching here
        //  at all proves the bus was never read.
        RenderDos33 ("PICTURE", PreviewDecoder::HexForm::Auto, content);

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

        RenderDos33 ("ODD", PreviewDecoder::HexForm::Bytes, content);

        //  The bytes are passed unchanged, for a view that draws only the
        //  visible rows, rather than rendered into lines of text here.
        Assert::AreEqual ((size_t) 777,  content.bytes.size());
        Assert::AreEqual ((int) 0x0803, (int) content.origin,
            L"addressed from the file's load address");
        Assert::IsTrue   (content.lines.empty(),
            L"and no lines are rendered for it");
    }



    TEST_METHOD (Hex_ABinaryOfDataOpensAsBytes_AndOfCodeAsADisassembly)
    {
        PreviewContent  content;
        vector<Byte>    sectors;
        VolumeListing   listing;
        FilePayload     payload;
        MemoryBus       bus;

        RenderDos33 ("ODD", PreviewDecoder::HexForm::Auto, content);

        Assert::IsTrue  (content.kind == Kind::Hex);
        Assert::IsFalse (content.disassembly, L"ODD's bytes are random, not code");
        Assert::IsTrue  (content.executable);

        LoadDos33 (sectors, listing);

        payload.bytes          = { 0xA9, 0x00, 0x85, 0xFB, 0x20, 0x58, 0xFC, 0xAD, 0x00, 0xC0, 0x10, 0xFB, 0x8D, 0x10, 0xC0, 0x60 };
        payload.loadAddress    = 0x0803;
        payload.hasLoadAddress = true;

        AssertSucceeded (PreviewDecoder::Render (FindEntry (listing, "ODD"), VolumeKind::Dos33, payload, PreviewDecoder::HexForm::Auto, bus, content));

        Assert::IsTrue (content.kind == Kind::Hex);
        Assert::IsTrue (content.disassembly);
        Assert::IsTrue (content.bytes.empty());
        Assert::IsTrue (!content.lines.empty());
        Assert::IsTrue (content.lines[0].rfind (L"0803  ", 0) == 0, L"the first line sits at the load address");

        payload.loadAddress = 0x0000;

        AssertSucceeded (PreviewDecoder::Render (FindEntry (listing, "ODD"), VolumeKind::Dos33, payload, PreviewDecoder::HexForm::Auto, bus, content));

        Assert::IsFalse (content.disassembly, L"Code loaded over the zero page is data");
        Assert::IsTrue  (content.executable);

        payload.loadAddress = 0xC080;

        AssertSucceeded (PreviewDecoder::Render (FindEntry (listing, "ODD"), VolumeKind::Dos33, payload, PreviewDecoder::HexForm::Auto, bus, content));

        Assert::IsFalse (content.disassembly, L"and so is code loaded into the I/O space");

        AssertSucceeded (PreviewDecoder::Render (FindEntry (listing, "ODD"), VolumeKind::Dos33, payload, PreviewDecoder::HexForm::Disassembly, bus, content));

        Assert::IsTrue (content.disassembly, L"unless the disassembly is chosen");
    }



    //  BASIC.SYSTEM loads a CMD file at $4000 and runs it.
    TEST_METHOD (Command_DisassemblesAtItsLoadAddress)
    {
        PreviewContent  content;
        FileEntry       entry;
        FilePayload     payload;
        MemoryBus       bus;

        entry.name    = "BEEP";
        entry.type    = ProDosVolume::kTypeCommand;
        payload.bytes = { 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07 };

        AssertSucceeded (PreviewDecoder::Render (entry, VolumeKind::ProDos, payload, PreviewDecoder::HexForm::Auto, bus, content));

        Assert::IsTrue (content.kind == Kind::Hex);
        Assert::IsTrue (content.executable,  L"A CMD file is 6502 code");
        Assert::IsTrue (content.disassembly, L"and always opens as a disassembly");
        Assert::IsTrue (content.lines[0].rfind (L"4000  ", 0) == 0, L"at $4000");
    }



    //  An EDASM REL file: its code's length, the code, a relocation record for
    //  the JSR's operand, the record list's ending zero, and an external
    //  symbol dictionary.
    TEST_METHOD (Relocatable_DisassemblesItsCodeAndListsItsRelocationRecords)
    {
        std::vector<Byte>          bytes = { 0x04, 0x00, 0x20, 0x03, 0x10, 0x60, 0x81, 0x01, 0x00, 0x00, 0x00, 0xC1, 0x00 };
        std::vector<std::wstring>  lines;

        PreviewDecoder::RenderDisassembly (bytes, 0x1000, VolumeKind::ProDos, true, lines);

        Assert::AreEqual ((size_t) 5, lines.size());
        Assert::AreEqual (std::wstring (L"1000  20 03 10  JSR $1003  ; relocated"),                 lines[0], L"The code starts past its length, at its origin");
        Assert::AreEqual (std::wstring (L"1003  60        RTS"),                                   lines[1]);
        Assert::AreEqual (std::wstring (L"1004            DFB $81,$01,$00,$00  ; relocation record"), lines[2]);
        Assert::AreEqual (std::wstring (L"1008            DFB $00  ; end of relocation records"),     lines[3]);
        Assert::AreEqual (std::wstring (L"1009            DFB $C1,$00  ; external symbols"),          lines[4]);
    }



    //  A font's header and width table, as MousePaint's TORONTO starts, is
    //  data; a loop of loads, stores and calls is code.
    TEST_METHOD (LooksLikeCode_TellsAProgramFromAFont)
    {
        std::vector<Byte>  font = { 0x80, 0x7F, 0x0B, 0x01, 0x01, 0x01, 0x07, 0x01, 0x05, 0x03, 0x06, 0x0A, 0x09, 0x09, 0x03, 0x04,
                                    0x04, 0x08, 0x08, 0x03, 0x07, 0x03, 0x06, 0x07, 0x0C, 0x09, 0x0A, 0x0E, 0x08, 0x02, 0x0F, 0x0B };
        std::vector<Byte>  code = { 0xA9, 0x00, 0x85, 0xFB, 0x20, 0x58, 0xFC, 0xA2, 0x10, 0xBD, 0x00, 0x20, 0x9D, 0x00, 0x04, 0xCA,
                                    0xD0, 0xF7, 0xAD, 0x00, 0xC0, 0x10, 0xFB, 0x8D, 0x10, 0xC0, 0x4C, 0x00, 0x08, 0x60 };

        Assert::IsFalse (PreviewDecoder::LooksLikeCode (font, 0x0000));
        Assert::IsTrue  (PreviewDecoder::LooksLikeCode (code, 0x0800));
    }



    TEST_METHOD (Disassembly_NamesTheMonitorsEntryPoints)
    {
        std::vector<Byte>          bytes = { 0x20, 0x58, 0xFC };
        std::vector<std::wstring>  lines;

        PreviewDecoder::RenderDisassembly (bytes, 0x0800, VolumeKind::Dos33, false, lines);

        Assert::AreEqual (std::wstring (L"0800  20 58 FC  JSR HOME"), lines[0]);
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
                                                 PreviewDecoder::HexForm::Auto, bus, content));

        Assert::IsTrue  (content.kind == Kind::Error);
        Assert::IsTrue  (content.lines.empty());
        Assert::IsFalse (content.message.empty());

        //  Cut the Integer program inside its last line.
        integer.bytes.resize (integer.bytes.size() - 2);

        AssertSucceeded (PreviewDecoder::Render (FindEntry (listing, "INTPROG"), VolumeKind::Dos33, integer,
                                                 PreviewDecoder::HexForm::Auto, bus, content));

        Assert::IsTrue   (content.kind == Kind::Error);
        Assert::AreEqual (integer.bytes.size() - 3, content.offset, L"the offset of the line that was cut");
    }



    TEST_METHOD (Catalog_Dos33ReadsAsCatalogPrintsIt)
    {
        VolumeListing   listing;
        PreviewContent  content;
        FileEntry       hello;
        FileEntry       loader;

        listing.volumeNumber    = 254;
        listing.hasVolumeNumber = true;
        listing.totalUnits      = 560;
        listing.freeUnits       = 283;

        hello.name       = "HELLO";
        hello.type       = 0x02;
        hello.isLocked   = true;
        hello.sizeUnits  = 3;
        loader.name      = "LOADER.OBJ0";
        loader.type      = 0x04;
        loader.sizeUnits = 6;

        listing.entries = { hello, loader };

        PreviewDecoder::RenderCatalog (listing, VolumeKind::Dos33, content);

        Assert::IsTrue   (content.kind == Kind::Catalog);
        Assert::AreEqual ((size_t) 6, content.lines.size());
        Assert::AreEqual (std::wstring (L"DOS 3.3 disk"),           content.heading);
        Assert::AreEqual (std::wstring (L"DISK VOLUME 254"),        content.lines[0]);
        Assert::IsTrue   (content.lines[1].empty());
        Assert::AreEqual (std::wstring (L"*A 003 HELLO"),           content.lines[2]);
        Assert::AreEqual (std::wstring (L" B 006 LOADER.OBJ0"),     content.lines[3]);
        Assert::IsTrue   (content.lines[4].empty());
        Assert::AreEqual (std::wstring (L"283 sectors free of 560"), content.lines[5]);
    }



    TEST_METHOD (Catalog_ProDosReadsAsCatPrintsIt)
    {
        VolumeListing   listing;
        PreviewContent  content;
        FileEntry       hello;
        FileEntry       subdir;

        listing.volumeName    = "CASSQUE";
        listing.hasVolumeName = true;
        listing.totalUnits    = 280;
        listing.freeUnits     = 214;

        hello.name         = "HELLO";
        hello.type         = 0xFC;
        hello.sizeUnits    = 1;
        hello.hasModified  = true;
        hello.modifiedUnix = 461548800;   // 17 August 1984
        subdir.name        = "SUBDIR";
        subdir.type        = 0x0F;
        subdir.isDirectory = true;
        subdir.sizeUnits   = 1;

        listing.entries = { hello, subdir };

        PreviewDecoder::RenderCatalog (listing, VolumeKind::ProDos, content);

        Assert::AreEqual ((size_t) 8, content.lines.size());
        Assert::AreEqual (std::wstring (L"ProDOS disk"), content.heading);
        Assert::AreEqual (std::wstring (L"/CASSQUE"), content.lines[0]);
        Assert::AreEqual (std::wstring (L" NAME" L"           " L"TYPE  BLOCKS  MODIFIED"), content.lines[2]);
        Assert::AreEqual (std::wstring (L" HELLO" L"           " L"BAS" L"       " L"1  17-AUG-84"), content.lines[4]);
        Assert::AreEqual (std::wstring (L" SUBDIR" L"          " L"DIR" L"       " L"1  <NO DATE>"), content.lines[5]);
        Assert::AreEqual (std::wstring (L"BLOCKS FREE:  214     BLOCKS USED:   66"), content.lines[7]);
    }



    TEST_METHOD (Catalog_PascalAndCpm_StartWithTheirFileSystem)
    {
        VolumeListing   listing;
        PreviewContent  content;

        PreviewDecoder::RenderCatalog (listing, VolumeKind::Pascal, content);
        Assert::AreEqual (std::wstring (L"Apple Pascal disk"), content.heading);

        PreviewDecoder::RenderCatalog (listing, VolumeKind::Cpm, content);
        Assert::AreEqual (std::wstring (L"CP/M disk"), content.heading);
    }



    TEST_METHOD (Catalog_HeadingIsDrawnInTheMutedColorAndOnlyThere)
    {
        constexpr uint32_t              kMuted = 0xFF808080;
        VolumeListing                   listing;
        PreviewContent                  content;
        SourceSyntax::Colors            colors;
        std::vector<DxuiTextView::Row>  rows;

        colors.address = kMuted;

        PreviewDecoder::RenderCatalog (listing, VolumeKind::ProDos, content);

        rows = PreviewText::BuildTextRows (content, false, colors);

        Assert::AreEqual (std::wstring (L"ProDOS disk"), rows[0].cells[0]);
        Assert::AreEqual ((size_t) 1, rows[0].spans.size());
        Assert::AreEqual (kMuted, rows[0].spans[0].argb);
        Assert::AreEqual ((int) content.heading.size(), rows[0].spans[0].length);
        Assert::IsTrue   (rows[1].cells[0].empty(), L"A blank row sets it apart");
        Assert::AreEqual (content.lines[0], rows[2].cells[0]);
        Assert::IsTrue   (rows[2].spans.empty(), L"The contents keep the normal color");
    }



    TEST_METHOD (NoHeading_NoHeadingRows)
    {
        PreviewContent                  content;
        std::vector<DxuiTextView::Row>  rows;

        content.kind  = Kind::Text;
        content.lines = { L"ONE" };

        rows = PreviewText::BuildTextRows (content, false);

        Assert::AreEqual ((size_t) 1, rows.size());
        Assert::AreEqual (std::wstring (L"ONE"), rows[0].cells[0]);
    }



    TEST_METHOD (Details_ReadFromTheRunnersLabeledLines)
    {
        std::vector<std::pair<std::wstring, std::wstring>>  details;
        std::string                                         message;

        message = "C:\\disks\\x.dsk: does not have a DOS or ProDOS file system\n"
                  "  geometry      35 tracks, 16 sectors, 256 bytes per sector\n"
                  "  boot sector   this disk is bootable (sector 0 contains boot code)\n"
                  "  requires_machine  2+\n";

        Assert::IsTrue   (PreviewDecoder::ParseDetails (message, details));
        Assert::AreEqual ((size_t) 4, details.size(), L"The headline naming the image is not a detail");
        Assert::AreEqual (std::wstring (L"File system"), details[0].first);
        Assert::AreEqual (std::wstring (L"Geometry"), details[1].first);
        Assert::AreEqual (std::wstring (L"35 tracks, 16 sectors, 256 bytes per sector"), details[1].second);
        Assert::AreEqual (std::wstring (L"Boot sector"), details[2].first);
        Assert::AreEqual (std::wstring (L"This disk is bootable (sector 0 contains boot code)"), details[2].second, L"A value starts with a capital");
        Assert::AreEqual (std::wstring (L"Requires_machine"), details[3].first, L"A label wider than the column ends at its gap");
        Assert::AreEqual (std::wstring (L"2+"), details[3].second);

        Assert::IsFalse  (PreviewDecoder::ParseDetails ("C:\\disks\\x.dsk: cannot be read\n", details), L"A plain failure has no details");
    }



    //  Under the custom keys' heading, a key and its value are as the image
    //  has them, with no capital added and no underscore changed.
    TEST_METHOD (Details_CustomKeys_AreAsStored)
    {
        std::vector<std::pair<std::wstring, std::wstring>>  details;
        std::string                                         message;

        message = "C:\\disks\\x.woz: does not have a DOS or ProDOS file system\n"
                + DiskImageSession::FormatDetailHeading ("WOZ metadata")
                + DiskImageSession::FormatDetailLine ("title", "choplifter")
                + DiskImageSession::FormatDetailHeading (DiskImageSession::kCustomHeading)
                + DiskImageSession::FormatDetailLine ("requires_rom", "any|either");

        Assert::IsTrue   (PreviewDecoder::ParseDetails (message, details));
        auto  has = [&details] (const wchar_t * label, const wchar_t * value) { return std::find (details.begin(), details.end(), std::pair<std::wstring, std::wstring> { label, value }) != details.end(); };

        Assert::IsTrue (has (L"Title", L"choplifter"),      L"a standard key starts with a capital, and its value is as stored");
        Assert::IsTrue (has (L"Custom", L""),               L"the custom keys' heading");
        Assert::IsTrue (has (L"requires_rom", L"any|either"), L"and a custom key and value as stored");
    }


    //  A value of several lines has each line after the first in the value
    //  column, and each is a separate row with no label.
    TEST_METHOD (Details_ReadAValueOfSeveralLines)
    {
        std::vector<std::pair<std::wstring, std::wstring>>  details;
        std::string                                         message;

        message = "C:\\disks\\x.woz: does not have a DOS or ProDOS file system\n"
                + DiskImageSession::FormatDetailLine ("media", "5.25-inch\ntracks synchronized")
                + DiskImageSession::FormatDetailLine ("flux", "none");

        Assert::IsTrue   (PreviewDecoder::ParseDetails (message, details));
        Assert::AreEqual ((size_t) 4, details.size());
        Assert::AreEqual (std::wstring (L"Media"), details[1].first);
        Assert::AreEqual (std::wstring (L"5.25-inch"), details[1].second);
        Assert::AreEqual (std::wstring(), details[2].first);
        Assert::AreEqual (std::wstring (L"Tracks synchronized"), details[2].second);
        Assert::AreEqual (std::wstring (L"Flux"), details[3].first);
    }



    //  Sections keep their order, and the file system starts the last one.
    TEST_METHOD (Details_ReadSections)
    {
        std::vector<std::pair<std::wstring, std::wstring>>  details;
        std::string                                         message;

        message = "C:\\disks\\x.woz: does not have a DOS or ProDOS file system\n"
                + DiskImageSession::FormatDetailHeading ("WOZ info")
                + DiskImageSession::FormatDetailLine ("format", "WOZ 2")
                + DiskImageSession::FormatDetailHeading ("Casso analysis")
                + DiskImageSession::FormatDetailLine ("flux", "none");

        Assert::IsTrue   (PreviewDecoder::ParseDetails (message, details));
        Assert::AreEqual ((size_t) 5, details.size());
        Assert::AreEqual (std::wstring (L"WOZ info"), details[0].first);
        Assert::IsTrue   (details[0].second.empty());
        Assert::AreEqual (std::wstring (L"Format"), details[1].first);
        Assert::AreEqual (std::wstring (L"Casso analysis"), details[2].first);
        Assert::AreEqual (std::wstring (L"File system"), details[3].first);
        Assert::AreEqual (std::wstring (L"Flux"), details[4].first);
    }



    //  The WOZ spec's machine codes, as names, one a line.
    TEST_METHOD (Details_SplitWozValues_AndKeepAnUnknownDateAsItIs)
    {
        std::string  date = DiskImageSession::DescribeImageDate ("2022-05-26T20:50:59.621Z");

        Assert::AreEqual (std::string ("Tony Ngo\nBenny Ngo"), DiskImageSession::SplitMetaValues ("Tony Ngo|Benny Ngo"));
        Assert::AreEqual (std::string ("sometime"), DiskImageSession::DescribeImageDate ("sometime"));
        Assert::IsTrue   (date.ends_with ("\n(2022-05-26T20:50:59.621Z)"), L"The recorded date follows the local one");
    }



    TEST_METHOD (Details_ListTheWozMachines)
    {
        Assert::AreEqual (std::string ("Apple ][\nEnhanced Apple //e\nApple IIgs\n9z"), DiskImageSession::DescribeMachines ("2|2e+|2gs|9z"));
    }


    TEST_METHOD (TextIsKnownByItsContentNotItsType)
    {
        const Byte  plain[]   = { 'H', 'I', '\r', 'T', 'H', 'E', 'R', 'E' };
        const Byte  apple[]   = { 0xC8, 0xC9, 0x8D, 0xA0, 0xD4, 0x00, 0x00, 0x00 };
        const Byte  binary[]  = { 0xA9, 0x00, 0x8D, 0x10, 0xC0, 0x60 };
        const Byte  zeros[]   = { 0x00, 0x00, 0x00 };

        Assert::IsTrue  (PreviewDecoder::LooksLikeText (plain),  L"Plain ASCII");
        Assert::IsTrue  (PreviewDecoder::LooksLikeText (apple),  L"High-bit Apple text, with its sector padding");
        Assert::IsFalse (PreviewDecoder::LooksLikeText (binary), L"Machine code");
        Assert::IsFalse (PreviewDecoder::LooksLikeText (zeros),  L"Nothing but padding");
    }


    TEST_METHOD (AppleText_SplitsAtEachReturn)
    {
        const Byte                 bytes[] = { 0xC8, 0xC9, 0x8D, 0x8D, 0xC1, 0x89, 0xC2, 0x8D, 0x0A, 0x00, 0x00, 0xC3 };
        std::vector<std::wstring>  lines;

        PreviewDecoder::SplitAppleText (bytes, lines);

        Assert::AreEqual ((size_t) 4, lines.size());
        Assert::AreEqual (std::wstring (L"HI"),       lines[0], L"The high bit is cleared");
        Assert::AreEqual (std::wstring(),            lines[1], L"Between two returns is an empty line");
        Assert::AreEqual (std::wstring (L"A       B"), lines[2], L"A tab is expanded to the next stop of eight, and a line feed after a return ends no line");
        Assert::AreEqual (std::wstring (L"C"),        lines[3], L"Zero bytes are dropped, and a last line without a return is a line");
    }



    //  A source line wraps under its operand, after the label and the
    //  mnemonic, and a comment line under its first character.
    TEST_METHOD (GetOperandColumn_AfterTheLabelAndTheMnemonic)
    {
        Assert::AreEqual (14, PreviewText::GetOperandColumn (L"START   LDA   #$00"));
        Assert::AreEqual (14, PreviewText::GetOperandColumn (L"        LDA   #$00"));
        Assert::AreEqual (0,  PreviewText::GetOperandColumn (L"        RTS"));
        Assert::AreEqual (0,  PreviewText::GetOperandColumn (L"* A COMMENT LINE"));
        Assert::AreEqual (0,  PreviewText::GetOperandColumn (L""));

        PreviewContent  preview;

        preview.kind      = PreviewContent::Kind::Text;
        preview.lines     = { L"START   LDA   #$00" };
        preview.assembler = true;
        Assert::AreEqual (14, PreviewText::BuildTextRows (preview, false, SourceSyntax::Colors()).front().wrapIndent, L"a Merlin file's line");
    }



    //  A decoder's reason, as the summary above a file's bytes has it: a
    //  row a sentence, each a warning.
    TEST_METHOD (BuildWarningRows_ARowEachSentence)
    {
        std::vector<DxuiTextView::Row>  rows = PreviewText::BuildWarningRows (L"The LZ4FH picture doesn't unpack. Its bytes are below.");



        Assert::AreEqual ((size_t) 2, rows.size());
        Assert::AreEqual (std::wstring (L"The LZ4FH picture doesn't unpack."), rows[0].cells[0]);
        Assert::AreEqual (std::wstring (L"Its bytes are below."),               rows[1].cells[0]);
        Assert::IsTrue   (rows[0].warning && rows[1].warning);
    }


    //  A saved picture is the pixels the preview draws, opaque, at their size.
    TEST_METHOD (EncodePng_WritesThePicturesPixels)
    {
        std::vector<uint32_t>  bgra = { 0xFFFF0000u, 0xFF00FF00u, 0xFF0000FFu, 0xFF123456u };
        std::vector<Byte>      png;
        RgbaImage              decoded;



        AssertSucceeded (PicturePreview::EncodePng (bgra, 2, 2, png));
        AssertSucceeded (PngCodec::DecodeRgba (png, decoded));

        Assert::AreEqual (2, decoded.width);
        Assert::AreEqual (2, decoded.height);
        Assert::IsTrue (decoded.rgba == std::vector<Byte> { 0xFF, 0x00, 0x00, 0xFF,  0x00, 0xFF, 0x00, 0xFF,
                                                            0x00, 0x00, 0xFF, 0xFF,  0x12, 0x34, 0x56, 0xFF });
    }
};
