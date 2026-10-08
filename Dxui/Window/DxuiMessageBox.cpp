#include "Pch.h"

#include "Window/DxuiMessageBox.h"

#include "Window/DxuiDialogWindow.h"
#include "Core/IDxuiControl.h"
#include "Core/DxuiPanel.h"
#include "Render/IDxuiPainter.h"
#include "Render/IDxuiTextRenderer.h"
#include "Theme/IDxuiTheme.h"
#include "Theme/DxuiColor.h"




//  Windows 11's status icons, as its InfoBar draws them: a filled shape in the
//  severity's color with its mark over it in the inverse of the text color.
//  Each glyph pair is the icon font's own: the triangle and exclamation point,
//  the circle and its cross or its i.
static constexpr wchar_t   s_kGlyphCircle      = L'\uF136';
static constexpr wchar_t   s_kGlyphTriangle    = L'\uF139';
static constexpr wchar_t   s_kGlyphExclamation = L'\uF13B';
static constexpr wchar_t   s_kGlyphCross       = L'\uF13D';
static constexpr wchar_t   s_kGlyphInfoMark    = L'\uF13F';
static constexpr uint32_t  s_kArgbInfo         = 0xFF4A9EDB;
static constexpr uint32_t  s_kArgbWarningDark  = 0xFFFCE100;   // SystemFillColorCaution
static constexpr uint32_t  s_kArgbWarningLight = 0xFF9D5D00;
static constexpr uint32_t  s_kArgbErrorDark    = 0xFFFF99A4;   // SystemFillColorCritical
static constexpr uint32_t  s_kArgbErrorLight   = 0xFFC42B1C;
static constexpr wchar_t   s_kIconFont   []    = L"Segoe Fluent Icons";

enum class DxuiMessageSeverity { None, Info, Warning, Error };

static constexpr int  s_kWidthDip        = 400;
static constexpr int  s_kIconColDip      =  40;   // glyph column width
static constexpr int  s_kIconTextGapDip  =  10;
static constexpr int  s_kGlyphSizeDip    =  28;
// The glyph's LINE box (ascent + descent) is taller than its em size, so the
// content row must reserve more than s_kGlyphSizeDip or a short message centers
// the glyph in too small a cell and clips the top of the icon.
static constexpr int  s_kGlyphRowDip     =  44;
static constexpr int  s_kLineHeightDip   =  20;
static constexpr int  s_kChromeHeightDip = 124;   // caption + button row + a margin above and below the content
static constexpr int  s_kMinHeightDip    = 148;
static constexpr int  s_kMaxHeightDip    = 560;





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiMessageBoxBody -- the content control: an optional semantic glyph in a
//  left column, and the wrapped message text (theme body font) beside it.
//
////////////////////////////////////////////////////////////////////////////////

class DxuiMessageBoxBody : public DxuiPanel
{
public:
    void  Set (std::wstring text, DxuiMessageSeverity severity)
    {
        m_text     = std::move (text);
        m_severity = severity;
    }

    void  Layout (const RECT & boundsPx, const DxuiDpiScaler & scaler) override
    {
        m_bounds = boundsPx;
        m_scaler = scaler;
        SetBounds (boundsPx);
    }

    void  Paint (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme) override
    {
        HRESULT  hr    = S_OK;
        RECT     tr    = m_bounds;

        UNREFERENCED_PARAMETER (painter);

        if (m_severity != DxuiMessageSeverity::None)
        {
            bool      dark      = DxuiColor::ComputeRelativeLuminance (theme.Background()) < 0.5f;
            int       iconColPx = m_scaler.ToPx (s_kIconColDip);
            wchar_t   shape[2]  = { (m_severity == DxuiMessageSeverity::Warning) ? s_kGlyphTriangle : s_kGlyphCircle, L'\0' };
            wchar_t   mark[2]   = { (m_severity == DxuiMessageSeverity::Warning) ? s_kGlyphExclamation
                                  : (m_severity == DxuiMessageSeverity::Error)   ? s_kGlyphCross
                                                                                 : s_kGlyphInfoMark, L'\0' };
            uint32_t  fill      = (m_severity == DxuiMessageSeverity::Warning) ? (dark ? s_kArgbWarningDark : s_kArgbWarningLight)
                                : (m_severity == DxuiMessageSeverity::Error)   ? (dark ? s_kArgbErrorDark   : s_kArgbErrorLight)
                                                                               : s_kArgbInfo;
            uint32_t  ink       = dark ? 0xFF000000u : 0xFFFFFFFFu;

            //  The glyph's line is taller than a short message; a box of its own,
            //  centered on the message, keeps it from being clipped.
            float     glyphH    = m_scaler.ToPxf ((float) s_kGlyphRowDip) * 1.5f;
            float     glyphTop  = (float) (m_bounds.top + m_bounds.bottom) * 0.5f - glyphH * 0.5f;

            for (const auto & [glyph, argb] : { std::pair<const wchar_t *, uint32_t> { shape, fill }, std::pair<const wchar_t *, uint32_t> { mark, ink } })
            {
                hr = text.DrawString (glyph, (float) m_bounds.left, glyphTop, (float) iconColPx, glyphH,
                                      argb, m_scaler.ToPxf ((float) s_kGlyphSizeDip), s_kIconFont,
                                      DxuiTextHAlign::Center, DxuiTextVAlign::Center, DxuiFontWeight::Normal, false);
                IGNORE_RETURN_VALUE (hr, S_OK);
            }

            tr.left += iconColPx + m_scaler.ToPx (s_kIconTextGapDip);
        }

        {
            DxuiFontHandle  bf = theme.BodyFont();

            hr = text.DrawString (
m_text.c_str(),
(float) tr.left,
(float) tr.top,
(float) (tr.right  - tr.left),
(float) (tr.bottom - tr.top),
theme.TextColor (DxuiTextRole::Body),
m_scaler.ToPxf (bf.sizeDip),
bf.face,
DxuiTextHAlign::Left,
DxuiTextVAlign::Center,
bf.weight,
true);
            IGNORE_RETURN_VALUE (hr, S_OK);
        }
    }

private:
    std::wstring         m_text;
    DxuiMessageSeverity  m_severity = DxuiMessageSeverity::None;
    RECT                 m_bounds   = {};
    DxuiDpiScaler        m_scaler;
};





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiMessageBoxWindow -- the modal dialog: builds the body + buttons in
//  OnCreate, then the free function shows it via ShowModalDialog.
//
////////////////////////////////////////////////////////////////////////////////

