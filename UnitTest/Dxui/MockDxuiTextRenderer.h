#pragma once

#include "Pch.h"



////////////////////////////////////////////////////////////////////////////////
//
//  MockDxuiTextRenderer
//
//  Recording `IDxuiTextRenderer` implementation. `DrawString` appends
//  a `RecordedTextCall` to an internal vector. `Measure` returns a
//  canned size: callers can register exact (text -> size) pairs via
//  `SetCannedMetrics`; unregistered text falls back to
//      width  = text.size() * 7.0f
//      height = 16.0f
//
//  No DirectWrite factory is created. Every method is allocation-
//  free on the steady state.
//
////////////////////////////////////////////////////////////////////////////////



enum class RecordedTextKind
{
    DrawString,
    FillRect,
    PushClipRect,
    PopClipRect,
};


struct RecordedTextCall
{
    RecordedTextKind  kind         = RecordedTextKind::DrawString;
    std::wstring      text;
    float             x            = 0.0f;
    float             y            = 0.0f;
    float             width        = 0.0f;
    float             height       = 0.0f;
    uint32_t          argb         = 0;
    float             fontSizeDip  = 0.0f;
    DxuiTextHAlign    hAlign       = DxuiTextHAlign::Left;
    DxuiTextVAlign    vAlign       = DxuiTextVAlign::Top;
};



class MockDxuiTextRenderer : public IDxuiTextRenderer
{
public:
    MockDxuiTextRenderer  () = default;
    ~MockDxuiTextRenderer() override = default;

    const std::vector<RecordedTextCall> &  Calls() const { return m_calls; }
    void  Reset() { m_calls.clear(); }

    void  SetCannedMetrics  (const std::wstring & text, SIZE sizeDip);

    HRESULT  DrawString    (const wchar_t      * text,
                            float                xDip,
                            float                yDip,
                            float                widthDip,
                            float                heightDip,
                            uint32_t             argbColor,
                            float                fontSizeDip,
                            const wchar_t      * fontFamily,
                            DxuiTextHAlign       hAlign,
                            DxuiTextVAlign       vAlign,
                            DWRITE_FONT_WEIGHT   weight,
                            bool                 wrap) override;

    HRESULT  PushClipRect  (float xDip, float yDip, float widthDip, float heightDip) override;
    HRESULT  PopClipRect   () override;

    HRESULT  FillRect      (float xDip, float yDip, float widthDip, float heightDip, uint32_t argbColor) override;

    HRESULT  MeasureString (const wchar_t * text,
                            float           fontSizeDip,
                            const wchar_t * fontFamily,
                            float         & outWidthDip,
                            float         & outHeightDip) override;

    HRESULT  DrawIconBitmap (const uint32_t * srcBgraPremul,
                             int              srcWidthPx,
                             int              srcHeightPx,
                             float            destXDip,
                             float            destYDip,
                             float            destWidthDip,
                             float            destHeightDip) override;

private:
    std::vector<RecordedTextCall>          m_calls;
    std::map<std::wstring, SIZE>           m_cannedMetrics;
};
