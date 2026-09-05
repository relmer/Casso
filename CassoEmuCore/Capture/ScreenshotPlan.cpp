#include "Pch.h"

#include "Capture/ScreenshotPlan.h"
#include "Devices/Printer/PrintFileNaming.h"





////////////////////////////////////////////////////////////////////////////////
//
//  Resolve
//
//  Four questions in order: where the pixels come from, whether they can be
//  had at all, whether a file is wanted, and what to call it.
//
//  RAW IS NEVER REFUSED. It reads the CPU-side framebuffer, which exists
//  whether or not anything is on screen, so a minimized window costs the other
//  two modes and not this one. That asymmetry is deliberate and is what makes
//  "capture while minimized" a sensible thing to do at all.
//
//  The overlay hide-list is a consequence rather than a separate decision: the
//  frame-rate readout, the pose readout, the compass and the mouse-capture
//  banner are painted over the rendered image, so any mode that reads rendered
//  pixels has to suppress them, and the one that does not, does not.
//
////////////////////////////////////////////////////////////////////////////////

ScreenshotPlan ScreenshotPlan::Resolve (const ScreenshotPlanInputs &              inputs,
                                        const function<bool (const fs::path &)> & exists)
{
    ScreenshotPlan   plan;
    fs::path         folder;
    bool             needsRendered = false;



    //  Source, and the rectangle that goes with it.
    switch (inputs.mode)
    {
        case ScreenshotMode::Raw:
            plan.source       = CaptureSource::Framebuffer;
            plan.sourceRectPx = { 0, 0, inputs.framebufferSize.cx, inputs.framebufferSize.cy };
            break;

        case ScreenshotMode::Crt:
            plan.source       = inputs.deskSceneActive ? CaptureSource::PictureTarget
                                                       : CaptureSource::BackBufferRegion;
            plan.sourceRectPx = inputs.picturePx;
            break;

        case ScreenshotMode::Scene:
            plan.source       = CaptureSource::BackBufferRegion;
            plan.sourceRectPx = inputs.viewportPx;
            break;
    }

    needsRendered      = (plan.source != CaptureSource::Framebuffer);
    plan.hideOverlays  = needsRendered;

    //  A minimized window has nothing rendered to read back. Saying so beats
    //  writing a black or stale image and letting the user discover it later.
    if (needsRendered && inputs.windowMinimized)
    {
        plan.refusal = CaptureRefusal::NothingRendered;
    }

    plan.writeFile = inputs.saveFile && (plan.refusal == CaptureRefusal::None);

    if (plan.writeFile)
    {
        //  An empty stored folder means the default, so the default can move
        //  in a later release without stranding anyone on a stale path.
        folder = inputs.folder.empty() ? (inputs.defaultPicturesFolder / kFolderName)
                                       : inputs.folder;

        plan.folderMustBeCreated = !exists (folder);

        plan.outputPath = PrintFileNaming::ComposeTimestampedPath (folder,
                                                                   kFileBaseName,
                                                                   kFileExtension,
                                                                   inputs.when,
                                                                   exists);
    }

    return plan;
}
