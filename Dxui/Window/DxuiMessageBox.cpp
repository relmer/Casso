#include "Pch.h"

#include "Window/DxuiMessageBox.h"

#include "Window/DxuiDialogWindow.h"
#include "Core/IDxuiControl.h"
#include "Core/DxuiPanel.h"
#include "Widgets/DxuiCheckbox.h"
#include "Render/IDxuiPainter.h"
#include "Render/IDxuiTextRenderer.h"
#include "Theme/IDxuiTheme.h"




// Icon glyphs (Segoe MDL2 Assets) + accent colors -- same set the shell's
// task-dialog path uses, so a Dxui message box reads identically.
static constexpr wchar_t   s_kGlyphInfo     = L'\uE946';   // MDL2 Info
static constexpr wchar_t   s_kGlyphWarning  = L'\uE7BA';   // MDL2 Warning
static constexpr wchar_t   s_kGlyphError    = L'\uEA39';   // MDL2 ErrorBadge
static constexpr uint32_t  s_kArgbInfo      = 0xFF4A9EDB;
static constexpr uint32_t  s_kArgbWarning   = 0xFFF5A623;
static constexpr uint32_t  s_kArgbError     = 0xFFE5424D;
static constexpr wchar_t   s_kMdl2Font   [] = L"Segoe MDL2 Assets";

static constexpr int  s_kWidthDip        = 400;
static constexpr int  s_kIconColDip      =  40;   // glyph column width
static constexpr int  s_kIconTextGapDip  =  10;
static constexpr int  s_kGlyphSizeDip    =  28;
// The glyph's LINE box (ascent + descent) is taller than its em size, so the
// content row must reserve more than s_kGlyphSizeDip or a short message centers
// the glyph in too small a cell and clips the top of the icon.
static constexpr int  s_kGlyphRowDip     =  44;
static constexpr int  s_kLineHeightDip   =  20;
static constexpr int  s_kChromeHeightDip = 104;   // caption + button row + content vpad
static constexpr int  s_kMinHeightDip    = 128;
static constexpr int  s_kMaxHeightDip    = 560;
static constexpr int  s_kCheckRowDip     =  32;   // a check box under the text, with room above it





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiMessageBoxBody -- the content control: an optional semantic glyph in a
//  left column, and the wrapped message text (theme body font) beside it.
//  It measures that text, so the dialog's height fits it.
//
////////////////////////////////////////////////////////////////////////////////

class DxuiMessageBoxBody : public DxuiPanel, public IDxuiMeasuredContent
{
public:
    void  Set (std::wstring text, wchar_t glyph, uint32_t glyphArgb)
    {
        m_text      = std::move (text);
        m_glyph     = glyph;
        m_glyphArgb = glyphArgb;
    }

    //  A check box under the text, such as a notice's box that turns it off.
    void  SetCheck (const std::wstring & label, bool isChecked, DxuiCheckbox::ChangeFn onChange)
    {
        m_check = CreateChild<DxuiCheckbox>();
        m_check->SetLabel    (label);
        m_check->SetChecked  (isChecked);
        m_check->SetOnChange (std::move (onChange));
    }

    void  Layout (const RECT & boundsPx, const DxuiDpiScaler & scaler) override
    {
        RECT  check = boundsPx;



        m_bounds = boundsPx;
        m_scaler = scaler;
        SetBounds (boundsPx);

        if (m_check != nullptr)
        {
            m_bounds.bottom -= scaler.ToPx (s_kCheckRowDip);
            check.top        = m_bounds.bottom;
            m_check->SetDpi (scaler.GetDpi());
            m_check->Layout (check, scaler);
        }
    }