class DxuiMessageBoxWindow : public DxuiDialogWindow
{
public:
    struct ButtonSpec
    {
        const wchar_t *  label = nullptr;
        int              id    = 0;
    };

    // Rough (renderer-free) wrapped-line estimate so the dialog can be sized
    // before its backend exists. Mirrors the shell's line-count heuristic.
    static int  EstimateTextHeightDip (const std::wstring & text, bool hasGlyph);

    void  Configure (std::wstring              text,
                     DxuiMessageSeverity       severity,
                     std::vector<ButtonSpec>   buttons)
    {
        m_text     = std::move (text);
        m_severity = severity;
        m_buttons  = std::move (buttons);
    }

protected:
    void  OnCreate() override
    {
        DxuiMessageBoxBody *  body = CreateDialogContent<DxuiMessageBoxBody> ();

        body->Set (m_text, m_severity);

        for (const ButtonSpec & b : m_buttons)
        {
            AddDialogButton (b.label, b.id);
        }
    }

private:
    std::wstring              m_text;
    DxuiMessageSeverity       m_severity  = DxuiMessageSeverity::None;
    std::vector<ButtonSpec>   m_buttons;
};





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiMessageBoxWindow::EstimateTextHeightDip
//
////////////////////////////////////////////////////////////////////////////////

