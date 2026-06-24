#include "Pch.h"

#include "Widgets/DxuiMenuBar.h"

#include "Core/DxuiThread.h"
#include "Render/IDxuiPainter.h"
#include "Render/IDxuiTextRenderer.h"
#include "Theme/IDxuiTheme.h"
#include "Window/DxuiHostWindow.h"
#include "Window/DxuiPopupHost.h"




namespace
{
    constexpr int      s_kBaseDpi                = 96;
    constexpr int      s_kNavHeightDip           = 32;
    constexpr int      s_kItemInternalPaddingDip = 8;
    constexpr int      s_kInterItemPaddingDip    = 4;
    constexpr int      s_kRowHeightDip           = 26;
    constexpr int      s_kSeparatorHeightDip     = 10;
    constexpr int      s_kSeparatorInsetDip      = 10;
    constexpr int      s_kMidpointDivisor        = 2;
    constexpr int      s_kDropdownWidthDip       = 300;
    constexpr int      s_kAccelOffsetDip         = 190;
    constexpr int      s_kRowPadLeftDip          = 10;
    constexpr int      s_kRowPadTopDip           = 5;
    constexpr int      s_kCheckGutterDip         = 18;
    constexpr float    s_kFontDip                = 14.0f;
    constexpr float    s_kUnderlineThicknessDip  = 1.0f;
    constexpr wchar_t  s_kFontFamily[]           = L"Segoe UI";
    constexpr wchar_t  s_kpszCheckMark[]         = L"\u2713";

    constexpr int  s_kFallbackGlyphWidthDip = 8;


    bool RectContains (const RECT & rect, int x, int y)
    {
        return x >= rect.left && x < rect.right && y >= rect.top && y < rect.bottom;
    }


    int ScaleDpi (int dipValue, UINT dpi)
    {
        UINT  effectiveDpi = (dpi == 0) ? (UINT) s_kBaseDpi : dpi;



        return MulDiv (dipValue, (int) effectiveDpi, s_kBaseDpi);
    }
}




////////////////////////////////////////////////////////////////////////////////
//
//  DxuiMenuBar::DxuiMenuBar
//
////////////////////////////////////////////////////////////////////////////////

DxuiMenuBar::DxuiMenuBar ()
{
}




////////////////////////////////////////////////////////////////////////////////
//
//  DxuiMenuBar::~DxuiMenuBar
//
////////////////////////////////////////////////////////////////////////////////

DxuiMenuBar::~DxuiMenuBar ()
{
    Close();
}




////////////////////////////////////////////////////////////////////////////////
//
//  DxuiMenuBar::SetItems
//
//  Replaces the menu strip contents. If a `DxuiMenuBarItem::altLetter`
//  is zero, the `&X` mnemonic on its label supplies the accelerator.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiMenuBar::SetItems (std::vector<DxuiMenuBarItem> items)
{
    DXUI_ASSERT_UI_THREAD();

    m_items = std::move (items);
    m_titleRects.assign (m_items.size(), RECT {});
    m_measuredItemWidthPx.clear();
    m_measuredAtDpi = 0;

    for (DxuiMenuBarItem & item : m_items)
    {
        std::wstring  stripped;
        int           mnIdx = -1;
        wchar_t       mnCh  = 0;

        if (item.altLetter != 0)
        {
            continue;
        }

        ParseMnemonic (item.label, stripped, mnIdx, mnCh);
        item.altLetter = mnCh;
    }

    if (m_openIndex >= (int) m_items.size())
    {
        m_openIndex = 0;
    }
    if (m_focusedIndex >= (int) m_items.size())
    {
        m_focusedIndex = 0;
    }
}




////////////////////////////////////////////////////////////////////////////////
//
//  DxuiMenuBar::SetPopupHost
//
//  Wires the menu bar to a popup-hosting `DxuiHostWindow`. When set,
//  an open submenu renders into a top-level popup (so it can escape the
//  window and occlude); with no host it falls back to the in-window
//  inline dropdown.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiMenuBar::SetPopupHost (DxuiHostWindow * host)
{
    DXUI_ASSERT_UI_THREAD();

    if (host != m_popupHost)
    {
        // Tear down any live popup against the CURRENT host before
        // repointing -- ReleaseActivePopup returns it to the old pool.
        ReleaseActivePopup();
        m_isOpen         = false;
        m_highlightIndex = -1;
    }

    m_popupHost = host;
}




////////////////////////////////////////////////////////////////////////////////
//
//  DxuiMenuBar::SetStripColors
//
////////////////////////////////////////////////////////////////////////////////

void DxuiMenuBar::SetStripColors (uint32_t stripArgb, uint32_t hoverArgb, uint32_t textArgb)
{
    DXUI_ASSERT_UI_THREAD();

    m_stripColorsSet     = true;
    m_stripBgOverride    = stripArgb;
    m_stripHoverOverride = hoverArgb;
    m_stripTextOverride  = textArgb;
}




////////////////////////////////////////////////////////////////////////////////
//
//  DxuiMenuBar::SetDropdownColors
//
////////////////////////////////////////////////////////////////////////////////

void DxuiMenuBar::SetDropdownColors (
    uint32_t bgArgb,
    uint32_t hoverArgb,
    uint32_t textArgb,
    uint32_t accelArgb,
    uint32_t borderArgb,
    uint32_t dividerArgb)
{
    DXUI_ASSERT_UI_THREAD();

    m_dropdownColorsSet   = true;
    m_dropBgOverride      = bgArgb;
    m_dropHoverOverride   = hoverArgb;
    m_dropTextOverride    = textArgb;
    m_dropAccelOverride   = accelArgb;
    m_dropBorderOverride  = borderArgb;
    m_dropDividerOverride = dividerArgb;
}




////////////////////////////////////////////////////////////////////////////////
//
//  DxuiMenuBar::Layout
//
//  Lays out the title strip starting at (x, y) spanning `width` pixels
//  at the given DPI. When `pTextForMeasure` is non-null each title
//  width is measured against the supplied text renderer; otherwise a
//  coarse glyph-width fallback is used (typically only on the first
//  layout pass before a renderer is available).
//
////////////////////////////////////////////////////////////////////////////////

