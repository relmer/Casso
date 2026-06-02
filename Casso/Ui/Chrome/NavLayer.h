#pragma once

#include "Pch.h"

#include "ChromeTheme.h"
#include "../DwriteTextRenderer.h"
#include "../DxUiPainter.h"





enum class NavMenu
{
    File    = 0,
    Edit    = 1,
    Machine = 2,
    Disk    = 3,
    View    = 4,
    Debug   = 5,
    Help    = 6,
};


struct NavCommandEntry
{
    WORD            commandId;
    NavMenu         menu;
    const wchar_t * label;
    const wchar_t * accelerator;
    bool            checkable = false;
};


class NavLayer
{
public:
    using DispatchFn = std::function<void (WORD commandId)>;
    using CheckFn    = std::function<bool (WORD commandId)>;

    NavLayer  ();
    ~NavLayer ();

    void                              Show                 ();
    void                              Hide                 ();
    void                              SetDispatch          (DispatchFn dispatch) { m_dispatch = std::move (dispatch); }
    void                              SetCheckQuery        (CheckFn query)       { m_isChecked = std::move (query); }

    static std::span<const NavCommandEntry> GetCommandEntries ();
    static const wchar_t                  * GetMenuName       (NavMenu menu);
    static std::string                      EmitParityMarkdown ();
    static bool                             IsSeparator        (const NavCommandEntry & entry);

    void                              Dispatch             (WORD commandId) const;
    void                              Layout               (int x, int y, int width, UINT dpi,
                                                            DwriteTextRenderer * pTextForMeasure = nullptr);
    void                              PaintStrip           (DxUiPainter             & painter,
                                                            DwriteTextRenderer      & text,
                                                            const ChromeVisualState & visual,
                                                            const ChromeTheme       & theme);
    void                              PaintDropdown        (DxUiPainter             & painter,
                                                            DwriteTextRenderer      & text,
                                                            const ChromeVisualState & visual,
                                                            const ChromeTheme       & theme);
    void                              Open                 (NavMenu menu, bool openedByKeyboard);
    void                              Close                ();
    bool                              IsOpen               () const { return m_isOpen; }
    bool                              IsOpenByKeyboard     () const { return m_isOpen && m_openedByKeyboard; }
    NavMenu                           OpenMenu             () const { return m_openMenu; }
    bool                              HandleAltKey         (wchar_t ch);
    bool                              HandleKey            (WPARAM vk);
    bool                              HandleMouseMove      (int x, int y);
    void                              ClearHover           ();
    bool                              HandleMouseDown      (int x, int y);
    bool                              HandleMouseUp        (int x, int y);
    int                               HighlightIndex       () const { return m_highlightIndex; }

private:
    static constexpr int              kMenuCount = 7;

    int                               MenuIndex            (NavMenu menu) const;
    RECT                              MenuRect             (NavMenu menu) const;
    RECT                              DropdownRect         () const;
    const NavCommandEntry           * EntryAt              (int index) const;
    int                               EntryCount           (NavMenu menu) const;
    int                               DropdownHeight       (NavMenu menu) const;
    int                               EntryHeight          (const NavCommandEntry & entry) const;
    int                               HitEntryIndex        (int x, int y) const;

    DispatchFn                        m_dispatch;
    CheckFn                           m_isChecked;
    RECT                              m_stripRect        = {};
    std::array<RECT, kMenuCount>      m_menuRects        = {};
    NavMenu                           m_openMenu         = NavMenu::File;
    NavMenu                           m_hoverMenu        = NavMenu::File;
    bool                              m_isOpen           = false;
    bool                              m_openedByKeyboard = false;
    bool                              m_hasHoverMenu     = false;
    int                               m_highlightIndex   = -1;
    int                               m_rowHeightPx      = 26;
    UINT                              m_dpi              = 96;
};