int DxuiMessageBoxWindow::EstimateTextHeightDip (const std::wstring & text, bool hasGlyph)
{
    int     contentW    = s_kWidthDip - 40 - (hasGlyph ? (s_kIconColDip + s_kIconTextGapDip) : 0);
    int     approxCharW = 7;   // ~13dip body font average advance
    int     cpl         = (std::max) (8, contentW / approxCharW);
    int     lines       = 0;
    size_t  pos         = 0;



    for (;;)
    {
        size_t  nl  = text.find (L'\n', pos);
        size_t  end = (nl == std::wstring::npos) ? text.size() : nl;
        int     len = (int) (end - pos);

        lines += (std::max) (1, (len + cpl - 1) / cpl);

        if (nl == std::wstring::npos)
        {
            break;
        }

        pos = nl + 1;
    }

    return (std::max) (1, lines) * s_kLineHeightDip;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiMessageBox
//
//  A themed replacement for the Win32 MessageBox, matching its signature.
//
//  The SIGNATURE is deliberately identical -- same uType flags, same return
//  values -- so a call site converts by changing the name and nothing else,
//  and no caller has to learn a new vocabulary of button sets and icons.
//
//  It exists because a stock MessageBox appears as a system-styled window in
//  the middle of custom skeuomorphic chrome, ignoring the active theme and the
//  window's own DPI handling.
//
//  The flag decode mirrors Win32's own layering: the type mask picks the
//  button set, the default mask selects which of them is default by INDEX, and
//  the icon mask picks a glyph and its color. An out-of-range default index
//  falls back to the first button rather than indexing past the set, since
//  MB_DEFBUTTON4 is legal to pass with a two-button type.
//
//  Buttons are built as data and handed to a DxuiWindow, so the dialog is the
//  same machinery as every other themed window rather than a special case.
//
////////////////////////////////////////////////////////////////////////////////

int DxuiMessageBox (HWND owner, const IDxuiTheme * theme, const wchar_t * text, const wchar_t * caption, UINT uType,
                    const std::vector<const wchar_t *> & buttonLabels)
{
    using                     ButtonSpec = DxuiMessageBoxWindow::ButtonSpec;
    DxuiMessageSeverity       severity   = DxuiMessageSeverity::None;
    int                       defIndex   = 0;
    int                       defaultCmd = 0;
    int                       heightDip  = 0;
    HRESULT                   hr         = S_OK;
    int                       choice     = 0;
    DxuiMessageBoxWindow      dlg;
    DxuiWindow::CreateParams  params;
    HINSTANCE                 hInst      = nullptr;



    std::vector<ButtonSpec>   buttons;
    std::wstring              body       = (text != nullptr) ? text : L"";
    defaultCmd = IDOK;
    choice = IDOK;

    switch (uType & MB_TYPEMASK)
    {
    case MB_OKCANCEL:    buttons = { { L"OK", IDOK }, { L"Cancel", IDCANCEL } }; break;
    case MB_YESNOCANCEL: buttons = { { L"Yes", IDYES }, { L"No", IDNO }, { L"Cancel", IDCANCEL } }; break;
    case MB_YESNO:       buttons = { { L"Yes", IDYES }, { L"No", IDNO } }; break;
    case MB_RETRYCANCEL: buttons = { { L"Retry", IDRETRY }, { L"Cancel", IDCANCEL } }; break;
    case MB_OK:
    default:             buttons = { { L"OK", IDOK } }; break;
    }

    //  Buttons that say what they do, as Windows' own confirmations do, in
    //  place of the set's words; each still returns its id.
    for (size_t i = 0; i < buttonLabels.size() && i < buttons.size(); i++)
    {
        if (buttonLabels[i] != nullptr)
        {
            buttons[i].label = buttonLabels[i];
        }
    }

    defIndex = (int) ((uType & MB_DEFMASK) >> 8);
    if (defIndex < 0 || defIndex >= (int) buttons.size())
    {
        defIndex = 0;
    }

    defaultCmd = buttons[(size_t) defIndex].id;

    switch (uType & MB_ICONMASK)
    {
    case MB_ICONHAND:        severity = DxuiMessageSeverity::Error;   break;  // == ICONERROR / ICONSTOP
    case MB_ICONQUESTION:    severity = DxuiMessageSeverity::Info;    break;  // no dedicated glyph
    case MB_ICONEXCLAMATION: severity = DxuiMessageSeverity::Warning; break;  // == ICONWARNING
    case MB_ICONASTERISK:    severity = DxuiMessageSeverity::Info;    break;  // == ICONINFORMATION
    default:                 break;
    }

    heightDip = std::clamp (s_kChromeHeightDip
                                + (std::max) (DxuiMessageBoxWindow::EstimateTextHeightDip (body, severity != DxuiMessageSeverity::None),
                                              (severity != DxuiMessageSeverity::None) ? s_kGlyphRowDip : 0),
                            s_kMinHeightDip,
                            s_kMaxHeightDip);

    hInst = (owner != nullptr)
                ? reinterpret_cast<HINSTANCE> (GetWindowLongPtrW (owner, GWLP_HINSTANCE))
                : GetModuleHandleW (nullptr);

    dlg.Configure (body, severity, buttons);

    params.title                    = (caption != nullptr) ? caption : L"";
    params.hInstance                = hInst;
    params.ownerHwnd                = owner;
    params.initialSizeDip           = { s_kWidthDip, heightDip };
    params.resizable                = false;
    params.insetContentBelowCaption = true;
    params.captionStyle             = DxuiCaptionStyle::CloseOnly;

    hr = dlg.Create (params);

    if (FAILED (hr))
    {
        // The Dxui backend could not be created -- fall back to the system box
        // so the caller still gets a decision rather than a silent default.
        choice = MessageBoxW (owner, text, caption, uType);
    }
    else
    {
        dlg.SetTheme (theme);

        // Center the box on its owner (Win32 MessageBox centers on the owner too),
        // clamped to the owner's monitor work area so it never lands off-screen. The
        // window is created hidden, so this places it before ShowModalDialog shows it.
        if (owner != nullptr && dlg.GetHwnd() != nullptr)
        {
            RECT   ownerR = {};
            RECT   dlgR   = {};

            if (GetWindowRect (owner, &ownerR) && GetWindowRect (dlg.GetHwnd(), &dlgR))
            {
                int    dw = dlgR.right  - dlgR.left;
                int    dh = dlgR.bottom - dlgR.top;
                int    x  = ownerR.left + ((ownerR.right  - ownerR.left) - dw) / 2;
                int    y  = ownerR.top  + ((ownerR.bottom - ownerR.top)  - dh) / 2;

                HMONITOR     mon = MonitorFromWindow (owner, MONITOR_DEFAULTTONEAREST);
                MONITORINFO  mi  = { sizeof (mi) };

                if (mon != nullptr && GetMonitorInfoW (mon, &mi))
                {
                    x = std::clamp (x, (int) mi.rcWork.left, (int) mi.rcWork.right  - dw);
                    y = std::clamp (y, (int) mi.rcWork.top,  (int) mi.rcWork.bottom - dh);
                }

                SetWindowPos (dlg.GetHwnd(), nullptr, x, y, 0, 0,
                              SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
            }
        }

        choice = dlg.ShowModalDialog (defaultCmd);
    }

    return choice;
}
