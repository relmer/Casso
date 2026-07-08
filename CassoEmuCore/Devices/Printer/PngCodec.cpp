#include "Pch.h"

#include "Devices/Printer/PngCodec.h"

#pragma comment (lib, "windowscodecs.lib")




////////////////////////////////////////////////////////////////////////////////
//
//  CreateFactory
//
//  Instantiates the WIC imaging factory. The caller must already have COM
//  initialised on this thread.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT PngCodec::CreateFactory (ComPtr<IWICImagingFactory> & outFactory)
{
    HRESULT   hr = S_OK;

    hr = CoCreateInstance (CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER,
                           IID_PPV_ARGS (&outFactory));
    CHR (hr);

Error:
    return hr;
}




////////////////////////////////////////////////////////////////////////////////
//
//  DecodeFirstFrame
//
//  Opens a PNG held in memory and hands back its first frame for pixel or
//  metadata access.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT PngCodec::DecodeFirstFrame (
    const vector<Byte> &            png,
    ComPtr<IWICImagingFactory> &    outFactory,
    ComPtr<IWICBitmapFrameDecode> & outFrame)
{
    HRESULT                     hr = S_OK;
    ComPtr<IWICStream>          stream;
    ComPtr<IWICBitmapDecoder>   decoder;

    CBREx (!png.empty (), E_INVALIDARG);

    hr = CreateFactory (outFactory);
    CHR (hr);

    hr = outFactory->CreateStream (&stream);
    CHR (hr);

    hr = stream->InitializeFromMemory (const_cast<BYTE *> (png.data ()), (DWORD) png.size ());
    CHR (hr);

    hr = outFactory->CreateDecoderFromStream (stream.Get (), nullptr,
                                              WICDecodeMetadataCacheOnDemand, &decoder);
    CHR (hr);

    hr = decoder->GetFrame (0, &outFrame);
    CHR (hr);

Error:
    return hr;
}




////////////////////////////////////////////////////////////////////////////////
//
//  EncodeRgba
//
//  Writes a 32bpp RGBA image to an in-memory PNG, stamping the physical
//  resolution as the pHYs chunk. WriteSource lets WIC convert to the encoder's
//  native channel order, so the caller's R,G,B,A byte layout is preserved.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT PngCodec::EncodeRgba (const RgbaImage & image, int dpi, vector<Byte> & outPng)
{
    HRESULT                         hr     = S_OK;
    ComPtr<IWICImagingFactory>      factory;
    ComPtr<IWICBitmap>              bitmap;
    ComPtr<IStream>                 stream;
    ComPtr<IWICBitmapEncoder>       encoder;
    ComPtr<IWICBitmapFrameEncode>   frame;
    ComPtr<IPropertyBag2>           props;
    WICRect                         rect   = { 0, 0, 0, 0 };
    HGLOBAL                         handle = nullptr;
    void *                          memory = nullptr;
    SIZE_T                          size   = 0;
    double                          res    = 0.0;

    CBREx (image.width > 0 && image.height > 0, E_INVALIDARG);
    CBREx (image.rgba.size () >= (size_t) image.width * image.height * 4, E_INVALIDARG);

    hr = CreateFactory (factory);
    CHR (hr);

    hr = factory->CreateBitmapFromMemory ((UINT) image.width, (UINT) image.height,
                                          GUID_WICPixelFormat32bppRGBA,
                                          (UINT) image.width * 4, (UINT) image.rgba.size (),
                                          const_cast<BYTE *> (image.rgba.data ()), &bitmap);
    CHR (hr);

    hr = CreateStreamOnHGlobal (nullptr, TRUE, &stream);
    CHR (hr);

    hr = factory->CreateEncoder (GUID_ContainerFormatPng, nullptr, &encoder);
    CHR (hr);

    hr = encoder->Initialize (stream.Get (), WICBitmapEncoderNoCache);
    CHR (hr);

    hr = encoder->CreateNewFrame (&frame, &props);
    CHR (hr);

    hr = frame->Initialize (props.Get ());
    CHR (hr);

    hr = frame->SetSize ((UINT) image.width, (UINT) image.height);
    CHR (hr);

    res = (double) (dpi > 0 ? dpi : 96);
    hr  = frame->SetResolution (res, res);
    CHR (hr);

    rect.Width  = image.width;
    rect.Height = image.height;
    hr = frame->WriteSource (bitmap.Get (), &rect);
    CHR (hr);

    hr = frame->Commit ();
    CHR (hr);

    hr = encoder->Commit ();
    CHR (hr);

    hr = GetHGlobalFromStream (stream.Get (), &handle);
    CHR (hr);

    size   = GlobalSize (handle);
    memory = GlobalLock (handle);
    CPREx (memory, E_FAIL);
    outPng.assign ((Byte *) memory, (Byte *) memory + size);
    GlobalUnlock (handle);

Error:
    return hr;
}




