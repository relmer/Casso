#pragma once

#include "Pch.h"

#include "Capture/ScreenshotMode.h"





////////////////////////////////////////////////////////////////////////////////
//
//  CaptureSource
//
//  Where a capture's pixels come from. Resolved from the mode and the live
//  presentation state, never chosen by the user.
//
//  Scene is ALWAYS BackBufferRegion, whether or not a desk scene is drawn.
//  The CRT chain's offscreen target holds the PICTURE; the desk scene samples
//  that target onto the glass afterwards, so the composed scene exists only in
//  the back buffer. Crt is the mode that varies: under a scene theme the
//  picture can be taken from the chain's target directly, while under a flat
//  theme the chain composites straight into the back buffer and the picture
//  has to be sub-rected out of it before chrome paints over the rest.
//
////////////////////////////////////////////////////////////////////////////////

enum class CaptureSource
{
    Framebuffer,        // the CPU-side emulated framebuffer
    PictureTarget,      // the CRT chain's own offscreen target
    BackBufferRegion,   // a sub-rectangle of the back buffer
};





////////////////////////////////////////////////////////////////////////////////
//
//  CaptureRefusal
//
//  Why a capture cannot happen. Not an error: refusing is a legitimate result
//  the user is told about, which is why it rides in the plan rather than in an
//  HRESULT.
//
////////////////////////////////////////////////////////////////////////////////

enum class CaptureRefusal
{
    None = 0,
    NothingRendered,    // minimized, and the mode needs rendered pixels
};





////////////////////////////////////////////////////////////////////////////////
//
//  ScreenshotPlanInputs
//
//  Everything the resolver is allowed to know.
//
//  The Pictures folder and the clock are PASSED IN rather than discovered:
//  a resolver that called SHGetKnownFolderPath or GetLocalTime could not be
//  driven by a test, and the constitution's Test Isolation rule is
//  non-negotiable about it.
//
////////////////////////////////////////////////////////////////////////////////

struct ScreenshotPlanInputs
{
    ScreenshotMode  mode                  = ScreenshotMode::Scene;
    bool            saveFile              = true;
    fs::path        folder;                            // empty means the default
    fs::path        defaultPicturesFolder;             // the host's Pictures folder
    RECT            viewportPx            = {};        // the scene viewport
    RECT            picturePx             = {};        // where the picture lands in the target
    SIZE            framebufferSize       = {};        // 560x384
    bool            deskSceneActive       = false;     // false for compact / flat themes
    bool            windowMinimized       = false;
    SYSTEMTIME      when                  = {};
};





////////////////////////////////////////////////////////////////////////////////
//
//  ScreenshotPlan
//
//  What happens when the user presses Screenshot, decided in one place.
//
//  The shell executes this and decides nothing further: it does not pick a
//  source, compose a path, or judge whether a folder should be created. That
//  is the whole point -- the decisions are where the defects live, and here
//  they are reachable by a test without an HWND or a D3D device.
//
////////////////////////////////////////////////////////////////////////////////

struct ScreenshotPlan
{
    CaptureRefusal  refusal             = CaptureRefusal::None;
    CaptureSource   source              = CaptureSource::BackBufferRegion;
    RECT            sourceRectPx        = {};
    bool            hideOverlays        = false;
    bool            writeFile           = false;
    fs::path        outputPath;
    bool            folderMustBeCreated = false;

    // The folder name under the host's Pictures folder. Sits beside
    // "Casso Prints" so a user finds both outputs together.
    static constexpr const wchar_t *  kFolderName = L"Casso Screenshots";

    // The screenshot filename's leading words. "Screenshot" is deliberately
    // absent -- the folder says it -- while "Casso" stays, because the folder
    // is not the context that travels when the file is dragged into an issue.
    static constexpr const wchar_t *  kFileBaseName = L"Casso";

    static constexpr const wchar_t *  kFileExtension = L".png";

    // Resolution is TOTAL: every combination of inputs yields a plan, refusals
    // included, so there is no failure return to forget to check. `exists`
    // reports whether a path is present, and is asked about both the candidate
    // filenames and the destination folder.
    static ScreenshotPlan  Resolve (const ScreenshotPlanInputs &              inputs,
                                    const function<bool (const fs::path &)> & exists);
};
