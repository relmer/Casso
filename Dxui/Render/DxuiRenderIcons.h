#pragma once

#include "Pch.h"

class DxuiTextRenderer;
struct DxuiVectorIcon;
struct DxuiVectorIconLayer;





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiRenderIcons
//
//  The renderer's pictures: vector icons as cached Direct2D geometry, SVG
//  icons as parsed documents, pictures as cached bitmaps, and the emulator
//  framebuffer's bitmap.
//
////////////////////////////////////////////////////////////////////////////////

class DxuiRenderIcons
{
public:
    explicit DxuiRenderIcons (DxuiTextRenderer & renderer);

    HRESULT  FillVectorIcon  (const DxuiVectorIcon & icon, float xDip, float yDip, float sizeDip, uint32_t foreground, uint32_t accent);
    HRESULT  DrawSvgIcon     (const std::string & svg, float xDip, float yDip, float sizeDip);
    HRESULT  DrawFramebuffer (const uint32_t * srcBgraPixels, int srcWidthPx, int srcHeightPx, float destXDip, float destYDip, float destWidthDip, float destHeightDip, bool sharp);
    HRESULT  DrawIconBitmap  (const uint32_t * srcBgraPremul, int srcWidthPx, int srcHeightPx, float destXDip, float destYDip, float destWidthDip, float destHeightDip);

    void  ReleaseBitmaps () { m_framebufferBitmaps.clear(); m_iconBitmaps.clear(); }

private:
    HRESULT  GetIconLayerGeometry (const DxuiVectorIcon & icon, const DxuiVectorIconLayer & layer, ID2D1PathGeometry ** outGeometry);

    DxuiTextRenderer  & m_renderer;

    //  A vector icon's layers as Direct2D geometry, built the first time each
    //  is drawn. Keyed by the layer, which is constant data with one address
    //  for the life of the program.
    std::unordered_map<const DxuiVectorIconLayer *, ComPtr<ID2D1PathGeometry>>  m_iconGeometry;

    //  Parsed SVG icons, by their text and the size they were parsed at.
    struct SvgIcon
    {
        size_t                      key  = 0;
        float                       size = 0.0f;
        ComPtr<ID2D1SvgDocument>    document;
    };

    std::vector<SvgIcon>     m_svgIcons;
    static constexpr size_t  kMaxSvgIcons = 64;

    //  A bitmap for each pixel buffer drawn, by its address, rewritten on
    //  each draw. D2D batches the draws of a frame, so one bitmap shared by
    //  every buffer would draw each picture of the frame as the last one.
    struct FramebufferBitmap
    {
        ComPtr<ID2D1Bitmap>  bitmap;
        int                  width  = 0;
        int                  height = 0;
    };

    static constexpr size_t  s_kMaxFramebufferBitmaps = 16;

    std::unordered_map<const uint32_t *, FramebufferBitmap>  m_framebufferBitmaps;

    // One cached bitmap per distinct picture. D2D batches the draws of a
    // frame, so pixels uploaded into a bitmap already drawn once would show
    // up in that earlier draw as well: every picture of a given size would
    // come out as whichever was uploaded last.
    struct IconBitmap
    {
        ComPtr<ID2D1Bitmap>  bitmap;
        uint64_t             key    = 0;
        int                  width  = 0;
        int                  height = 0;
    };

    static constexpr size_t           s_kMaxIconBitmaps    = 64;

    // Creates a bitmap for the picture, uploads it, and adds it to the cache,
    // dropping the oldest entry once the cache is full.
    HRESULT  AddIconBitmap (const uint32_t * srcBgraPremul, int srcWidthPx, int srcHeightPx,
                            uint64_t key, ID2D1Bitmap * & outBitmap);

    // Tells one picture from another; equal pixels share a cache entry.
    static uint64_t  HashPixels (const uint32_t * pixels, size_t count);

    std::vector<IconBitmap>           m_iconBitmaps;
};