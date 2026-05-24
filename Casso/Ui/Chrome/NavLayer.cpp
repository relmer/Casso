#include "Pch.h"

#include "NavLayer.h"

#include "resource.h"





namespace
{
    constexpr int       s_kBaseDpi        = 96;
    constexpr int       s_kNavHeightPx    = 32;
    constexpr int       s_kMenuPadXPx     = 12;
    constexpr int       s_kMenuGapPx      = 4;
    constexpr int       s_kRowHeightPx    = 26;
    constexpr int       s_kDropdownWidthPx = 300;
    constexpr int       s_kAccelOffsetPx  = 190;
    constexpr float     s_kFontDip        = 14.0f;
    constexpr wchar_t   s_kFontFamily[]   = L"Segoe UI";


    constexpr NavCommandEntry s_kEntries[] =
    {
        { IDM_FILE_OPEN,                NavMenu::File,    L"Switch Machine...",      L"Ctrl+O"        },
        { IDM_FILE_EXIT,                NavMenu::File,    L"Exit",                   nullptr          },
        { IDM_EDIT_COPY_TEXT,           NavMenu::Edit,    L"Copy Text",              L"Ctrl+Shift+C"  },
        { IDM_EDIT_COPY_SCREENSHOT,     NavMenu::Edit,    L"Copy Screenshot",        L"Ctrl+Alt+C"    },
        { IDM_EDIT_PASTE,               NavMenu::Edit,    L"Paste",                  L"Ctrl+V"        },
        { IDM_MACHINE_RESET,            NavMenu::Machine, L"Reset",                  L"Ctrl+R"        },
        { IDM_MACHINE_POWERCYCLE,       NavMenu::Machine, L"Power Cycle",            L"Ctrl+Shift+R"  },
        { IDM_MACHINE_PAUSE,            NavMenu::Machine, L"Pause",                  L"Pause"         },
        { IDM_MACHINE_STEP,             NavMenu::Machine, L"Step",                   L"F11"           },
        { IDM_MACHINE_SPEED_1X,         NavMenu::Machine, L"Speed: 1x (Authentic)",  nullptr          },
        { IDM_MACHINE_SPEED_2X,         NavMenu::Machine, L"Speed: 2x",              nullptr          },
        { IDM_MACHINE_SPEED_MAX,        NavMenu::Machine, L"Speed: Maximum",         nullptr          },
        { IDM_MACHINE_INFO,             NavMenu::Machine, L"Machine Info...",        nullptr          },
        { IDM_DISK_INSERT1,             NavMenu::Disk,    L"Insert Drive 1...",      L"Ctrl+1"        },
        { IDM_DISK_EJECT1,              NavMenu::Disk,    L"Eject Drive 1",          L"Ctrl+Shift+1"  },
        { IDM_DISK_WRITEPROTECT1,       NavMenu::Disk,    L"Write Protect Drive 1",  nullptr          },
        { IDM_DISK_INSERT2,             NavMenu::Disk,    L"Insert Drive 2...",      L"Ctrl+2"        },
        { IDM_DISK_EJECT2,              NavMenu::Disk,    L"Eject Drive 2",          L"Ctrl+Shift+2"  },
        { IDM_DISK_WRITEPROTECT2,       NavMenu::Disk,    L"Write Protect Drive 2",  nullptr          },
        { IDM_DISK_WRITEMODE_BUFFER,    NavMenu::Disk,    L"Write Mode: Buffer and Flush", nullptr    },
        { IDM_DISK_WRITEMODE_COW,       NavMenu::Disk,    L"Write Mode: Copy on Write",    nullptr    },
        { IDM_VIEW_COLOR,               NavMenu::View,    L"Color (NTSC)",           nullptr          },
        { IDM_VIEW_GREEN,               NavMenu::View,    L"Green Monochrome",       nullptr          },
        { IDM_VIEW_AMBER,               NavMenu::View,    L"Amber Monochrome",       nullptr          },
        { IDM_VIEW_WHITE,               NavMenu::View,    L"White Monochrome",       nullptr          },
        { IDM_VIEW_FULLSCREEN,          NavMenu::View,    L"Fullscreen",             L"Alt+Enter"     },
        { IDM_VIEW_RESET_SIZE,          NavMenu::View,    L"Reset Window Size",      L"Ctrl+0"        },
        { IDM_VIEW_CRT_SHADER,          NavMenu::View,    L"CRT Shader",             nullptr          },
        { IDM_VIEW_SETTINGS,            NavMenu::View,    L"Settings...",            L"Ctrl+,"        },
        { IDM_VIEW_DISKII_DEBUG,        NavMenu::View,    L"Disk II Debug...",       L"Ctrl+Shift+D"  },
        { IDM_HELP_KEYMAP,              NavMenu::Help,    L"Keyboard Map",           L"F1"            },
        { IDM_HELP_DEBUG,               NavMenu::Help,    L"Debug Console",          L"Ctrl+D"        },
        { IDM_HELP_ABOUT,               NavMenu::Help,    L"About Casso...",         nullptr          },
    };


