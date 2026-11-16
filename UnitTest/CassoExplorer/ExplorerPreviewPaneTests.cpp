#include "Pch.h"
#include "ExplorerWindowPartsRig.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerPreviewPaneTests
//
//  A disk image's thumbnail, verdict and catalog in the preview pane, run
//  through ExplorerWindowParts with no window.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (ExplorerPreviewPaneTests)
{
public:

    using Rig = ExplorerWindowPartsRig;


    //  The preview of a disk image has the picture of its screen and the
    //  line with its boot result under it, at 100% and at 150%: beside the
    //  platter in a wide pane, and three quarters as tall as it is wide. The
    //  platter's analysis reads the file from the disk, so the image is in a
    //  folder of the test's in the temporary folder as well.
    TEST_METHOD (Preview_HasADiskImagesScreenAndResultLine_At100And150Percent)
    {
        for (UINT dpi : { 96u, 144u })
        {
            Rig                    rig;
            FakeDiskPictures       pictures;
            ThumbnailRecord        record;
            bool                   isDrawn = false;
            RECT                   screen  = {};
            std::filesystem::path  folder  = std::filesystem::temp_directory_path() / std::format (L"CassoPreviewScreen{}_{}", ::GetCurrentProcessId(), dpi);
            std::filesystem::path  image   = folder / L"a.dsk";



            record.verdict      = "booted";
            record.machine      = "Apple2eEnhanced";
            pictures.resultLine = DiskImagePictures::DescribeRecord (record, false);

            rig.app.pictures = &pictures;
            rig.scaler.SetDpi (dpi);
            rig.prefs.previewVisible = true;
            rig.Seed ("CassoExplorer/dos33.dsk", image.c_str());
            std::filesystem::create_directories (folder);
            AssertSucceeded (DiskImageStore::WriteFileAtomically (image.string(), rig.io.files[image.string()]));
            rig.Open();
            Assert::IsTrue (rig.browser.NavigateToAddress (folder.wstring()));
            rig.parts->listPane.FillList();
            Assert::IsTrue (rig.parts->listPane.SelectRowNamed (L"a.dsk"));
            rig.parts->preview.FillPreview();

            isDrawn = Rig::WaitFor (rig, [&rig]()
            {
                rig.parts->layout.RecomputeLayout();

                return rig.parts->preview.GetThumbnail().IsScreenShown();
            });

            Assert::IsTrue (isDrawn, std::format (L"The screen's picture at {} dpi", dpi).c_str());
            Assert::AreEqual (pictures.resultLine, rig.parts->preview.GetThumbnail().GetResultLine());

            screen = rig.parts->preview.GetThumbnail().GetScreenRect();
            Assert::AreEqual ((long) rig.scaler.ToPx (PreviewDiskThumbnail::kScreenDip), screen.right - screen.left);
            Assert::AreEqual ((screen.right - screen.left) * 3 / 4, screen.bottom - screen.top);

            std::filesystem::remove_all (folder);
        }
    }


    //  From one disk image with a picture to the next, the picture and its
    //  result line are drawn while the next image is analyzed, so the catalog
    //  stays put through the analysis and after it.
    TEST_METHOD (Preview_TheScreenIsDrawnWhileTheNextImageIsAnalyzed_At100And150Percent)
    {
        for (UINT dpi : { 96u, 144u })
        {
            Rig                    rig;
            FakeDiskPictures       pictures;
            ThumbnailRecord        record;
            std::filesystem::path  folder = std::filesystem::temp_directory_path() / std::format (L"CassoPreviewScreenNext{}_{}", ::GetCurrentProcessId(), dpi);
            LONG                   top    = 0;



            record.verdict      = "booted";
            record.machine      = "Apple2eEnhanced";
            pictures.resultLine = DiskImagePictures::DescribeRecord (record, false);

            rig.app.pictures = &pictures;
            rig.scaler.SetDpi (dpi);
            rig.prefs.previewVisible = true;
            std::filesystem::create_directories (folder);

            for (const wchar_t * name : { L"a.dsk", L"b.dsk" })
            {
                std::filesystem::path  image = folder / name;

                rig.Seed ("CassoExplorer/dos33.dsk", image.c_str());
                AssertSucceeded (DiskImageStore::WriteFileAtomically (image.string(), rig.io.files[image.string()]));
            }

            rig.Open();
            rig.parts->layout.SetClient (RECT { 0, 0, rig.scaler.ToPx (1280), rig.scaler.ToPx (900) });
            Assert::IsTrue (rig.browser.NavigateToAddress (folder.wstring()));
            rig.parts->listPane.FillList();

            auto  select = [&rig] (const wchar_t * name)
            {
                Assert::IsTrue (rig.parts->listPane.SelectRowNamed (name));
                rig.parts->preview.FillPreview();
                rig.parts->layout.RecomputeLayout();
            };

            auto  read = [&rig]()
            {
                Assert::IsTrue (Rig::WaitFor (rig, [&rig]()
                {
                    rig.parts->layout.RecomputeLayout();

                    return rig.parts->preview.GetThumbnail().IsReady();
                }));

                rig.parts->layout.RecomputeLayout();
            };

            select (L"a.dsk");
            read();
            Assert::IsTrue (rig.parts->preview.GetThumbnail().IsScreenShown(), L"a's picture");
            top = rig.parts->preview.GetContentRect().top;

            select (L"b.dsk");
            Assert::IsFalse  (rig.parts->preview.GetThumbnail().IsReady(), L"b is being analyzed");
            Assert::IsTrue   (rig.parts->preview.GetThumbnail().IsScreenShown(), L"b's picture is drawn while it is analyzed");
            Assert::AreEqual (pictures.resultLine, rig.parts->preview.GetThumbnail().GetResultLine());
            Assert::AreEqual (top, rig.parts->preview.GetContentRect().top, std::format (L"the catalog stays put while b is analyzed at {} dpi", dpi).c_str());

            read();
            Assert::AreEqual (top, rig.parts->preview.GetContentRect().top, std::format (L"and when b's analysis finishes at {} dpi", dpi).c_str());

            std::filesystem::remove_all (folder);
        }
    }


    //  The preview of a disk image has its file-system verdict at the top, at
    //  100% and at 150%: Bad as an error infobar with its first finding of
    //  damage and Repair disk image, Suspect as an info infobar with its
    //  first suspect finding and Inspect disk image, and nothing for OK. The
    //  analysis reads the file from the disk, so each image is in a folder of
    //  the test's in the temporary folder as well.
    TEST_METHOD (Preview_HasTheFileSystemVerdictAboveTheThumbnail_At100And150Percent)
    {
        struct Case { const wchar_t * name; vector<Byte> bytes; const wchar_t * text; DxuiInfoBanner::Severity severity; const wchar_t * action; };

        const Case  cases[] =
        {
            { L"bad.dsk",     RepairFixtures::MakeDos33Bitmap().damaged,       L"File system damaged.\nTrack 18, sector $E is used by HELLO but marked free.\n\nInspect for more details.",
              DxuiInfoBanner::Severity::Error, L"" },
            { L"suspect.dsk", RepairFixtures::MakeDos33NextTrackOff().damaged, L"Possible inconsistencies found.\nThe VTOC's next track to allocate, track 40, is off the disk.\n\nInspect for more details.",
              DxuiInfoBanner::Severity::Info,  L"" },
            { L"ok.dsk",      RepairFixtures::MakeDos33(),                     L"", DxuiInfoBanner::Severity::Info, L"" },
        };



        for (UINT dpi : { 96u, 144u })
        {
            for (const Case & c : cases)
            {
                Rig                    rig;
                bool                   isDrawn = false;
                std::filesystem::path  folder  = std::filesystem::temp_directory_path() / std::format (L"CassoPreviewVerdict{}_{}", ::GetCurrentProcessId(), dpi);
                std::filesystem::path  image   = folder / c.name;
                std::string            narrow  = image.string();
                RECT                   pane    = {};
                RECT                   banner  = {};
                RECT                   repair  = {};

                rig.scaler.SetDpi (dpi);
                rig.prefs.previewVisible = true;
                AssertSucceeded (rig.fs.WriteAllText (image.c_str(), std::string (c.bytes.begin(), c.bytes.end())));
                rig.io.files[narrow]  = c.bytes;
                rig.io.stamps[narrow] = FileStamp { c.bytes.size(), 100 };
                std::filesystem::create_directories (folder);
                AssertSucceeded (DiskImageStore::WriteFileAtomically (narrow, c.bytes));
                rig.Open();
                Assert::IsTrue (rig.browser.NavigateToAddress (folder.wstring()));
                rig.parts->listPane.FillList();
                Assert::IsTrue (rig.parts->listPane.SelectRowNamed (c.name));
                rig.parts->preview.FillPreview();

                isDrawn = Rig::WaitFor (rig, [&rig]()
                {
                    rig.parts->layout.RecomputeLayout();

                    return rig.parts->preview.GetThumbnail().IsReady();
                });

                Assert::IsTrue (isDrawn, c.name);
                rig.parts->layout.RecomputeLayout();

                const PreviewDiskThumbnail  & thumbnail = rig.parts->preview.GetThumbnail();
                bool                          isShown   = *c.text != L'\0';

                pane   = rig.parts->preview.GetContentRect();
                banner = thumbnail.GetVerdict().GetBounds();

                Assert::AreEqual (std::wstring (c.text), thumbnail.GetVerdict().GetText(), std::format (L"{} at {} dpi", c.name, dpi).c_str());
                Assert::AreEqual (isShown, thumbnail.GetVerdict().IsVisible(), c.name);

                if (isShown)
                {
                    Assert::IsTrue (thumbnail.GetVerdict().GetSeverity() == c.severity, c.name);
                    Assert::AreEqual ((size_t) 0, thumbnail.GetVerdict().GetActionCount(), L"the infobar has no button; Inspect... is under it");
                    Assert::IsTrue   (banner.left >= pane.left && banner.right <= pane.right && banner.bottom <= pane.top, L"the infobar is inside the pane, above the catalog");
                }

                //  Inspect... at the platter area's bottom right, or under the
                //  platter at the right where the pane is too narrow beside it.
                repair = thumbnail.GetInspectButton().GetBounds();
                Assert::IsTrue (thumbnail.GetInspectButton().IsVisible(), c.name);
                Assert::IsTrue ((repair.left >= thumbnail.GetPlatterRect().right && repair.bottom == thumbnail.GetPlatterRect().bottom) ||
                                repair.top >= thumbnail.GetPlatterRect().bottom, std::format (L"Inspect... beside the platter's bottom or under it at {} dpi", dpi).c_str());
                Assert::IsTrue (repair.right <= pane.right && repair.right >= pane.right - rig.scaler.ToPx (40), L"at the right");

                std::filesystem::remove_all (folder);
            }
        }
    }


    //  From one damaged image to the next, the infobar's space stays while the
    //  next is read, empty, so the catalog doesn't move down when its infobar
    //  arrives. After a clean image is read the space closes.
    TEST_METHOD (Preview_TheInfobarsSpaceStaysWhileTheNextImageIsRead_At100And150Percent)
    {
        vector<Byte>  damaged = RepairFixtures::MakeDos33Bitmap().damaged;
        vector<Byte>  clean   = RepairFixtures::MakeDos33();



        for (UINT dpi : { 96u, 144u })
        {
            Rig                    rig;
            std::filesystem::path  folder = std::filesystem::temp_directory_path() / std::format (L"CassoPreviewReserve{}_{}", ::GetCurrentProcessId(), dpi);
            LONG                   top    = 0;

            rig.scaler.SetDpi (dpi);
            rig.prefs.previewVisible = true;
            std::filesystem::create_directories (folder);

            for (std::pair<const wchar_t *, const vector<Byte> *> file : { std::pair { L"a.dsk", &damaged }, { L"b.dsk", &damaged }, { L"c.dsk", &clean } })
            {
                std::string  narrow = (folder / file.first).string();

                AssertSucceeded (rig.fs.WriteAllText ((folder / file.first).c_str(), std::string (file.second->begin(), file.second->end())));
                rig.io.files[narrow]  = *file.second;
                rig.io.stamps[narrow] = FileStamp { file.second->size(), 100 };
                AssertSucceeded (DiskImageStore::WriteFileAtomically (narrow, *file.second));
            }

            rig.Open();
            rig.parts->layout.SetClient (RECT { 0, 0, rig.scaler.ToPx (1280), rig.scaler.ToPx (900) });
            Assert::IsTrue (rig.browser.NavigateToAddress (folder.wstring()));
            rig.parts->listPane.FillList();

            auto  select = [&rig] (const wchar_t * name)
            {
                Assert::IsTrue (rig.parts->listPane.SelectRowNamed (name));
                rig.parts->preview.FillPreview();
                rig.parts->layout.RecomputeLayout();
            };

            auto  read = [&rig]()
            {
                Assert::IsTrue (Rig::WaitFor (rig, [&rig]()
                {
                    rig.parts->layout.RecomputeLayout();

                    return rig.parts->preview.GetThumbnail().IsReady();
                }));

                rig.parts->layout.RecomputeLayout();
            };

            select (L"a.dsk");
            read();
            Assert::IsTrue (rig.parts->preview.GetThumbnail().GetVerdict().IsVisible(), L"a's infobar");
            top = rig.parts->preview.GetContentRect().top;

            select (L"b.dsk");
            Assert::IsFalse  (rig.parts->preview.GetThumbnail().IsReady(), L"b is being read");
            Assert::IsFalse  (rig.parts->preview.GetThumbnail().GetVerdict().IsVisible(), L"no infobar while b is read");
            Assert::AreEqual (top, rig.parts->preview.GetContentRect().top, std::format (L"the catalog stays put while b is read at {} dpi", dpi).c_str());

            read();
            Assert::IsTrue   (rig.parts->preview.GetThumbnail().GetVerdict().IsVisible(), L"b's infobar");
            Assert::AreEqual (top, rig.parts->preview.GetContentRect().top, std::format (L"and when b's infobar arrives at {} dpi", dpi).c_str());

            select (L"c.dsk");
            Assert::AreEqual (top, rig.parts->preview.GetContentRect().top, L"the space stays while c is read");

            read();
            Assert::IsFalse (rig.parts->preview.GetThumbnail().GetVerdict().IsVisible(), L"c has no infobar");
            Assert::IsTrue  (rig.parts->preview.GetContentRect().top < top, L"and the space closes");

            std::filesystem::remove_all (folder);
        }
    }


    //  After F2 renames the disk image in the preview pane, the thumbnail isn't
    //  read again and the pane isn't laid out again.
    TEST_METHOD (Rename_OfTheImageInThePreview_KeepsItsThumbnail)
    {
        Rig                    rig;
        DxuiKeyEvent           key;
        vector<Byte>           bytes   = RepairFixtures::MakeDos33Bitmap().damaged;
        std::filesystem::path  folder  = std::filesystem::temp_directory_path() / std::format (L"CassoPreviewRename{}", ::GetCurrentProcessId());
        std::filesystem::path  before  = folder / L"a.dsk";
        std::filesystem::path  after   = folder / L"b.dsk";
        LONG                   top     = 0;



        rig.prefs.previewVisible = true;
        std::filesystem::create_directories (folder);
        AssertSucceeded (rig.fs.WriteAllText (before.c_str(), std::string (bytes.begin(), bytes.end())));
        rig.io.files[before.string()]  = bytes;
        rig.io.stamps[before.string()] = FileStamp { bytes.size(), 100 };
        AssertSucceeded (DiskImageStore::WriteFileAtomically (before.string(), bytes));

        rig.verbs->onRename = [&] (const std::wstring &, const std::wstring &)
        {
            AssertSucceeded (rig.fs.WriteAllText (after.c_str(), std::string (bytes.begin(), bytes.end())));
            AssertSucceeded (rig.fs.Delete (before.wstring()));
            rig.io.files[after.string()]  = bytes;
            rig.io.stamps[after.string()] = FileStamp { bytes.size(), 100 };
            rig.io.files.erase (before.string());
            rig.io.stamps.erase (before.string());
            std::filesystem::rename (before, after);
            return S_OK;
        };

        rig.Open();
        rig.parts->layout.SetClient (RECT { 0, 0, rig.scaler.ToPx (1280), rig.scaler.ToPx (900) });
        Assert::IsTrue (rig.browser.NavigateToAddress (folder.wstring()));
        rig.parts->listPane.FillList();
        Assert::IsTrue (rig.parts->listPane.SelectRowNamed (L"a.dsk"));
        rig.parts->preview.FillPreview();

        Assert::IsTrue (Rig::WaitFor (rig, [&rig]()
        {
            rig.parts->layout.RecomputeLayout();

            return rig.parts->preview.GetThumbnail().IsReady();
        }));

        rig.parts->layout.RecomputeLayout();
        top = rig.parts->preview.GetContentRect().top;

        rig.parts->rename.BeginRename();

        key.kind = DxuiKeyEventKind::Char;
        key.vk   = L'b';
        rig.parts->rename.HandleRenameKey (key);
        key.kind = DxuiKeyEventKind::Down;
        key.vk   = VK_RETURN;
        rig.parts->rename.HandleRenameKey (key);

        rig.parts->preview.FillPreview();
        rig.parts->layout.RecomputeLayout();

        Assert::AreEqual (after.wstring(), rig.parts->preview.GetThumbnail().GetPath());
        Assert::IsTrue   (rig.parts->preview.GetThumbnail().IsReady(), L"the thumbnail isn't read again");
        Assert::IsTrue   (rig.parts->preview.GetThumbnail().GetVerdict().IsVisible(), L"its infobar stays");
        Assert::AreEqual (top, rig.parts->preview.GetContentRect().top, L"the catalog stays put");

        std::filesystem::remove_all (folder);
    }


    //  In a pane too short for a disk image's thumbnail and catalog, the wheel
    //  scrolls the thumbnail away before the catalog, clipped under the
    //  toolbar, and back again, at 100% and at 150%.
    TEST_METHOD (Preview_TheWheelScrollsTheThumbnailAwayBeforeTheCatalog_At100And150Percent)
    {
        vector<Byte>  bytes = RepairFixtures::MakeDos33Bitmap().damaged;



        for (UINT dpi : { 96u, 144u })
        {
            Rig                    rig;
            bool                   isDrawn  = false;
            std::filesystem::path  folder   = std::filesystem::temp_directory_path() / std::format (L"CassoPreviewScroll{}_{}", ::GetCurrentProcessId(), dpi);
            std::filesystem::path  image    = folder / L"bad.dsk";
            std::string            narrow   = image.string();
            RECT                   start    = {};
            RECT                   banner   = {};
            RECT                   moved    = {};
            RECT                   bar      = {};
            LONG                   catalogY = 0;
            MockDxuiTextRenderer   text;
            MockDxuiTheme          theme;
            DxuiMouseEvent         wheel;

            rig.scaler.SetDpi (dpi);
            rig.prefs.previewVisible = true;
            AssertSucceeded (rig.fs.WriteAllText (image.c_str(), std::string (bytes.begin(), bytes.end())));
            rig.io.files[narrow]  = bytes;
            rig.io.stamps[narrow] = FileStamp { bytes.size(), 100 };
            std::filesystem::create_directories (folder);
            AssertSucceeded (DiskImageStore::WriteFileAtomically (narrow, bytes));
            rig.Open();
            rig.parts->layout.SetClient (RECT { 0, 0, rig.scaler.ToPx (1280), rig.scaler.ToPx (440) });
            Assert::IsTrue (rig.browser.NavigateToAddress (folder.wstring()));
            rig.parts->listPane.FillList();
            Assert::IsTrue (rig.parts->listPane.SelectRowNamed (L"bad.dsk"));
            rig.parts->preview.FillPreview();

            isDrawn = Rig::WaitFor (rig, [&rig]()
            {
                rig.parts->layout.RecomputeLayout();

                return rig.parts->preview.GetThumbnail().IsReady();
            });

            Assert::IsTrue (isDrawn);
            rig.parts->layout.RecomputeLayout();

            start  = rig.parts->preview.GetContentRect();
            banner = rig.parts->preview.GetThumbnail().GetVerdict().GetBounds();

            wheel.kind        = DxuiMouseEventKind::Wheel;
            wheel.wheelDelta  = -1.0f;
            wheel.positionDip = { (banner.left + banner.right) / 2, banner.bottom - 2 };

            Assert::IsTrue (rig.parts->input.HandleMouse (wheel), std::format (L"the thumbnail takes the wheel at {} dpi", dpi).c_str());
            Assert::IsTrue (rig.parts->preview.GetThumbnail().GetVerdict().GetBounds().top < banner.top, L"the thumbnail moves up");

            for (int i = 0; i < 40; i++)
            {
                (void) rig.parts->input.HandleMouse (wheel);
            }

            moved = rig.parts->preview.GetThumbnail().GetVerdict().GetBounds();
            Assert::IsTrue (moved.top < banner.top, L"the infobar scrolls up");
            Assert::IsTrue (rig.parts->preview.GetContentRect().top <= start.top && rig.parts->preview.GetContentRect().top >= rig.parts->preview.GetRect().top,
                            L"the thumbnail stays inside the pane");

            (void) rig.parts->input.HandleMouse (wheel);
            Assert::AreEqual (moved.top, rig.parts->preview.GetThumbnail().GetVerdict().GetBounds().top, L"and stops");

            wheel.wheelDelta = 1.0f;

            for (int i = 0; i < 40; i++)
            {
                (void) rig.parts->input.HandleMouse (wheel);
            }

            Assert::AreEqual (start.top, rig.parts->preview.GetContentRect().top, L"the wheel brings it back");
            Assert::AreEqual (banner.top, rig.parts->preview.GetThumbnail().GetVerdict().GetBounds().top);

            //  One bar down the pane's right edge, from the thumbnail's top,
            //  governs both.
            bar = rig.parts->preview.GetThumbnail().GetScrollbar().GetBounds();
            Assert::IsTrue  (rig.parts->preview.GetThumbnail().GetScrollbar().IsVisible(), L"the pane has its bar");
            Assert::IsTrue  (bar.right == rig.parts->preview.GetRect().right && bar.top < banner.top && bar.bottom == rig.parts->preview.GetRect().bottom, L"down the pane's right edge");
            Assert::AreEqual (0, rig.parts->preview.GetThumbnail().GetScrollbar().GetPos());

            rig.parts->preview.ScrollTo (rig.scaler.ToPx (40));
            rig.parts->preview.SyncScrollbar();
            Assert::IsTrue   (rig.parts->preview.GetThumbnail().GetVerdict().GetBounds().top < banner.top, L"the bar scrolls the thumbnail");
            Assert::AreEqual (rig.scaler.ToPx (40), rig.parts->preview.GetThumbnail().GetScrollbar().GetPos());

            //  Scrolled to the end, with the catalog in view beside the bar.
            isDrawn = Rig::WaitFor (rig, [&rig, &text, &theme]()
            {
                static_cast<DxuiTextView *> (rig.parts->preview.GetText().GetControl())->Measure (text, theme);
                rig.parts->layout.RecomputeLayout();
                rig.parts->preview.ScrollTo (rig.scaler.ToPx (100000));

                return rig.parts->preview.GetContentRect().bottom - rig.parts->preview.GetContentRect().top > rig.scaler.ToPx (40);
            });

            Assert::IsTrue (isDrawn, std::format (L"the catalog in view at {} dpi", dpi).c_str());

            rig.parts->preview.SyncScrollbar();

            //  The arrow over the bar, from its left edge to its right, though
            //  the catalog beside it is the I-beam.
            bar      = rig.parts->preview.GetThumbnail().GetScrollbar().GetBounds();
            catalogY = (rig.parts->preview.GetContentRect().top + rig.parts->preview.GetContentRect().bottom) / 2;

            for (LONG x : { bar.left, (bar.left + bar.right) / 2, bar.right - 1 })
            {
                Assert::IsTrue (rig.parts->input.GetCursorForPoint (POINT { x, catalogY }) == IDC_ARROW, std::format (L"the arrow over the bar at x {}, {} dpi", x, dpi).c_str());
            }

            Assert::IsTrue (rig.parts->input.GetCursorForPoint (POINT { bar.left - rig.scaler.ToPx (20), catalogY }) == IDC_IBEAM, std::format (L"the I-beam over the catalog at {} dpi", dpi).c_str());

            std::filesystem::remove_all (folder);
        }
    }


    //  A DOS 3.3 disk whose VTOC header is lost doesn't mount, so its preview
    //  has its details; their File system line has what the analysis found.
    TEST_METHOD (Preview_DetailsOfADiskWithALostHeaderHaveItsFileSystem)
    {
        vector<Byte>           bytes   = RepairFixtures::MakeDos33HeaderZeroed().damaged;
        Rig                    rig;
        bool                   isDrawn = false;
        std::filesystem::path  folder  = std::filesystem::temp_directory_path() / std::format (L"CassoPreviewHeader{}", ::GetCurrentProcessId());
        std::filesystem::path  image   = folder / L"lost.dsk";
        std::string            narrow  = image.string();
        std::wstring           system;



        rig.prefs.previewVisible = true;
        AssertSucceeded (rig.fs.WriteAllText (image.c_str(), std::string (bytes.begin(), bytes.end())));
        rig.io.files[narrow]  = bytes;
        rig.io.stamps[narrow] = FileStamp { bytes.size(), 100 };
        std::filesystem::create_directories (folder);
        AssertSucceeded (DiskImageStore::WriteFileAtomically (narrow, bytes));
        rig.Open();
        Assert::IsTrue (rig.browser.NavigateToAddress (folder.wstring()));
        rig.parts->listPane.FillList();
        Assert::IsTrue (rig.parts->listPane.SelectRowNamed (L"lost.dsk"));
        rig.parts->preview.FillPreview();

        isDrawn = Rig::WaitFor (rig, [&rig]()
        {
            rig.parts->layout.RecomputeLayout();

            return rig.parts->preview.GetThumbnail().IsReady();
        });

        Assert::IsTrue (isDrawn);
        rig.parts->preview.TakeThumbnail();
        Assert::AreEqual (std::wstring (L"DOS 3.3, damaged"), rig.parts->preview.GetThumbnail().GetFileSystemText());
        Assert::IsTrue   (rig.browser.GetPreview().kind == PreviewContent::Kind::Details, L"the disk doesn't mount");

        for (const DxuiTextView::Row & row : static_cast<DxuiTextView *> (rig.parts->preview.GetText().GetControl())->GetRows())
        {
            system = (row.cells.size() > 1 && row.cells[0].starts_with (L"File system")) ? row.cells[1] : system;
        }

        Assert::AreEqual (std::wstring (L"DOS 3.3, damaged"), system);

        std::filesystem::remove_all (folder);
    }
};
