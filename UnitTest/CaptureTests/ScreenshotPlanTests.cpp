#include "Pch.h"

#include "Capture/ScreenshotPlan.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  ScreenshotPlanTests
//
//  Every decision the Screenshot command makes, driven without an HWND or a
//  D3D device -- which is the whole reason the resolver exists.
//
//  The "exists" predicate stands in for both questions the resolver asks the
//  filesystem: whether a candidate filename is taken, and whether the
//  destination folder is there. Nothing touches disk.
//
////////////////////////////////////////////////////////////////////////////////

namespace ScreenshotPlanTests
{
    static SYSTEMTIME FixedTime()
    {
        SYSTEMTIME   st = {};
        st.wYear   = 2026;
        st.wMonth  = 9;
        st.wDay    = 5;
        st.wHour   = 14;
        st.wMinute = 32;
        st.wSecond = 7;
        return st;
    }


    static bool NothingExists (const fs::path &)
    {
        return false;
    }


    static bool EverythingExists (const fs::path &)
    {
        return true;
    }


    // A window with a scene viewport, a picture inside it, and a desk scene
    // drawn -- the ordinary skeuomorphic case.
    static ScreenshotPlanInputs SceneThemeInputs()
    {
        ScreenshotPlanInputs   in;
        in.mode                  = ScreenshotMode::Scene;
        in.saveFile              = true;
        in.defaultPicturesFolder = L"C:\\Users\\x\\Pictures";
        in.viewportPx            = { 0, 0, 1280, 800 };
        in.picturePx             = { 340, 120, 940, 530 };
        in.framebufferSize       = { 560, 384 };
        in.deskSceneActive       = true;
        in.windowMinimized       = false;
        in.when                  = FixedTime();
        return in;
    }




