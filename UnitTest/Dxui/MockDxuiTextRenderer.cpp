#include "Pch.h"

#include "MockDxuiTextRenderer.h"





////////////////////////////////////////////////////////////////////////////////
//
//  SetCannedMetrics
//
////////////////////////////////////////////////////////////////////////////////

void MockDxuiTextRenderer::SetCannedMetrics (const std::wstring & text, SIZE sizeDip)
{
    m_cannedMetrics[text] = sizeDip;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DrawString
//
////////////////////////////////////////////////////////////////////////////////

HRESULT MockDxuiTextRenderer::DrawString (
    const wchar_t      * text,
    float                xDip,
    float                yDip,
    float                widthDip,
    float                heightDip,
    uint32_t             argbColor,
    float                fontSizeDip,
    const wchar_t      * /*fontFamily*/,
    DxuiTextHAlign       hAlign,
    DxuiTextVAlign       vAlign,
    DxuiFontWeight       /*weight*/,
    bool                 /*wrap*/)
{
    RecordedTextCall  call;



    call.kind        = RecordedTextKind::DrawString;
    call.text        = (text != nullptr) ? text : L"";
    call.x           = xDip;
    call.y           = yDip;
    call.width       = widthDip;
    call.height      = heightDip;
    call.argb        = argbColor;
    call.fontSizeDip = fontSizeDip;
    call.hAlign      = hAlign;
    call.vAlign      = vAlign;
    m_calls.push_back (call);

    return S_OK;
}





////////////////////////////////////////////////////////////////////////////////
//
//  PushClipRect
//
////////////////////////////////////////////////////////////////////////////////

HRESULT MockDxuiTextRenderer::PushClipRect (float xDip, float yDip, float widthDip, float heightDip)
{
    RecordedTextCall  call;



    call.kind   = RecordedTextKind::PushClipRect;
    call.x      = xDip;
    call.y      = yDip;
    call.width  = widthDip;
    call.height = heightDip;
    m_calls.push_back (call);

    return S_OK;
}





////////////////////////////////////////////////////////////////////////////////
//
//  PopClipRect
//
////////////////////////////////////////////////////////////////////////////////

HRESULT MockDxuiTextRenderer::PopClipRect()
{
    RecordedTextCall  call;



    call.kind = RecordedTextKind::PopClipRect;
    m_calls.push_back (call);

    return S_OK;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FillRect
//
////////////////////////////////////////////////////////////////////////////////

HRESULT MockDxuiTextRenderer::FillRect (float xDip, float yDip, float widthDip, float heightDip, uint32_t argbColor)
{
    RecordedTextCall  call;



    call.kind   = RecordedTextKind::FillRect;
    call.x      = xDip;
    call.y      = yDip;
    call.width  = widthDip;
    call.height = heightDip;
    call.argb   = argbColor;
    m_calls.push_back (call);

    return S_OK;
}





////////////////////////////////////////////////////////////////////////////////
//
//  MeasureString
//
//  Returns the canned size if the caller registered one for the
//  exact text; otherwise width = text.size() * 7.0f, height = 16.0f.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT MockDxuiTextRenderer::MeasureString (
    const wchar_t * text,
    float           /*fontSizeDip*/,
    const wchar_t * /*fontFamily*/,
    float         & outWidthDip,
    float         & outHeightDip)
{
    constexpr float  s_kFallbackGlyphWidthDip = 7.0f;
    constexpr float  s_kFallbackLineHeightDip = 16.0f;
    std::wstring     key                      = (text != nullptr) ? text : L"";
    auto             it                       = m_cannedMetrics.find (key);



    if (m_measureReturnsZero)
    {
        outWidthDip  = 0.0f;
        outHeightDip = 0.0f;
    }
    else if (it != m_cannedMetrics.end())
    {
        outWidthDip  = (float) it->second.cx;
        outHeightDip = (float) it->second.cy;
    }
    else
    {
        outWidthDip  = (float) key.size() * s_kFallbackGlyphWidthDip;
        outHeightDip = s_kFallbackLineHeightDip;
    }

    return S_OK;
}





////////////////////////////////////////////////////////////////////////////////
//
//  MeasureStringWrapped
//
//  A model, not a layout: the single-line width divided by the box is the
//  line count, and the height is that many lines. Canned metrics are returned
//  untouched -- a test that states a block's size means it.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT MockDxuiTextRenderer::MeasureStringWrapped (
    const wchar_t * text,
    float           fontSizeDip,
    const wchar_t * fontFamily,
    float           maxWidthDip,
    float         & outWidthDip,
    float         & outHeightDip)
{
    std::wstring  key    = (text != nullptr) ? text : L"";
    auto          canned = m_cannedMetrics.find (key);
    HRESULT       hr     = MeasureString (text, fontSizeDip, fontFamily, outWidthDip, outHeightDip);
    int           lines  = 0;



    if (FAILED (hr) || canned != m_cannedMetrics.end() || m_measureReturnsZero)
    {
        return hr;
    }

    if (maxWidthDip < 1.0f || outWidthDip <= maxWidthDip)
    {
        return hr;
    }

    lines = (int) std::ceil (outWidthDip / maxWidthDip);

    outHeightDip *= (float) lines;
    outWidthDip   = maxWidthDip;

    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DrawIconBitmap
//
//  No-op for tests: paint paths in this phase don't exercise icon
//  blitting. Returns S_OK so callers' IGNORE_RETURN_VALUE flows match
//  the runtime path.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT MockDxuiTextRenderer::DrawIconBitmap (
    const uint32_t * /*srcBgraPremul*/,
    int              /*srcWidthPx*/,
    int              /*srcHeightPx*/,
    float            /*destXDip*/,
    float            /*destYDip*/,
    float            /*destWidthDip*/,
    float            /*destHeightDip*/)
{
    return S_OK;
}
