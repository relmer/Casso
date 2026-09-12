#pragma once

#include "Pch.h"
#include "Core/IDxuiControl.h"
#include "Render/IDxuiTextRenderer.h"
#include "Widgets/DxuiPopupMenu.h"



class DxuiHwndSource;





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiMenuBar
//
//  Generic Win11-style application menu bar: a horizontal strip of top-level
//  titles (File / Edit / View / ...), each owning a list of `DxuiPopupMenuItem`
//  rows. The bar owns ONE `DxuiPopupMenu` and shows it under whichever title
//  is open, so the rows, their checks, accelerators, disabled state and
//  submenus are the popup menu's and nothing here paints a dropdown of its
//  own.
//
//  Behavior mirrors the Windows desktop convention plus modern Win11
//  hover semantics:
//      * Click a top-level title to toggle its menu open / closed.
//      * Once any menu is open, hovering an adjacent title swaps to it
//        without requiring another click.
//      * Alt+letter routes to the menu whose `&X` mnemonic matches.
//      * Left / Right swap the active menu while one is open.
//      * Up / Down move the highlight inside the open menu.
//      * Escape dismisses the menu and returns focus to the bar.
//      * Clicking outside the open menu dismisses it; clicking the same
//        title that opened it dismisses it.
//
//  Row indices the bar reports and accepts count SELECTABLE rows only, with
//  separators skipped, which is the numbering keyboard navigation and every
//  caller has always used. The popup menu counts every item; the bar
//  converts at the boundary.
//
//  The strip paints via `IDxuiPainter` + `IDxuiTextRenderer` and theme-colors
//  through `IDxuiTheme`. The host can override the strip palette via
//  `SetStripColors`; `SetDropdownColors` is forwarded to the popup menu, so a
//  host's chrome override lands in the one place every menu paints from.
//
//  Every public method asserts `DXUI_ASSERT_UI_THREAD()`.
//
//  Win32 mnemonic syntax: `&X` marks X as the menu accelerator; `&&`
//  is a literal `&`. `altLetter` on a `DxuiMenuBarItem` overrides the
//  auto-derived mnemonic when non-zero.
//
////////////////////////////////////////////////////////////////////////////////



struct DxuiMenuBarItem
{
    std::wstring                     label;
    wchar_t                          altLetter = 0;
    std::vector<DxuiPopupMenuItem>   submenu;
};



class DxuiMenuBar : public IDxuiControl
{
public:
    DxuiMenuBar  ();
    ~DxuiMenuBar () override;

    void  SetItems          (std::vector<DxuiMenuBarItem> items);
    void  SetPopupHost      (DxuiHwndSource * host);
    void  SetStripColors    (uint32_t stripArgb, uint32_t hoverArgb, uint32_t textArgb);
    void  SetDropdownColors (uint32_t bgArgb,
                             uint32_t hoverArgb,
                             uint32_t textArgb,
                             uint32_t accelArgb,
                             uint32_t borderArgb,
                             uint32_t dividerArgb);

    //  The rect an open menu is kept inside when it is painted in-window.
    //  Unbounded until a host supplies one; a hosted popup flips on its own.
    void  SetHostClientRect (const RECT & clientRect) { m_hostClient = clientRect; }

    //
    //  Install the text renderer used by the IDxuiControl::Layout
    //  override to measure menu-title strings, and by the popup menu to
    //  measure its rows. The renderer must outlive any subsequent Layout
    //  or Open call. Passing nullptr (the default) makes both fall back to
    //  a fixed-pitch estimate so unit tests can drive layout without
    //  standing up a real IDxuiTextRenderer.
    //
    void  SetTextRendererForMeasure (IDxuiTextRenderer * pText) { m_textRendererForMeasure = pText; }

    void  Layout            (int x, int y, int width, UINT dpi, IDxuiTextRenderer * pTextForMeasure = nullptr);
    void  Hide              ();
    void  Open              (int menuIndex, bool keyboardActivated);
    void  Close             ();
    void  CloseAll          ();

    int   OpenIndex         () const { return m_isOpen ? m_openIndex    : -1;       }
    int   OpenMenuIndex     () const { return m_openIndex;                          }
    bool  IsOpen            () const { return m_isOpen;                             }
    bool  IsOpenByKeyboard  () const { return m_isOpen && m_openedByKeyboard;       }
    int   GetHighlightIndex () const;
    int   GetHoverIndex     () const { return m_hoverIndex;                         }
    int   GetMenuCount      () const { return (int) m_items.size();                 }
    void  SetFocusedMenu    (int menuIndex);
    void  ClearFocus        ();
    bool  HasFocus          () const { return m_hasFocus;                           }
    int   GetFocusedMenu    () const { return m_focusedIndex;                       }

    bool  HandleAltKey      (wchar_t ch);
    bool  HandleKey         (WPARAM vk);
    bool  HandleMouseMove   (int x, int y);
    void  ClearHover        ();
    bool  HandleMouseDown   (int x, int y);
    bool  HandleMouseUp     (int x, int y);

