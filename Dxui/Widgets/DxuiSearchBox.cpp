#include "Pch.h"

#include "DxuiSearchBox.h"
#include "Theme/IDxuiTheme.h"
#include "Theme/DxuiColor.h"
#include "Render/IDxuiPainter.h"
#include "Render/IDxuiTextRenderer.h"




static constexpr int       s_kPadDip           = 6;          // outer left / right inset
static constexpr int       s_kGlyphSlotDip     = 20;         // width reserved for a leading / trailing glyph
static constexpr float     s_kGlyphFontDip     = 13.0f;
static constexpr float     s_kSlideDip         = 8.0f;       // distance the magnifier slides left as it fades
static constexpr int64_t   s_kSlideDurationMs  = 120;        // focus magnifier slide / fade duration
static constexpr float     s_kShownEpsilon     = 0.01f;      // below this the magnifier is treated as hidden
static constexpr wchar_t   s_kMdl2Family[]     = L"Segoe MDL2 Assets";
static constexpr wchar_t   s_kMagnifierGlyph[] = L"\uE721";  // MDL2 "Search"
static constexpr wchar_t   s_kClearGlyph[]     = L"\uE711";  // MDL2 "Cancel"
static constexpr uint32_t  s_kFallbackFrameBg  = 0xFF1A1F26;
static constexpr uint32_t  s_kFallbackBorder   = 0xFF445566;




////////////////////////////////////////////////////////////////////////////////
//
//  DxuiSearchBox  (constructor)
//
//  Marks the hosted input chromeless so the search box owns the frame.
//  Change detection is done by snapshotting the value around each edit
//  (OnChar / OnKey) rather than chaining the input's callback, so the
//  widget stores no self-referential state and stays trivially copyable.
//
////////////////////////////////////////////////////////////////////////////////

DxuiSearchBox::DxuiSearchBox ()
{
    m_focusable = true;
    m_input.SetChromeless (true);
}




////////////////////////////////////////////////////////////////////////////////
//
//  SetDpi
//
////////////////////////////////////////////////////////////////////////////////

void DxuiSearchBox::SetDpi (UINT dpi)
{
    m_scaler.SetDpi (dpi);
    m_input.SetDpi (dpi);
    RelayoutInput();
}




////////////////////////////////////////////////////////////////////////////////
//
//  SetRect
//
////////////////////////////////////////////////////////////////////////////////

void DxuiSearchBox::SetRect (const RECT & rect)
{
    SetBounds (rect);
    RelayoutInput();
}




////////////////////////////////////////////////////////////////////////////////
//
//  SetText
//
//  Programmatic value change. Does not fire OnChange (the caller owns the
//  new value); relays out so any clear-button visibility change applies.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiSearchBox::SetText (const std::wstring & text)
{
    m_input.SetText (text);
    RelayoutInput();
}




////////////////////////////////////////////////////////////////////////////////
//
//  Clear
//
//  Empties the field (X-button action), keeps focus, and fires OnChange.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiSearchBox::Clear ()
{
    m_input.SetText (L"");
    m_clearHover   = false;
    m_clearPressed = false;
    RelayoutInput();
    FireChange();
}




////////////////////////////////////////////////////////////////////////////////
//
//  SetFocused
//
////////////////////////////////////////////////////////////////////////////////

void DxuiSearchBox::SetFocused (bool focused)
{
    m_focused = focused;
    m_input.SetFocused (focused);
    m_lastTickMs = 0;   // reseed the animation clock so the next Tick has a sane dt
    RelayoutInput();
}




////////////////////////////////////////////////////////////////////////////////
//
//  OnLButtonDown
//
//  A press on the clear glyph empties the field; any other press inside
//  the box places the caret in the hosted input.
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiSearchBox::OnLButtonDown (int x, int y)
{
    bool  consumed = false;


    if (IsClearVisible() && HitTestClear (x, y))
    {
        m_clearPressed = true;
        Clear();
        consumed = true;
    }
    else
    {
        consumed = m_input.OnLButtonDown (x, y);
    }

    return consumed;
}




////////////////////////////////////////////////////////////////////////////////
//
//  OnLButtonUp
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiSearchBox::OnLButtonUp (int x, int y)
{
    m_clearPressed = false;
    return m_input.OnLButtonUp (x, y);
}




////////////////////////////////////////////////////////////////////////////////
//
//  OnMouseMove
//
////////////////////////////////////////////////////////////////////////////////

void DxuiSearchBox::OnMouseMove (int x, int y)
{
    m_clearHover = IsClearVisible() && HitTestClear (x, y);
    m_input.OnMouseMove (x, y);
}