void DxuiMenuBar::Layout (int x, int y, int width, UINT dpi, IDxuiTextRenderer * pTextForMeasure)
{
    int    currentX = x;
    int    pad      = ScaleDpi (s_kItemInternalPaddingDip, dpi);
    int    gap      = ScaleDpi (s_kInterItemPaddingDip,    dpi);
    int    height   = ScaleDpi (s_kNavHeightDip, dpi);
    UINT   eDpi     = (dpi == 0) ? (UINT) s_kBaseDpi : dpi;
    float  fontDip  = s_kFontDip * (float) eDpi / (float) s_kBaseDpi;


    DXUI_ASSERT_UI_THREAD();

    if (pTextForMeasure != nullptr)
    {
        m_textRendererForMeasure = pTextForMeasure;
    }

    m_stripRect.left   = x;
    m_stripRect.top    = y;
    m_stripRect.right  = x + width;
    m_stripRect.bottom = y + height;
    m_rowHeightPx      = ScaleDpi (s_kRowHeightDip, dpi);
    m_dpi              = eDpi;
    m_titleRects.assign (m_items.size(), RECT {});

    // Menu-item text widths depend only on the item set and DPI, never
    // on window size. Cache successful measurements and reuse them so a
    // resize (which re-runs Layout) never re-measures -- DirectWrite can
    // transiently return a zero-width layout mid-resize, which would
    // otherwise collapse item spacing into the crude fallback path.
    if (m_measuredAtDpi != eDpi || m_measuredItemWidthPx.size() != m_items.size())
    {
        m_measuredItemWidthPx.assign (m_items.size(), 0);
        m_measuredAtDpi = eDpi;
    }

    for (size_t i = 0; i < m_items.size(); i++)
    {
        std::wstring  stripped;
        int           mnIdx     = -1;
        wchar_t       mnCh      = 0;
        int           menuW     = 0;
        int           textW     = 0;
        float         textWidth = 0.0f;
        float         textHt    = 0.0f;
        HRESULT       hrMeasure = E_FAIL;

        ParseMnemonic (m_items[i].label, stripped, mnIdx, mnCh);

        if (m_measuredItemWidthPx[i] > 0)
        {
            // Reuse a previously-cached good measurement.
            textW = m_measuredItemWidthPx[i];
        }
        else
        {
            if (pTextForMeasure != nullptr)
            {
                hrMeasure = pTextForMeasure->MeasureString (stripped.c_str(), fontDip, s_kFontFamily, textWidth, textHt);
            }

            if (SUCCEEDED (hrMeasure) && textWidth > 0.0f)
            {
                textW                    = (int) (textWidth + 0.5f);
                m_measuredItemWidthPx[i] = textW;
            }
            else
            {
                // Renderer not ready / transient measurement failure.
                // Use a DPI-scaled glyph estimate and leave the cache
                // slot empty so the next Layout re-measures.
                textW = (int) stripped.size() * ScaleDpi (s_kFallbackGlyphWidthDip, dpi);
            }
        }

        menuW = textW + pad * 2;

        m_titleRects[i].left   = currentX;
        m_titleRects[i].top    = y;
        m_titleRects[i].right  = currentX + menuW;
        m_titleRects[i].bottom = y + height;
        currentX += menuW + gap;
    }
}




////////////////////////////////////////////////////////////////////////////////
//
//  DxuiMenuBar::Hide
//
////////////////////////////////////////////////////////////////////////////////

void DxuiMenuBar::Hide ()
{
    DXUI_ASSERT_UI_THREAD();

    Close();
}




////////////////////////////////////////////////////////////////////////////////
//
//  DxuiMenuBar::Open
//
////////////////////////////////////////////////////////////////////////////////

void DxuiMenuBar::Open (int menuIndex, bool keyboardActivated)
{
    DXUI_ASSERT_UI_THREAD();

    if (menuIndex < 0 || menuIndex >= (int) m_items.size())
    {
        return;
    }

    // Already showing this menu's popup: resting/moving over the same
    // open title must NOT churn the popup (HandleMouseMove re-Opens on
    // every move). Just refresh the keyboard flag and return.
    if (m_isOpen && m_openIndex == menuIndex && m_activePopup != nullptr)
    {
        m_openedByKeyboard = keyboardActivated;
        return;
    }

    // Switch popups: drop the prior menu's popup FIRST -- its onClosed
    // callback clears m_isOpen / m_highlightIndex -- THEN set this menu's
    // state and raise its popup, so the clear can't clobber the new state.
    if (m_popupHost != nullptr)
    {
        ReleaseActivePopup();
    }

    m_openIndex        = menuIndex;
    m_isOpen           = true;
    m_openedByKeyboard = keyboardActivated;
    m_highlightIndex   = FirstEnabledRow (menuIndex);

    if (m_popupHost != nullptr)
    {
        ShowDropdownPopup();
    }
}




////////////////////////////////////////////////////////////////////////////////
//
//  DxuiMenuBar::Close
//
////////////////////////////////////////////////////////////////////////////////

void DxuiMenuBar::Close ()
{
    DXUI_ASSERT_UI_THREAD();

    m_isOpen         = false;
    m_highlightIndex = -1;

    ReleaseActivePopup();
}




////////////////////////////////////////////////////////////////////////////////
//
//  DxuiMenuBar::CloseAll
//
////////////////////////////////////////////////////////////////////////////////

void DxuiMenuBar::CloseAll ()
{
    DXUI_ASSERT_UI_THREAD();

    Close();
}




////////////////////////////////////////////////////////////////////////////////
//
//  DxuiMenuBar::SetFocusedMenu
//
////////////////////////////////////////////////////////////////////////////////

void DxuiMenuBar::SetFocusedMenu (int menuIndex)
{
    DXUI_ASSERT_UI_THREAD();

    if (menuIndex < 0 || menuIndex >= (int) m_items.size())
    {
        return;
    }

    m_focusedIndex = menuIndex;
    m_hasFocus     = true;
}




