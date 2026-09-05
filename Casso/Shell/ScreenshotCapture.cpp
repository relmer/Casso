#include "Pch.h"

#include "ScreenshotCapture.h"

#include "ClipboardManager.h"
#include "D3DRenderer.h"

#include "Devices/Printer/PngCodec.h"





////////////////////////////////////////////////////////////////////////////////
//
//  CopyFramebuffer
//
//  The Raw path: the emulated framebuffer straight out of memory.
//
//  No conversion. The framebuffer's 0xAARRGGBB words are B,G,R,A in memory,
//  which is what a CapturedImage holds, so this is a memcpy under the lock
//  rather than a per-pixel walk.
//
//  The lock is held for the copy alone. Encoding and writing happen outside
//  it, because the emulator thread should not wait on a disk.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT ScreenshotCapture::CopyFramebuffer (const Sources & sources, CapturedImage & outImage)
{
    HRESULT   hr     = S_OK;
    size_t    bytes  = 0;
    bool      usable = false;



    usable = (sources.framebuffer != nullptr)
          && (sources.framebufferSize.cx > 0)
          && (sources.framebufferSize.cy > 0);
    CBRAEx (usable, E_UNEXPECTED);

    outImage.widthPx  = (int) sources.framebufferSize.cx;
    outImage.heightPx = (int) sources.framebufferSize.cy;

    bytes = (size_t) outImage.widthPx * outImage.heightPx * CapturedImage::kBytesPerPixel;
    outImage.bgra.resize (bytes);

    if (sources.framebufferMutex != nullptr)
    {
        std::lock_guard<std::mutex>   lock (*sources.framebufferMutex);

        memcpy (outImage.bgra.data(), sources.framebuffer, bytes);
    }
    else
    {
        memcpy (outImage.bgra.data(), sources.framebuffer, bytes);
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  AcquirePixels
//
//  Routes the plan's source to the thing that can supply it. The switch is
//  exhaustive and has no default arm, so a source added later fails to compile
//  here rather than silently capturing the wrong thing.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT ScreenshotCapture::AcquirePixels (const ScreenshotPlan & plan,
                                          const Sources &        sources,
                                          CapturedImage &        outImage)
{
    HRESULT   hr = S_OK;



    switch (plan.source)
    {
        case CaptureSource::Framebuffer:
            hr = CopyFramebuffer (sources, outImage);
            CHR (hr);
            break;

        case CaptureSource::PictureTarget:
            CBRAEx (sources.renderer != nullptr, E_UNEXPECTED);
            hr = sources.renderer->CaptureSceneTargetRegion (plan.sourceRectPx, outImage);
            CHR (hr);
            break;

        case CaptureSource::BackBufferRegion:
            CBRAEx (sources.renderer != nullptr, E_UNEXPECTED);
            hr = sources.renderer->CaptureBackBufferRegion (plan.sourceRectPx, outImage);
            CHR (hr);
            break;
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  WriteFile
//
//  Encode and write, with the folder created first if the plan said so.
//
//  The conversion to RGBA happens here and nowhere else: the PNG encoder is
//  the only consumer that wants that order.
//
//  dpi 0 is deliberate -- it makes WIC write 96/96, which is indistinguishable
//  from no pHYs at all. Casso presents the picture at square pixels, so a
//  square-pixel file already matches the application, and declaring anything
//  else would make the two disagree in viewers that honor the chunk.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT ScreenshotCapture::WriteFile (const ScreenshotPlan &        plan,
                                      const CapturedImage &         image,
                                      const vector<MetadataEntry> & textChunks)
{
    HRESULT           hr        = S_OK;
    RgbaImage         rgba;
    vector<Byte>      png;
    std::error_code   ec;
    bool              isOpen    = false;
    bool              wroteWell = false;



    if (plan.folderMustBeCreated)
    {
        fs::create_directories (plan.outputPath.parent_path(), ec);
    }

    hr = CapturedImage::ToRgbaImage (image, rgba);
    CHR (hr);

    hr = PngCodec::EncodeRgba (rgba, 0, textChunks, png);
    CHR (hr);

    {
        std::ofstream   out (plan.outputPath, std::ios::binary | std::ios::trunc);

        isOpen = out.is_open();
        CBREx (isOpen, HRESULT_FROM_WIN32 (ERROR_ACCESS_DENIED));

        out.write ((const char *) png.data(), (std::streamsize) png.size());

        wroteWell = out.good();
        CBREx (wroteWell, HRESULT_FROM_WIN32 (ERROR_WRITE_FAULT));
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Deliver
//
//  Feed both sinks from one captured image.
//
//  THE SINKS ARE INDEPENDENT ON PURPOSE. A clipboard held by another
//  application must not cost the file, and a full disk must not cost the paste
//  the user was about to make. So neither result is checked before the other
//  runs, and both land in the outcome for the caller to report.
//
//  A refusal returns S_OK. The user asked for something reasonable that could
//  not be done, which is a thing to tell them rather than an error to handle.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT ScreenshotCapture::Deliver (const ScreenshotPlan &        plan,
                                    const Sources &               sources,
                                    const vector<MetadataEntry> & textChunks,
                                    const CapturedImage &         image,
                                    CaptureOutcome &              outOutcome)
{
    HRESULT         hr     = S_OK;
    HRESULT         hrFile = S_OK;
    bool            usable = false;



    outOutcome         = CaptureOutcome();
    outOutcome.refusal = plan.refusal;

    BAIL_OUT_IF (plan.refusal != CaptureRefusal::None, S_OK);

    usable = image.IsValid();
    CBREx (usable, E_UNEXPECTED);

    if (sources.clipboard != nullptr)
    {
        outOutcome.clipboardOk = sources.clipboard->CopyScreenshot (sources.hwnd, image);
    }

    if (plan.writeFile)
    {
        outOutcome.writeAttempted = true;
        outOutcome.path           = plan.outputPath;

        hrFile = WriteFile (plan, image, textChunks);

        outOutcome.fileWritten = SUCCEEDED (hrFile);

        // A failed write is reported through the outcome, not the result code.
        // The capture itself worked and the clipboard may well hold it.
        IGNORE_RETURN_VALUE (hrFile, S_OK);
    }

Error:
    return hr;
}
