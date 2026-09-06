#pragma once

#include "Pch.h"

#include "Capture/CaptureOutcome.h"
#include "Capture/CapturedImage.h"
#include "Capture/ScreenshotPlan.h"
#include "Devices/Printer/PngMetadata.h"


class D3DRenderer;
class ClipboardManager;





////////////////////////////////////////////////////////////////////////////////
//
//  ScreenshotCapture
//
//  Executes a resolved ScreenshotPlan against the things only the process has:
//  the D3D context, the clipboard, and the filesystem.
//
//  IT DECIDES NOTHING. Every choice it acts on -- which source, which
//  rectangle, whether a file is wanted, where it goes, what it is called,
//  whether the folder needs creating -- arrives in the plan, resolved and
//  unit-tested in core. What is left here is the part a test could not drive
//  anyway: copying pixels off a GPU and putting bytes on a disk.
//
//  The one thing it does judge is whether each step SUCCEEDED, and that goes
//  into a CaptureOutcome rather than being turned into words here.
//
////////////////////////////////////////////////////////////////////////////////

class ScreenshotCapture
{
public:
    // What the process can offer. Passed in rather than reached for, so the
    // dependencies are visible at the call site instead of buried in a shell
    // pointer this class would otherwise have to know the shape of.
    struct Sources
    {
        HWND                hwnd             = nullptr;
        D3DRenderer *       renderer         = nullptr;
        ClipboardManager *  clipboard        = nullptr;

        // The emulated framebuffer and its lock, for the Raw path. The lock is
        // taken for the copy so a capture is one coherent frame rather than a
        // tear across two.
        const uint32_t *    framebuffer      = nullptr;
        std::mutex *        framebufferMutex = nullptr;
        SIZE                framebufferSize  = {};
    };

    // ACQUISITION AND DELIVERY ARE SEPARATE because they happen at different
    // moments. A Framebuffer capture can be taken at any time, but the other
    // two must be read inside the paint that produced them -- after the CRT
    // composite for Crt, after the chrome walk for Scene, and in both cases
    // before Present, since a presented flip-model back buffer is discarded.
    // So the shell acquires from within its paint hooks and delivers once the
    // frame is over, rather than doing both in one call.
    static HRESULT  AcquirePixels (const ScreenshotPlan & plan,
                                   const Sources &        sources,
                                   CapturedImage &        outImage);

    // The two sinks, run independently: neither failure prevents the other.
    // A refused plan delivers nothing and reports the refusal.
    //
    // `textChunks` is the metadata to stamp on the file, composed by the
    // caller. Empty is valid and produces a file with no tEXt.
    static HRESULT  Deliver (const ScreenshotPlan &        plan,
                             const Sources &               sources,
                             const vector<MetadataEntry> & textChunks,
                             const CapturedImage &         image,
                             CaptureOutcome &              outOutcome);

    // The destination when no folder is configured: <Pictures>\Casso
    // Screenshots. Resolved here rather than in the plan because
    // SHGetKnownFolderPath is a syscall and the resolver must stay drivable
    // by a test; the plan takes the Pictures folder as an input.
    static fs::path  DefaultFolder ();

    // Shell UI the settings page cannot open for itself. Returns false when
    // the user backs out, which is not a failure -- backing out of a picker
    // means keeping what was already configured.
    static bool  BrowseForFolder (HWND owner, fs::path & outFolder);

    // Opens the screenshots folder in Explorer, creating it first if the
    // feature has not written to it yet -- opening nothing would look broken
    // when the setting is merely unused.
    static void  RevealFolder (const string & configuredFolder);

private:
    static HRESULT  CopyFramebuffer (const Sources & sources, CapturedImage & outImage);

    static HRESULT  WriteFile (const ScreenshotPlan &        plan,
                               const CapturedImage &         image,
                               const vector<MetadataEntry> & textChunks);
};