////////////////////////////////////////////////////////////////////////////////
//
//  DxuiMenuBar::ClearFocus
//
////////////////////////////////////////////////////////////////////////////////

void DxuiMenuBar::ClearFocus ()
{
    DXUI_ASSERT_UI_THREAD();

    m_hasFocus = false;
}




////////////////////////////////////////////////////////////////////////////////
//
//  DxuiMenuBar::HandleAltKey
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiMenuBar::HandleAltKey (wchar_t ch)
{
    wchar_t  lower = (wchar_t) towlower (ch);


    DXUI_ASSERT_UI_THREAD();

    for (size_t i = 0; i < m_items.size(); i++)
    {
        if (m_items[i].altLetter != 0 && m_items[i].altLetter == lower)
        {
            Open ((int) i, true);
            return true;
        }
    }

    return false;
}




////////////////////////////////////////////////////////////////////////////////
//
//  DxuiMenuBar::HandleKey
//
//  Keyboard navigation while a submenu is open. Returns true if the
//  key was consumed.
//
//      Escape / F10     dismiss
//      Left  / Shift+Tab swap to previous menu
//      Right / Tab       swap to next menu
//      Up   / Down       move highlight within open submenu
//      Enter / Space     dispatch the highlighted entry
//      A-Z               mnemonic activation within the open submenu
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiMenuBar::HandleKey (WPARAM vk)
{
    int  count       = VisibleRowCount (m_openIndex);
    int  menuCount   = (int) m_items.size();


    DXUI_ASSERT_UI_THREAD();

    if (!m_isOpen)
    {
        return false;
    }

    if (vk == VK_ESCAPE || vk == VK_F10)
    {
        Close();
        return true;
    }

    if (menuCount <= 0)
    {
        return false;
    }

    if (vk == VK_LEFT || (vk == VK_TAB && (GetKeyState (VK_SHIFT) & 0x8000)))
    {
        int  next = (m_openIndex <= 0) ? (menuCount - 1) : (m_openIndex - 1);
        Open (next, m_openedByKeyboard);
        return true;
    }

    if (vk == VK_RIGHT || vk == VK_TAB)
    {
        int  next = (m_openIndex + 1) % menuCount;
        Open (next, m_openedByKeyboard);
        return true;
    }

    if (count <= 0)
    {
        return false;
    }

    if (vk == VK_DOWN)
    {
        m_highlightIndex = NextEnabledRow (m_openIndex, m_highlightIndex, +1);
        if (m_activePopup != nullptr)
        {
            m_activePopup->MarkDirty();
        }
        return true;
    }

    if (vk == VK_UP)
    {
        m_highlightIndex = NextEnabledRow (m_openIndex, m_highlightIndex, -1);
        if (m_activePopup != nullptr)
        {
            m_activePopup->MarkDirty();
        }
        return true;
    }

    if (vk == VK_RETURN || vk == VK_SPACE)
    {
        const DxuiMenuBarSubitem * entry = EntryAt (m_openIndex, m_highlightIndex);

        if (entry != nullptr && entry->enabled && entry->dispatch)
        {
            entry->dispatch();
            Close();
            return true;
        }
    }

    if (vk >= 'A' && vk <= 'Z')
    {
        wchar_t  lower = (wchar_t) towlower ((wchar_t) vk);
        int      row   = 0;

        for (const DxuiMenuBarSubitem & sub : m_items[m_openIndex].submenu)
        {
            std::wstring  stripped;
            int           mnIdx = -1;
            wchar_t       mnCh  = 0;

            if (sub.isSeparator)
            {
                continue;
            }

            ParseMnemonic (sub.label, stripped, mnIdx, mnCh);

            if (mnCh != 0 && mnCh == lower && sub.enabled && sub.dispatch)
            {
                m_highlightIndex = row;
                sub.dispatch();
                Close();
                return true;
            }
            row++;
        }
    }

    return false;
}




////////////////////////////////////////////////////////////////////////////////
//
//  DxuiMenuBar::HandleMouseMove
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiMenuBar::HandleMouseMove (int x, int y)
{
    int  hitTitle = 0;
    int  hitEntry = 0;


    DXUI_ASSERT_UI_THREAD();

    // Ignore synthetic same-position moves. Windows posts a WM_MOUSEMOVE
    // at the unchanged cursor position whenever the dropdown popup shows
    // or hides under the pointer; without this guard a keyboard menu
    // switch is instantly overridden by the resting mouse's title.
    if (m_haveLastMousePos && x == m_lastMouseX && y == m_lastMouseY)
    {
        return false;
    }
    m_haveLastMousePos = true;
    m_lastMouseX       = x;
    m_lastMouseY       = y;

    hitTitle = HitTitleIndex (x, y);
    hitEntry = HitEntryIndex (x, y);

    m_hoverIndex = hitTitle;

    if (hitTitle >= 0)
    {
        if (m_isOpen)
        {
            Open (hitTitle, m_openedByKeyboard);
        }
        return true;
    }

    if (hitEntry >= 0)
    {
        m_highlightIndex = hitEntry;
        return true;
    }

    return false;
}




////////////////////////////////////////////////////////////////////////////////
//
//  DxuiMenuBar::ClearHover
//
//  Drop hover state and submenu highlight so the strip paints idle.
//  Called when the cursor leaves the host window. Leaves the open /
//  closed state alone -- a click-opened submenu stays open while the
//  pointer wanders outside the chrome.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiMenuBar::ClearHover ()
{
    DXUI_ASSERT_UI_THREAD();

    m_hoverIndex       = -1;
    m_highlightIndex   = -1;
    m_haveLastMousePos = false;
}




////////////////////////////////////////////////////////////////////////////////
//
//  DxuiMenuBar::HandleMouseDown
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiMenuBar::HandleMouseDown (int x, int y)
{
    int  hitTitle = HitTitleIndex (x, y);


    DXUI_ASSERT_UI_THREAD();

    if (hitTitle >= 0)
    {
        if (m_isOpen && m_openIndex == hitTitle)
        {
            Close();
        }
        else
        {
            Open (hitTitle, false);
        }
        return true;
    }

    if (m_isOpen && !RectContains (DropdownRect(), x, y))
    {
        Close();
    }

    return false;
}