    void  Paint (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme) override
    {
        HRESULT  hr    = S_OK;
        RECT     tr    = m_bounds;

        UNREFERENCED_PARAMETER (painter);

        if (m_glyph != 0)
        {
            wchar_t  g[2]      = { m_glyph, L'\0' };
            int      iconColPx = m_scaler.ToPx (s_kIconColDip);

            hr = text.DrawString (
g,
(float) m_bounds.left,
(float) m_bounds.top,
(float) iconColPx,
(float) (m_bounds.bottom - m_bounds.top),
m_glyphArgb,
m_scaler.ToPxf ((float) s_kGlyphSizeDip),
s_kMdl2Font,
DxuiTextHAlign::Center,
DxuiTextVAlign::Center,
DxuiFontWeight::Normal,
false);
            IGNORE_RETURN_VALUE (hr, S_OK);

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

        if (m_check != nullptr)
        {
            m_check->Paint (painter, text, theme);
        }
    }

    bool  TryMeasureHeightPx (IDxuiTextRenderer & text, const IDxuiTheme & theme, int widthPx, int & heightPx) override;

private:
    DxuiCheckbox * m_check     = nullptr;
    std::wstring   m_text;
    wchar_t        m_glyph     = 0;
    uint32_t       m_glyphArgb = 0;
    RECT           m_bounds    = {};
    DxuiDpiScaler  m_scaler;
};





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiMessageBoxBody::TryMeasureHeightPx
//
//  The text wrapped at the width Paint draws it at, beside the glyph column
//  when there is a glyph; the glyph row is the floor for a short message.
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiMessageBoxBody::TryMeasureHeightPx (IDxuiTextRenderer & text, const IDxuiTheme & theme, int widthPx, int & heightPx)
{
    HRESULT         hr       = S_OK;
    DxuiFontHandle  bf       = theme.BodyFont();
    float           textW    = (float) widthPx;
    float           measureW = 0.0f;
    float           measureH = 0.0f;



    if (m_glyph != 0)
    {
        textW -= (float) (m_scaler.ToPx (s_kIconColDip) + m_scaler.ToPx (s_kIconTextGapDip));
    }

    CBR (textW > 0.0f);

    hr = text.MeasureStringWrapped (m_text.c_str(), m_scaler.ToPxf (bf.sizeDip), bf.face, textW, measureW, measureH);
    CHR (hr);

    heightPx = (int) std::ceil (measureH);

    if (m_glyph != 0)
    {
        heightPx = std::max (heightPx, m_scaler.ToPx (s_kGlyphRowDip));
    }

    if (m_check != nullptr)
    {
        heightPx += m_scaler.ToPx (s_kCheckRowDip);
    }

Error:
    return SUCCEEDED (hr);
}





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
        std::wstring  label;
        int           id    = 0;
    };

    // Rough (renderer-free) wrapped-line estimate for the window's first
    // size, before its backend exists; the body's measurement then sets the
    // height (DxuiDialogWindow::FitHeightToContent).
    static int  EstimateTextHeightDip (const std::wstring & text, bool hasGlyph);

    void  Configure (std::wstring              text,
                     wchar_t                   glyph,
                     uint32_t                  glyphArgb,
                     std::vector<ButtonSpec>   buttons)
    {
        m_text      = std::move (text);
        m_glyph     = glyph;
        m_glyphArgb = glyphArgb;
        m_buttons   = std::move (buttons);
    }

    void  SetCheck  (const std::wstring & label, bool isChecked) { m_checkLabel = label; m_isChecked = isChecked; }
    bool  IsChecked() const                                      { return m_isChecked; }

    //  The box with its buttons, and the check box under the text when
    //  checkLabel is given, ioChecked its state before and after.
    static int  Run (HWND                                      owner,
                     const IDxuiTheme                        * theme,
                     const wchar_t                           * text,
                     const wchar_t                           * caption,
                     UINT                                      uType,
                     const std::vector<DxuiMessageBoxButton>  & buttons,
                     int                                       defaultId,
                     const std::wstring                      * checkLabel,
                     bool                                    * ioChecked);

protected:
    void  OnCreate() override
    {
        m_body = CreateDialogContent<DxuiMessageBoxBody>();

        m_body->Set (m_text, m_glyph, m_glyphArgb);

        //  The state is copied here as it changes, since the box's controls
        //  are destroyed with its window when it closes.
        if (!m_checkLabel.empty())
        {
            m_body->SetCheck (m_checkLabel, m_isChecked, [this] (bool isChecked) { m_isChecked = isChecked; });
        }

        for (const ButtonSpec & b : m_buttons)
        {
            AddDialogButton (b.label, b.id);
        }
    }

private:
    DxuiMessageBoxBody      * m_body      = nullptr;
    std::wstring              m_checkLabel;
    bool                      m_isChecked = false;
    std::wstring              m_text;
    wchar_t                   m_glyph     = 0;
    uint32_t                  m_glyphArgb = 0;
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
//  DxuiMakeMessageBoxParams
//
//  The box is placed when its window is created, centered on the owner, the
//  way every other owned dialog is. Moving it there after creating it at the
//  cascade position left it sized for the cascade monitor's DPI and, with the
//  owner minimized, measured the owner's off-screen parking spot, which the
//  clamp turned into the corner of the primary monitor.
//
////////////////////////////////////////////////////////////////////////////////