    TEST_CLASS (ScreenshotPlanTests)
    {
    public:

        //
        //  Source selection
        //

        // Scene reads the back buffer whether or not a desk scene is drawn.
        // The CRT chain's target holds the PICTURE; the scene is composed only
        // in the back buffer, so there is nowhere else for it to come from.
        TEST_METHOD (SceneAlwaysReadsTheBackBufferWithADeskScene)
        {
            ScreenshotPlanInputs   in   = SceneThemeInputs();
            ScreenshotPlan         plan = ScreenshotPlan::Resolve (in, NothingExists);

            Assert::IsTrue (plan.source == CaptureSource::BackBufferRegion);
            Assert::AreEqual ((long) 1280, plan.sourceRectPx.right);
            Assert::AreEqual ((long) 800,  plan.sourceRectPx.bottom);
        }


        TEST_METHOD (SceneAlsoReadsTheBackBufferWithoutADeskScene)
        {
            ScreenshotPlanInputs   in = SceneThemeInputs();
            in.deskSceneActive = false;

            ScreenshotPlan   plan = ScreenshotPlan::Resolve (in, NothingExists);

            Assert::IsTrue (plan.source == CaptureSource::BackBufferRegion);
            Assert::IsTrue (plan.refusal == CaptureRefusal::None,
                L"A flat theme gives the viewport, not an error and not another mode");
            Assert::AreEqual ((long) 1280, plan.sourceRectPx.right);
        }


        //
        //  Full scene takes in the machine's own chrome
        //

        // The defect: in a flat theme the drive widgets are not in the
        // viewport at all -- they sit in a band docked under it -- so Full
        // scene stopped at the picture's edge and came back identical to
        // Screen only, missing the very thing the mode exists to include.
        TEST_METHOD (SceneUnionsTheDriveBandUnderAFlatTheme)
        {
            ScreenshotPlanInputs   in = SceneThemeInputs();
            in.deskSceneActive  = false;
            in.viewportPx       = { 0, 60, 1280, 700 };
            in.machineChromePx  = { 0, 700, 1280, 800 };

            ScreenshotPlan   plan = ScreenshotPlan::Resolve (in, NothingExists);

            Assert::AreEqual ((long) 60,   plan.sourceRectPx.top,    L"still starts at the picture");
            Assert::AreEqual ((long) 800,  plan.sourceRectPx.bottom, L"and now reaches the drives");
            Assert::AreEqual ((long) 1280, plan.sourceRectPx.right);
        }


        // A desk scene models the drives inside the viewport, so the shell
        // passes nothing here and the rect must not move.
        TEST_METHOD (SceneIsTheViewportAloneWhenThereIsNoSuchChrome)
        {
            ScreenshotPlanInputs   in = SceneThemeInputs();
            in.viewportPx      = { 0, 60, 1280, 700 };
            in.machineChromePx = {};

            ScreenshotPlan   plan = ScreenshotPlan::Resolve (in, NothingExists);

            Assert::AreEqual ((long) 60,  plan.sourceRectPx.top);
            Assert::AreEqual ((long) 700, plan.sourceRectPx.bottom);
        }


        // Only Full scene widens. Screen only is the picture by definition,
        // and widening it would make the two modes the same from the other
        // direction.
        TEST_METHOD (TheOtherTwoModesIgnoreTheMachineChrome)
        {
            ScreenshotPlanInputs   in = SceneThemeInputs();
            in.deskSceneActive  = false;
            in.picturePx        = { 340, 120, 940, 530 };
            in.machineChromePx  = { 0, 700, 1280, 800 };

            in.mode = ScreenshotMode::Crt;
            ScreenshotPlan   crt = ScreenshotPlan::Resolve (in, NothingExists);
            Assert::AreEqual ((long) 530, crt.sourceRectPx.bottom);

            in.mode = ScreenshotMode::Raw;
            ScreenshotPlan   raw = ScreenshotPlan::Resolve (in, NothingExists);
            Assert::AreEqual ((long) 384, raw.sourceRectPx.bottom);
        }


        //
        //  Union, on its own
        //

        // An empty rect means "there is no such thing right now". Unioning it
        // as a region at the origin would drag every flat-theme capture up to
        // the top-left corner of the window.
        TEST_METHOD (UnionTreatsAnEmptyRectAsAbsentRatherThanAsTheOrigin)
        {
            RECT   real = { 100, 200, 300, 400 };

            Assert::AreEqual ((long) 100, ScreenshotPlan::Union (real, RECT{}).left);
            Assert::AreEqual ((long) 200, ScreenshotPlan::Union (real, RECT{}).top);
            Assert::AreEqual ((long) 100, ScreenshotPlan::Union (RECT{}, real).left);
            Assert::AreEqual ((long) 0,   ScreenshotPlan::Union (RECT{}, RECT{}).right);
        }


        TEST_METHOD (UnionTakesTheOuterEdgeOnEverySide)
        {
            RECT   a = { 10, 20, 100, 200 };
            RECT   b = {  5, 50, 400,  60 };
            RECT   u = ScreenshotPlan::Union (a, b);

            Assert::AreEqual ((long) 5,   u.left);
            Assert::AreEqual ((long) 20,  u.top);
            Assert::AreEqual ((long) 400, u.right);
            Assert::AreEqual ((long) 200, u.bottom);
        }


        // Crt is the mode that splits: with a scene it can take the picture
        // straight from the chain's target, without one it must sub-rect the
        // back buffer before chrome paints.
        TEST_METHOD (CrtReadsThePictureTargetWhenASceneIsDrawn)
        {
            ScreenshotPlanInputs   in = SceneThemeInputs();
            in.mode = ScreenshotMode::Crt;

            ScreenshotPlan   plan = ScreenshotPlan::Resolve (in, NothingExists);

            Assert::IsTrue (plan.source == CaptureSource::PictureTarget);
            Assert::AreEqual ((long) 340, plan.sourceRectPx.left);
            Assert::AreEqual ((long) 940, plan.sourceRectPx.right);
        }


        TEST_METHOD (CrtReadsTheBackBufferUnderAFlatTheme)
        {
            ScreenshotPlanInputs   in = SceneThemeInputs();
            in.mode            = ScreenshotMode::Crt;
            in.deskSceneActive = false;

            ScreenshotPlan   plan = ScreenshotPlan::Resolve (in, NothingExists);

            Assert::IsTrue (plan.source == CaptureSource::BackBufferRegion);
            Assert::AreEqual ((long) 340, plan.sourceRectPx.left);
        }


        TEST_METHOD (RawReadsTheFramebufferAtItsFixedSize)
        {
            ScreenshotPlanInputs   in = SceneThemeInputs();
            in.mode = ScreenshotMode::Raw;

            ScreenshotPlan   plan = ScreenshotPlan::Resolve (in, NothingExists);

            Assert::IsTrue (plan.source == CaptureSource::Framebuffer);
            Assert::AreEqual ((long) 0,   plan.sourceRectPx.left);
            Assert::AreEqual ((long) 0,   plan.sourceRectPx.top);
            Assert::AreEqual ((long) 560, plan.sourceRectPx.right);
            Assert::AreEqual ((long) 384, plan.sourceRectPx.bottom);
        }


        // The window is 1280x800 and raw is still 560x384. That independence
        // is the entire reason the mode exists.
        TEST_METHOD (RawIgnoresTheWindowSize)
        {
            ScreenshotPlanInputs   in = SceneThemeInputs();
            in.mode       = ScreenshotMode::Raw;
            in.viewportPx = { 0, 0, 400, 300 };

            ScreenshotPlan   plan = ScreenshotPlan::Resolve (in, NothingExists);

            Assert::AreEqual ((long) 560, plan.sourceRectPx.right);
            Assert::AreEqual ((long) 384, plan.sourceRectPx.bottom);
        }


        //
        //  Overlay suppression
        //

        TEST_METHOD (RenderedModesHideTheOverlays)
        {
            ScreenshotPlanInputs   scene = SceneThemeInputs();
            ScreenshotPlanInputs   crt   = SceneThemeInputs();
            crt.mode = ScreenshotMode::Crt;

            Assert::IsTrue (ScreenshotPlan::Resolve (scene, NothingExists).hideOverlays);
            Assert::IsTrue (ScreenshotPlan::Resolve (crt,   NothingExists).hideOverlays);
        }


        // The overlays were never in the framebuffer, so hiding them would buy
        // a repaint and nothing else.
        TEST_METHOD (RawDoesNotHideTheOverlays)
        {
            ScreenshotPlanInputs   in = SceneThemeInputs();
            in.mode = ScreenshotMode::Raw;

            Assert::IsFalse (ScreenshotPlan::Resolve (in, NothingExists).hideOverlays);
        }


        //
        //  Refusal
        //

        TEST_METHOD (MinimizedRefusesSceneAndCrt)
        {
            ScreenshotPlanInputs   scene = SceneThemeInputs();
            ScreenshotPlanInputs   crt   = SceneThemeInputs();
            scene.windowMinimized = true;
            crt.windowMinimized   = true;
            crt.mode              = ScreenshotMode::Crt;

            ScreenshotPlan   scenePlan = ScreenshotPlan::Resolve (scene, NothingExists);
            ScreenshotPlan   crtPlan   = ScreenshotPlan::Resolve (crt,   NothingExists);

            Assert::IsTrue (scenePlan.refusal == CaptureRefusal::NothingRendered);
            Assert::IsTrue (crtPlan.refusal   == CaptureRefusal::NothingRendered);
            Assert::IsFalse (scenePlan.writeFile, L"A refused capture writes nothing");
            Assert::IsTrue (scenePlan.outputPath.empty());
        }


        // Raw does not care what is on screen, which is what makes capturing
        // from a minimized window a sensible thing to do at all.
        TEST_METHOD (MinimizedStillAllowsRaw)
        {
            ScreenshotPlanInputs   in = SceneThemeInputs();
            in.mode            = ScreenshotMode::Raw;
            in.windowMinimized = true;

            ScreenshotPlan   plan = ScreenshotPlan::Resolve (in, NothingExists);

            Assert::IsTrue (plan.refusal == CaptureRefusal::None);
            Assert::IsTrue (plan.writeFile);
            Assert::IsFalse (plan.outputPath.empty());
        }


        //
        //  Destination
        //

        TEST_METHOD (EmptyFolderFallsBackToPicturesCassoScreenshots)
        {
            ScreenshotPlanInputs   in   = SceneThemeInputs();
            ScreenshotPlan         plan = ScreenshotPlan::Resolve (in, NothingExists);

            Assert::AreEqual (wstring (L"C:\\Users\\x\\Pictures\\Casso Screenshots"),
                              plan.outputPath.parent_path().wstring());
            Assert::AreEqual (wstring (L"Casso 2026-09-05 143207.png"),
                              plan.outputPath.filename().wstring());
        }


        TEST_METHOD (AConfiguredFolderOverridesTheDefault)
        {
            ScreenshotPlanInputs   in = SceneThemeInputs();
            in.folder = L"D:\\Shots";

            ScreenshotPlan   plan = ScreenshotPlan::Resolve (in, NothingExists);

            Assert::AreEqual (wstring (L"D:\\Shots"), plan.outputPath.parent_path().wstring());
        }


        TEST_METHOD (AnAbsentFolderIsMarkedForCreation)
        {
            ScreenshotPlanInputs   in   = SceneThemeInputs();
            ScreenshotPlan         plan = ScreenshotPlan::Resolve (in, NothingExists);

            Assert::IsTrue (plan.folderMustBeCreated);
        }


        // A folder deleted since it was configured is recreated by policy
        // rather than by a rescue path in the shell.
        TEST_METHOD (APresentFolderIsNotMarkedForCreation)
        {
            ScreenshotPlanInputs   in = SceneThemeInputs();

            // Everything exists: the folder is there, and so are the first
            // candidate names, so this also exercises the suffix.
            ScreenshotPlan   plan = ScreenshotPlan::Resolve (in, EverythingExists);

            Assert::IsFalse (plan.folderMustBeCreated);
        }


        TEST_METHOD (ACollidingNameTakesASuffix)
        {
            ScreenshotPlanInputs   in = SceneThemeInputs();

            ScreenshotPlan   plan = ScreenshotPlan::Resolve (in,
                [] (const fs::path & p)
                {
                    return p.filename().wstring() == L"Casso 2026-09-05 143207.png";
                });

            Assert::AreEqual (wstring (L"Casso 2026-09-05 143207 (2).png"),
                              plan.outputPath.filename().wstring());
        }


        //
        //  Saving switched off
        //

        TEST_METHOD (SavingOffPlansNoFileButStillCaptures)
        {
            ScreenshotPlanInputs   in = SceneThemeInputs();
            in.saveFile = false;

            ScreenshotPlan   plan = ScreenshotPlan::Resolve (in, NothingExists);

            Assert::IsFalse (plan.writeFile);
            Assert::IsTrue (plan.outputPath.empty());
            Assert::IsFalse (plan.folderMustBeCreated,
                L"No file means no folder to create");
            Assert::IsTrue (plan.refusal == CaptureRefusal::None,
                L"Clipboard-only is a choice, not a refusal");
            Assert::IsTrue (plan.source == CaptureSource::BackBufferRegion);
        }
    };
}