////////////////////////////////////////////////////////////////////////////////
//
//  DxuiMenuBar::HandleMouseUp
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiMenuBar::HandleMouseUp (int x, int y)
{
    int                          hitEntry = HitEntryIndex (x, y);
    const DxuiMenuBarSubitem  *  entry    = nullptr;


    DXUI_ASSERT_UI_THREAD();

    if (hitEntry >= 0)
    {
        entry = EntryAt (m_openIndex, hitEntry);
        if (entry != nullptr && entry->enabled && entry->dispatch)
        {
            entry->dispatch();
            Close();
            return true;
        }
    }

    return false;
}




////////////////////////////////////////////////////////////////////////////////
//
//  DxuiMenuBar::PaintStrip
//
////////////////////////////////////////////////////////////////////////////////

void DxuiMenuBar::PaintStrip (
    IDxuiPainter      & painter,
    IDxuiTextRenderer & text,
    const IDxuiTheme  & theme,
    UINT                dpi)
{
    HRESULT   hr        = S_OK;
    UINT      eDpi      = (dpi == 0) ? (UINT) s_kBaseDpi : dpi;
    float     fontDip   = s_kFontDip * (float) eDpi / (float) s_kBaseDpi;
    bool      showCues  = ShouldShowMnemonicCues (IsOpenByKeyboard());
    uint32_t  stripBg   = m_stripColorsSet ? m_stripBgOverride    : theme.Background();
    uint32_t  stripHov  = m_stripColorsSet ? m_stripHoverOverride : theme.HoverBackground();
    uint32_t  stripFg   = m_stripColorsSet ? m_stripTextOverride  : theme.Foreground();


    DXUI_ASSERT_UI_THREAD();

    painter.FillRect ((float) m_stripRect.left,
                      (float) m_stripRect.top,
                      (float) (m_stripRect.right - m_stripRect.left),
                      (float) (m_stripRect.bottom - m_stripRect.top),
                      stripBg);

    for (size_t i = 0; i < m_items.size(); i++)
    {
        std::wstring  stripped;
        int           mnIdx = -1;
        wchar_t       mnCh  = 0;
        float         rectW = (float) (m_titleRects[i].right - m_titleRects[i].left);
        float         rectH = (float) (m_titleRects[i].bottom - m_titleRects[i].top);

        ParseMnemonic (m_items[i].label, stripped, mnIdx, mnCh);

        if ((m_hoverIndex == (int) i) ||
            (m_isOpen && m_openIndex == (int) i) ||
            (m_hasFocus && !m_isOpen && m_focusedIndex == (int) i))
        {
            painter.FillRect ((float) m_titleRects[i].left,
                              (float) m_titleRects[i].top,
                              rectW,
                              rectH,
                              stripHov);
        }

        IGNORE_RETURN_VALUE (hr, text.DrawString (stripped.c_str(),
                                                  (float) m_titleRects[i].left,
                                                  (float) m_titleRects[i].top,
                                                  rectW,
                                                  rectH,
                                                  stripFg,
                                                  fontDip,
                                                  s_kFontFamily,
                                                  DxuiTextHAlign::Center,
                                                  DxuiTextVAlign::Center,
                                                  DWRITE_FONT_WEIGHT_NORMAL,
                                                  false));

        if (showCues && mnIdx >= 0 && !stripped.empty())
        {
            float        fullW    = 0.0f;
            float        fullH    = 0.0f;
            float        prefixW  = 0.0f;
            float        charW    = 0.0f;
            std::wstring prefix   = stripped.substr (0, (size_t) mnIdx);
            std::wstring prefixCh = stripped.substr (0, (size_t) mnIdx + 1);
            HRESULT      hrM      = text.MeasureString (stripped.c_str(), fontDip, s_kFontFamily, fullW, fullH);

            if (FAILED (hrM) || fullW <= 0.0f)
            {
                continue;
            }

            if (!prefix.empty())
            {
                float ignH = 0.0f;
                IGNORE_RETURN_VALUE (hrM, text.MeasureString (prefix.c_str(), fontDip, s_kFontFamily, prefixW, ignH));
            }
            {
                float pcW  = 0.0f;
                float ignH = 0.0f;
                IGNORE_RETURN_VALUE (hrM, text.MeasureString (prefixCh.c_str(), fontDip, s_kFontFamily, pcW, ignH));
                charW = pcW - prefixW;
            }

            float baseX = (float) m_titleRects[i].left + (rectW - fullW) / 2.0f + prefixW;
            float baseY = (float) m_titleRects[i].top  + (rectH + fullH) / 2.0f;

            painter.FillRect (baseX, baseY, charW, s_kUnderlineThicknessDip, stripFg);
        }
    }
}




////////////////////////////////////////////////////////////////////////////////
//
//  DxuiMenuBar::PaintDropdown
//
////////////////////////////////////////////////////////////////////////////////

void DxuiMenuBar::PaintDropdown (
    IDxuiPainter      & painter,
    IDxuiTextRenderer & text,
    const IDxuiTheme  & theme,
    UINT                dpi)
{
    DXUI_ASSERT_UI_THREAD();

    if (!m_isOpen)
    {
        return;
    }

    PaintDropdownRows (painter, text, DropdownRect(), ResolveDropdownPalette (theme), dpi);
}




////////////////////////////////////////////////////////////////////////////////
//
//  DxuiMenuBar::PaintDropdownRows
//
//  Draws the submenu background, border, and rows into `rect` with the
//  resolved palette. Shared by the in-window PaintDropdown (rect =
//  DropdownRect) and the popup render hook (rect = popup-local, origin 0).
//
////////////////////////////////////////////////////////////////////////////////

