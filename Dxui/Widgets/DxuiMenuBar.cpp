#include "Pch.h"

#include "DxuiMenuBar.h"

#include "Core/DxuiThread.h"
#include "Render/IDxuiPainter.h"
#include "Render/IDxuiTextRenderer.h"
#include "Theme/IDxuiTheme.h"
#include "Theme/DxuiTheme.h"
#include "Window/DxuiHwndSource.h"
#include "Window/DxuiPopupHost.h"




static constexpr int      s_kBaseDpi                = 96;
static constexpr int      s_kNavHeightDip           = 32;
static constexpr int      s_kItemInternalPaddingDip = 8;
static constexpr int      s_kInterItemPaddingDip    = 4;
static constexpr int      s_kRowHeightDip           = 26;
static constexpr int      s_kSeparatorHeightDip     = 10;
static constexpr int      s_kSeparatorInsetDip      = 10;
static constexpr int      s_kMidpointDivisor        = 2;
static constexpr int      s_kDropdownWidthDip       = 300;
static constexpr int      s_kAccelOffsetDip         = 190;
static constexpr int      s_kRowPadLeftDip          = 10;
static constexpr int      s_kRowPadTopDip           = 5;
static constexpr int      s_kCheckGutterDip         = 18;
static constexpr float    s_kFontDip                = 14.0f;
static constexpr float    s_kUnderlineThicknessDip  = 1.0f;
static constexpr const wchar_t * s_kFontFamily           = DxuiTheme::kBodyFace;
static constexpr wchar_t  s_kpszCheckMark[]         = L"\u2713";