////////////////////////////////////////////////////////////////////////////////
//
//  OnKey
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiSearchBox::OnKey (WPARAM vk)
{
    std::wstring  before   = m_input.Text();
    bool          consumed = m_input.OnKey (vk);


    if (m_input.Text() != before)
    {
        FireChange();
    }

    return consumed;
}




////////////////////////////////////////////////////////////////////////////////
//
//  OnChar
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiSearchBox::OnChar (wchar_t ch)
{
    std::wstring  before   = m_input.Text();
    bool          consumed = m_input.OnChar (ch);


    if (m_input.Text() != before)
    {
        FireChange();
    }

    return consumed;
}




////////////////////////////////////////////////////////////////////////////////
//
//  Paint
//
//  Draws the frame, the hosted (chromeless) input, the fading / sliding
//  magnifier, and the trailing clear glyph.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiSearchBox::Paint (IDxuiPainter & painter, IDxuiTextRenderer & text) const
{
    HRESULT   hr        = S_OK;
    float     x         = (float) m_boundsDip.left;
    float     y         = (float) m_boundsDip.top;
    float     w         = (float) (m_boundsDip.right  - m_boundsDip.left);
    float     h         = (float) (m_boundsDip.bottom - m_boundsDip.top);
    float     pad       = m_scaler.Pxf ((float) s_kPadDip);
    float     glyphSlot = m_scaler.Pxf ((float) s_kGlyphSlotDip);
    float     glyphDip  = m_scaler.Pxf (s_kGlyphFontDip);
    float     slide     = m_scaler.Pxf (s_kSlideDip);
    uint32_t  frameBg   = (m_theme != nullptr) ? m_theme->BackgroundElevated() : s_kFallbackFrameBg;
    uint32_t  border    = (m_theme != nullptr) ? m_theme->Border()             : s_kFallbackBorder;
    uint32_t  focusRing = (m_theme != nullptr) ? m_theme->FocusRing()          : s_kFallbackBorder;
    uint32_t  accent    = (m_theme != nullptr) ? m_theme->Accent()             : s_kFallbackBorder;
    uint32_t  muted     = (m_theme != nullptr) ? m_theme->ForegroundMuted()    : s_kFallbackBorder;



    painter.FillRect    (x, y, w, h, frameBg);
    painter.OutlineRect (x, y, w, h, 1.0f, m_focused ? focusRing : border);

    m_input.Paint (painter, text);

    if (m_glyphShown > s_kShownEpsilon)
    {
        float     magX = x + pad - (1.0f - m_glyphShown) * slide;
        uint32_t  argb = DxuiColor::ScaleAlpha (accent, m_glyphShown);

        hr = text.DrawString (s_kMagnifierGlyph,
                              magX,
                              y,
                              glyphSlot,
                              h,
                              argb,
                              glyphDip,
                              s_kMdl2Family,
                              DxuiTextHAlign::Center,
                              DxuiTextVAlign::Center);
        IGNORE_RETURN_VALUE (hr, S_OK);
    }

    if (IsClearVisible())
    {
        RECT      r        = ClearGlyphRect();
        uint32_t  clearArg = m_clearHover ? accent : muted;

        hr = text.DrawString (s_kClearGlyph,
                              (float) r.left,
                              (float) r.top,
                              (float) (r.right - r.left),
                              (float) (r.bottom - r.top),
                              clearArg,
                              glyphDip,
                              s_kMdl2Family,
                              DxuiTextHAlign::Center,
                              DxuiTextVAlign::Center);
        IGNORE_RETURN_VALUE (hr, S_OK);
    }
}




////////////////////////////////////////////////////////////////////////////////
//
//  DxuiSearchBox::Layout  (IDxuiControl override)
//
////////////////////////////////////////////////////////////////////////////////

void DxuiSearchBox::Layout (const RECT & boundsDip, const DxuiDpiScaler & scaler)
{
    SetBounds (boundsDip);
    m_scaler.SetDpi (scaler.Dpi());
    m_input.SetDpi (scaler.Dpi());
    RelayoutInput();
}




////////////////////////////////////////////////////////////////////////////////
//
//  DxuiSearchBox::Paint  (IDxuiControl override)
//
////////////////////////////////////////////////////////////////////////////////

void DxuiSearchBox::Paint (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme)
{
    m_theme = &theme;
    m_input.SetTheme (&theme);
    static_cast<const DxuiSearchBox *> (this)->Paint (painter, text);
}




