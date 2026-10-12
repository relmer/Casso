#include "Pch.h"

#include "Devices/Printer/PngCodec.h"
#include "Ui/Dialogs/ReleaseNotesLayout.h"
#include "Update/ReleaseNotesFormatter.h"
#include "Update/UpdateService.h"
#include "FakeInstallEnvironment.h"
#include "FakePackageDeployer.h"
#include "FakeSignatureVerifier.h"
#include "FakeUpdateHost.h"
#include "MockHttpClient.h"
#include "MockUpdateFileSystem.h"
#include "RecordingResultPoster.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  ReleaseNotesImageTests
//
//  Images in the release notes, end to end over mocks: finding them in the
//  markdown and the README's HTML tables, resolving their URLs against the
//  release tag, laying them out, and fetching and decoding them on the
//  service's worker. The PNG is made at test time by the core codec.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (ReleaseNotesImageTests)
{
public:

    static constexpr float  kCharPx = 10.0f;

    bool  m_ownsCom = false;



    TEST_METHOD_INITIALIZE (InitCom)
    {
        HRESULT  hr = CoInitializeEx (nullptr, COINIT_APARTMENTTHREADED);

        m_ownsCom = (hr == S_OK || hr == S_FALSE);
    }



    TEST_METHOD_CLEANUP (UninitCom)
    {
        if (m_ownsCom)
        {
            CoUninitialize();
        }
    }



    static float Measure (const std::wstring & text, const NotesRunStyle &)
    {
        return (float) text.size() * kCharPx;
    }



    //
    //  A 2x1 PNG: one opaque red pixel and one half-transparent white one.
    //
    static std::vector<Byte> MakeTinyPng()
    {
        RgbaImage          image;
        std::vector<Byte>  png;
        HRESULT            hr = S_OK;

        image.Allocate (2, 1, 0xFF, 0, 0);
        image.GetPixel (1, 0)[0] = 0xFF;
        image.GetPixel (1, 0)[1] = 0xFF;
        image.GetPixel (1, 0)[2] = 0xFF;
        image.GetPixel (1, 0)[3] = 0x80;

        hr = PngCodec::EncodeRgba (image, 96, png);
        AssertSucceeded (hr);
        return png;
    }



    //  Finding images

    TEST_METHOD (Format_MarkdownImage_IsAnImageLine)
    {
        std::vector<FormattedLine>  lines;



        ReleaseNotesFormatter::Format ("Before\n\n![The desk](Assets/desk.png)\n\nAfter", lines);

        Assert::AreEqual ((size_t) 5, lines.size());
        Assert::IsTrue   (lines[2].kind == FormattedLineKind::Image);
        Assert::AreEqual (std::string ("Assets/desk.png"), lines[2].image.src);
        Assert::AreEqual (std::string ("The desk"),        lines[2].image.alt);
        Assert::AreEqual (0, lines[2].image.widthPercent);
    }



    TEST_METHOD (Format_ImgTag_ReadsSrcAltAndPercentWidth)
    {
        std::vector<FormattedLine>  lines;



        ReleaseNotesFormatter::Format ("<img src=\"https://example.com/a.png\" alt='Shot' width=\"59%\" />\n"
                                       "<img src=\"b.png\" width=\"320\">", lines);

        Assert::AreEqual ((size_t) 2, lines.size());
        Assert::AreEqual (std::string ("https://example.com/a.png"), lines[0].image.src);
        Assert::AreEqual (std::string ("Shot"),                       lines[0].image.alt);
        Assert::AreEqual (59, lines[0].image.widthPercent);
        Assert::AreEqual (std::string ("b.png"), lines[1].image.src);
        Assert::IsTrue   (lines[1].image.alt.empty(), L"no alt is an empty alt");
        Assert::AreEqual (0, lines[1].image.widthPercent, L"a pixel width is not kept");
    }



    //  The README's two-column table, as written there: each cell's <sub> is
    //  its image's caption, and none of the table markup shows.
    TEST_METHOD (Format_TableCells_FlattenToImagesWithCaptions)
    {
        std::vector<FormattedLine>  lines;
        std::string                 readme =
            "<table align=\"center\" width=\"100%\">\n"
            "<tr>\n"
            "  <td valign=\"top\" width=\"50%\" align=\"center\"><img src=\"Assets/one.png\" alt=\"First\" width=\"100%\" /><br /><sub>Caption one</sub></td>\n"
            "  <td valign=\"top\" width=\"50%\" align=\"center\"><img src=\"Assets/two.png\" alt=\"Second\" width=\"100%\" /><br /><sub>Caption <b>two</b></sub></td>\n"
            "</tr>\n"
            "</table>\n"
            "\n"
            "<p align=\"center\"><sub>A note under the table.</sub></p>\n";



        ReleaseNotesFormatter::Format (readme, lines);

        Assert::AreEqual ((size_t) 4, lines.size());
        Assert::IsTrue   (lines[0].kind == FormattedLineKind::Image);
        Assert::AreEqual (std::string ("Caption one"), lines[0].image.caption);
        Assert::AreEqual (100, lines[0].image.widthPercent);
        Assert::IsTrue   (lines[1].kind == FormattedLineKind::Image);
        Assert::AreEqual (std::string ("Caption two"), lines[1].image.caption, L"tags inside a caption are dropped");
        Assert::IsTrue   (lines[2].kind == FormattedLineKind::Blank);
        Assert::IsTrue   (lines[3].kind == FormattedLineKind::Paragraph, L"a <sub> with no image before it is text");
        Assert::AreEqual (std::string ("A note under the table."), lines[3].runs[0].text);
    }



    TEST_METHOD (Format_CaptionPairsOnlyWithinItsCell)
    {
        std::vector<FormattedLine>  lines;



        ReleaseNotesFormatter::Format ("<td><img src=\"a.png\"></td><td><sub>Not a's</sub></td>", lines);

        Assert::AreEqual ((size_t) 2, lines.size());
        Assert::IsTrue   (lines[0].image.caption.empty());
        Assert::AreEqual (std::string ("Not a's"), lines[1].runs[0].text);
    }



    TEST_METHOD (Format_BadgeInALink_IsTheImage)
    {
        std::vector<FormattedLine>  lines;



        ReleaseNotesFormatter::Format ("[![CI](https://example.com/badge.svg)](https://example.com/ci) Build status", lines);

        Assert::AreEqual ((size_t) 2, lines.size());
        Assert::AreEqual (std::string ("https://example.com/badge.svg"), lines[0].image.src);
        Assert::AreEqual (std::string ("CI"), lines[0].image.alt);
        Assert::AreEqual (std::string (" Build status"), lines[1].runs[0].text);
    }



    //  Resolving

    TEST_METHOD (ResolveImageUrl_RelativeAbsoluteAndRefused)
    {
        std::wstring  url;



        Assert::IsTrue   (UpdateService::TryResolveImageUrl ("Assets/a.png", "v1.30.0", url));
        Assert::AreEqual (std::wstring (L"https://raw.githubusercontent.com/relmer/Casso/v1.30.0/Assets/a.png"), url);
        Assert::IsTrue   (UpdateService::TryResolveImageUrl ("./Assets/a.png", "v1.30.0", url));
        Assert::AreEqual (std::wstring (L"https://raw.githubusercontent.com/relmer/Casso/v1.30.0/Assets/a.png"), url);
        Assert::IsTrue   (UpdateService::TryResolveImageUrl ("/Assets/a.png", "v1.30.0", url));
        Assert::IsTrue   (url.ends_with (L"/v1.30.0/Assets/a.png"));
        Assert::IsTrue   (UpdateService::TryResolveImageUrl ("https://example.com/b.png", "v1.30.0", url));
        Assert::AreEqual (std::wstring (L"https://example.com/b.png"), url);

        Assert::IsFalse (UpdateService::TryResolveImageUrl ("http://example.com/b.png", "v1.30.0", url));
        Assert::IsFalse (UpdateService::TryResolveImageUrl ("data:image/png;base64,AAAA", "v1.30.0", url));
        Assert::IsFalse (UpdateService::TryResolveImageUrl ("//example.com/b.png", "v1.30.0", url));
        Assert::IsFalse (UpdateService::TryResolveImageUrl ("ftp://example.com/b.png", "v1.30.0", url));
        Assert::IsFalse (UpdateService::TryResolveImageUrl ("", "v1.30.0", url));
    }



    //  Release notes are UTF-8, so a path outside ASCII has to arrive as the
    //  character it encodes. Widening byte by byte sign-extended each byte of
    //  the e-acute into U+FFC3 U+FFA9.
    TEST_METHOD (ResolveImageUrl_DecodesUtf8Paths)
    {
        std::wstring  url;



        Assert::IsTrue   (UpdateService::TryResolveImageUrl ("Assets/Caf\xC3\xA9.png", "v1.30.0", url));
        Assert::AreEqual (std::wstring (L"https://raw.githubusercontent.com/relmer/Casso/v1.30.0/Assets/Caf\u00E9.png"), url);
        Assert::IsTrue   (UpdateService::TryResolveImageUrl ("https://example.com/Caf\xC3\xA9.png", "v1.30.0", url));
        Assert::AreEqual (std::wstring (L"https://example.com/Caf\u00E9.png"), url);
    }



    //  Layout

    static FormattedLine MakeImageLine (int widthPercent, const std::string & caption)
    {
        FormattedLine  line;

        line.kind               = FormattedLineKind::Image;
        line.image.src          = "a.png";
        line.image.alt          = "Alt";
        line.image.caption      = caption;
        line.image.widthPercent = widthPercent;
        return line;
    }



    TEST_METHOD (Layout_LoadedImage_FitsTheWidthKeepingAspect)
    {
        NotesLayoutMetrics             metrics;
        std::vector<PlacedNotesRun>    runs;
        std::vector<PlacedNotesImage>  images;
        auto                           big = [] (const std::string &) { return NotesImageState { true, false, 1000, 500 }; };



        ReleaseNotesLayout::Flow ({ MakeImageLine (0, "") }, 400.0f, metrics, Measure, big, runs, images);

        Assert::AreEqual ((size_t) 1, images.size());
        Assert::AreEqual (400.0f, images[0].width);
        Assert::AreEqual (200.0f, images[0].height);
        Assert::AreEqual (0.0f,   images[0].x);
    }



    TEST_METHOD (Layout_SmallImage_IsNeverUpscaledPastItsDpiSize)
    {
        NotesLayoutMetrics             metrics;
        std::vector<PlacedNotesRun>    runs;
        std::vector<PlacedNotesImage>  images;
        auto                           tiny  = [] (const std::string &) { return NotesImageState { true, false, 100, 50 }; };



        metrics.imageScale = 1.5f;
        ReleaseNotesLayout::Flow ({ MakeImageLine (0, "") }, 400.0f, metrics, Measure, tiny, runs, images);

        Assert::AreEqual (150.0f, images[0].width, L"its size at 144 DPI, not the column");
        Assert::AreEqual (75.0f,  images[0].height);
        Assert::AreEqual (125.0f, images[0].x, L"centered");
    }



    TEST_METHOD (Layout_PercentWidth_IsOfTheBody)
    {
        NotesLayoutMetrics             metrics;
        std::vector<PlacedNotesRun>    runs;
        std::vector<PlacedNotesImage>  images;
        auto                           big = [] (const std::string &) { return NotesImageState { true, false, 1000, 1000 }; };



        ReleaseNotesLayout::Flow ({ MakeImageLine (50, "") }, 400.0f, metrics, Measure, big, runs, images);

        Assert::AreEqual (200.0f, images[0].width);
        Assert::AreEqual (100.0f, images[0].x);
    }



    TEST_METHOD (Layout_Placeholder_ThenCaptionCenteredBeneath)
    {
        NotesLayoutMetrics             metrics;
        std::vector<PlacedNotesRun>    runs;
        std::vector<PlacedNotesImage>  images;



        ReleaseNotesLayout::Flow ({ MakeImageLine (0, "Cap") }, 400.0f, metrics, Measure, nullptr, runs, images);

        Assert::IsFalse  (images[0].isLoaded);
        Assert::AreEqual (std::wstring (L"Alt"), images[0].alt);
        Assert::AreEqual (metrics.placeholderPx, images[0].height);
        Assert::AreEqual ((size_t) 1, runs.size());
        Assert::IsTrue   (runs[0].style.muted);
        Assert::AreEqual (metrics.captionSizePx, runs[0].style.sizePx);
        Assert::AreEqual (185.0f, runs[0].x, L"30 pixels of caption centered in 400");
        Assert::AreEqual (metrics.placeholderPx, runs[0].y, L"directly beneath the box");
    }



    TEST_METHOD (Layout_FailedImage_KeepsItsAltBox)
    {
        NotesLayoutMetrics             metrics;
        std::vector<PlacedNotesRun>    runs;
        std::vector<PlacedNotesImage>  images;
        auto                           failed = [] (const std::string &) { return NotesImageState { false, true, 0, 0 }; };



        ReleaseNotesLayout::Flow ({ MakeImageLine (0, "") }, 400.0f, metrics, Measure, failed, runs, images);

        Assert::IsTrue   (images[0].isFailed);
        Assert::IsFalse  (images[0].isLoaded);
        Assert::AreEqual (metrics.placeholderPx, images[0].height);
    }



    //  Resizing

    TEST_METHOD (Reflow_TextRewrapsAndImagesRescaleWithTheWidth)
    {
        NotesLayoutMetrics             metrics;
        std::vector<PlacedNotesRun>    runs;
        std::vector<PlacedNotesImage>  images;
        std::vector<FormattedLine>     lines;
        FormattedLine                  text;
        auto                           big    = [] (const std::string &) { return NotesImageState { true, false, 1000, 500 }; };
        float                          narrow = 0.0f;
        float                          wide   = 0.0f;



        text.kind = FormattedLineKind::Paragraph;
        text.runs = { { "one two three four five six" } };
        lines     = { text, MakeImageLine (0, "") };

        narrow = ReleaseNotesLayout::Flow (lines, 120.0f, metrics, Measure, big, runs, images);
        Assert::AreEqual (120.0f, images[0].width, L"the image follows the narrow body");
        Assert::AreEqual (60.0f,  images[0].height);
        Assert::AreEqual (metrics.lineHeightPx * 3.0f, images[0].y - metrics.imageGapPx, L"three lines of text at 120");

        wide = ReleaseNotesLayout::Flow (lines, 900.0f, metrics, Measure, big, runs, images);
        Assert::AreEqual (900.0f, images[0].width, L"and the wide one");
        Assert::AreEqual (450.0f, images[0].height);
        Assert::AreEqual (metrics.lineHeightPx, images[0].y - metrics.imageGapPx, L"one line of text at 900");
        Assert::IsTrue   (wide != narrow);

        ReleaseNotesLayout::Flow (lines, 1500.0f, metrics, Measure, big, runs, images);
        Assert::AreEqual (1000.0f, images[0].width, L"but never past its own size");
    }



    TEST_METHOD (ScaleScrollPos_KeepsTheFractionDown)
    {
        Assert::AreEqual (500, ReleaseNotesLayout::ScaleScrollPos (250, 1000, 2000));
        Assert::AreEqual (100, ReleaseNotesLayout::ScaleScrollPos (200, 2000, 1000));
        Assert::AreEqual (0,   ReleaseNotesLayout::ScaleScrollPos (0,   1000, 2000), L"the top stays the top");
        Assert::AreEqual (0,   ReleaseNotesLayout::ScaleScrollPos (300, 0,    2000), L"no old height, no position to keep");
    }


    //  Fetching

    struct Rig
    {
        MockHttpClient                  http;
        FakeSignatureVerifier           verifier;
        FakeInstallEnvironment          environment;
        MockUpdateFileSystem            fileSystem;
        FakePackageDeployer             deployer;
        FakeUpdateHost                  host;
        RecordingResultPoster           poster;
        std::unique_ptr<UpdateService>  service;

        Rig()
        {
            UpdateServiceDeps  deps;

            deps.http        = &http;
            deps.verifier    = &verifier;
            deps.environment = &environment;
            deps.fileSystem  = &fileSystem;
            deps.deployer    = &deployer;
            deps.host        = &host;
            deps.poster      = &poster;
            deps.clock       = [] () { return (std::int64_t) 0; };

            service = std::make_unique<UpdateService> (deps);
        }
    };



    TEST_METHOD (Fetch_DecodesPremultipliedAndCaches)
    {
        Rig  rig;



        rig.http.SetBytes (UpdateService::kpszRawHost, L"/relmer/Casso/v1.30.0/Assets/a.png", MakeTinyPng());

        AssertSucceeded (rig.service->StartFetchImages ("v1.30.0", { "Assets/a.png" }));
        rig.service->Wait();
        AssertSucceeded (rig.service->StartFetchImages ("v1.30.0", { "Assets/a.png" }));
        rig.service->Wait();

        Assert::AreEqual ((size_t) 2, rig.poster.results.size());
        Assert::AreEqual ((size_t) 1, rig.http.GetRequestCount(), L"the second fetch comes from the cache");

        for (const std::unique_ptr<UpdateResult> & result : rig.poster.results)
        {
            Assert::IsTrue    (result->kind == UpdateResultKind::Image);
            Assert::IsTrue    (result->failure == UpdateFailure::None);
            Assert::AreEqual  (std::string ("Assets/a.png"), result->imageSrc);
            Assert::IsNotNull (result->image.get());
            Assert::AreEqual  (2, result->image->width);
            Assert::AreEqual  (1, result->image->height);
            Assert::AreEqual  ((uint32_t) 0xFFFF0000, result->image->bgraPremul[0], L"opaque red, BGRA");
            Assert::AreEqual  ((uint32_t) 0x80808080, result->image->bgraPremul[1], L"half-transparent white, premultiplied");
        }
    }



    TEST_METHOD (Fetch_UnfetchableSource_FailsWithoutARequest)
    {
        Rig  rig;



        AssertSucceeded (rig.service->StartFetchImages ("v1.30.0", { "http://example.com/a.png", "data:image/png;base64,AA" }));
        rig.service->Wait();

        Assert::AreEqual ((size_t) 2, rig.poster.results.size());
        Assert::AreEqual ((size_t) 0, rig.http.GetRequestCount());
        Assert::IsTrue   (rig.poster.results[0]->failure != UpdateFailure::None);
        Assert::IsNull   (rig.poster.results[0]->image.get());
    }



    TEST_METHOD (Fetch_OversizedOrUndecodable_Fails)
    {
        Rig                rig;
        std::vector<Byte>  huge (UpdateService::kMaxImageBytes + 1, 0);



        rig.http.SetBytes (UpdateService::kpszRawHost, L"/relmer/Casso/v1.30.0/huge.png", huge);
        rig.http.SetBytes (UpdateService::kpszRawHost, L"/relmer/Casso/v1.30.0/junk.png", { 1, 2, 3 });

        AssertSucceeded (rig.service->StartFetchImages ("v1.30.0", { "huge.png", "junk.png", "missing.png" }));
        rig.service->Wait();

        Assert::AreEqual ((size_t) 3, rig.poster.results.size());

        for (const std::unique_ptr<UpdateResult> & result : rig.poster.results)
        {
            Assert::IsTrue (result->failure != UpdateFailure::None);
            Assert::IsNull (result->image.get());
        }
    }



    TEST_METHOD (Fetch_CanceledStopsPosting)
    {
        Rig              rig;
        UpdateService  * service = rig.service.get();



        rig.http.SetBytes (UpdateService::kpszRawHost, L"/relmer/Casso/v1.30.0/a.png", MakeTinyPng());
        rig.http.onGet = [service] (const HttpRequest &) { service->CancelImages(); };

        AssertSucceeded (rig.service->StartFetchImages ("v1.30.0", { "a.png", "b.png" }));
        rig.service->Wait();

        Assert::AreEqual ((size_t) 0, rig.poster.results.size(), L"nothing posted once the dialog closed");
        Assert::AreEqual ((size_t) 1, rig.http.GetRequestCount(), L"and nothing more fetched");
    }
};