    bool RectContains (const RECT & rect, int x, int y)
    {
        return x >= rect.left && x < rect.right && y >= rect.top && y < rect.bottom;
    }


    int Scale (int value, UINT dpi)
    {
        UINT  effectiveDpi = (dpi == 0) ? s_kBaseDpi : dpi;



        return MulDiv (value, (int) effectiveDpi, s_kBaseDpi);
    }
}




////////////////////////////////////////////////////////////////////////////////
//
//  NavLayer
//
////////////////////////////////////////////////////////////////////////////////

NavLayer::NavLayer ()
{
}




////////////////////////////////////////////////////////////////////////////////
//
//  ~NavLayer
//
////////////////////////////////////////////////////////////////////////////////

NavLayer::~NavLayer ()
{
    Hide();
}




////////////////////////////////////////////////////////////////////////////////
//
//  GetCommandEntries
//
////////////////////////////////////////////////////////////////////////////////

std::span<const NavCommandEntry> NavLayer::GetCommandEntries ()
{
    return std::span<const NavCommandEntry> (s_kEntries, std::size (s_kEntries));
}




////////////////////////////////////////////////////////////////////////////////
//
//  GetMenuName
//
////////////////////////////////////////////////////////////////////////////////

const wchar_t * NavLayer::GetMenuName (NavMenu menu)
{
    switch (menu)
    {
    case NavMenu::File:    return L"File";
    case NavMenu::Edit:    return L"Edit";
    case NavMenu::Machine: return L"Machine";
    case NavMenu::Disk:    return L"Disk";
    case NavMenu::View:    return L"View";
    case NavMenu::Help:    return L"Help";
    }

    return L"?";
}




////////////////////////////////////////////////////////////////////////////////
//
//  EmitParityMarkdown
//
////////////////////////////////////////////////////////////////////////////////

std::string NavLayer::EmitParityMarkdown ()
{
    std::ostringstream  os;



    os << "# Menu Command Parity\n\n";
    os << "Auto-generated by `NavLayer::EmitParityMarkdown` from the `s_kEntries` table. Do not edit by hand.\n\n";
    os << "| ID | Menu | Label | Accelerator |\n";
    os << "|----|------|-------|-------------|\n";

    for (const NavCommandEntry & e : s_kEntries)
    {
        char            buf[256]      = {};
        const wchar_t * menuName      = GetMenuName (e.menu);
        char            menuBuf[32]   = {};
        char            labelBuf[128] = {};
        char            accelBuf[64]  = {};

        WideCharToMultiByte (CP_UTF8, 0, menuName, -1, menuBuf, sizeof (menuBuf), nullptr, nullptr);
        WideCharToMultiByte (CP_UTF8, 0, e.label, -1, labelBuf, sizeof (labelBuf), nullptr, nullptr);

        if (e.accelerator != nullptr)
        {
            WideCharToMultiByte (CP_UTF8, 0, e.accelerator, -1, accelBuf, sizeof (accelBuf), nullptr, nullptr);
        }

        snprintf (buf, sizeof (buf), "| %u | %s | %s | %s |\n", (unsigned) e.commandId, menuBuf, labelBuf, accelBuf);
        os << buf;
    }

    return os.str();
}