////////////////////////////////////////////////////////////////////////////////
//
//  EncodeIndexed
//
//  Writes an 8bpp palette-indexed PNG. PNG supports 8bpp indexed natively, so
//  the index bytes round-trip losslessly.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT PngCodec::EncodeIndexed (
    int                  width,
    int                  height,
    const vector<Byte> & indices,
    const uint32_t *     palette,
    int                  paletteCount,
    int                  dpi,
    vector<Byte> &       outPng)
{
    HRESULT                         hr     = S_OK;
    ComPtr<IWICImagingFactory>      factory;
    ComPtr<IStream>                 stream;
    ComPtr<IWICBitmapEncoder>       encoder;
    ComPtr<IWICBitmapFrameEncode>   frame;
    ComPtr<IPropertyBag2>           props;
    ComPtr<IWICPalette>             wicPalette;
    WICPixelFormatGUID              format = GUID_WICPixelFormat8bppIndexed;
    HGLOBAL                         handle = nullptr;
    void *                          memory = nullptr;
    SIZE_T                          size   = 0;
    double                          res    = 0.0;

    CBREx (width > 0 && height > 0, E_INVALIDARG);
    CBREx (paletteCount > 0 && paletteCount <= 256, E_INVALIDARG);
    CBREx (palette != nullptr, E_INVALIDARG);
    CBREx (indices.size () >= (size_t) width * height, E_INVALIDARG);

    hr = CreateFactory (factory);
    CHR (hr);

    hr = CreateStreamOnHGlobal (nullptr, TRUE, &stream);
    CHR (hr);

    hr = factory->CreateEncoder (GUID_ContainerFormatPng, nullptr, &encoder);
    CHR (hr);

    hr = encoder->Initialize (stream.Get (), WICBitmapEncoderNoCache);
    CHR (hr);

    hr = encoder->CreateNewFrame (&frame, &props);
    CHR (hr);

    hr = frame->Initialize (props.Get ());
    CHR (hr);

    hr = frame->SetSize ((UINT) width, (UINT) height);
    CHR (hr);

    res = (double) (dpi > 0 ? dpi : 96);
    hr  = frame->SetResolution (res, res);
    CHR (hr);

    hr = frame->SetPixelFormat (&format);
    CHR (hr);
    CBREx (format == GUID_WICPixelFormat8bppIndexed, E_FAIL);

    hr = factory->CreatePalette (&wicPalette);
    CHR (hr);

    hr = wicPalette->InitializeCustom (const_cast<WICColor *> (palette), (UINT) paletteCount);
    CHR (hr);

    hr = frame->SetPalette (wicPalette.Get ());
    CHR (hr);

    hr = frame->WritePixels ((UINT) height, (UINT) width, (UINT) indices.size (),
                             const_cast<BYTE *> (indices.data ()));
    CHR (hr);

    hr = frame->Commit ();
    CHR (hr);

    hr = encoder->Commit ();
    CHR (hr);

    hr = GetHGlobalFromStream (stream.Get (), &handle);
    CHR (hr);

    size   = GlobalSize (handle);
    memory = GlobalLock (handle);
    CPREx (memory, E_FAIL);
    outPng.assign ((Byte *) memory, (Byte *) memory + size);
    GlobalUnlock (handle);

Error:
    return hr;
}




////////////////////////////////////////////////////////////////////////////////
//
//  DecodeRgba
//
//  Decodes any PNG to 32bpp RGBA.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT PngCodec::DecodeRgba (const vector<Byte> & png, RgbaImage & outImage)
{
    HRESULT                         hr        = S_OK;
    ComPtr<IWICImagingFactory>      factory;
    ComPtr<IWICBitmapFrameDecode>   frame;
    ComPtr<IWICBitmapSource>        converted;
    UINT                            width     = 0;
    UINT                            height    = 0;

    hr = DecodeFirstFrame (png, factory, frame);
    CHR (hr);

    hr = WICConvertBitmapSource (GUID_WICPixelFormat32bppRGBA, frame.Get (), &converted);
    CHR (hr);

    hr = converted->GetSize (&width, &height);
    CHR (hr);

    outImage.Allocate ((int) width, (int) height, 0, 0, 0);

    hr = converted->CopyPixels (nullptr, width * 4, (UINT) outImage.rgba.size (), outImage.rgba.data ());
    CHR (hr);

Error:
    return hr;
}




////////////////////////////////////////////////////////////////////////////////
//
//  DecodeIndexed
//
//  Recovers the 8bpp index plane from a PNG produced by EncodeIndexed. Fails if
//  the image is not natively 8bpp indexed (guards against feeding it a
//  truecolour PNG that would silently lose the index semantics).
//
////////////////////////////////////////////////////////////////////////////////

HRESULT PngCodec::DecodeIndexed (
    const vector<Byte> & png,
    int &                outWidth,
    int &                outHeight,
    vector<Byte> &       outIndices)
{
    HRESULT                         hr      = S_OK;
    ComPtr<IWICImagingFactory>      factory;
    ComPtr<IWICBitmapFrameDecode>   frame;
    WICPixelFormatGUID              format;
    UINT                            width   = 0;
    UINT                            height  = 0;

    hr = DecodeFirstFrame (png, factory, frame);
    CHR (hr);

    hr = frame->GetPixelFormat (&format);
    CHR (hr);
    CBREx (format == GUID_WICPixelFormat8bppIndexed, E_FAIL);

    hr = frame->GetSize (&width, &height);
    CHR (hr);

    outIndices.assign ((size_t) width * height, 0);

    hr = frame->CopyPixels (nullptr, width, (UINT) outIndices.size (), outIndices.data ());
    CHR (hr);

    outWidth  = (int) width;
    outHeight = (int) height;

Error:
    return hr;
}




////////////////////////////////////////////////////////////////////////////////
//
//  ReadDpi
//
//  Returns the pHYs physical resolution in dpi. WIC reports 96 for an image
//  with no pHYs chunk, so absence and an explicit 96 dpi are indistinguishable.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT PngCodec::ReadDpi (const vector<Byte> & png, int & outDpi)
{
    HRESULT                         hr     = S_OK;
    ComPtr<IWICImagingFactory>      factory;
    ComPtr<IWICBitmapFrameDecode>   frame;
    double                          dx     = 0.0;
    double                          dy     = 0.0;

    hr = DecodeFirstFrame (png, factory, frame);
    CHR (hr);

    hr = frame->GetResolution (&dx, &dy);
    CHR (hr);

    outDpi = (int) (dx + 0.5);

Error:
    return hr;
}
