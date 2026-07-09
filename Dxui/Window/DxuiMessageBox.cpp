#include "Pch.h"

#include "Window/DxuiMessageBox.h"

#include "Window/DxuiDialogWindow.h"
#include "Core/IDxuiControl.h"
#include "Core/DxuiPanel.h"
#include "Render/IDxuiPainter.h"
#include "Render/IDxuiTextRenderer.h"
#include "Theme/IDxuiTheme.h"




namespace
{
    // Icon glyphs (Segoe MDL2 Assets) + accent colours -- same set the shell's
    // task-dialog path uses, so a Dxui message box reads identically.
    constexpr wchar_t   s_kGlyphInfo     = L'\uE946';   // MDL2 Info
    constexpr wchar_t   s_kGlyphWarning  = L'\uE7BA';   // MDL2 Warning
    constexpr wchar_t   s_kGlyphError    = L'\uEA39';   // MDL2 ErrorBadge
    constexpr uint32_t  s_kArgbInfo      = 0xFF4A9EDB;
    constexpr uint32_t  s_kArgbWarning   = 0xFFF5A623;
    constexpr uint32_t  s_kArgbError     = 0xFFE5424D;
    constexpr wchar_t   s_kMdl2Font   [] = L"Segoe MDL2 Assets";

    constexpr int  s_kWidthDip        = 400;
    constexpr int  s_kIconColDip      =  40;   // glyph column width
    constexpr int  s_kIconTextGapDip  =  10;
    constexpr int  s_kGlyphSizeDip    =  28;
    constexpr int  s_kLineHeightDip   =  20;
    constexpr int  s_kChromeHeightDip = 104;   // caption + button row + content vpad
    constexpr int  s_kMinHeightDip    = 128;
    constexpr int  s_kMaxHeightDip    = 560;


    struct ButtonSpec
    {
        const wchar_t *  label = nullptr;
        int              id    = 0;
    };