void DxuiMenuBar::PaintDropdownRows (
    IDxuiPainter           & painter,
    IDxuiTextRenderer      & text,
    const RECT             & rect,
    const DropdownPalette  & pal,
    UINT                     dpi) const
{
    HRESULT   hr                 = S_OK;
    int       row                = 0;
    int       y                  = 0;
    UINT      eDpi               = (dpi == 0) ? (UINT) s_kBaseDpi : dpi;
    float     fontDip            = s_kFontDip * (float) eDpi / (float) s_kBaseDpi;
    int       rowPadLeftPx       = ScaleDpi (s_kRowPadLeftDip,    eDpi);
    int       rowPadTopPx        = ScaleDpi (s_kRowPadTopDip,     eDpi);
    int       accelOffsetPx      = ScaleDpi (s_kAccelOffsetDip,   eDpi);
    int       separatorInsetPx   = ScaleDpi (s_kSeparatorInsetDip, eDpi);
    bool      showCues           = ShouldShowMnemonicCues (IsOpenByKeyboard());
    bool      menuHasCheckable   = false;
    int       checkGutterPx      = 0;
    int       labelLeftPx        = rowPadLeftPx;


    DXUI_ASSERT_UI_THREAD();

    for (const DxuiMenuBarSubitem & sub : m_items[m_openIndex].submenu)
    {
        if (sub.checkable)
        {
            menuHasCheckable = true;
            break;
        }
    }

    if (menuHasCheckable)
    {
        checkGutterPx = ScaleDpi (s_kCheckGutterDip, eDpi);
        labelLeftPx   = rowPadLeftPx + checkGutterPx;
    }

    painter.FillRect ((float) rect.left,
                      (float) rect.top,
                      (float) (rect.right - rect.left),
                      (float) (rect.bottom - rect.top),
                      pal.bg);
    painter.OutlineRect ((float) rect.left,
                         (float) rect.top,
                         (float) (rect.right - rect.left),
                         (float) (rect.bottom - rect.top),
                         1.0f,
                         pal.border);

    for (const DxuiMenuBarSubitem & sub : m_items[m_openIndex].submenu)
    {
        std::wstring  stripped;
        int           mnIdx       = -1;
        wchar_t       mnCh        = 0;
        int           entryHeight = EntryHeightPx (sub);
        uint32_t      labelArgb   = sub.enabled ? pal.text  : pal.disabled;
        uint32_t      hotkeyArgb  = sub.enabled ? pal.accel : pal.disabled;

        if (sub.isSeparator)
        {
            painter.FillRect ((float) (rect.left + separatorInsetPx),
                              (float) (rect.top + y + entryHeight / s_kMidpointDivisor),
                              (float) (rect.right - rect.left - separatorInsetPx - separatorInsetPx),
                              s_kUnderlineThicknessDip,
                              pal.divider);
            y += entryHeight;
            continue;
        }

        ParseMnemonic (sub.label, stripped, mnIdx, mnCh);

        if (row == m_highlightIndex)
        {
            painter.FillRect ((float) rect.left,
                              (float) (rect.top + y),
                              (float) (rect.right - rect.left),
                              (float) entryHeight,
                              pal.hover);
        }

        IGNORE_RETURN_VALUE (hr, text.DrawString (stripped.c_str(),
                                                  (float) (rect.left + labelLeftPx),
                                                  (float) (rect.top + y + rowPadTopPx),
                                                  (float) accelOffsetPx,
                                                  (float) entryHeight,
                                                  labelArgb,
                                                  fontDip,
                                                  s_kFontFamily));

        if (sub.checkable && sub.isChecked && sub.isChecked())
        {
            IGNORE_RETURN_VALUE (hr, text.DrawString (s_kpszCheckMark,
                                                      (float) (rect.left + rowPadLeftPx),
                                                      (float) (rect.top + y + rowPadTopPx),
                                                      (float) checkGutterPx,
                                                      (float) entryHeight,
                                                      labelArgb,
                                                      fontDip,
                                                      s_kFontFamily));
        }

        if (showCues && mnIdx >= 0 && !stripped.empty() && sub.enabled)
        {
            float        prefixW  = 0.0f;
            float        charW    = 0.0f;
            float        fullH    = 0.0f;
            std::wstring prefix   = stripped.substr (0, (size_t) mnIdx);
            std::wstring prefixCh = stripped.substr (0, (size_t) mnIdx + 1);
            HRESULT      hrM      = S_OK;

            if (!prefix.empty())
            {
                IGNORE_RETURN_VALUE (hrM, text.MeasureString (prefix.c_str(), fontDip, s_kFontFamily, prefixW, fullH));
            }
            else
            {
                std::wstring oneCh (1, stripped[(size_t) mnIdx]);
                IGNORE_RETURN_VALUE (hrM, text.MeasureString (oneCh.c_str(), fontDip, s_kFontFamily, prefixW, fullH));
                prefixW = 0.0f;
            }
            {
                float pcW  = 0.0f;
                float ignH = 0.0f;
                IGNORE_RETURN_VALUE (hrM, text.MeasureString (prefixCh.c_str(), fontDip, s_kFontFamily, pcW, ignH));
                charW = pcW - prefixW;
            }

            float baseX = (float) (rect.left + labelLeftPx) + prefixW;
            float baseY = (float) (rect.top + y + rowPadTopPx) + fullH;

            painter.FillRect (baseX, baseY, charW, s_kUnderlineThicknessDip, labelArgb);
        }

        if (!sub.hotkey.empty())
        {
            IGNORE_RETURN_VALUE (hr, text.DrawString (sub.hotkey.c_str(),
                                                      (float) (rect.left + accelOffsetPx),
                                                      (float) (rect.top + y + rowPadTopPx),
                                                      (float) (rect.right - rect.left - accelOffsetPx),
                                                      (float) entryHeight,
                                                      hotkeyArgb,
                                                      fontDip,
                                                      s_kFontFamily));
        }
        y += entryHeight;
        row++;
    }
}




////////////////////////////////////////////////////////////////////////////////
//
//  DxuiMenuBar::Layout (IDxuiControl override)
//
////////////////////////////////////////////////////////////////////////////////