static constexpr int  s_kFallbackGlyphWidthDip = 8;





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiMenuBar::IsPointInRect
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiMenuBar::IsPointInRect (const RECT & rect, int x, int y)
{
    return x >= rect.left && x < rect.right && y >= rect.top && y < rect.bottom;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiMenuBar::ScaleDpi
//
////////////////////////////////////////////////////////////////////////////////

int DxuiMenuBar::ScaleDpi (int dipValue, UINT dpi)
{
    UINT  effectiveDpi = (dpi == 0) ? (UINT) s_kBaseDpi : dpi;



    return MulDiv (dipValue, (int) effectiveDpi, s_kBaseDpi);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiMenuBar::DxuiMenuBar
//
////////////////////////////////////////////////////////////////////////////////

DxuiMenuBar::DxuiMenuBar()
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiMenuBar::~DxuiMenuBar
//
////////////////////////////////////////////////////////////////////////////////

DxuiMenuBar::~DxuiMenuBar()
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
//  Wires the menu bar to a popup-hosting `DxuiHwndSource`. When set,
//  an open submenu renders into a top-level popup (so it can escape the
//  window and occlude); with no host it falls back to the in-window
//  inline dropdown.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiMenuBar::SetPopupHost (DxuiHwndSource * host)
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
//  DxuiMenuBar::GetStripHeightPx
//
////////////////////////////////////////////////////////////////////////////////

int DxuiMenuBar::GetStripHeightPx (UINT dpi)
{
    return ScaleDpi (s_kNavHeightDip, dpi);
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

void DxuiMenuBar::Hide()
{
    DXUI_ASSERT_UI_THREAD();

    Close();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiMenuBar::Open
//
//  Opens a menu, or switches to it from an already-open one.
//
//  Re-opening the SAME menu is a no-op except for the keyboard flag, and that
//  case is load-bearing: HandleMouseMove calls Open on every move over a
//  title, so without the early test the popup would be torn down and rebuilt
//  on each mouse move, flickering and losing its highlight.
//
//  A genuine switch releases the previous popup BEFORE setting the new state.
//  The outgoing popup's onClosed callback clears m_isOpen and m_highlightIndex,
//  so raising the new popup first would have that clear land on the menu just
//  opened and leave the bar showing a popup it believes is closed.
//
//  The highlight starts on the first ENABLED row so a keyboard user who opens
//  a menu and presses Enter activates something rather than a disabled item.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiMenuBar::Open (int menuIndex, bool keyboardActivated)
{
    bool  canOpen          = HasMenu (menuIndex);
    // Resting or moving over the ALREADY-open title must not churn the popup
    // (HandleMouseMove re-Opens on every move); that case only refreshes the
    // keyboard flag.
    bool  isAlreadyShowing = m_isOpen && m_openIndex == menuIndex && m_activePopup != nullptr;



    DXUI_ASSERT_UI_THREAD();

    if (canOpen && isAlreadyShowing)
    {
        m_openedByKeyboard = keyboardActivated;
    }
    else if (canOpen)
    {
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
        m_highlightIndex   = GetFirstEnabledRow (menuIndex);

        if (m_popupHost != nullptr)
        {
            ShowDropdownPopup();
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiMenuBar::Close
//
////////////////////////////////////////////////////////////////////////////////

void DxuiMenuBar::Close()
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

void DxuiMenuBar::CloseAll()
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

void DxuiMenuBar::ClearFocus()
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
    wchar_t  lower   = (wchar_t) towlower (ch);
    size_t   i       = 0;
    int      matched = -1;



    DXUI_ASSERT_UI_THREAD();

    for (i = 0; i < m_items.size() && matched < 0; i++)
    {
        if (m_items[i].altLetter != 0 && m_items[i].altLetter == lower)
        {
            matched = (int) i;
        }
    }

    if (matched >= 0)
    {
        Open (matched, true);
    }

    return matched >= 0;
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
    const DxuiMenuBarSubitem  * entry     = nullptr;
    int                         count     = GetVisibleRowCount (m_openIndex);
    int                         menuCount = (int) m_items.size();
    int                         next      = 0;
    // The three guards the original ladder re-tested at each step: a key is
    // only interesting while open, menu-switching also needs menus, and
    // row navigation also needs rows.
    bool  hasMenus  = m_isOpen && menuCount > 0;
    bool  hasRows   = hasMenus && count > 0;
    bool  isPrevKey = vk == VK_LEFT  || (vk == VK_TAB && (GetKeyState (VK_SHIFT) & 0x8000));
    bool  isNextKey = vk == VK_RIGHT || vk == VK_TAB;
    bool  handled   = false;



    DXUI_ASSERT_UI_THREAD();

    if (m_isOpen && (vk == VK_ESCAPE || vk == VK_F10))
    {
        Close();
        handled = true;
    }
    else if (hasMenus && isPrevKey)
    {
        next = (m_openIndex <= 0) ? (menuCount - 1) : (m_openIndex - 1);
        Open (next, m_openedByKeyboard);
        handled = true;
    }
    else if (hasMenus && isNextKey)
    {
        next = (m_openIndex + 1) % menuCount;
        Open (next, m_openedByKeyboard);
        handled = true;
    }
    else if (hasRows && (vk == VK_DOWN || vk == VK_UP))
    {
        m_highlightIndex = GetNextEnabledRow (m_openIndex, m_highlightIndex, (vk == VK_DOWN) ? +1 : -1);

        if (m_activePopup != nullptr)
        {
            m_activePopup->MarkDirty();
        }

        handled = true;
    }
    else if (hasRows && (vk == VK_RETURN || vk == VK_SPACE))
    {
        // A highlighted-but-undispatchable row leaves the key unconsumed,
        // exactly as before.
        entry = GetEntryAt (m_openIndex, m_highlightIndex);

        if (entry != nullptr && entry->IsEnabled() && entry->dispatch)
        {
            entry->dispatch();
            Close();
            handled = true;
        }
    }
    else if (hasRows && vk >= 'A' && vk <= 'Z')
    {
        handled = ActivateMnemonicRow ((wchar_t) vk);
    }

    return handled;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiMenuBar::ActivateMnemonicRow
//
//  Dispatches the open submenu's row whose label carries mnemonic `ch`, and
//  reports whether one was found. A row that matches but is disabled or has
//  no dispatch is not a match, so the key stays unconsumed.
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiMenuBar::ActivateMnemonicRow (wchar_t ch)
{
    const DxuiMenuBarSubitem  * hit      = nullptr;
    std::wstring                stripped;
    wchar_t                     lower    = (wchar_t) towlower (ch);
    wchar_t                     mnCh     = 0;
    int                         mnIdx    = -1;
    int                         row      = 0;
    int                         hitRow   = -1;



    for (const DxuiMenuBarSubitem & sub : m_items[m_openIndex].submenu)
    {
        if (!sub.isSeparator)
        {
            if (hit == nullptr)
            {
                ParseMnemonic (sub.GetLabelText(), stripped, mnIdx, mnCh);

                if (mnCh != 0 && mnCh == lower && sub.IsEnabled() && sub.dispatch)
                {
                    hit    = &sub;
                    hitRow = row;
                }
            }

            row++;
        }
    }

    if (hit != nullptr)
    {
        // Highlight before dispatching: the callback can close or rebuild the
        // menu, and the original set the highlight first for that reason.
        m_highlightIndex = hitRow;
        hit->dispatch();
        Close();
    }

    return hit != nullptr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiMenuBar::HandleMouseMove
//
//  Tracks hover across the title strip and the open dropdown, switching menus
//  on the way.
//
//  The synthetic-move guard is the important part. Windows posts a
//  WM_MOUSEMOVE at the UNCHANGED cursor position whenever the dropdown popup
//  shows or hides under the pointer -- so opening a menu from the KEYBOARD
//  generates a move at wherever the mouse happens to be resting, which would
//  immediately switch the open menu to whatever title is under it. Comparing
//  against the last position distinguishes a real move from that echo.
//
//  Hovering a title only SWITCHES menus while one is already open. A closed
//  strip merely tracks hover for painting, which is what keeps a menu from
//  springing open just because the pointer crossed the bar.
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiMenuBar::HandleMouseMove (int x, int y)
{
    int   hitTitle    = 0;
    int   hitEntry    = 0;
    bool  handled     = false;
    // Windows posts a WM_MOUSEMOVE at the UNCHANGED cursor position whenever
    // the dropdown popup shows or hides under the pointer. Without this guard
    // a keyboard menu switch is instantly overridden by the resting mouse's
    // title, so a synthetic repeat is not a real move.
    bool  isRealMove  = !(m_haveLastMousePos && x == m_lastMouseX && y == m_lastMouseY);



    DXUI_ASSERT_UI_THREAD();

    if (isRealMove)
    {
        m_haveLastMousePos = true;
        m_lastMouseX       = x;
        m_lastMouseY       = y;

        hitTitle     = HitTitleIndex (x, y);
        hitEntry     = HitEntryIndex (x, y);
        m_hoverIndex = hitTitle;

        if (hitTitle >= 0)
        {
            // Hovering a title only switches menus while one is already open;
            // a closed strip just tracks hover for painting.
            if (m_isOpen)
            {
                Open (hitTitle, m_openedByKeyboard);
            }

            handled = true;
        }
        else if (hitEntry >= 0)
        {
            m_highlightIndex = hitEntry;
            handled          = true;
        }
    }

    return handled;
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

void DxuiMenuBar::ClearHover()
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
//  Press handling for the strip: open, toggle shut, or dismiss.
//
//  Clicking the ALREADY-open title closes it, which is what makes the title
//  behave like a toggle rather than re-opening the menu the click was meant to
//  dismiss.
//
//  A click outside both the strip and the dropdown closes the menu but is
//  deliberately NOT consumed -- the return value reports only whether a title
//  was hit. Swallowing it would cost the user a click every time they dismiss
//  a menu by clicking the thing they actually wanted.
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiMenuBar::HandleMouseDown (int x, int y)
{
    int   hitTitle = HitTitleIndex (x, y);
    bool  onTitle  = (hitTitle >= 0);



    DXUI_ASSERT_UI_THREAD();

    if (onTitle && m_isOpen && m_openIndex == hitTitle)
    {
        // Clicking the open title toggles it shut.
        Close();
    }
    else if (onTitle)
    {
        Open (hitTitle, false);
    }
    else if (m_isOpen && !IsPointInRect (GetDropdownRect(), x, y))
    {
        // A click outside both the strip and the dropdown dismisses, but is
        // NOT consumed -- whatever is underneath still gets it.
        Close();
    }

    return onTitle;
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
    bool                         handled  = false;



    DXUI_ASSERT_UI_THREAD();

    if (hitEntry >= 0)
    {
        entry = GetEntryAt (m_openIndex, hitEntry);
    }

    if (entry != nullptr && entry->IsEnabled() && entry->dispatch)
    {
        entry->dispatch();
        Close();
        handled = true;
    }

    return handled;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiMenuBar::PaintStrip
//
//  Paints the title strip: background, each title, and the mnemonic underlines.
//
//  A title is highlighted for three unrelated reasons -- pointer hover, being
//  the open menu, or holding keyboard focus while the strip is CLOSED. The
//  last is qualified deliberately: once a menu is open the open-menu highlight
//  is the truthful one, and painting both would show two active titles.
//
//  Mnemonic underlines appear only when the cues are enabled (the Windows
//  convention that Alt reveals them), so a mouse user sees clean labels.
//
//  Drawing an underline requires knowing where a character SITS inside a
//  centered string, which the text renderer does not report -- so its offset
//  is derived by measuring the prefix before it and the prefix including it,
//  and taking the difference as the character width. The centered start is
//  recovered the same way, from the full string width against the title rect.
//  A failed or zero-width measurement skips just that underline rather than
//  painting one at a guessed position.
//
//  Strip colors fall back to the theme unless explicitly overridden, so a host
//  can tint the bar to match custom chrome without re-theming everything.
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

        hr = text.DrawString (stripped.c_str(),
                              (float) m_titleRects[i].left,
                              (float) m_titleRects[i].top,
                              rectW,
                              rectH,
                              stripFg,
                              fontDip,
                              s_kFontFamily,
                              DxuiTextHAlign::Center,
                              DxuiTextVAlign::Center,
                              DxuiFontWeight::Normal,
                              false);
        IGNORE_RETURN_VALUE (hr, S_OK);

        if (showCues && mnIdx >= 0 && !stripped.empty())
        {
            float         fullW    = 0.0f;
            float         fullH    = 0.0f;
            float         prefixW  = 0.0f;
            float         charW    = 0.0f;
            std::wstring  prefix   = stripped.substr (0, (size_t) mnIdx);
            std::wstring  prefixCh = stripped.substr (0, (size_t) mnIdx + 1);
            HRESULT       hrM      = text.MeasureString (stripped.c_str(), fontDip, s_kFontFamily, fullW, fullH);
            float         baseX    = 0.0f;
            float         baseY    = 0.0f;

            if (FAILED (hrM) || fullW <= 0.0f)
            {
                continue;
            }

            if (!prefix.empty())
            {
                float ignH = 0.0f;
                hrM = text.MeasureString (prefix.c_str(), fontDip, s_kFontFamily, prefixW, ignH);
                IGNORE_RETURN_VALUE (hrM, S_OK);
            }

            {
                float pcW  = 0.0f;
                float ignH = 0.0f;
                hrM = text.MeasureString (prefixCh.c_str(), fontDip, s_kFontFamily, pcW, ignH);
                IGNORE_RETURN_VALUE (hrM, S_OK);
                charW = pcW - prefixW;
            }

            baseX = (float) m_titleRects[i].left + (rectW - fullW) / 2.0f + prefixW;
            baseY = (float) m_titleRects[i].top  + (rectH + fullH) / 2.0f;

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

    PaintDropdownRows (painter, text, GetDropdownRect(), ResolveDropdownPalette (theme), dpi);
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
        int           entryHeight = GetEntryHeightPx (sub);
        uint32_t      labelArgb   = sub.IsEnabled() ? pal.text  : pal.disabled;
        uint32_t      hotkeyArgb  = sub.IsEnabled() ? pal.accel : pal.disabled;

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

        ParseMnemonic (sub.GetLabelText(), stripped, mnIdx, mnCh);

        if (row == m_highlightIndex)
        {
            painter.FillRect ((float) rect.left,
                              (float) (rect.top + y),
                              (float) (rect.right - rect.left),
                              (float) entryHeight,
                              pal.hover);
        }

        {
            // A row with no hotkey owns the whole width; otherwise the label
            // stops where the accelerator column starts.
            float  labelW = sub.hotkey.empty()
                          ? (float) (rect.right - rect.left - labelLeftPx - rowPadLeftPx)
                          : (float) accelOffsetPx;

            hr = text.DrawString (stripped.c_str(),
                                  (float) (rect.left + labelLeftPx),
                                  (float) (rect.top + y + rowPadTopPx),
                                  labelW,
                                  (float) entryHeight,
                                  labelArgb,
                                  fontDip,
                                  s_kFontFamily);
            IGNORE_RETURN_VALUE (hr, S_OK);
        }

        if (sub.checkable && sub.isChecked && sub.isChecked())
        {
            hr = text.DrawString (s_kpszCheckMark,
                                  (float) (rect.left + rowPadLeftPx),
                                  (float) (rect.top + y + rowPadTopPx),
                                  (float) checkGutterPx,
                                  (float) entryHeight,
                                  labelArgb,
                                  fontDip,
                                  s_kFontFamily);
            IGNORE_RETURN_VALUE (hr, S_OK);
        }

        if (showCues && mnIdx >= 0 && !stripped.empty() && sub.IsEnabled())
        {
            float         prefixW  = 0.0f;
            float         charW    = 0.0f;
            float         fullH    = 0.0f;
            std::wstring  prefix   = stripped.substr (0, (size_t) mnIdx);
            std::wstring  prefixCh = stripped.substr (0, (size_t) mnIdx + 1);
            HRESULT       hrM      = S_OK;
            float         baseX    = 0.0f;
            float         baseY    = 0.0f;

            if (!prefix.empty())
            {
                hrM = text.MeasureString (prefix.c_str(), fontDip, s_kFontFamily, prefixW, fullH);
                IGNORE_RETURN_VALUE (hrM, S_OK);
            }
            else
            {
                std::wstring oneCh (1, stripped[(size_t) mnIdx]);
                hrM = text.MeasureString (oneCh.c_str(), fontDip, s_kFontFamily, prefixW, fullH);
                IGNORE_RETURN_VALUE (hrM, S_OK);
                prefixW = 0.0f;
            }

            {
                float pcW  = 0.0f;
                float ignH = 0.0f;
                hrM = text.MeasureString (prefixCh.c_str(), fontDip, s_kFontFamily, pcW, ignH);
                IGNORE_RETURN_VALUE (hrM, S_OK);
                charW = pcW - prefixW;
            }

            baseX = (float) (rect.left + labelLeftPx) + prefixW;
            baseY = (float) (rect.top + y + rowPadTopPx) + fullH;

            painter.FillRect (baseX, baseY, charW, s_kUnderlineThicknessDip, labelArgb);
        }

        if (!sub.hotkey.empty())
        {
            hr = text.DrawString (sub.hotkey.c_str(),
                                  (float) (rect.left + accelOffsetPx),
                                  (float) (rect.top + y + rowPadTopPx),
                                  (float) (rect.right - rect.left - accelOffsetPx),
                                  (float) entryHeight,
                                  hotkeyArgb,
                                  fontDip,
                                  s_kFontFamily);
            IGNORE_RETURN_VALUE (hr, S_OK);
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
            scaler.GetDpi(),
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

    // Keep the popup's colors fresh: its render hook gets no theme, so
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
    bool  handled = false;



    DXUI_ASSERT_UI_THREAD();

    if (ev.kind == DxuiKeyEventKind::Down)
    {
        handled = HandleKey (ev.vk);
    }

    return handled;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiMenuBar::OnMouse
//
//  The IDxuiControl entry point: unpacks the event and forwards to the
//  per-gesture handlers.
//
//  Kept as a thin adapter because those handlers take plain coordinates and
//  are therefore unit-testable without constructing framework events -- which
//  is where the menu bar's behavior is actually covered.
//
//  Only the LEFT button is acted on. A right-click over the strip belongs to
//  whatever context menu the host provides, so it is reported unhandled rather
//  than silently eaten by the bar.
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiMenuBar::OnMouse (const DxuiMouseEvent & ev)
{
    bool  handled = false;



    DXUI_ASSERT_UI_THREAD();

    switch (ev.kind)
    {
    case DxuiMouseEventKind::Move:
        handled = HandleMouseMove (ev.positionDip.x, ev.positionDip.y);
        break;
    case DxuiMouseEventKind::Down:
        if (ev.button == DxuiMouseButton::Left)
        {
            handled = HandleMouseDown (ev.positionDip.x, ev.positionDip.y);
        }

        break;
    case DxuiMouseEventKind::Up:
        if (ev.button == DxuiMouseButton::Left)
        {
            handled = HandleMouseUp (ev.positionDip.x, ev.positionDip.y);
        }

        break;
    default:
        break;
    }

    return handled;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiMenuBar::GetMenuRect
//
////////////////////////////////////////////////////////////////////////////////

RECT DxuiMenuBar::GetMenuRect (int menuIndex) const
{
    RECT  rect = {};



    if (HasTitleRect (menuIndex))
    {
        rect = m_titleRects[menuIndex];
    }

    return rect;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiMenuBar::GetMenuStripContentWidthPx
//
////////////////////////////////////////////////////////////////////////////////

int DxuiMenuBar::GetMenuStripContentWidthPx() const
{
    return m_titleRects.empty() ? 0 : m_titleRects.back().right;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiMenuBar::GetDropdownWidthPx
//
//  The dropdown was a fixed 300 DIP. Any label longer than that wrapped and
//  overran the row beneath it, which is invisible until a caller has something
//  long to say -- and menu labels that name a file routinely do.
//
//  Measures the widest row instead: label plus its accelerator, inside the
//  same gutters the rows are drawn with, floored at the old fixed width so
//  every existing menu keeps the width it had.
//
////////////////////////////////////////////////////////////////////////////////

int DxuiMenuBar::GetDropdownWidthPx (size_t index, UINT dpi) const
{
    int   minWidthPx    = ScaleDpi (s_kDropdownWidthDip, dpi);
    int   rowPadLeftPx  = ScaleDpi (s_kRowPadLeftDip, dpi);
    int   checkGutterPx = ScaleDpi (s_kCheckGutterDip, dpi);
    int   gapPx         = ScaleDpi (s_kRowPadLeftDip, dpi) * 2;
    int   widestPx      = 0;
    float fontDip       = (float) ScaleDpi ((int) s_kFontDip, dpi);



    if (index >= m_items.size())
    {
        return minWidthPx;
    }

    for (const DxuiMenuBarSubitem & sub : m_items[index].submenu)
    {
        std::wstring  stripped;
        wchar_t       mnCh  = 0;
        int           mnIdx = -1;
        int           rowPx = 0;

        if (sub.isSeparator)
        {
            continue;
        }

        ParseMnemonic (sub.GetLabelText(), stripped, mnIdx, mnCh);

        rowPx = MeasureRunPx (stripped, fontDip, dpi);

        if (!sub.hotkey.empty())
        {
            rowPx += gapPx + MeasureRunPx (sub.hotkey, fontDip, dpi);
        }

        if (rowPx > widestPx)
        {
            widestPx = rowPx;
        }
    }

    widestPx += rowPadLeftPx + checkGutterPx + rowPadLeftPx;

    return (widestPx > minWidthPx) ? widestPx : minWidthPx;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiMenuBar::MeasureRunPx
//
//  One text run in pixels, from the cached measuring renderer when there is
//  one and a DPI-scaled glyph estimate otherwise -- the same fallback the
//  title row uses, so a transient renderer failure degrades identically.
//
////////////////////////////////////////////////////////////////////////////////

int DxuiMenuBar::MeasureRunPx (const std::wstring & run, float fontDip, UINT dpi) const
{
    HRESULT  hr     = E_FAIL;
    float    width  = 0.0f;
    float    height = 0.0f;



    if (m_textRendererForMeasure != nullptr)
    {
        hr = m_textRendererForMeasure->MeasureString (run.c_str(), fontDip, s_kFontFamily,
                                                      width, height);
    }

    if (SUCCEEDED (hr) && width > 0.0f)
    {
        return (int) (width + 0.5f);
    }

    return (int) run.size() * ScaleDpi (s_kFallbackGlyphWidthDip, dpi);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiMenuBar::GetDropdownRect
//
////////////////////////////////////////////////////////////////////////////////

RECT DxuiMenuBar::GetDropdownRect() const
{
    RECT  rect          = {};
    RECT  title         = {};
    int   dropdownWidth = GetDropdownWidthPx ((size_t) m_openIndex, m_dpi);



    if (HasTitleRect (m_openIndex))
    {
        title       = m_titleRects[m_openIndex];
        rect.left   = title.left;
        rect.top    = title.bottom;
        rect.right  = title.left + dropdownWidth;
        rect.bottom = title.bottom + GetDropdownHeightPx (m_openIndex);
    }

    return rect;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiMenuBar::HitTitleIndex
//
////////////////////////////////////////////////////////////////////////////////

int DxuiMenuBar::HitTitleIndex (int x, int y) const
{
    size_t  i   = 0;
    int     hit = -1;



    for (i = 0; i < m_titleRects.size() && hit < 0; i++)
    {
        if (IsPointInRect (m_titleRects[i], x, y))
        {
            hit = (int) i;
        }
    }

    return hit;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiMenuBar::HitEntryIndex
//
//  Which dropdown ROW is under a point, or -1.
//
//  Rows are walked and accumulated rather than divided, because entry heights
//  are not uniform: a separator is much shorter than a command. A single
//  divide would misidentify every row after the first separator.
//
//  Two counters are tracked for the same reason. The pixel cursor advances by
//  every entry including separators, while the returned INDEX counts only
//  selectable rows -- so the index handed back matches the numbering keyboard
//  navigation and the callbacks use, in which separators do not exist.
//
//  A hit on a separator returns -1: it consumes the position without selecting
//  anything, so dragging across a separator does not highlight the row beyond
//  it.
//
////////////////////////////////////////////////////////////////////////////////

int DxuiMenuBar::HitEntryIndex (int x, int y) const
{
    RECT  rect     = GetDropdownRect();
    int   row      = 0;
    int   currentY = 0;
    int   localY   = y - rect.top;
    int   hit      = -1;
    bool  isInDrop = m_isOpen && IsPointInRect (rect, x, y);
    bool  found    = false;



    if (isInDrop)
    {
        for (const DxuiMenuBarSubitem & sub : m_items[m_openIndex].submenu)
        {
            int  entryHeight = GetEntryHeightPx (sub);

            if (!found && localY >= currentY && localY < currentY + entryHeight)
            {
                // A separator swallows the hit rather than selecting a row.
                hit   = sub.isSeparator ? -1 : row;
                found = true;
            }

            currentY += entryHeight;

            if (!sub.isSeparator)
            {
                row++;
            }
        }
    }

    return hit;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiMenuBar::GetEntryHeightPx
//
////////////////////////////////////////////////////////////////////////////////

int DxuiMenuBar::GetEntryHeightPx (const DxuiMenuBarSubitem & sub) const
{
    return sub.isSeparator ? ScaleDpi (s_kSeparatorHeightDip, m_dpi) : m_rowHeightPx;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiMenuBar::GetDropdownHeightPx
//
////////////////////////////////////////////////////////////////////////////////

int DxuiMenuBar::GetDropdownHeightPx (int menuIndex) const
{
    int  height = 0;



    if (HasMenu (menuIndex))
    {
        for (const DxuiMenuBarSubitem & sub : m_items[menuIndex].submenu)
        {
            height += GetEntryHeightPx (sub);
        }
    }

    return height;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiMenuBar::GetVisibleRowCount
//
////////////////////////////////////////////////////////////////////////////////

int DxuiMenuBar::GetVisibleRowCount (int menuIndex) const
{
    int  count = 0;



    if (HasMenu (menuIndex))
    {
        for (const DxuiMenuBarSubitem & sub : m_items[menuIndex].submenu)
        {
            if (!sub.isSeparator)
            {
                count++;
            }
        }
    }

    return count;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiMenuBar::GetFirstEnabledRow
//
//  Where a freshly-opened menu puts its highlight.
//
//  Preferring the first ENABLED row means opening a menu whose top item is
//  grayed out (a Paste with an empty clipboard) still lands the caret
//  somewhere Enter will do something.
//
//  When nothing is enabled the highlight still goes to row 0 rather than
//  nowhere, so the dropdown opens looking active instead of blank; -1 is
//  reserved for a menu with no rows at all.
//
//  The walk goes through EntryAt by row index rather than iterating the
//  submenu directly, which keeps the skip-separators rule in one place instead
//  of open-coding it a third time. VisibleRowCount already answers 0 for an
//  out-of-range menu, so the loop covers the bad-index case with no guard of
//  its own.
//
////////////////////////////////////////////////////////////////////////////////

int DxuiMenuBar::GetFirstEnabledRow (int menuIndex) const
{
    const DxuiMenuBarSubitem *  entry = nullptr;
    int                         count = GetVisibleRowCount (menuIndex);
    int                         row   = 0;
    int                         first = -1;



    // VisibleRowCount already answers 0 for an out-of-range menu, so the
    // loop below covers the bad-index case without its own guard. Walking by
    // row index through EntryAt also keeps the "skip separators" rule in one
    // place instead of open-coding it a third time.
    for (row = 0; row < count && first < 0; row++)
    {
        entry = GetEntryAt (menuIndex, row);

        if (entry != nullptr && entry->IsEnabled())
        {
            first = row;
        }
    }

    // Nothing enabled: still highlight the first visible row so the dropdown
    // opens somewhere, and answer -1 only when there are no rows at all.
    if (first < 0 && count > 0)
    {
        first = 0;
    }

    return first;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiMenuBar::GetNextEnabledRow
//
//  Walks visible (non-separator) rows from `startRow` in `direction`
//  and returns the next enabled row index, wrapping around. Returns
//  startRow when no other enabled row exists.
//
////////////////////////////////////////////////////////////////////////////////

int DxuiMenuBar::GetNextEnabledRow (int menuIndex, int startRow, int direction) const
{
    const DxuiMenuBarSubitem *  candidate = nullptr;
    int                         count     = GetVisibleRowCount (menuIndex);
    int                         next      = startRow;
    int                         step      = 0;
    bool                        isFound   = false;
    // No rows at all is -1; rows but none enabled leaves the caret put.
    int                         found     = (count <= 0) ? -1 : startRow;



    for (step = 0; step < count && !isFound; step++)
    {
        next      = (next + direction + count) % count;
        candidate = GetEntryAt (menuIndex, next);

        if (candidate != nullptr && candidate->IsEnabled())
        {
            found   = next;
            isFound = true;
        }
    }

    return found;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiMenuBar::GetEntryAt
//
//  Maps a visible ROW index to its submenu entry, skipping separators.
//
//  This is the single place the two numbering schemes are reconciled: the
//  submenu vector holds separators, while every index the rest of the menu bar
//  deals in -- keyboard highlight, hit testing, callbacks -- counts only
//  selectable rows. Routing all row lookups through here is what keeps that
//  translation from being re-derived, slightly differently, at each call site.
//
//  Returns null for an out-of-range menu or row, so callers can test the
//  pointer instead of pre-validating bounds.
//
////////////////////////////////////////////////////////////////////////////////

const DxuiMenuBarSubitem * DxuiMenuBar::GetEntryAt (int menuIndex, int rowIndex) const
{
    const DxuiMenuBarSubitem *  entry = nullptr;
    int                         row   = 0;



    if (HasMenu (menuIndex) && rowIndex >= 0)
    {
        for (const DxuiMenuBarSubitem & sub : m_items[menuIndex].submenu)
        {
            if (!sub.isSeparator)
            {
                // `row` only ever passes rowIndex once, so this cannot
                // overwrite an entry already found.
                if (row == rowIndex)
                {
                    entry = &sub;
                }

                row++;
            }
        }
    }

    return entry;
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
    return openedByKeyboard || (GetAsyncKeyState (VK_MENU) & 0x8000) != 0;
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

void DxuiMenuBar::ShowDropdownPopup()
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
    bool                       hasAnchor    = HasTitleRect (m_openIndex);



    DXUI_ASSERT_UI_THREAD();

    // Nothing to raise: no host, one already up, or the strip has not been
    // laid out yet so there is no title to anchor under.
    BAIL_OUT_IF (m_popupHost == nullptr || m_activePopup != nullptr, S_OK);
    BAIL_OUT_IF (!hasAnchor, S_OK);

    owner         = m_popupHost->GetHwnd();
    m_activePopup = m_popupHost->AcquirePopup();

    BAIL_OUT_IF (m_activePopup == nullptr, S_OK);

    title        = m_titleRects[m_openIndex];
    dropWidthPx  = GetDropdownWidthPx ((size_t) m_openIndex, eDpi);
    dropHeightPx = GetDropdownHeightPx (m_openIndex);

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
    showParams.sizeDip.cx       = MulDiv (dropWidthPx, s_kBaseDpi, (int) eDpi);
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

Error:
    return;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiMenuBar::ReleaseActivePopup
//
////////////////////////////////////////////////////////////////////////////////

void DxuiMenuBar::ReleaseActivePopup()
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

    placed       = m_activePopup->GetPlacedRectScreenPx();
    local.left   = 0;
    local.top    = 0;
    local.right  = placed.right  - placed.left;
    local.bottom = placed.bottom - placed.top;

    PaintDropdownRows (painter, text, local, m_cachedPalette, m_dpi);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiMenuBar::GetPopupRowAtLocalY
//
//  Popup-local y -> non-separator row index (mirrors HitEntryIndex's
//  index space). Returns -1 over a separator or past the last row.
//
////////////////////////////////////////////////////////////////////////////////

int DxuiMenuBar::GetPopupRowAtLocalY (int localYPx) const
{
    int   row      = 0;
    int   currentY = 0;
    int   hit      = -1;
    bool  found    = false;



    if (HasMenu (m_openIndex))
    {
        for (const DxuiMenuBarSubitem & sub : m_items[m_openIndex].submenu)
        {
            int  entryHeight = GetEntryHeightPx (sub);

            if (!found && localYPx >= currentY && localYPx < currentY + entryHeight)
            {
                // A separator swallows the hit rather than selecting a row.
                hit   = sub.isSeparator ? -1 : row;
                found = true;
            }

            currentY += entryHeight;

            if (!sub.isSeparator)
            {
                row++;
            }
        }
    }

    return hit;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiMenuBar::OnPopupMove
//
////////////////////////////////////////////////////////////////////////////////

void DxuiMenuBar::OnPopupMove (POINT localPx)
{
    int  row = GetPopupRowAtLocalY (localPx.y);



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
    int                          row      = GetPopupRowAtLocalY (localPx.y);
    const DxuiMenuBarSubitem  *  entry    = (row >= 0) ? GetEntryAt (m_openIndex, row) : nullptr;
    std::function<void()>        dispatch;



    DXUI_ASSERT_UI_THREAD();

    if (entry != nullptr && entry->IsEnabled() && entry->dispatch)
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