DxuiWindow::CreateParams DxuiMakeMessageBoxParams (
    HWND             owner,
    const wchar_t  * caption,
    int              heightDip)
{
    DxuiWindow::CreateParams  params;



    params.title                    = (caption != nullptr) ? caption : L"";
    params.ownerHwnd                = owner;
    params.placement                = DxuiWindowPlacement::CenteredOnOwner;
    params.initialSizeDip           = { s_kWidthDip, heightDip };
    params.resizable                = false;
    params.insetContentBelowCaption = true;
    params.captionStyle             = DxuiCaptionStyle::CloseOnly;

    return params;
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

int DxuiMessageBox (HWND owner, const IDxuiTheme * theme, const wchar_t * text, const wchar_t * caption, UINT uType)
{
    std::vector<DxuiMessageBoxButton>  buttons;
    int                                defIndex = 0;



    switch (uType & MB_TYPEMASK)
    {
    case MB_OKCANCEL:    buttons = { { L"OK", IDOK }, { L"Cancel", IDCANCEL } }; break;
    case MB_YESNOCANCEL: buttons = { { L"Yes", IDYES }, { L"No", IDNO }, { L"Cancel", IDCANCEL } }; break;
    case MB_YESNO:       buttons = { { L"Yes", IDYES }, { L"No", IDNO } }; break;
    case MB_RETRYCANCEL: buttons = { { L"Retry", IDRETRY }, { L"Cancel", IDCANCEL } }; break;
    case MB_OK:
    default:             buttons = { { L"OK", IDOK } }; break;
    }

    defIndex = (int) ((uType & MB_DEFMASK) >> 8);

    if (defIndex < 0 || defIndex >= (int) buttons.size())
    {
        defIndex = 0;
    }

    return DxuiMessageBox (owner, theme, text, caption, uType, buttons, buttons[(size_t) defIndex].id);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiMessageBox (buttons)
//
////////////////////////////////////////////////////////////////////////////////

int DxuiMessageBox (HWND                                      owner,
                    const IDxuiTheme                        * theme,
                    const wchar_t                           * text,
                    const wchar_t                           * caption,
                    UINT                                      uType,
                    const std::vector<DxuiMessageBoxButton>  & buttons,
                    int                                       defaultId)
{
    return DxuiMessageBoxWindow::Run (owner, theme, text, caption, uType, buttons, defaultId, nullptr, nullptr);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiMessageBox (check box)
//
//  The box with uType's buttons, as the first form has them, and a check box
//  under the text.
//
////////////////////////////////////////////////////////////////////////////////

int DxuiMessageBox (HWND                 owner,
                    const IDxuiTheme   * theme,
                    const wchar_t      * text,
                    const wchar_t      * caption,
                    UINT                 uType,
                    const std::wstring & checkLabel,
                    bool               & ioChecked)
{
    std::vector<DxuiMessageBoxButton>  buttons = (uType & MB_TYPEMASK) == MB_OKCANCEL ? std::vector<DxuiMessageBoxButton> { { L"OK", IDOK }, { L"Cancel", IDCANCEL } }
                                                                                      : std::vector<DxuiMessageBoxButton> { { L"OK", IDOK } };



    return DxuiMessageBoxWindow::Run (owner, theme, text, caption, uType, buttons, IDOK, &checkLabel, &ioChecked);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiMessageBoxWindow::Run
//
//  The icon mask picks a glyph and its color; the rest of uType is unused.
//  Without a Dxui backend the system box is used instead, its OK as the
//  first button, and ioChecked is left as it was.
//
////////////////////////////////////////////////////////////////////////////////

int DxuiMessageBoxWindow::Run (HWND                                      owner,
                               const IDxuiTheme                        * theme,
                               const wchar_t                           * text,
                               const wchar_t                           * caption,
                               UINT                                      uType,
                               const std::vector<DxuiMessageBoxButton>  & buttons,
                               int                                       defaultId,
                               const std::wstring                      * checkLabel,
                               bool                                    * ioChecked)
{
    using                     ButtonSpec = DxuiMessageBoxWindow::ButtonSpec;
    wchar_t                   glyph      = 0;
    uint32_t                  glyphArgb  = 0;
    int                       heightDip  = 0;
    HRESULT                   hr         = S_OK;
    int                       choice     = IDCANCEL;
    DxuiMessageBoxWindow      dlg;
    DxuiWindow::CreateParams  params;
    HINSTANCE                 hInst      = nullptr;
    std::vector<ButtonSpec>   specs;
    std::wstring              body       = (text != nullptr) ? text : L"";
    UINT                      systemType = UINT_MAX;
    bool                      isSystem   = false;



    for (const DxuiMessageBoxButton & button : buttons)
    {
        specs.push_back ({ button.label, button.id });
    }

    switch (uType & MB_ICONMASK)
    {
    case MB_ICONHAND:        glyph = s_kGlyphError;   glyphArgb = s_kArgbError;   break;  // == ICONERROR / ICONSTOP
    case MB_ICONQUESTION:    glyph = s_kGlyphInfo;    glyphArgb = s_kArgbInfo;    break;  // no dedicated glyph
    case MB_ICONEXCLAMATION: glyph = s_kGlyphWarning; glyphArgb = s_kArgbWarning; break;  // == ICONWARNING
    case MB_ICONASTERISK:    glyph = s_kGlyphInfo;    glyphArgb = s_kArgbInfo;    break;  // == ICONINFORMATION
    default:                 break;
    }

    heightDip = std::clamp (s_kChromeHeightDip
                                + (std::max) (DxuiMessageBoxWindow::EstimateTextHeightDip (body, glyph != 0),
                                              (glyph != 0) ? s_kGlyphRowDip : 0),
                            s_kMinHeightDip,
                            s_kMaxHeightDip);

    hInst = (owner != nullptr)
                ? reinterpret_cast<HINSTANCE> (GetWindowLongPtrW (owner, GWLP_HINSTANCE))
                : GetModuleHandleW (nullptr);

    dlg.Configure (body, glyph, glyphArgb, specs);

    if (checkLabel != nullptr && ioChecked != nullptr)
    {
        dlg.SetCheck (*checkLabel, *ioChecked);
        heightDip = std::min (heightDip + s_kCheckRowDip, s_kMaxHeightDip);
    }

    params           = DxuiMakeMessageBoxParams (owner, caption, heightDip);
    params.hInstance = hInst;

    hr = dlg.Create (params);

    if (FAILED (hr))
    {
        // The Dxui backend could not be created -- fall back to the system box
        // so the caller gets a decision rather than a silent default. A set
        // of buttons the system box has uses that system type, and its choice
        // is returned; any other set is OK, for the first button, and Cancel.
        systemType = DxuiGetSystemMessageBoxType (buttons);
        isSystem   = systemType != UINT_MAX;
        choice     = MessageBoxW (owner, text, caption, (uType & ~MB_TYPEMASK) | (isSystem ? systemType : ((buttons.size() > 1) ? MB_OKCANCEL : MB_OK)));

        if (!isSystem)
        {
            choice = (choice == IDOK && !buttons.empty()) ? buttons.front().id : IDCANCEL;
        }
    }
    else
    {
        dlg.SetTheme (theme);

        choice = dlg.ShowModalDialog (defaultId);

        if (ioChecked != nullptr)
        {
            *ioChecked = dlg.IsChecked();
        }
    }

    return choice;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiGetSystemMessageBoxType
//
////////////////////////////////////////////////////////////////////////////////

UINT DxuiGetSystemMessageBoxType (const std::vector<DxuiMessageBoxButton> & buttons)
{
    static const std::pair<UINT, std::vector<int>>  kTypes[] =
    {
        { MB_OK,          { IDOK } },
        { MB_OKCANCEL,    { IDOK, IDCANCEL } },
        { MB_YESNO,       { IDYES, IDNO } },
        { MB_YESNOCANCEL, { IDYES, IDNO, IDCANCEL } },
        { MB_RETRYCANCEL, { IDRETRY, IDCANCEL } },
    };
    std::vector<int>  ids;



    for (const DxuiMessageBoxButton & button : buttons)
    {
        ids.push_back (button.id);
    }

    for (const auto & [type, set] : kTypes)
    {
        if (set == ids)
        {
            return type;
        }
    }

    return UINT_MAX;
}
