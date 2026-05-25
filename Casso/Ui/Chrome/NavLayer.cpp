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
    constexpr int       s_kDropdownWidthDp = 300;
    constexpr int       s_kAccelOffsetDp   = 190;
    constexpr int       s_kRowPadLeftDp    = 10;
    constexpr int       s_kRowPadTopDp     = 5;
    constexpr float     s_kFontDip        = 14.0f;
    constexpr float     s_kUnderlineThicknessDip = 1.0f;
    constexpr wchar_t   s_kFontFamily[]   = L"Segoe UI";


    // Label syntax: `&X` marks `X` as the menu mnemonic (Win32
    // convention). The `&` is stripped before rendering; the marked
    // glyph is underlined when mnemonic cues should be shown.
    constexpr NavCommandEntry s_kEntries[] =
    {
        { IDM_FILE_OPEN,                NavMenu::File,    L"&Switch machine...",     L"Ctrl+O"        },
        { IDM_FILE_EXIT,                NavMenu::File,    L"E&xit",                  nullptr          },
        { IDM_EDIT_COPY_TEXT,           NavMenu::Edit,    L"&Copy text",             L"Ctrl+Shift+C"  },
        { IDM_EDIT_COPY_SCREENSHOT,     NavMenu::Edit,    L"Copy &screenshot",       L"Ctrl+Alt+C"    },
        { IDM_EDIT_PASTE,               NavMenu::Edit,    L"&Paste",                 L"Ctrl+V"        },
        { IDM_MACHINE_RESET,            NavMenu::Machine, L"&Reset",                 L"Ctrl+R"        },
        { IDM_MACHINE_POWERCYCLE,       NavMenu::Machine, L"Po&wer cycle",           L"Ctrl+Shift+R"  },
        { IDM_MACHINE_PAUSE,            NavMenu::Machine, L"&Pause",                 L"Pause"         },
        { IDM_MACHINE_STEP,             NavMenu::Machine, L"&Step",                  L"F11"           },
        { IDM_MACHINE_SPEED_1X,         NavMenu::Machine, L"Speed: &1x (authentic)", nullptr          },
        { IDM_MACHINE_SPEED_2X,         NavMenu::Machine, L"Speed: &2x",             nullptr          },
        { IDM_MACHINE_SPEED_MAX,        NavMenu::Machine, L"Speed: &maximum",        nullptr          },
        { IDM_MACHINE_INFO,             NavMenu::Machine, L"Machine &info...",       nullptr          },
        { IDM_DISK_INSERT1,             NavMenu::Disk,    L"&Insert drive 1...",     L"Ctrl+1"        },
        { IDM_DISK_EJECT1,              NavMenu::Disk,    L"&Eject drive 1",         L"Ctrl+Shift+1"  },
        { IDM_DISK_WRITEPROTECT1,       NavMenu::Disk,    L"&Write protect drive 1", nullptr          },
        { IDM_DISK_INSERT2,             NavMenu::Disk,    L"Insert drive &2...",     L"Ctrl+2"        },
        { IDM_DISK_EJECT2,              NavMenu::Disk,    L"Eje&ct drive 2",         L"Ctrl+Shift+2"  },
        { IDM_DISK_WRITEPROTECT2,       NavMenu::Disk,    L"Write &protect drive 2", nullptr          },
        { IDM_DISK_WRITEMODE_BUFFER,    NavMenu::Disk,    L"Write &mode: buffer and flush", nullptr   },
        { IDM_DISK_WRITEMODE_COW,       NavMenu::Disk,    L"Write m&ode: copy on write",    nullptr   },
        { IDM_VIEW_COLOR,               NavMenu::View,    L"&Color",                 nullptr          },
        { IDM_VIEW_GREEN,               NavMenu::View,    L"&Green monochrome",      nullptr          },
        { IDM_VIEW_AMBER,               NavMenu::View,    L"&Amber monochrome",      nullptr          },
        { IDM_VIEW_WHITE,               NavMenu::View,    L"&White monochrome",      nullptr          },
        { IDM_VIEW_FULLSCREEN,          NavMenu::View,    L"&Fullscreen",            L"Alt+Enter"     },
        { IDM_VIEW_RESET_SIZE,          NavMenu::View,    L"&Reset window size",     L"Ctrl+0"        },
        { IDM_VIEW_CRT_SHADER,          NavMenu::View,    L"CRT &shader",            nullptr          },
        { IDM_VIEW_SETTINGS,            NavMenu::View,    L"Se&ttings...",           L"Ctrl+,"        },
        { IDM_VIEW_DISKII_DEBUG,        NavMenu::View,    L"&Disk II debug...",      L"Ctrl+Shift+D"  },
        { IDM_HELP_KEYMAP,              NavMenu::Help,    L"&Keyboard map",          L"F1"            },
        { IDM_HELP_DEBUG,               NavMenu::Help,    L"De&bug console",         L"Ctrl+D"        },
        { IDM_HELP_ABOUT,               NavMenu::Help,    L"&About Casso...",        nullptr          },
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


    // Parse a Win32-style label ("E&xit") into a stripped string
    // ("Exit") plus the index of the mnemonic char in the stripped
    // string (1 here) and its lower-cased char ('x'). A literal "&&"
    // collapses to a single '&' and never marks a mnemonic. If no
    // mnemonic marker is present, outIndex is -1.
    void ParseMnemonic (const wchar_t * label, std::wstring & outStripped, int & outIndex, wchar_t & outLower)
    {
        outStripped.clear();
        outIndex = -1;
        outLower = 0;

        if (label == nullptr)
        {
            return;
        }

        for (const wchar_t * p = label; *p != L'\0'; p++)
        {
            if (*p == L'&')
            {
                if (*(p + 1) == L'&')
                {
                    outStripped.push_back (L'&');
                    p++;
                    continue;
                }

                if (outIndex < 0 && *(p + 1) != L'\0')
                {
                    outIndex = (int) outStripped.size();
                    outLower = (wchar_t) towlower (*(p + 1));
                }
                continue;
            }
            outStripped.push_back (*p);
        }
    }


    // True when menu mnemonic underlines should be visible.
    // Cues appear when (a) the user is holding Alt (Windows convention
    // for "show me the access keys") or (b) the menu was opened via
    // keyboard (F10 or Alt+mnemonic) — keyboard navigation implies the
    // user wants to see the access keys. Mouse-opened menus stay clean
    // unless Alt is pressed.
    bool ShouldShowMnemonicCues (bool openedByKeyboard)
    {
        if (openedByKeyboard)
        {
            return true;
        }

        return (GetAsyncKeyState (VK_MENU) & 0x8000) != 0;
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
    case NavMenu::File:    return L"&File";
    case NavMenu::Edit:    return L"&Edit";
    case NavMenu::Machine: return L"&Machine";
    case NavMenu::Disk:    return L"&Disk";
    case NavMenu::View:    return L"&View";
    case NavMenu::Help:    return L"&Help";
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
        std::wstring    menuStripped;
        std::wstring    labelStripped;
        int             mnIdx         = -1;
        wchar_t         mnCh          = 0;
        char            menuBuf[32]   = {};
        char            labelBuf[128] = {};
        char            accelBuf[64]  = {};

        ParseMnemonic (menuName, menuStripped,  mnIdx, mnCh);
        ParseMnemonic (e.label,  labelStripped, mnIdx, mnCh);

        WideCharToMultiByte (CP_UTF8, 0, menuStripped.c_str(),  -1, menuBuf,  sizeof (menuBuf),  nullptr, nullptr);
        WideCharToMultiByte (CP_UTF8, 0, labelStripped.c_str(), -1, labelBuf, sizeof (labelBuf), nullptr, nullptr);

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
    int    currentX = x;
    int    pad      = Scale (s_kMenuPadXPx, dpi);
    int    gap      = Scale (s_kMenuGapPx,  dpi);
    int    height   = Scale (s_kNavHeightPx, dpi);
    UINT   eDpi     = (dpi == 0) ? (UINT) s_kBaseDpi : dpi;
    float  fontDip  = s_kFontDip * (float) eDpi / (float) s_kBaseDpi;



    m_stripRect.left   = x;
    m_stripRect.top    = y;
    m_stripRect.right  = x + width;
    m_stripRect.bottom = y + height;
    m_rowHeightPx      = Scale (s_kRowHeightPx, dpi);
    m_dpi              = eDpi;

    for (int i = 0; i < kMenuCount; i++)
    {
        const wchar_t *  name      = GetMenuName ((NavMenu) i);
        std::wstring     stripped;
        int              mnIdx     = -1;
        wchar_t          mnCh      = 0;
        int              menuW     = 0;
        float            textWidth = 0.0f;
        float            textHt    = 0.0f;
        HRESULT          hrMeasure = E_FAIL;

        ParseMnemonic (name, stripped, mnIdx, mnCh);

        if (pTextForMeasure != nullptr)
        {
            hrMeasure = pTextForMeasure->MeasureString (stripped.c_str(), fontDip, s_kFontFamily, textWidth, textHt);
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

void NavLayer::Open (NavMenu menu, bool openedByKeyboard)
{
    m_openMenu         = menu;
    m_isOpen           = true;
    m_openedByKeyboard = openedByKeyboard;
    m_highlightIndex   = (EntryCount (menu) > 0) ? 0 : -1;
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
        std::wstring    stripped;
        int             mnIdx = -1;
        wchar_t         mnCh  = 0;

        ParseMnemonic (name, stripped, mnIdx, mnCh);

        if (mnCh != 0 && mnCh == lower)
        {
            Open ((NavMenu) i, true);
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
    int  count       = EntryCount (m_openMenu);
    int  openMenuIdx = MenuIndex (m_openMenu);



    if (!m_isOpen)
    {
        return false;
    }

    if (vk == VK_ESCAPE || vk == VK_F10)
    {
        Close();
        return true;
    }

    if (vk == VK_LEFT || (vk == VK_TAB && (GetKeyState (VK_SHIFT) & 0x8000)))
    {
        int  nextMenu = (openMenuIdx <= 0) ? (kMenuCount - 1) : (openMenuIdx - 1);
        Open ((NavMenu) nextMenu, m_openedByKeyboard);
        return true;
    }

    if (vk == VK_RIGHT || vk == VK_TAB)
    {
        int  nextMenu = (openMenuIdx + 1) % kMenuCount;
        Open ((NavMenu) nextMenu, m_openedByKeyboard);
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

    if (vk == VK_RETURN || vk == VK_SPACE)
    {
        const NavCommandEntry * entry = EntryAt (m_highlightIndex);

        if (entry != nullptr)
        {
            Dispatch (entry->commandId);
            Close();
            return true;
        }
    }

    // Mnemonic activation within an open menu: typing the marked
    // letter (no Alt) invokes that item directly (Win32 convention).
    if (vk >= 'A' && vk <= 'Z')
    {
        wchar_t  lower = (wchar_t) towlower ((wchar_t) vk);
        int      row   = 0;

        for (const NavCommandEntry & entry : s_kEntries)
        {
            std::wstring  stripped;
            int           mnIdx = -1;
            wchar_t       mnCh  = 0;

            if (entry.menu != m_openMenu)
            {
                continue;
            }

            ParseMnemonic (entry.label, stripped, mnIdx, mnCh);

            if (mnCh != 0 && mnCh == lower)
            {
                m_highlightIndex = row;
                Dispatch (entry.commandId);
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
                Open ((NavMenu) i, m_openedByKeyboard);
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
                Open ((NavMenu) i, false);
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
    const ChromeVisualState & visual,
    const ChromeTheme       & theme)
{
    HRESULT  hr           = S_OK;
    UINT     dpi          = (visual.dpi == 0) ? (UINT) s_kBaseDpi : visual.dpi;
    float    fontDip      = s_kFontDip * (float) dpi / (float) s_kBaseDpi;
    bool     showCues     = ShouldShowMnemonicCues (IsOpenByKeyboard());



    painter.FillRect ((float) m_stripRect.left,
                      (float) m_stripRect.top,
                      (float) (m_stripRect.right - m_stripRect.left),
                      (float) (m_stripRect.bottom - m_stripRect.top),
                      theme.navStripArgb);

    for (int i = 0; i < kMenuCount; i++)
    {
        const wchar_t *  name      = GetMenuName ((NavMenu) i);
        std::wstring     stripped;
        int              mnIdx     = -1;
        wchar_t          mnCh      = 0;
        float            rectW     = (float) (m_menuRects[i].right - m_menuRects[i].left);
        float            rectH     = (float) (m_menuRects[i].bottom - m_menuRects[i].top);

        ParseMnemonic (name, stripped, mnIdx, mnCh);

        if ((m_hasHoverMenu && m_hoverMenu == (NavMenu) i) || (m_isOpen && m_openMenu == (NavMenu) i))
        {
            painter.FillRect ((float) m_menuRects[i].left,
                              (float) m_menuRects[i].top,
                              rectW,
                              rectH,
                              theme.navHoverArgb);
        }

        IGNORE_RETURN_VALUE (hr, text.DrawString (stripped.c_str(),
                                                  (float) m_menuRects[i].left,
                                                  (float) m_menuRects[i].top,
                                                  rectW,
                                                  rectH,
                                                  theme.navItemTextArgb,
                                                  fontDip,
                                                  s_kFontFamily,
                                                  DwriteTextRenderer::HAlign::Center,
                                                  DwriteTextRenderer::VAlign::Center));

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

            float baseX = (float) m_menuRects[i].left + (rectW - fullW) / 2.0f + prefixW;
            float baseY = (float) m_menuRects[i].top  + (rectH + fullH) / 2.0f;

            painter.FillRect (baseX, baseY, charW, s_kUnderlineThicknessDip, theme.navItemTextArgb);
        }
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
    const ChromeVisualState & visual,
    const ChromeTheme       & theme)
{
    HRESULT  hr            = S_OK;
    RECT     rect          = DropdownRect();
    int      row           = 0;
    UINT     dpi           = (visual.dpi == 0) ? (UINT) s_kBaseDpi : visual.dpi;
    float    fontDip       = s_kFontDip * (float) dpi / (float) s_kBaseDpi;
    int      rowPadLeftPx  = Scale (s_kRowPadLeftDp,   dpi);
    int      rowPadTopPx   = Scale (s_kRowPadTopDp,    dpi);
    int      accelOffsetPx = Scale (s_kAccelOffsetDp,  dpi);
    bool     showCues      = ShouldShowMnemonicCues (IsOpenByKeyboard());



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
        std::wstring  stripped;
        int           mnIdx = -1;
        wchar_t       mnCh  = 0;

        if (entry.menu != m_openMenu)
        {
            continue;
        }

        ParseMnemonic (entry.label, stripped, mnIdx, mnCh);

        if (row == m_highlightIndex)
        {
            painter.FillRect ((float) rect.left,
                              (float) (rect.top + row * m_rowHeightPx),
                              (float) (rect.right - rect.left),
                              (float) m_rowHeightPx,
                              theme.dropdownHoverArgb);
        }

        IGNORE_RETURN_VALUE (hr, text.DrawString (stripped.c_str(),
                                                  (float) (rect.left + rowPadLeftPx),
                                                  (float) (rect.top + row * m_rowHeightPx + rowPadTopPx),
                                                  (float) accelOffsetPx,
                                                  (float) m_rowHeightPx,
                                                  theme.dropdownItemTextArgb,
                                                  fontDip,
                                                  s_kFontFamily));

        if (showCues && mnIdx >= 0 && !stripped.empty())
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
                float pcW = 0.0f;
                float ignH = 0.0f;
                IGNORE_RETURN_VALUE (hrM, text.MeasureString (prefixCh.c_str(), fontDip, s_kFontFamily, pcW, ignH));
                charW = pcW - prefixW;
            }

            float baseX = (float) (rect.left + rowPadLeftPx) + prefixW;
            float baseY = (float) (rect.top + row * m_rowHeightPx + rowPadTopPx) + fullH;

            painter.FillRect (baseX, baseY, charW, s_kUnderlineThicknessDip, theme.dropdownItemTextArgb);
        }

        if (entry.accelerator != nullptr)
        {
            IGNORE_RETURN_VALUE (hr, text.DrawString (entry.accelerator,
                                                      (float) (rect.left + accelOffsetPx),
                                                      (float) (rect.top + row * m_rowHeightPx + rowPadTopPx),
                                                      (float) (rect.right - rect.left - accelOffsetPx),
                                                      (float) m_rowHeightPx,
                                                      theme.dropdownAccelArgb,
                                                      fontDip,
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
    RECT  menu          = MenuRect (m_openMenu);
    RECT  rect          = {};
    int   dropdownWidth = Scale (s_kDropdownWidthDp, m_dpi);
    int   rows = EntryCount (m_openMenu);



    rect.left   = menu.left;
    rect.top    = menu.bottom;
    rect.right  = menu.left + dropdownWidth;
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
