#include "Pch.h"
#include "Theme/DxuiTheme.h"

#include "DxuiMenuBar.h"
#include "Core/DxuiSystemSettings.h"
#include "Window/DxuiHwndSource.h"

#include "Core/UnicodeSymbols.h"




static constexpr int      s_kBaseDpi                = 96;
static constexpr int      s_kNavHeightDip           = 32;
static constexpr int      s_kItemInternalPaddingDip = 8;
static constexpr int      s_kInterItemPaddingDip    = 4;
static constexpr float    s_kUnderlineThicknessDip  = 1.0f;
static constexpr const wchar_t * s_kFontFamily           = DxuiTheme::kBodyFace;

static constexpr int  s_kFallbackGlyphWidthDip = 8;

// An in-window menu is kept inside the host client rect. Until a host says
// what that is, nothing constrains it.
static constexpr LONG s_kUnboundedClientPx = 1L << 28;





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
//  The popup takes no mouse capture, so the strip keeps seeing the pointer
//  and can swap titles on hover, and so a click on a title reaches the bar
//  BEFORE the popup dismisses itself. That ordering is what makes the
//  title toggle correctly without a time-based reopen guard, which is
//  therefore switched off.
//
////////////////////////////////////////////////////////////////////////////////

DxuiMenuBar::DxuiMenuBar()
{
    m_hostClient.left   = -s_kUnboundedClientPx;
    m_hostClient.top    = -s_kUnboundedClientPx;
    m_hostClient.right  =  s_kUnboundedClientPx;
    m_hostClient.bottom =  s_kUnboundedClientPx;

    m_dropdown.SetGrabsCapture (false);
    m_dropdown.SetReopenGuard  (false);
    m_dropdown.SetOnClosed ([this] (bool) { m_isOpen = false; });
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
//  An open menu is closed first, since its rows point into the list
//  being replaced.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiMenuBar::SetItems (std::vector<DxuiMenuBarItem> items)
{
    DXUI_ASSERT_UI_THREAD();

    Close();

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
//  Wires the menu bar to a popup-hosting `DxuiHwndSource`. When set, an
//  open menu renders into a top-level popup (so it can escape the window
//  and occlude); with no host it falls back to the in-window menu the
//  owner paints. A live popup is released against the CURRENT host before
//  repointing.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiMenuBar::SetPopupHost (DxuiHwndSource * host)
{
    DXUI_ASSERT_UI_THREAD();

    if (host != m_popupHost)
    {
        Close();
    }

    m_popupHost = host;
    m_dropdown.SetPopupHost (host);
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

    m_dropdown.SetColors (bgArgb, hoverArgb, textArgb, accelArgb, borderArgb, dividerArgb);
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
//  DxuiMenuBar::GetMenuFontPx
//
//  The em size of the system menu font at a DPI, re-read only when the DPI
//  changes. Layout and PaintStrip both ask on every call, and the metrics
//  come from a system-parameters query that has no business running per
//  frame.
//
////////////////////////////////////////////////////////////////////////////////

float DxuiMenuBar::GetMenuFontPx (UINT eDpi)
{
    if (m_metricsDpi != eDpi)
    {
        m_metrics    = DxuiMenuMetrics::FromSystem (eDpi);
        m_metricsDpi = eDpi;
    }

    return m_metrics.fontPx;
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
    float  fontDip  = GetMenuFontPx (eDpi);



    DXUI_ASSERT_UI_THREAD();

    if (pTextForMeasure != nullptr)
    {
        m_textRendererForMeasure = pTextForMeasure;
    }

    m_stripRect.left   = x;
    m_stripRect.top    = y;
    m_stripRect.right  = x + width;
    m_stripRect.bottom = y + height;
    m_dpi              = eDpi;
    m_titleRects.assign (m_items.size(), RECT {});
    m_dropdown.SetDpi (eDpi);

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
//  A genuine switch hides the previous menu BEFORE setting the new state.
//  The outgoing menu's closed callback clears m_isOpen, so raising the new
//  one first would have that clear land on the menu just opened and leave
//  the bar showing a menu it believes is closed.
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
    bool  isAlreadyShowing = m_isOpen && m_openIndex == menuIndex && m_dropdown.IsVisible();



    DXUI_ASSERT_UI_THREAD();

    if (canOpen && isAlreadyShowing)
    {
        m_openedByKeyboard = keyboardActivated;
    }
    else if (canOpen)
    {
        // Walking from one title to another is a move WITHIN menu mode, not
        // an entry into it, so the open animation plays only for the first.
        bool  wasOpen = m_isOpen && m_dropdown.IsVisible();

        m_dropdown.Hide();
        m_dropdown.SetRevealSuppressed (wasOpen);

        m_openIndex        = menuIndex;
        m_isOpen           = true;
        m_openedByKeyboard = keyboardActivated;

        ShowOpenMenu();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiMenuBar::ShowOpenMenu
//
//  Raises the popup menu under the open title with that title's rows. With
//  no strip laid out yet the anchor is an empty rect at the origin, which is
//  what a test that opens a menu without a Layout gets; with no text
//  renderer installed the popup measures against the null renderer and
//  takes its glyph-width fallback.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiMenuBar::ShowOpenMenu()
{
    RECT                 anchor = {};
    IDxuiTextRenderer &  text   = (m_textRendererForMeasure != nullptr) ? *m_textRendererForMeasure : m_nullText;



    if (HasTitleRect (m_openIndex))
    {
        anchor = m_titleRects[(size_t) m_openIndex];
    }

    m_dropdown.SetShowMnemonicCues (ShouldShowMnemonicCues (m_openedByKeyboard));
    m_dropdown.ShowUnder (anchor, m_items[(size_t) m_openIndex].submenu, text, m_hostClient);

    if (m_dropdown.IsVisible())
    {
        m_dropdown.HighlightFirst();
    }
    else
    {
        m_isOpen = false;
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

    m_isOpen = false;
    m_dropdown.Hide();
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
//  DxuiMenuBar::GetHighlightIndex
//
//  The highlighted row in selectable-row numbering, or -1 while closed.
//
////////////////////////////////////////////////////////////////////////////////

int DxuiMenuBar::GetHighlightIndex() const
{
    return m_isOpen ? ToRowIndex (m_dropdown.GetHighlight()) : -1;
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
//  Keyboard navigation while a menu is open. Returns true if the key was
//  consumed.
//
//      Escape / F10      dismiss
//      Left  / Shift+Tab swap to previous menu
//      Right / Tab       swap to next menu
//      Up   / Down       move highlight within the open menu
//      Enter / Space     dispatch the highlighted entry
//      A-Z               mnemonic activation within the open menu
//
//  Up and Down go to the popup menu, which owns the skip-and-wrap rule.
//  Enter stays here: a highlighted row that cannot dispatch leaves the key
//  UNCONSUMED and the menu open, exactly as before, which the popup's own
//  key handling would not report.
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiMenuBar::HandleKey (WPARAM vk)
{
    const DxuiPopupMenuItem  * entry     = nullptr;
    int                        count     = GetVisibleRowCount (m_openIndex);
    int                        menuCount = (int) m_items.size();
    int                        next      = 0;
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
        handled = m_dropdown.OnKey (vk);
    }
    else if (hasRows && (vk == VK_RETURN || vk == VK_SPACE))
    {
        // A highlighted-but-undispatchable row leaves the key unconsumed,
        // exactly as before.
        entry = GetEntryAt (m_openIndex, GetHighlightIndex());

        if (entry != nullptr && entry->command != nullptr && entry->command->IsEnabled() && entry->command->dispatch)
        {
            m_dropdown.ActivateRow (m_dropdown.GetHighlight());
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
//  Dispatches the open menu's row whose label carries mnemonic `ch`, and
//  reports whether one was found. A row that matches but is disabled or has
//  no dispatch is not a match, so the key stays unconsumed.
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiMenuBar::ActivateMnemonicRow (wchar_t ch)
{
    const std::vector<DxuiPopupMenuItem> &  rows  = m_items[(size_t) m_openIndex].submenu;
    std::wstring                            stripped;
    wchar_t                                 lower = (wchar_t) towlower (ch);
    wchar_t                                 mnCh  = 0;
    int                                     mnIdx = -1;
    int                                     hit   = -1;



    for (int i = 0; i < (int) rows.size() && hit < 0; i++)
    {
        const DxuiCommand *  cmd = rows[(size_t) i].command;

        if (rows[(size_t) i].kind == DxuiPopupMenuItem::Kind::Separator || cmd == nullptr)
        {
            continue;
        }

        ParseMnemonic (cmd->GetLabelText(), stripped, mnIdx, mnCh);

        if (mnCh != 0 && mnCh == lower && cmd->IsEnabled() && cmd->dispatch)
        {
            hit = i;
        }
    }

    if (hit >= 0)
    {
        // Highlight before dispatching: the callback can close or rebuild the
        // menu, and the original set the highlight first for that reason.
        m_dropdown.SetHighlight (hit);
        m_dropdown.ActivateRow (hit);
    }

    return hit >= 0;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiMenuBar::HandleMouseMove
//
//  Tracks hover across the title strip and the open menu, switching menus
//  on the way.
//
//  The synthetic-move guard is the important part. Windows posts a
//  WM_MOUSEMOVE at the UNCHANGED cursor position whenever the popup shows or
//  hides under the pointer -- so opening a menu from the KEYBOARD generates a
//  move at wherever the mouse happens to be resting, which would immediately
//  switch the open menu to whatever title is under it. Comparing against the
//  last position distinguishes a real move from that echo.
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
    // the popup shows or hides under the pointer. Without this guard a
    // keyboard menu switch is instantly overridden by the resting mouse's
    // title, so a synthetic repeat is not a real move.
    bool  isRealMove  = !(m_haveLastMousePos && x == m_lastMouseX && y == m_lastMouseY);



    DXUI_ASSERT_UI_THREAD();

    if (isRealMove)
    {
        m_haveLastMousePos = true;
        m_lastMouseX       = x;
        m_lastMouseY       = y;

        hitTitle     = HitTitleIndex (x, y);
        hitEntry     = m_isOpen ? m_dropdown.HitTestRow (x, y) : -1;
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
            m_dropdown.SetHighlight (hitEntry);
            handled = true;
        }
    }

    return handled;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiMenuBar::ClearHover
//
//  Drop hover state and menu highlight so the strip paints idle.
//  Called when the cursor leaves the host window. Leaves the open /
//  closed state alone -- a click-opened menu stays open while the
//  pointer wanders outside the chrome.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiMenuBar::ClearHover()
{
    DXUI_ASSERT_UI_THREAD();

    m_hoverIndex       = -1;
    m_haveLastMousePos = false;
    m_dropdown.SetHighlight (-1);
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
//  A click outside both the strip and the menu closes the menu but is
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
        // A click outside both the strip and the menu dismisses, but is
        // NOT consumed -- whatever is underneath still gets it.
        Close();
    }

    return onTitle;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiMenuBar::HandleMouseUp
//
//  A release over an enabled row picks it. No press on the same row is
//  required, which is the bar's long-standing behavior: the press that
//  opened the menu was on the title, and the release lands on the row.
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiMenuBar::HandleMouseUp (int x, int y)
{
    int                        hit     = m_isOpen ? m_dropdown.HitTestRow (x, y) : -1;
    const DxuiPopupMenuItem  * entry   = nullptr;
    bool                       handled = false;



    DXUI_ASSERT_UI_THREAD();

    if (hit >= 0)
    {
        entry = &m_items[(size_t) m_openIndex].submenu[(size_t) hit];
    }

    if (entry != nullptr && entry->command != nullptr && entry->command->IsEnabled() && entry->command->dispatch)
    {
        m_dropdown.ActivateRow (hit);
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
    float     fontDip   = GetMenuFontPx (eDpi);
    bool      showCues  = ShouldShowMnemonicCues (IsOpenByKeyboard());
    uint32_t  stripBg   = m_stripColorsSet ? m_stripBgOverride    : theme.Background();
    uint32_t  stripHov  = m_stripColorsSet ? m_stripHoverOverride : theme.HoverBackground();
    uint32_t  stripFg   = m_stripColorsSet ? m_stripTextOverride  : theme.Foreground();



    DXUI_ASSERT_UI_THREAD();

    // The popup's hosted render hook gets no theme, so it reads the one
    // installed here, every frame, which also keeps it current across a
    // theme switch. The cue flag follows Alt live, as the strip's does.
    m_dropdown.SetTheme (&theme);
    m_dropdown.SetShowMnemonicCues (showCues);

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
//  The in-window menu. A hosted popup paints itself and this is a no-op
//  for it, so a host that calls this at a chosen point in its paint order
//  gets the menu drawn last either way.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiMenuBar::PaintDropdown (
    IDxuiPainter      & painter,
    IDxuiTextRenderer & text,
    const IDxuiTheme  & theme,
    UINT                dpi)
{
    UNREFERENCED_PARAMETER (dpi);

    DXUI_ASSERT_UI_THREAD();

    if (!m_isOpen)
    {
        return;
    }

    m_dropdown.SetTheme (&theme);
    m_dropdown.SetShowMnemonicCues (ShouldShowMnemonicCues (IsOpenByKeyboard()));
    m_dropdown.Paint (painter, text);
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

    PaintStrip    (painter, text, theme, m_dpi);
    PaintDropdown (painter, text, theme, m_dpi);
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
        rect = m_titleRects[(size_t) menuIndex];
    }

    return rect;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiMenuBar::GetDropdownRect
//
//  Where the open menu is, in the owner's coordinates; empty while closed.
//
////////////////////////////////////////////////////////////////////////////////

RECT DxuiMenuBar::GetDropdownRect() const
{
    RECT  rect = {};



    if (m_isOpen && m_dropdown.IsVisible())
    {
        rect = m_dropdown.GetRect();
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
//  DxuiMenuBar::GetVisibleRowCount
//
////////////////////////////////////////////////////////////////////////////////

int DxuiMenuBar::GetVisibleRowCount (int menuIndex) const
{
    int  count = 0;



    if (HasMenu (menuIndex))
    {
        for (const DxuiPopupMenuItem & row : m_items[(size_t) menuIndex].submenu)
        {
            if (row.kind != DxuiPopupMenuItem::Kind::Separator)
            {
                count++;
            }
        }
    }

    return count;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiMenuBar::ToRowIndex
//
//  Converts an item index of the open menu, which counts separators, into
//  the selectable-row index the bar reports, which does not. -1 passes
//  through, and so does an index that lands on a separator, which the
//  popup never highlights.
//
////////////////////////////////////////////////////////////////////////////////

int DxuiMenuBar::ToRowIndex (int itemIndex) const
{
    const std::vector<DxuiPopupMenuItem> *  rows = nullptr;
    int                                     row  = 0;



    if (itemIndex < 0 || !HasMenu (m_openIndex))
    {
        return -1;
    }

    rows = &m_items[(size_t) m_openIndex].submenu;

    if (itemIndex >= (int) rows->size() || (*rows)[(size_t) itemIndex].kind == DxuiPopupMenuItem::Kind::Separator)
    {
        return -1;
    }

    for (int i = 0; i < itemIndex; i++)
    {
        if ((*rows)[(size_t) i].kind != DxuiPopupMenuItem::Kind::Separator)
        {
            row++;
        }
    }

    return row;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiMenuBar::GetEntryAt
//
//  Maps a selectable ROW index to its item, skipping separators.
//
//  This is the single place the two numbering schemes are reconciled in
//  that direction: the item list holds separators, while every index the
//  rest of the menu bar deals in -- keyboard highlight, callbacks -- counts
//  only selectable rows. Returns null for an out-of-range menu or row, so
//  callers can test the pointer instead of pre-validating bounds.
//
////////////////////////////////////////////////////////////////////////////////

const DxuiPopupMenuItem * DxuiMenuBar::GetEntryAt (int menuIndex, int rowIndex) const
{
    const DxuiPopupMenuItem *  entry = nullptr;
    int                        row   = 0;



    if (HasMenu (menuIndex) && rowIndex >= 0)
    {
        for (const DxuiPopupMenuItem & item : m_items[(size_t) menuIndex].submenu)
        {
            if (item.kind != DxuiPopupMenuItem::Kind::Separator)
            {
                // `row` only ever passes rowIndex once, so this cannot
                // overwrite an entry already found.
                if (row == rowIndex)
                {
                    entry = &item;
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
//  Menu mnemonic underlines appear when (a) the system says to underline
//  access keys at all times, (b) the user is holding Alt (the convention
//  for "show me the access keys") or (c) the menu was opened via keyboard
//  (F10 or Alt+mnemonic) -- keyboard navigation implies the user wants to
//  see the access keys. Mouse-opened menus stay clean unless Alt is also
//  pressed.
//
//  (a) is the accessibility setting, and it is checked FIRST because it is
//  the user saying the Alt-to-reveal convention does not work for them.
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiMenuBar::ShouldShowMnemonicCues (bool openedByKeyboard)
{
    if (DxuiSystemSettings::Instance().AlwaysShowKeyboardCues())
    {
        return true;
    }

    return openedByKeyboard || (GetAsyncKeyState (VK_MENU) & 0x8000) != 0;
}