void DxuiMenuBar::Layout (const RECT & boundsDip, const DxuiDpiScaler & scaler)
{
    RECT  controlBounds = boundsDip;


    DXUI_ASSERT_UI_THREAD();

    Layout (boundsDip.left,
            boundsDip.top,
            boundsDip.right - boundsDip.left,
            scaler.Dpi(),
            m_textRendererForMeasure);

    if (controlBounds.bottom <= controlBounds.top)
    {
        controlBounds.bottom = m_stripRect.bottom;
    }
    SetBounds (controlBounds);
}




////////////////////////////////////////////////////////////////////////////////
//
//  DxuiMenuBar::Paint (IDxuiControl override)
//
////////////////////////////////////////////////////////////////////////////////

void DxuiMenuBar::Paint (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme)
{
    DXUI_ASSERT_UI_THREAD();

    // Keep the popup's colours fresh: its render hook gets no theme, so
    // the resolved palette is cached here every frame (the strip always
    // paints, even while the dropdown is popup-backed).
    m_cachedPalette = ResolveDropdownPalette (theme);

    PaintStrip (painter, text, theme, m_dpi);

    // The open submenu renders in its own popup when a host is wired;
    // skip the in-window dropdown so it is not drawn twice.
    if (m_activePopup == nullptr)
    {
        PaintDropdown (painter, text, theme, m_dpi);
    }
}




////////////////////////////////////////////////////////////////////////////////
//
//  DxuiMenuBar::OnKey (IDxuiControl override)
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiMenuBar::OnKey (const DxuiKeyEvent & ev)
{
    DXUI_ASSERT_UI_THREAD();

    if (ev.kind != DxuiKeyEventKind::Down)
    {
        return false;
    }

    return HandleKey (ev.vk);
}




////////////////////////////////////////////////////////////////////////////////
//
//  DxuiMenuBar::OnMouse (IDxuiControl override)
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiMenuBar::OnMouse (const DxuiMouseEvent & ev)
{
    DXUI_ASSERT_UI_THREAD();

    switch (ev.kind)
    {
    case DxuiMouseEventKind::Move:
        return HandleMouseMove (ev.positionDip.x, ev.positionDip.y);
    case DxuiMouseEventKind::Down:
        if (ev.button == DxuiMouseButton::Left)
        {
            return HandleMouseDown (ev.positionDip.x, ev.positionDip.y);
        }
        return false;
    case DxuiMouseEventKind::Up:
        if (ev.button == DxuiMouseButton::Left)
        {
            return HandleMouseUp (ev.positionDip.x, ev.positionDip.y);
        }
        return false;
    default:
        return false;
    }
}




////////////////////////////////////////////////////////////////////////////////
//
//  DxuiMenuBar::MenuRect
//
////////////////////////////////////////////////////////////////////////////////

RECT DxuiMenuBar::MenuRect (int menuIndex) const
{
    if (menuIndex < 0 || menuIndex >= (int) m_titleRects.size())
    {
        return RECT {};
    }

    return m_titleRects[menuIndex];
}




////////////////////////////////////////////////////////////////////////////////
//
//  DxuiMenuBar::DropdownRect
//
////////////////////////////////////////////////////////////////////////////////

RECT DxuiMenuBar::DropdownRect () const
{
    RECT  rect          = {};
    RECT  title         = {};
    int   dropdownWidth = ScaleDpi (s_kDropdownWidthDip, m_dpi);



    if (m_openIndex < 0 || m_openIndex >= (int) m_titleRects.size())
    {
        return rect;
    }

    title       = m_titleRects[m_openIndex];
    rect.left   = title.left;
    rect.top    = title.bottom;
    rect.right  = title.left + dropdownWidth;
    rect.bottom = title.bottom + DropdownHeightPx (m_openIndex);
    return rect;
}




////////////////////////////////////////////////////////////////////////////////
//
//  DxuiMenuBar::HitTitleIndex
//
////////////////////////////////////////////////////////////////////////////////

int DxuiMenuBar::HitTitleIndex (int x, int y) const
{
    for (size_t i = 0; i < m_titleRects.size(); i++)
    {
        if (RectContains (m_titleRects[i], x, y))
        {
            return (int) i;
        }
    }

    return -1;
}




////////////////////////////////////////////////////////////////////////////////
//
//  DxuiMenuBar::HitEntryIndex
//
////////////////////////////////////////////////////////////////////////////////

int DxuiMenuBar::HitEntryIndex (int x, int y) const
{
    RECT  rect     = DropdownRect();
    int   row      = 0;
    int   currentY = 0;
    int   localY   = y - rect.top;



    if (!m_isOpen || !RectContains (rect, x, y))
    {
        return -1;
    }

    for (const DxuiMenuBarSubitem & sub : m_items[m_openIndex].submenu)
    {
        int  entryHeight = EntryHeightPx (sub);

        if (localY >= currentY && localY < currentY + entryHeight)
        {
            return sub.isSeparator ? -1 : row;
        }

        currentY += entryHeight;
        if (!sub.isSeparator)
        {
            row++;
        }
    }

    return -1;
}




////////////////////////////////////////////////////////////////////////////////
//
//  DxuiMenuBar::EntryHeightPx
//
////////////////////////////////////////////////////////////////////////////////

int DxuiMenuBar::EntryHeightPx (const DxuiMenuBarSubitem & sub) const
{
    if (sub.isSeparator)
    {
        return ScaleDpi (s_kSeparatorHeightDip, m_dpi);
    }

    return m_rowHeightPx;
}




////////////////////////////////////////////////////////////////////////////////
//
//  DxuiMenuBar::DropdownHeightPx
//
////////////////////////////////////////////////////////////////////////////////

int DxuiMenuBar::DropdownHeightPx (int menuIndex) const
{
    int  height = 0;



    if (menuIndex < 0 || menuIndex >= (int) m_items.size())
    {
        return 0;
    }

    for (const DxuiMenuBarSubitem & sub : m_items[menuIndex].submenu)
    {
        height += EntryHeightPx (sub);
    }

    return height;
}




