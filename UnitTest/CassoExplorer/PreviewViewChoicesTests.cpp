#include "Pch.h"
#include "../EhmTestHelper.h"
#include "CassoExplorer/Model/PreviewViewChoices.h"
#include "Core/MemoryBus.h"
#include "Machines/Apple2/Common/ProDosVolume.h"
#include "Machines/Apple2/Common/VolumeImage.h"
#include "Machines/Apple2/Common/VolumeTypes.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  PreviewViewChoicesTests
//
//  View as: the views listed for a file's bytes and its default view, a
//  chosen view rendered in place of the default, and a chosen view that
//  doesn't apply falling back to the default.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (PreviewViewChoicesTests)
{
public:

    //  A ProDOS binary of `bytes` bytes loading at `address`.
    static FilePayload MakeBinary (size_t bytes, Word address)
    {
        FilePayload  payload;



        payload.bytes.assign (bytes, 0x2A);
        payload.type           = ProDosVolume::kTypeBinary;
        payload.loadAddress    = address;
        payload.hasLoadAddress = true;

        return payload;
    }


    static FileEntry MakeEntry (const FilePayload & payload)
    {
        FileEntry  entry;



        entry.name           = "DATA";
        entry.type           = payload.type;
        entry.loadAddress    = payload.loadAddress;
        entry.hasLoadAddress = true;
        entry.eofBytes       = (uint32_t) payload.bytes.size();

        return entry;
    }


    TEST_METHOD (List_OffersThePictureModesOfTheFilesSize)
    {
        PreviewContent           hex;
        std::vector<ViewChoice>  choices;



        hex.kind = PreviewContent::Kind::Hex;

        PreviewViewChoices::List (MakeBinary (8192, 0x4000), hex, choices);
        Assert::IsTrue (choices == std::vector<ViewChoice> { ViewChoice::Text, ViewChoice::Disassembly, ViewChoice::HiRes });

        PreviewViewChoices::List (MakeBinary (16384, 0x4000), hex, choices);
        Assert::IsTrue (choices == std::vector<ViewChoice> { ViewChoice::Text, ViewChoice::Disassembly, ViewChoice::DoubleHiRes });

        PreviewViewChoices::List (MakeBinary (1024, 0x4000), hex, choices);
        Assert::IsTrue (choices == std::vector<ViewChoice> { ViewChoice::Text, ViewChoice::Disassembly, ViewChoice::LoRes });

        PreviewViewChoices::List (MakeBinary (500, 0x4000), hex, choices);
        Assert::IsTrue (choices == std::vector<ViewChoice> { ViewChoice::Text, ViewChoice::Disassembly });
    }


    TEST_METHOD (List_LeavesOutTheDefaultAndAddsTheResourceFork)
    {
        PreviewContent           picture;
        FilePayload              payload = MakeBinary (8192, 0x2000);
        std::vector<ViewChoice>  choices;



        picture.kind        = PreviewContent::Kind::Picture;
        picture.pictureMode = PicturePreview::Mode::HiRes;
        payload.resourceFork.assign (300, 1);

        PreviewViewChoices::List (payload, picture, choices);

        Assert::IsTrue (choices == std::vector<ViewChoice> { ViewChoice::Text, ViewChoice::Hex, ViewChoice::Disassembly, ViewChoice::ResourceFork });
    }


    TEST_METHOD (Render_ShowsTheChosenViewInPlaceOfTheDefault)
    {
        FilePayload     payload = MakeBinary (8192, 0x6000);
        FileEntry       entry   = MakeEntry (payload);
        MemoryBus       bus;
        PreviewContent  content;



        AssertSucceeded (PreviewViewChoices::Render (ViewChoice::Default, entry, VolumeKind::ProDos, payload, PreviewDecoder::HexForm::Auto, bus, content,
                                                     PicturePreview::Monitor::Color, PicturePreview::kDefaultCustomArgb, DecodeOptions()));

        Assert::IsTrue (content.viewAs == ViewChoice::Default);
        Assert::IsTrue (std::find (content.viewChoices.begin(), content.viewChoices.end(), ViewChoice::HiRes) != content.viewChoices.end());
        Assert::IsFalse (content.defaultView.empty());

        AssertSucceeded (PreviewViewChoices::Render (ViewChoice::HiRes, entry, VolumeKind::ProDos, payload, PreviewDecoder::HexForm::Auto, bus, content,
                                                     PicturePreview::Monitor::Color, PicturePreview::kDefaultCustomArgb, DecodeOptions()));

        Assert::IsTrue (content.kind == PreviewContent::Kind::Picture);
        Assert::IsTrue (content.pictureMode == PicturePreview::Mode::HiRes);
        Assert::IsTrue (content.viewAs == ViewChoice::HiRes);
        Assert::IsFalse (content.bgra.empty());
    }


    TEST_METHOD (Render_FallsBackToTheDefaultForAViewThatDoesntApply)
    {
        FilePayload     payload = MakeBinary (500, 0x4000);
        FileEntry       entry   = MakeEntry (payload);
        MemoryBus       bus;
        PreviewContent  content;



        AssertSucceeded (PreviewViewChoices::Render (ViewChoice::HiRes, entry, VolumeKind::ProDos, payload, PreviewDecoder::HexForm::Auto, bus, content,
                                                     PicturePreview::Monitor::Color, PicturePreview::kDefaultCustomArgb, DecodeOptions()));

        Assert::IsTrue (content.viewAs == ViewChoice::Default);
        Assert::IsTrue (content.kind != PreviewContent::Kind::Picture);
    }


    TEST_METHOD (RenderAs_TextAndTheResourceFork)
    {
        FilePayload     payload;
        FileEntry       entry;
        MemoryBus       bus;
        PreviewContent  content;



        payload.bytes        = { 0xC8, 0xC9, 0x8D, 0xD4, 0xC8, 0xC5, 0xD2, 0xC5 };   // HI, THERE in Apple text
        payload.resourceFork = { 1, 2, 3 };
        entry                = MakeEntry (payload);

        AssertSucceeded (PreviewViewChoices::RenderAs (ViewChoice::Text, entry, VolumeKind::ProDos, payload, PreviewDecoder::HexForm::Auto, bus, content,
                                                       PicturePreview::Monitor::Color, PicturePreview::kDefaultCustomArgb));

        Assert::IsTrue (content.kind == PreviewContent::Kind::Text);
        Assert::AreEqual (std::wstring (L"HI"), content.lines[0]);
        Assert::AreEqual (std::wstring (L"THERE"), content.lines[1]);

        AssertSucceeded (PreviewViewChoices::RenderAs (ViewChoice::ResourceFork, entry, VolumeKind::ProDos, payload, PreviewDecoder::HexForm::Auto, bus, content,
                                                       PicturePreview::Monitor::Color, PicturePreview::kDefaultCustomArgb));

        Assert::IsTrue (content.kind == PreviewContent::Kind::Hex);
        Assert::IsTrue (content.bytes == std::vector<Byte> { 1, 2, 3 });
    }


    TEST_METHOD (DescribeDefault_ReturnsEachKindsLabel)
    {
        PreviewContent  content;



        content.kind = PreviewContent::Kind::Listing;
        Assert::AreEqual (std::wstring (L"BASIC listing"), PreviewViewChoices::DescribeDefault (content));

        content.kind        = PreviewContent::Kind::Picture;
        content.pictureMode = PicturePreview::Mode::DoubleHiRes;
        Assert::AreEqual (std::wstring (L"Double hi-res picture"), PreviewViewChoices::DescribeDefault (content));

        content.kind        = PreviewContent::Kind::Hex;
        content.disassembly = true;
        Assert::AreEqual (std::wstring (L"Disassembly"), PreviewViewChoices::DescribeDefault (content));
    }
};