////////////////////////////////////////////////////////////////////////////////
//
//  DxuiSearchBox::Tick  (IDxuiControl override)
//
//  Advances the magnifier presence toward its target (shown only while
//  empty and unfocused) and re-lays the hosted input so its leading inset
//  tracks the slide.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiSearchBox::Tick (int64_t nowMs)
{
    float    target = (!m_focused && m_input.Text().empty()) ? 1.0f : 0.0f;
    int64_t  dt     = 0;
    float    step   = 0.0f;
    float    prev   = m_glyphShown;


    if (m_lastTickMs == 0)
    {
        m_lastTickMs = nowMs;
    }

    dt           = nowMs - m_lastTickMs;
    m_lastTickMs = nowMs;
    step         = (float) dt / (float) s_kSlideDurationMs;

    if (m_glyphShown < target)
    {
        m_glyphShown = std::min (target, m_glyphShown + step);
    }
    else if (m_glyphShown > target)
    {
        m_glyphShown = std::max (target, m_glyphShown - step);
    }

    if (m_glyphShown != prev)
    {
        RelayoutInput();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiSearchBox::OnMouse  (IDxuiControl override)
//
//  Bridges the panel-tree mouse channel to the bespoke host-driven entry
//  points so the search box works inside a DxuiPanel dialog / tree.
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiSearchBox::OnMouse (const DxuiMouseEvent & ev)
{
    bool  consumed = false;


    switch (ev.kind)
    {
    case DxuiMouseEventKind::Down:
        if (ev.button == DxuiMouseButton::Left)
        {
            consumed = OnLButtonDown (ev.positionDip.x, ev.positionDip.y);
        }
        break;

    case DxuiMouseEventKind::Up:
        if (ev.button == DxuiMouseButton::Left)
        {
            consumed = OnLButtonUp (ev.positionDip.x, ev.positionDip.y);
        }
        break;

    case DxuiMouseEventKind::Move:
        OnMouseMove (ev.positionDip.x, ev.positionDip.y);
        break;

    default:
        break;
    }

    return consumed;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiSearchBox::OnKey  (IDxuiControl override)
//
//  Bridges the panel-tree key channel to the bespoke OnKey(WPARAM).
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiSearchBox::OnKey (const DxuiKeyEvent & ev)
{
    bool  consumed = false;


    if (ev.kind == DxuiKeyEventKind::Down)
    {
        consumed = OnKey (static_cast<WPARAM> (ev.vk));
    }

    return consumed;
}




////////////////////////////////////////////////////////////////////////////////
//
//  RelayoutInput
//
//  Positions the hosted input between the (animated) leading magnifier
//  slot and the trailing clear-glyph slot.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiSearchBox::RelayoutInput ()
{
    int   pad       = m_scaler.Px (s_kPadDip);
    int   glyphSlot = m_scaler.Px (s_kGlyphSlotDip);
    int   leading   = pad + (int) ((float) glyphSlot * m_glyphShown);
    int   trailing  = pad + (IsClearVisible() ? glyphSlot : 0);
    RECT  r         = { m_boundsDip.left  + leading,
                        m_boundsDip.top,
                        m_boundsDip.right - trailing,
                        m_boundsDip.bottom };


    if (r.right < r.left)
    {
        r.right = r.left;
    }

    m_input.SetRect (r);
}




////////////////////////////////////////////////////////////////////////////////
//
//  IsClearVisible
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiSearchBox::IsClearVisible () const
{
    return m_focused && !m_input.Text().empty();
}




////////////////////////////////////////////////////////////////////////////////
//
//  ClearGlyphRect
//
//  The full-height square slot at the right edge that hosts the clear (X)
//  glyph and absorbs its clicks.
//
////////////////////////////////////////////////////////////////////////////////

RECT DxuiSearchBox::ClearGlyphRect () const
{
    int  pad       = m_scaler.Px (s_kPadDip);
    int  glyphSlot = m_scaler.Px (s_kGlyphSlotDip);

    return RECT { m_boundsDip.right - pad - glyphSlot,
                  m_boundsDip.top,
                  m_boundsDip.right - pad,
                  m_boundsDip.bottom };
}




////////////////////////////////////////////////////////////////////////////////
//
//  HitTestClear
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiSearchBox::HitTestClear (int x, int y) const
{
    RECT  r = ClearGlyphRect();

    return x >= r.left && x < r.right && y >= r.top && y < r.bottom;
}




////////////////////////////////////////////////////////////////////////////////
//
//  FireChange
//
////////////////////////////////////////////////////////////////////////////////

void DxuiSearchBox::FireChange ()
{
    if (m_change)
    {
        m_change (m_input.Text());
    }
}