////////////////////////////////////////////////////////////////////////////////
//
//  DxuiMenuBar::VisibleRowCount
//
////////////////////////////////////////////////////////////////////////////////

int DxuiMenuBar::VisibleRowCount (int menuIndex) const
{
    int  count = 0;



    if (menuIndex < 0 || menuIndex >= (int) m_items.size())
    {
        return 0;
    }

    for (const DxuiMenuBarSubitem & sub : m_items[menuIndex].submenu)
    {
        if (!sub.isSeparator)
        {
            count++;
        }
    }

    return count;
}




////////////////////////////////////////////////////////////////////////////////
//
//  DxuiMenuBar::FirstEnabledRow
//
////////////////////////////////////////////////////////////////////////////////

int DxuiMenuBar::FirstEnabledRow (int menuIndex) const
{
    int  row = 0;



    if (menuIndex < 0 || menuIndex >= (int) m_items.size())
    {
        return -1;
    }

    for (const DxuiMenuBarSubitem & sub : m_items[menuIndex].submenu)
    {
        if (sub.isSeparator)
        {
            continue;
        }

        if (sub.enabled)
        {
            return row;
        }
        row++;
    }

    return (row > 0) ? 0 : -1;
}




////////////////////////////////////////////////////////////////////////////////
//
//  DxuiMenuBar::NextEnabledRow
//
//  Walks visible (non-separator) rows from `startRow` in `direction`
//  and returns the next enabled row index, wrapping around. Returns
//  startRow when no other enabled row exists.
//
////////////////////////////////////////////////////////////////////////////////

int DxuiMenuBar::NextEnabledRow (int menuIndex, int startRow, int direction) const
{
    int  count = VisibleRowCount (menuIndex);
    int  next  = startRow;



    if (count <= 0)
    {
        return -1;
    }

    for (int step = 0; step < count; step++)
    {
        const DxuiMenuBarSubitem * candidate = nullptr;

        next = (next + direction + count) % count;
        candidate = EntryAt (menuIndex, next);
        if (candidate != nullptr && candidate->enabled)
        {
            return next;
        }
    }

    return startRow;
}




////////////////////////////////////////////////////////////////////////////////
//
//  DxuiMenuBar::EntryAt
//
////////////////////////////////////////////////////////////////////////////////

const DxuiMenuBarSubitem * DxuiMenuBar::EntryAt (int menuIndex, int rowIndex) const
{
    int  row = 0;



    if (menuIndex < 0 || menuIndex >= (int) m_items.size() || rowIndex < 0)
    {
        return nullptr;
    }

    for (const DxuiMenuBarSubitem & sub : m_items[menuIndex].submenu)
    {
        if (sub.isSeparator)
        {
            continue;
        }

        if (row == rowIndex)
        {
            return &sub;
        }
        row++;
    }

    return nullptr;
}




////////////////////////////////////////////////////////////////////////////////
//
//  DxuiMenuBar::ParseMnemonic
//
//  Parses a Win32-style label ("E&xit") into a stripped string ("Exit")
//  plus the index of the mnemonic char in the stripped string and its
//  lower-cased character. A literal "&&" collapses to a single '&' and
//  never marks a mnemonic. When no marker is present `outIndex` is -1.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiMenuBar::ParseMnemonic (
    const std::wstring  & label,
    std::wstring        & outStripped,
    int                 & outIndex,
    wchar_t             & outLower)
{
    outStripped.clear();
    outIndex = -1;
    outLower = 0;

    for (size_t i = 0; i < label.size(); i++)
    {
        wchar_t  ch = label[i];

        if (ch == L'&')
        {
            if (i + 1 < label.size() && label[i + 1] == L'&')
            {
                outStripped.push_back (L'&');
                i++;
                continue;
            }

            if (outIndex < 0 && i + 1 < label.size())
            {
                outIndex = (int) outStripped.size();
                outLower = (wchar_t) towlower (label[i + 1]);
            }
            continue;
        }
        outStripped.push_back (ch);
    }
}