////////////////////////////////////////////////////////////////////////////////
//
//  Show
//
////////////////////////////////////////////////////////////////////////////////

void NavLayer::Show ()
{
}




////////////////////////////////////////////////////////////////////////////////
//
//  Hide
//
////////////////////////////////////////////////////////////////////////////////

void NavLayer::Hide ()
{
    Close();
}




////////////////////////////////////////////////////////////////////////////////
//
//  Dispatch
//
////////////////////////////////////////////////////////////////////////////////

void NavLayer::Dispatch (WORD commandId) const
{
    if (m_dispatch)
    {
        m_dispatch (commandId);
    }
}




////////////////////////////////////////////////////////////////////////////////
//
//  Layout
//
////////////////////////////////////////////////////////////////////////////////

void NavLayer::Layout (int x, int y, int width, UINT dpi, DwriteTextRenderer * pTextForMeasure)
{
    int  currentX = x;
    int  pad      = Scale (s_kMenuPadXPx, dpi);
    int  gap      = Scale (s_kMenuGapPx,  dpi);
    int  height   = Scale (s_kNavHeightPx, dpi);



    m_stripRect.left   = x;
    m_stripRect.top    = y;
    m_stripRect.right  = x + width;
    m_stripRect.bottom = y + height;
    m_rowHeightPx      = Scale (s_kRowHeightPx, dpi);

    for (int i = 0; i < kMenuCount; i++)
    {
        const wchar_t *  name      = GetMenuName ((NavMenu) i);
        int              menuW     = 0;
        float            textWidth = 0.0f;
        float            textHt    = 0.0f;
        HRESULT          hrMeasure = E_FAIL;

        if (pTextForMeasure != nullptr)
        {
            hrMeasure = pTextForMeasure->MeasureString (name, s_kFontDip, s_kFontFamily, textWidth, textHt);
        }

        if (SUCCEEDED (hrMeasure) && textWidth > 0.0f)
        {
            // ceil to avoid sub-pixel truncation. DPI scaling is
            // already baked into the D2D context's per-device DPI
            // so we draw in device-independent pixels here.
            menuW = (int) (textWidth + 0.5f) + pad * 2;
        }
        else
        {
            // Fallback when no measurer is available (called before
            // BeginDraw at startup). 8 px per char is a rough Segoe UI
            // baseline; the next Layout pass with a live measurer
            // tightens it.
            menuW = ((int) wcslen (name) * 8) + pad * 2;
        }

        m_menuRects[i].left   = currentX;
        m_menuRects[i].top    = y;
        m_menuRects[i].right  = currentX + menuW;
        m_menuRects[i].bottom = y + height;
        currentX += menuW + gap;
    }
}




////////////////////////////////////////////////////////////////////////////////
//
//  Open
//
////////////////////////////////////////////////////////////////////////////////

void NavLayer::Open (NavMenu menu)
{
    m_openMenu       = menu;
    m_isOpen         = true;
    m_highlightIndex = (EntryCount (menu) > 0) ? 0 : -1;
}




////////////////////////////////////////////////////////////////////////////////
//
//  Close
//
////////////////////////////////////////////////////////////////////////////////

void NavLayer::Close ()
{
    m_isOpen         = false;
    m_highlightIndex = -1;
}




////////////////////////////////////////////////////////////////////////////////
//
//  HandleAltKey
//
////////////////////////////////////////////////////////////////////////////////

bool NavLayer::HandleAltKey (wchar_t ch)
{
    wchar_t  lower = (wchar_t) towlower (ch);



    for (int i = 0; i < kMenuCount; i++)
    {
        const wchar_t * name = GetMenuName ((NavMenu) i);

        if ((wchar_t) towlower (name[0]) == lower)
        {
            Open ((NavMenu) i);
            return true;
        }
    }

    return false;
}




////////////////////////////////////////////////////////////////////////////////
//
//  HandleKey
//
////////////////////////////////////////////////////////////////////////////////