    void  PaintStrip        (IDxuiPainter      & painter,
                             IDxuiTextRenderer & text,
                             const IDxuiTheme  & theme,
                             UINT                dpi);
    void  PaintDropdown     (IDxuiPainter      & painter,
                             IDxuiTextRenderer & text,
                             const IDxuiTheme  & theme,
                             UINT                dpi);

    // IDxuiControl overrides.
    //  The open dropdown's submenu delay needs a heartbeat the resting
    //  pointer does not provide; a host forwards both of these to it.
    bool  WantsTick () const { return m_dropdown.WantsTick(); }
    void  TickMenus (int64_t nowMs) { m_dropdown.Tick (nowMs); }

    void  Layout          (const RECT & boundsDip, const DxuiDpiScaler & scaler) override;
    void  Paint           (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme) override;
    bool  OnKey           (const DxuiKeyEvent   & ev) override;
    bool  OnMouse         (const DxuiMouseEvent & ev) override;

    // Test seam: returns the per-menu strip rect computed by the last
    // Layout. Tests do not need to drive a real text renderer.
    RECT  GetMenuRect     (int menuIndex) const;
    RECT  GetDropdownRect () const;

    // Minimum client width (physical px) that keeps every menu title on
    // the strip: the right edge of the last title after the most recent
    // Layout (the strip is anchored at the client left). Zero before the
    // first Layout. Lets the host clamp the window's minimum width so the
    // titles never clip.
    int   GetMenuStripContentWidthPx () const;

    // The strip's height in physical pixels at `dpi`, known before any
    // Layout: a host that stacks the menu bar with other chrome needs the
    // height to place them, and asking the bar afterwards would mean laying
    // it out at a position computed from a height it had not reported yet.
    static int  GetStripHeightPx (UINT dpi);

    // Public reusable helper. Parses a Win32-style label ("E&xit") into
    // a stripped string ("Exit"), the index of the mnemonic in the
    // stripped string, and its lower-cased character. "&&" collapses
    // to a literal "&" and never marks a mnemonic.
    static void  ParseMnemonic  (const std::wstring & label,
                                 std::wstring       & outStripped,
                                 int                & outIndex,
                                 wchar_t            & outLower);

private:
    static bool  IsPointInRect (const RECT & rect, int x, int y);
    static int   ScaleDpi      (int dipValue, UINT dpi);

    // Index-range predicates. The two vectors are filled at different times --
    // m_items when the menu is built, m_titleRects when it is laid out -- so a
    // valid item index is not automatically a valid rect index, and the callers
    // that read one must not bounds-check against the other.
    bool  HasMenu           (int menuIndex) const { return menuIndex >= 0 && menuIndex < (int) m_items.size(); }
    bool  HasTitleRect      (int menuIndex) const { return menuIndex >= 0 && menuIndex < (int) m_titleRects.size(); }

    bool  ActivateMnemonicRow (wchar_t ch);
    void  ShowOpenMenu        ();

    int   HitTitleIndex       (int x, int y) const;
    int   GetVisibleRowCount  (int menuIndex) const;
    int   ToRowIndex          (int itemIndex) const;
    const DxuiPopupMenuItem *  GetEntryAt (int menuIndex, int rowIndex) const;

    static bool  ShouldShowMnemonicCues (bool openedByKeyboard);

    //  The em size of the system menu font at a DPI, cached because the
    //  strip asks for it on every layout and every paint. The titles wear
    //  the same font the dropdowns under them do, which is the font Windows
    //  gives a menu.
    float  GetMenuFontPx (UINT eDpi);


    std::vector<DxuiMenuBarItem>  m_items;
    DxuiMenuMetrics               m_metrics;
    UINT                          m_metricsDpi       = 0;
    DxuiHwndSource              * m_popupHost        = nullptr;
    DxuiPopupMenu                 m_dropdown;
    DxuiNullTextRenderer          m_nullText;
    RECT                          m_hostClient       = {};
    bool                          m_haveLastMousePos = false;
    int                           m_lastMouseX       = 0;
    int                           m_lastMouseY       = 0;

    RECT                          m_stripRect        = {};
    std::vector<RECT>             m_titleRects;
    // Cached per-item text widths (device pixels) and the DPI they
    // were measured at. Menu-item text never changes on resize, only
    // the strip position, so a successful measurement is cached and
    // reused -- avoiding a per-resize re-measure that can transiently
    // return zero width and collapse item spacing.
    std::vector<int>              m_measuredItemWidthPx;
    UINT                          m_measuredAtDpi    = 0;
    int                           m_openIndex        = 0;
    int                           m_hoverIndex       = -1;
    int                           m_focusedIndex     = 0;
    bool                          m_isOpen           = false;
    bool                          m_openedByKeyboard = false;
    bool                          m_hasFocus         = false;
    UINT                          m_dpi              = 96;

    bool                          m_stripColorsSet      = false;
    uint32_t                      m_stripBgOverride     = 0;
    uint32_t                      m_stripHoverOverride  = 0;
    uint32_t                      m_stripTextOverride   = 0;

    IDxuiTextRenderer           * m_textRendererForMeasure = nullptr;
};