    // Rough (renderer-free) wrapped-line estimate so the dialog can be sized
    // before its backend exists. Mirrors the shell's line-count heuristic.
    int  EstimateTextHeightDip (const std::wstring & text, bool hasGlyph)
    {
        int     contentW    = s_kWidthDip - 40 - (hasGlyph ? (s_kIconColDip + s_kIconTextGapDip) : 0);
        int     approxCharW = 7;   // ~13dip body font average advance
        int     cpl         = (std::max) (8, contentW / approxCharW);
        int     lines       = 0;
        size_t  pos         = 0;

        for (;;)
        {
            size_t  nl  = text.find (L'\n', pos);
            size_t  end = (nl == std::wstring::npos) ? text.size () : nl;
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




    ////////////////////////////////////////////////////////////////////////////
    //
    //  MessageBoxBody -- the content control: an optional semantic glyph in a
    //  left column, and the wrapped message text (theme body font) beside it.
    //
    ////////////////////////////////////////////////////////////////////////////

    class MessageBoxBody : public DxuiPanel
    {
    public:
        void  Set (std::wstring text, wchar_t glyph, uint32_t glyphArgb)
        {
            m_text      = std::move (text);
            m_glyph     = glyph;
            m_glyphArgb = glyphArgb;
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

            if (m_glyph != 0)
            {
                wchar_t  g[2]      = { m_glyph, L'\0' };
                int      iconColPx = m_scaler.Px (s_kIconColDip);

                IGNORE_RETURN_VALUE (hr, text.DrawString (
                    g,
                    (float) m_bounds.left,
                    (float) m_bounds.top,
                    (float) iconColPx,
                    (float) (m_bounds.bottom - m_bounds.top),
                    m_glyphArgb,
                    m_scaler.Pxf ((float) s_kGlyphSizeDip),
                    s_kMdl2Font,
                    DxuiTextHAlign::Center,
                    DxuiTextVAlign::Center,
                    DxuiFontWeight::Normal,
                    false));

                tr.left += iconColPx + m_scaler.Px (s_kIconTextGapDip);
            }

            {
                DxuiFontHandle  bf = theme.BodyFont ();

                IGNORE_RETURN_VALUE (hr, text.DrawString (
                    m_text.c_str (),
                    (float) tr.left,
                    (float) tr.top,
                    (float) (tr.right  - tr.left),
                    (float) (tr.bottom - tr.top),
                    theme.TextColor (DxuiTextRole::Body),
                    m_scaler.Pxf (bf.sizeDip),
                    bf.face,
                    DxuiTextHAlign::Left,
                    DxuiTextVAlign::Center,
                    bf.weight,
                    true));
            }
        }

    private:
        std::wstring   m_text;
        wchar_t        m_glyph     = 0;
        uint32_t       m_glyphArgb = 0;
        RECT           m_bounds    = {};
        DxuiDpiScaler  m_scaler;
    };




    ////////////////////////////////////////////////////////////////////////////
    //
    //  DxuiMessageBoxWindow -- the modal dialog: builds the body + buttons in
    //  OnCreate, then the free function shows it via ShowModalDialog.
    //
    ////////////////////////////////////////////////////////////////////////////

    class DxuiMessageBoxWindow : public DxuiDialogWindow
    {
    public:
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

    protected:
        void  OnCreate () override
        {
            MessageBoxBody *  body = CreateDialogContent<MessageBoxBody> ();

            body->Set (m_text, m_glyph, m_glyphArgb);

            for (const ButtonSpec & b : m_buttons)
            {
                AddDialogButton (b.label, b.id);
            }
        }

    private:
        std::wstring              m_text;
        wchar_t                   m_glyph     = 0;
        uint32_t                  m_glyphArgb = 0;
        std::vector<ButtonSpec>   m_buttons;
    };
}




////////////////////////////////////////////////////////////////////////////////
//
//  DxuiMessageBox
//
////////////////////////////////////////////////////////////////////////////////

int DxuiMessageBox (HWND owner, const IDxuiTheme * theme, const wchar_t * text, const wchar_t * caption, UINT uType)
{
    std::vector<ButtonSpec>   buttons;
    std::wstring              body       = (text != nullptr) ? text : L"";
    wchar_t                   glyph      = 0;
    uint32_t                  glyphArgb  = 0;
    int                       defIndex   = 0;
    int                       defaultCmd = IDOK;
    int                       heightDip  = 0;
    HINSTANCE                 hInst      = nullptr;
    HRESULT                   hr         = S_OK;
    DxuiMessageBoxWindow      dlg;
    DxuiWindow::CreateParams  params;

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
    if (defIndex < 0 || defIndex >= (int) buttons.size ())
    {
        defIndex = 0;
    }
    defaultCmd = buttons[(size_t) defIndex].id;

    switch (uType & MB_ICONMASK)
    {
    case MB_ICONHAND:        glyph = s_kGlyphError;   glyphArgb = s_kArgbError;   break;  // == ICONERROR / ICONSTOP
    case MB_ICONQUESTION:    glyph = s_kGlyphInfo;    glyphArgb = s_kArgbInfo;    break;  // no dedicated glyph
    case MB_ICONEXCLAMATION: glyph = s_kGlyphWarning; glyphArgb = s_kArgbWarning; break;  // == ICONWARNING
    case MB_ICONASTERISK:    glyph = s_kGlyphInfo;    glyphArgb = s_kArgbInfo;    break;  // == ICONINFORMATION
    default:                 break;
    }

    heightDip = std::clamp (s_kChromeHeightDip
                                + (std::max) (EstimateTextHeightDip (body, glyph != 0),
                                              (glyph != 0) ? s_kGlyphSizeDip : 0),
                            s_kMinHeightDip,
                            s_kMaxHeightDip);

    hInst = (owner != nullptr)
                ? reinterpret_cast<HINSTANCE> (GetWindowLongPtrW (owner, GWLP_HINSTANCE))
                : GetModuleHandleW (nullptr);

    dlg.Configure (body, glyph, glyphArgb, buttons);

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
        return MessageBoxW (owner, text, caption, uType);
    }

    dlg.SetTheme (theme);

    return dlg.ShowModalDialog (defaultCmd);
}