bool NavLayer::HandleKey (WPARAM vk)
{
    int  count = EntryCount (m_openMenu);



    if (!m_isOpen)
    {
        return false;
    }

    if (vk == VK_ESCAPE)
    {
        Close();
        return true;
    }

    if (count <= 0)
    {
        return false;
    }

    if (vk == VK_DOWN)
    {
        m_highlightIndex = (m_highlightIndex + 1) % count;
        return true;
    }

    if (vk == VK_UP)
    {
        m_highlightIndex = (m_highlightIndex + count - 1) % count;
        return true;
    }

    if (vk == VK_RETURN)
    {
        const NavCommandEntry * entry = EntryAt (m_highlightIndex);

        if (entry != nullptr)
        {
            Dispatch (entry->commandId);
            Close();
            return true;
        }
    }

    return false;
}




////////////////////////////////////////////////////////////////////////////////
//
//  HandleMouseMove
//
////////////////////////////////////////////////////////////////////////////////

bool NavLayer::HandleMouseMove (int x, int y)
{
    int  hitEntry = HitEntryIndex (x, y);



    m_hasHoverMenu = false;

    for (int i = 0; i < kMenuCount; i++)
    {
        if (RectContains (m_menuRects[i], x, y))
        {
            m_hoverMenu    = (NavMenu) i;
            m_hasHoverMenu = true;
            if (m_isOpen)
            {
                Open ((NavMenu) i);
            }
            return true;
        }
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
//  HandleMouseDown
//
////////////////////////////////////////////////////////////////////////////////

bool NavLayer::HandleMouseDown (int x, int y)
{
    for (int i = 0; i < kMenuCount; i++)
    {
        if (RectContains (m_menuRects[i], x, y))
        {
            if (m_isOpen && m_openMenu == (NavMenu) i)
            {
                Close();
            }
            else
            {
                Open ((NavMenu) i);
            }
            return true;
        }
    }

    if (m_isOpen && !RectContains (DropdownRect(), x, y))
    {
        Close();
    }

    return false;
}




////////////////////////////////////////////////////////////////////////////////
//
//  HandleMouseUp
//
////////////////////////////////////////////////////////////////////////////////

bool NavLayer::HandleMouseUp (int x, int y)
{
    int                    hitEntry = HitEntryIndex (x, y);
    const NavCommandEntry * entry    = nullptr;



    if (hitEntry >= 0)
    {
        entry = EntryAt (hitEntry);
        if (entry != nullptr)
        {
            Dispatch (entry->commandId);
            Close();
            return true;
        }
    }

    return false;
}




////////////////////////////////////////////////////////////////////////////////
//
//  PaintStrip
//
////////////////////////////////////////////////////////////////////////////////

void NavLayer::PaintStrip (
    DxUiPainter             & painter,
    DwriteTextRenderer      & text,
    const ChromeVisualState & /*visual*/,
    const ChromeTheme       & theme)
{
    HRESULT  hr = S_OK;



    painter.FillRect ((float) m_stripRect.left,
                      (float) m_stripRect.top,
                      (float) (m_stripRect.right - m_stripRect.left),
                      (float) (m_stripRect.bottom - m_stripRect.top),
                      theme.navStripArgb);

    for (int i = 0; i < kMenuCount; i++)
    {
        if ((m_hasHoverMenu && m_hoverMenu == (NavMenu) i) || (m_isOpen && m_openMenu == (NavMenu) i))
        {
            painter.FillRect ((float) m_menuRects[i].left,
                              (float) m_menuRects[i].top,
                              (float) (m_menuRects[i].right - m_menuRects[i].left),
                              (float) (m_menuRects[i].bottom - m_menuRects[i].top),
                              theme.navHoverArgb);
        }

        IGNORE_RETURN_VALUE (hr, text.DrawString (GetMenuName ((NavMenu) i),
                                                  (float) m_menuRects[i].left,
                                                  (float) m_menuRects[i].top,
                                                  (float) (m_menuRects[i].right - m_menuRects[i].left),
                                                  (float) (m_menuRects[i].bottom - m_menuRects[i].top),
                                                  theme.navItemTextArgb,
                                                  s_kFontDip,
                                                  s_kFontFamily,
                                                  DwriteTextRenderer::HAlign::Center,
                                                  DwriteTextRenderer::VAlign::Center));
    }
}




////////////////////////////////////////////////////////////////////////////////
//
//  PaintDropdown
//
////////////////////////////////////////////////////////////////////////////////

void NavLayer::PaintDropdown (
    DxUiPainter             & painter,
    DwriteTextRenderer      & text,
    const ChromeVisualState & /*visual*/,
    const ChromeTheme       & theme)
{
    HRESULT  hr   = S_OK;
    RECT     rect = DropdownRect();
    int      row  = 0;



    if (!m_isOpen)
    {
        return;
    }

    painter.FillRect ((float) rect.left,
                      (float) rect.top,
                      (float) (rect.right - rect.left),
                      (float) (rect.bottom - rect.top),
                      theme.dropdownBgArgb);
    painter.OutlineRect ((float) rect.left,
                         (float) rect.top,
                         (float) (rect.right - rect.left),
                         (float) (rect.bottom - rect.top),
                         1.0f,
                         theme.navHoverArgb);

    for (const NavCommandEntry & entry : s_kEntries)
    {
        if (entry.menu != m_openMenu)
        {
            continue;
        }

        if (row == m_highlightIndex)
        {
            painter.FillRect ((float) rect.left,
                              (float) (rect.top + row * m_rowHeightPx),
                              (float) (rect.right - rect.left),
                              (float) m_rowHeightPx,
                              theme.dropdownHoverArgb);
        }

        IGNORE_RETURN_VALUE (hr, text.DrawString (entry.label,
                                                  (float) (rect.left + 10),
                                                  (float) (rect.top + row * m_rowHeightPx + 5),
                                                  (float) s_kAccelOffsetPx,
                                                  (float) m_rowHeightPx,
                                                  theme.dropdownItemTextArgb,
                                                  s_kFontDip,
                                                  s_kFontFamily));
        if (entry.accelerator != nullptr)
        {
            IGNORE_RETURN_VALUE (hr, text.DrawString (entry.accelerator,
                                                      (float) (rect.left + s_kAccelOffsetPx),
                                                      (float) (rect.top + row * m_rowHeightPx + 5),
                                                      (float) (rect.right - rect.left - s_kAccelOffsetPx),
                                                      (float) m_rowHeightPx,
                                                      theme.dropdownAccelArgb,
                                                      s_kFontDip,
                                                      s_kFontFamily));
        }
        row++;
    }
}




////////////////////////////////////////////////////////////////////////////////
//
//  Helpers
//
////////////////////////////////////////////////////////////////////////////////

int NavLayer::MenuIndex (NavMenu menu) const
{
    return (int) menu;
}


RECT NavLayer::MenuRect (NavMenu menu) const
{
    return m_menuRects[MenuIndex (menu)];
}


RECT NavLayer::DropdownRect () const
{
    RECT  menu = MenuRect (m_openMenu);
    RECT  rect = {};
    int   rows = EntryCount (m_openMenu);



    rect.left   = menu.left;
    rect.top    = menu.bottom;
    rect.right  = menu.left + s_kDropdownWidthPx;
    rect.bottom = menu.bottom + rows * m_rowHeightPx;
    return rect;
}


const NavCommandEntry * NavLayer::EntryAt (int index) const
{
    int  row = 0;



    for (const NavCommandEntry & entry : s_kEntries)
    {
        if (entry.menu != m_openMenu)
        {
            continue;
        }

        if (row == index)
        {
            return &entry;
        }
        row++;
    }

    return nullptr;
}


int NavLayer::EntryCount (NavMenu menu) const
{
    int  count = 0;



    for (const NavCommandEntry & entry : s_kEntries)
    {
        if (entry.menu == menu)
        {
            count++;
        }
    }

    return count;
}


int NavLayer::HitEntryIndex (int x, int y) const
{
    RECT  rect = DropdownRect();
    int   row  = 0;



    if (!m_isOpen || !RectContains (rect, x, y))
    {
        return -1;
    }

    row = (y - rect.top) / m_rowHeightPx;
    if (row >= EntryCount (m_openMenu))
    {
        return -1;
    }

    return row;
}
