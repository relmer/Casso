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
//  CountWrappedLines
//
//  Greedy word wrap, the rule every real text renderer uses: put each word on
//  the current line if it fits, otherwise start a new one. A word longer than
//  the box gets a line of its own rather than being broken, which is also
//  what the real one does at these sizes.
//
////////////////////////////////////////////////////////////////////////////////

int MockDxuiTextRenderer::CountWrappedLines (const std::wstring & text,
                                             float                glyphWidthDip,
                                             float                maxWidthDip)
{
    size_t  start = 0;
    float   line  = 0.0f;
    int     lines = 1;



    if (glyphWidthDip <= 0.0f)
    {
        return 1;
    }

    while (start <= text.size())
    {
        size_t  end   = text.find (L' ', start);
        size_t  count = (end == std::wstring::npos ? text.size() : end) - start;
        float   word  = (float) count * glyphWidthDip;
        float   space = (line > 0.0f) ? glyphWidthDip : 0.0f;

        if (line > 0.0f && line + space + word > maxWidthDip)
        {
            lines++;
            line = word;
        }
        else
        {
            line += space + word;
        }

        if (end == std::wstring::npos)
        {
            break;
        }

        start = end + 1;
    }

    return lines;
}





////////////////////////////////////////////////////////////////////////////////
//
//  MeasureStringWrapped
//
//  A model, not a layout, but a model that BREAKS AT WORDS the way a real
//  renderer does. Dividing the single-line width by the box was the simpler
//  rule and it flattered every caller: it says a box of exactly width/2 fits
//  two lines, which no word-breaking renderer can promise. A widget that
//  sized a box from that arithmetic then painted a third line with one word
//  stranded on it, and nothing here could reproduce it.
//
//  Canned metrics are returned untouched -- a test that states a block's size
//  means it, wrapped or not.
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

    lines = CountWrappedLines (key, outWidthDip / (float) (key.empty() ? 1 : key.size()), maxWidthDip);

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