////////////////////////////////////////////////////////////////////////////////
//
//  DxuiMenuBar::ShouldShowMnemonicCues
//
//  Menu mnemonic underlines appear when (a) the user is holding Alt
//  (Windows convention for "show me the access keys") or (b) the menu
//  was opened via keyboard (F10 or Alt+mnemonic) -- keyboard navigation
//  implies the user wants to see the access keys. Mouse-opened menus
//  stay clean unless Alt is also pressed.
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiMenuBar::ShouldShowMnemonicCues (bool openedByKeyboard)
{
    if (openedByKeyboard)
    {
        return true;
    }

    return (GetAsyncKeyState (VK_MENU) & 0x8000) != 0;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiMenuBar::ResolveDropdownPalette
//
////////////////////////////////////////////////////////////////////////////////

DxuiMenuBar::DropdownPalette DxuiMenuBar::ResolveDropdownPalette (const IDxuiTheme & theme) const
{
    DropdownPalette  pal;


    pal.bg       = m_dropdownColorsSet ? m_dropBgOverride      : theme.BackgroundElevated();
    pal.hover    = m_dropdownColorsSet ? m_dropHoverOverride   : theme.HoverBackground();
    pal.text     = m_dropdownColorsSet ? m_dropTextOverride    : theme.Foreground();
    pal.accel    = m_dropdownColorsSet ? m_dropAccelOverride   : theme.ForegroundMuted();
    pal.border   = m_dropdownColorsSet ? m_dropBorderOverride  : theme.Border();
    pal.divider  = m_dropdownColorsSet ? m_dropDividerOverride : theme.Divider();
    pal.disabled = theme.ForegroundDisabled();

    return pal;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiMenuBar::ShowDropdownPopup
//
//  Raises the open submenu in a top-level popup (no capture, so the
//  owner keeps hover-switch). Anchored under the open title.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiMenuBar::ShowDropdownPopup ()
{
    DxuiPopupHost::ShowParams  showParams;
    POINT                      topLeft      = {};
    POINT                      botRight     = {};
    HWND                       owner        = nullptr;
    HRESULT                    hr           = S_OK;
    RECT                       title        = {};
    UINT                       eDpi         = (m_dpi == 0) ? (UINT) s_kBaseDpi : m_dpi;
    int                        dropWidthPx  = 0;
    int                        dropHeightPx = 0;


    DXUI_ASSERT_UI_THREAD();

    if (m_popupHost == nullptr || m_activePopup != nullptr)
    {
        return;
    }
    if (m_openIndex < 0 || m_openIndex >= (int) m_titleRects.size())
    {
        return;
    }

    owner         = m_popupHost->Hwnd();
    m_activePopup = m_popupHost->AcquirePopup();
    if (m_activePopup == nullptr)
    {
        return;
    }

    title        = m_titleRects[m_openIndex];
    dropWidthPx  = ScaleDpi (s_kDropdownWidthDip, eDpi);
    dropHeightPx = DropdownHeightPx (m_openIndex);

    // Anchor on the title (window-client px) -> screen px.
    topLeft.x  = title.left;
    topLeft.y  = title.top;
    botRight.x = title.left + dropWidthPx;
    botRight.y = title.bottom;
    ClientToScreen (owner, &topLeft);
    ClientToScreen (owner, &botRight);

    showParams.ownerHwnd        = owner;
    showParams.anchorRectScreen = { topLeft.x, topLeft.y, botRight.x, botRight.y };
    showParams.placement        = DxuiPopupPlacement::Below;
    showParams.flipIfOffscreen  = true;
    showParams.dismiss          = DxuiPopupDismiss::OnClickOutside;
    showParams.grabsCapture     = false;
    showParams.input            = DxuiPopupInput::Interactive;
    showParams.shadow           = true;
    // Show scales sizeDip by the owner DPI: width is the DIP constant
    // directly; height converts the measured pixel height back to DIPs.
    showParams.sizeDip.cx       = s_kDropdownWidthDip;
    showParams.sizeDip.cy       = MulDiv (dropHeightPx, s_kBaseDpi, (int) eDpi);
    showParams.backgroundArgb   = m_cachedPalette.bg;
    showParams.renderContent    = [this] (IDxuiPainter & p, IDxuiTextRenderer & t) { RenderDropdownPopup (p, t); };
    showParams.onMoveInside     = [this] (POINT localPx) { OnPopupMove  (localPx); };
    showParams.onClickInside    = [this] (POINT localPx) { OnPopupClick (localPx); };
    showParams.onClosed         = [this] () { m_activePopup = nullptr; m_isOpen = false; m_highlightIndex = -1; };

    hr = m_activePopup->Show (std::move (showParams));
    if (FAILED (hr))
    {
        m_popupHost->ReleasePopup (m_activePopup);
        m_activePopup = nullptr;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiMenuBar::ReleaseActivePopup
//
////////////////////////////////////////////////////////////////////////////////

void DxuiMenuBar::ReleaseActivePopup ()
{
    DxuiPopupHost *  popup = m_activePopup;


    // Null first so the popup's onClosed callback is a no-op and cannot
    // double-release.
    m_activePopup = nullptr;

    if (popup != nullptr && m_popupHost != nullptr)
    {
        m_popupHost->ReleasePopup (popup);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiMenuBar::RenderDropdownPopup
//
//  Popup render hook (popup-local pixels, origin top-left). Reuses the
//  shared row painter with the cached palette.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiMenuBar::RenderDropdownPopup (IDxuiPainter & painter, IDxuiTextRenderer & text) const
{
    RECT  placed = {};
    RECT  local  = {};


    if (m_activePopup == nullptr)
    {
        return;
    }

    placed       = m_activePopup->PlacedRectScreenPx();
    local.left   = 0;
    local.top    = 0;
    local.right  = placed.right  - placed.left;
    local.bottom = placed.bottom - placed.top;

    PaintDropdownRows (painter, text, local, m_cachedPalette, m_dpi);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiMenuBar::PopupRowAtLocalY
//
//  Popup-local y -> non-separator row index (mirrors HitEntryIndex's
//  index space). Returns -1 over a separator or past the last row.
//
////////////////////////////////////////////////////////////////////////////////

int DxuiMenuBar::PopupRowAtLocalY (int localYPx) const
{
    int  row      = 0;
    int  currentY = 0;


    if (m_openIndex < 0 || m_openIndex >= (int) m_items.size())
    {
        return -1;
    }

    for (const DxuiMenuBarSubitem & sub : m_items[m_openIndex].submenu)
    {
        int  entryHeight = EntryHeightPx (sub);

        if (localYPx >= currentY && localYPx < currentY + entryHeight)
        {
            return sub.isSeparator ? -1 : row;
        }

        currentY += entryHeight;
        if (!sub.isSeparator)
        {
            row++;
        }
    }

    return -1;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiMenuBar::OnPopupMove
//
////////////////////////////////////////////////////////////////////////////////

void DxuiMenuBar::OnPopupMove (POINT localPx)
{
    int  row = PopupRowAtLocalY (localPx.y);


    if (row >= 0 && row != m_highlightIndex)
    {
        m_highlightIndex = row;
        if (m_activePopup != nullptr)
        {
            m_activePopup->MarkDirty();
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiMenuBar::OnPopupClick
//
////////////////////////////////////////////////////////////////////////////////

void DxuiMenuBar::OnPopupClick (POINT localPx)
{
    int                          row      = PopupRowAtLocalY (localPx.y);
    const DxuiMenuBarSubitem  *  entry    = (row >= 0) ? EntryAt (m_openIndex, row) : nullptr;
    std::function<void()>        dispatch;


    DXUI_ASSERT_UI_THREAD();

    if (entry != nullptr && entry->enabled && entry->dispatch)
    {
        dispatch = entry->dispatch;
    }

    // Tear the popup down BEFORE running the command so a command that
    // spins a modal loop does not do so under the live popup window.
    Close();

    if (dispatch)
    {
        dispatch();
    }
}
