#pragma once

#include "Pch.h"
#include "Core/IDxuiControl.h"



class DxuiHostWindow;



////////////////////////////////////////////////////////////////////////////////
//
//  DxuiMenuBar
//
//  Generic Win11-style application menu bar. A horizontal strip of
//  top-level menu titles (File / Edit / View / ...) each owning a
//  vertical submenu of clickable items. Items support labels,
//  optional accelerator hint text, dispatch callbacks, check-state
//  query callbacks, enabled flags, checkable flags, and separator
//  rows.
//
//  Behaviour mirrors the Windows desktop convention plus modern Win11
//  hover semantics:
//      * Click a top-level title to toggle its submenu open / closed.
//      * Once any submenu is open, hovering an adjacent title swaps
//        to it without requiring another click.
//      * Alt+letter routes to the menu whose `&X` mnemonic matches.
//      * Left / Right swap the active submenu while one is open.
//      * Up / Down move the highlight inside the open submenu.
//      * Escape dismisses the submenu and returns focus to the bar.
//      * Clicking outside the open submenu dismisses it; clicking the
//        same title that opened it dismisses it.
//
//  The widget paints via `IDxuiPainter` + `IDxuiTextRenderer` and
//  theme-colours through `IDxuiTheme`. The host application can
//  override the strip / dropdown palette via `SetStripColors` /
//  `SetDropdownColors` when the default `IDxuiTheme` mapping is too
//  generic (Casso's chrome supplies legacy nav-specific colours so
//  the menu surface keeps visual parity with the rest of the shell).
//
//  Derives from `IDxuiControl` so it slots into `DxuiPanel` trees.
//  `Paint()` paints both the strip and (if open) the submenu so
//  consumers that hand the bar to a panel get the whole thing for
//  free; chrome consumers that need fine z-order control use the
//  explicit `PaintStrip` / `PaintDropdown` helpers and call them at
//  different points in their composite render order.
//
//  Every public method asserts `DXUI_ASSERT_UI_THREAD()`.
//
//  Win32 mnemonic syntax: `&X` marks X as the menu accelerator; `&&`
//  is a literal `&`. `altLetter` on a `DxuiMenuBarItem` overrides the
//  auto-derived mnemonic when non-zero.
//
////////////////////////////////////////////////////////////////////////////////



struct DxuiMenuBarSubitem
{
    std::wstring             label;
    std::wstring             hotkey;
    std::function<void()>    dispatch;
    std::function<bool()>    isChecked;
    bool                     enabled     = true;
    bool                     checkable   = false;
    bool                     isSeparator = false;
};


struct DxuiMenuBarItem
{
    std::wstring                     label;
    wchar_t                          altLetter = 0;
    std::vector<DxuiMenuBarSubitem>  submenu;
};



class DxuiMenuBar : public IDxuiControl
{
public:
    DxuiMenuBar  ();
    ~DxuiMenuBar () override;

    void  SetItems          (std::vector<DxuiMenuBarItem> items);
    void  SetPopupHost      (DxuiHostWindow * host);
    void  SetStripColors    (uint32_t stripArgb, uint32_t hoverArgb, uint32_t textArgb);
    void  SetDropdownColors (uint32_t bgArgb,
                             uint32_t hoverArgb,
                             uint32_t textArgb,
                             uint32_t accelArgb,
                             uint32_t borderArgb,
                             uint32_t dividerArgb);

    void  Layout            (int x, int y, int width, UINT dpi, IDxuiTextRenderer * pTextForMeasure = nullptr);
    void  Hide              ();
    void  Open              (int menuIndex, bool keyboardActivated);
    void  Close             ();
    void  CloseAll          ();

    int   OpenIndex         () const { return m_isOpen ? m_openIndex    : -1;       }
    int   OpenMenuIndex     () const { return m_openIndex;                          }
    bool  IsOpen            () const { return m_isOpen;                             }
    bool  IsOpenByKeyboard  () const { return m_isOpen && m_openedByKeyboard;       }
    int   HighlightIndex    () const { return m_highlightIndex;                     }
    int   MenuCount         () const { return (int) m_items.size();                 }
    void  SetFocusedMenu    (int menuIndex);
    void  ClearFocus        ();
    bool  HasFocus          () const { return m_hasFocus;                           }
    int   FocusedMenu       () const { return m_focusedIndex;                       }

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
    void  Layout            (const RECT & boundsDip, const DxuiDpiScaler & scaler) override;
    void  Paint             (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme) override;
    bool  OnKey             (const DxuiKeyEvent   & ev) override;
    bool  OnMouse           (const DxuiMouseEvent & ev) override;

    // Test seam: returns the per-menu strip rect computed by the last
    // Layout. Tests do not need to drive a real text renderer.
    RECT  MenuRect          (int menuIndex) const;
    RECT  DropdownRect      () const;

private:
    int   HitTitleIndex     (int x, int y) const;
    int   HitEntryIndex     (int x, int y) const;
    int   EntryHeightPx     (const DxuiMenuBarSubitem & sub) const;
    int   DropdownHeightPx  (int menuIndex) const;
    int   NextEnabledRow    (int menuIndex, int startRow, int direction) const;
    int   FirstEnabledRow   (int menuIndex) const;
    int   VisibleRowCount   (int menuIndex) const;
    const DxuiMenuBarSubitem *  EntryAt  (int menuIndex, int rowIndex) const;

    static void  ParseMnemonic  (const std::wstring & label,
                                 std::wstring       & outStripped,
                                 int                & outIndex,
                                 wchar_t            & outLower);
    static bool  ShouldShowMnemonicCues (bool openedByKeyboard);


    std::vector<DxuiMenuBarItem>  m_items;
    DxuiHostWindow              * m_popupHost        = nullptr;

    RECT                          m_stripRect        = {};
    std::vector<RECT>             m_titleRects;
    int                           m_openIndex        = 0;
    int                           m_hoverIndex       = -1;
    int                           m_focusedIndex     = 0;
    bool                          m_isOpen           = false;
    bool                          m_openedByKeyboard = false;
    bool                          m_hasFocus         = false;
    int                           m_highlightIndex   = -1;
    int                           m_rowHeightPx      = 26;
    UINT                          m_dpi              = 96;

    bool                          m_stripColorsSet      = false;
    uint32_t                      m_stripBgOverride     = 0;
    uint32_t                      m_stripHoverOverride  = 0;
    uint32_t                      m_stripTextOverride   = 0;

    bool                          m_dropdownColorsSet   = false;
    uint32_t                      m_dropBgOverride      = 0;
    uint32_t                      m_dropHoverOverride   = 0;
    uint32_t                      m_dropTextOverride    = 0;
    uint32_t                      m_dropAccelOverride   = 0;
    uint32_t                      m_dropBorderOverride  = 0;
    uint32_t                      m_dropDividerOverride = 0;
};
