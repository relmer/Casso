#include "Pch.h"

#include "Window/DxuiMessageBox.h"

#include "Window/DxuiDialogWindow.h"
#include "Core/IDxuiControl.h"
#include "Core/DxuiPanel.h"
#include "Render/IDxuiPainter.h"
#include "Render/IDxuiTextRenderer.h"
#include "Theme/IDxuiTheme.h"
#include "Theme/DxuiColor.h"




//  The icons of Windows' own message box, in either theme: a red circle
//  with a white cross, a yellow triangle with a black exclamation point, and
//  a blue circle with a white i. Each glyph pair is the icon font's own: the
//  triangle and exclamation point, the circle and its cross or its i.
static constexpr wchar_t   s_kGlyphCircle      = L'\uF136';
static constexpr wchar_t   s_kGlyphTriangle    = L'\uF139';
static constexpr wchar_t   s_kGlyphExclamation = L'\uF13B';
static constexpr wchar_t   s_kGlyphCross       = L'\uF13D';
static constexpr wchar_t   s_kGlyphInfoMark    = L'\uF13F';
static constexpr uint32_t  s_kArgbInfo         = 0xFF0067C0;
static constexpr uint32_t  s_kArgbWarning      = 0xFFFFC83D;
static constexpr uint32_t  s_kArgbError        = 0xFFD13438;
static constexpr uint32_t  s_kArgbMarkLight    = 0xFFFFFFFF;
static constexpr uint32_t  s_kArgbMarkDark     = 0xFF000000;
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
static constexpr int  s_kWorkAreaMarginDip = 40;   // kept clear above and below a box as tall as the screen allows





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
        HRESULT  hr      = S_OK;
        RECT     tr      = m_bounds;
        bool     laidOut = false;

        UNREFERENCED_PARAMETER (painter);

        if (m_severity != DxuiMessageSeverity::None)
        {
            int       iconColPx = m_scaler.ToPx (s_kIconColDip);
            wchar_t   shape[2]  = { (m_severity == DxuiMessageSeverity::Warning) ? s_kGlyphTriangle : s_kGlyphCircle, L'\0' };
            wchar_t   mark[2]   = { (m_severity == DxuiMessageSeverity::Warning) ? s_kGlyphExclamation
                                  : (m_severity == DxuiMessageSeverity::Error)   ? s_kGlyphCross
                                                                                 : s_kGlyphInfoMark, L'\0' };
            uint32_t  fill      = (m_severity == DxuiMessageSeverity::Warning) ? s_kArgbWarning
                                : (m_severity == DxuiMessageSeverity::Error)   ? s_kArgbError
                                                                               : s_kArgbInfo;
            uint32_t  ink       = (m_severity == DxuiMessageSeverity::Warning) ? s_kArgbMarkDark : s_kArgbMarkLight;

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

        //  A paint before the first layout has an empty box and draws no text.
        laidOut = tr.right > tr.left && tr.bottom > tr.top;

        if (laidOut && m_text.find (L"**") != std::wstring::npos)
        {
            PaintRuns (text, theme, tr);
        }
        else if (laidOut)
        {
            DxuiFontHandle   bf      = theme.BodyFont();
            float            fullW   = 0.0f;
            float            fullH   = 0.0f;
            DxuiTextVAlign   vAlign  = DxuiTextVAlign::Center;

            //  Text taller than its box starts at the top rather than spilling
            //  up under the caption, and a measure that fails takes it for
            //  tall. The box clips it either way, so it never runs over the
            //  caption or the buttons.
            hr = text.MeasureStringWrapped (m_text.c_str(), m_scaler.ToPxf (bf.sizeDip), bf.face, (float) (tr.right - tr.left), fullW, fullH);
            vAlign = (FAILED (hr) || fullH > (float) (tr.bottom - tr.top)) ? DxuiTextVAlign::Top : DxuiTextVAlign::Center;

            NoteOverrun (SUCCEEDED (hr) ? fullH : 0.0f, tr);

            hr = text.PushClipRect ((float) tr.left, (float) tr.top, (float) (tr.right - tr.left), (float) (tr.bottom - tr.top));
            IGNORE_RETURN_VALUE (hr, S_OK);

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
vAlign,
bf.weight,
true);
            IGNORE_RETURN_VALUE (hr, S_OK);

            hr = text.PopClipRect();
            IGNORE_RETURN_VALUE (hr, S_OK);
        }
    }

    bool  TryMeasureHeightPx (IDxuiTextRenderer & text, const IDxuiTheme & theme, int widthPx, int & heightPx) override;

    //  How many pixels the text drawn last ran past the bottom of its box,
    //  and none after this call; the window grows by it.
    int   TakeOverrunPx()
    {
        int  overrun = m_overrunPx;

        m_overrunPx = 0;
        return overrun;
    }

private:
    //  The painted text's height against its box, as the paint measured it:
    //  whatever the sizing measured before, the text drawn is the one that
    //  has to fit.
    void  NoteOverrun (float textHeightPx, const RECT & box)
    {
        int  overrun = (int) std::ceil (textHeightPx) - (int) (box.bottom - box.top);

        m_overrunPx = std::max (m_overrunPx, overrun);
    }

    //  Text with semibold runs, a line at a time: a line of one regular run
    //  wraps, and a line with semibold runs is drawn run after run on one
    //  line. The lines are measured first, so the block is centered as a
    //  whole when it fits.
    void  PaintRuns (IDxuiTextRenderer & text, const IDxuiTheme & theme, const RECT & tr)
    {
        HRESULT                                 hr      = S_OK;
        DxuiFontHandle                          bf      = theme.BodyFont();
        float                                   sizePx  = m_scaler.ToPxf (bf.sizeDip);
        float                                   widthPx = (float) (tr.right - tr.left);
        float                                   lineH   = 0.0f;
        float                                   ignored = 0.0f;
        float                                   total   = 0.0f;
        float                                   y       = (float) tr.top;
        std::vector<std::vector<DxuiTextRun>>   lines;
        std::vector<float>                      heights;



        DxuiParseTextRuns (m_text, lines);

        hr = text.MeasureStringWeighted (L"Ag", sizePx, bf.face, bf.weight, ignored, lineH);
        lineH = SUCCEEDED (hr) ? lineH : m_scaler.ToPxf ((float) s_kLineHeightDip);

        for (const std::vector<DxuiTextRun> & line : lines)
        {
            float  h = lineH;

            if (line.size() == 1 && !line[0].semiBold)
            {
                hr = text.MeasureStringWrapped (line[0].text.c_str(), sizePx, bf.face, widthPx, ignored, h);
                h  = SUCCEEDED (hr) ? h : lineH;
            }

            heights.push_back (h);
            total += h;
        }

        if (total < (float) (tr.bottom - tr.top))
        {
            y += ((float) (tr.bottom - tr.top) - total) * 0.5f;
        }

        NoteOverrun (total, tr);

        hr = text.PushClipRect ((float) tr.left, (float) tr.top, widthPx, (float) (tr.bottom - tr.top));
        IGNORE_RETURN_VALUE (hr, S_OK);

        for (size_t i = 0; i < lines.size(); i++)
        {
            float  x    = (float) tr.left;
            bool   wrap = lines[i].size() == 1 && !lines[i][0].semiBold;

            for (const DxuiTextRun & run : lines[i])
            {
                DxuiFontWeight  weight = run.semiBold ? DxuiFontWeight::SemiBold : bf.weight;
                float           runW   = 0.0f;

                hr = text.DrawString (run.text.c_str(), x, y, (float) tr.right - x, heights[i],
                                      theme.TextColor (DxuiTextRole::Body), sizePx, bf.face,
                                      DxuiTextHAlign::Left, DxuiTextVAlign::Top, weight, wrap);
                IGNORE_RETURN_VALUE (hr, S_OK);

                hr = text.MeasureStringWeighted (run.text.c_str(), sizePx, bf.face, weight, runW, ignored);
                x += SUCCEEDED (hr) ? runW : 0.0f;
            }

            y += heights[i];
        }

        hr = text.PopClipRect();
        IGNORE_RETURN_VALUE (hr, S_OK);
    }

    std::wstring         m_text;
    DxuiMessageSeverity  m_severity  = DxuiMessageSeverity::None;
    RECT                 m_bounds    = {};
    DxuiDpiScaler        m_scaler;
    int                  m_overrunPx = 0;
};





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiMessageBoxBody::TryMeasureHeightPx
//
//  The text wrapped at the width Paint draws it at, beside the glyph column
//  when there is a glyph, measured as Paint lays it out: a message with
//  semibold runs line by line, as PaintRuns draws it. The glyph row is the
//  floor for a short message.
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiMessageBoxBody::TryMeasureHeightPx (IDxuiTextRenderer & text, const IDxuiTheme & theme, int widthPx, int & heightPx)
{
    HRESULT                                 hr       = S_OK;
    DxuiFontHandle                          bf       = theme.BodyFont();
    float                                   sizePx   = m_scaler.ToPxf (bf.sizeDip);
    float                                   textW    = (float) widthPx;
    float                                   measureW = 0.0f;
    float                                   measureH = 0.0f;
    float                                   lineH    = 0.0f;
    float                                   h        = 0.0f;
    std::vector<std::vector<DxuiTextRun>>   lines;



    if (m_severity != DxuiMessageSeverity::None)
    {
        textW -= (float) (m_scaler.ToPx (s_kIconColDip) + m_scaler.ToPx (s_kIconTextGapDip));
    }

    CBR (textW > 0.0f);

    if (m_text.find (L"**") == std::wstring::npos)
    {
        hr = text.MeasureStringWrapped (m_text.c_str(), sizePx, bf.face, textW, measureW, measureH);
        CHR (hr);
    }
    else
    {
        DxuiParseTextRuns (m_text, lines);

        hr = text.MeasureStringWeighted (L"Ag", sizePx, bf.face, bf.weight, measureW, lineH);
        CHR (hr);

        for (const std::vector<DxuiTextRun> & line : lines)
        {
            h = lineH;

            if (line.size() == 1 && !line[0].semiBold)
            {
                hr = text.MeasureStringWrapped (line[0].text.c_str(), sizePx, bf.face, textW, measureW, h);
                CHR (hr);
            }

            measureH += h;
        }
    }

    heightPx = (int) std::ceil (measureH);

    if (m_severity != DxuiMessageSeverity::None)
    {
        heightPx = std::max (heightPx, m_scaler.ToPx (s_kGlyphRowDip));
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
        const wchar_t *  label = nullptr;
        int              id    = 0;
    };

    // Rough (renderer-free) wrapped-line estimate for the window's first
    // size, before its backend exists; the body's measurement then sets the
    // height (DxuiDialogWindow::FitHeightToContent).
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
        m_body = CreateDialogContent<DxuiMessageBoxBody> ();

        m_body->Set (m_text, m_severity);

        for (const ButtonSpec & b : m_buttons)
        {
            AddDialogButton (b.label, b.id);
        }
    }

    //  The last guard against a box too short for its message: text the
    //  paint found running past its box grows the window by that much,
    //  however the sizing before it measured.
    void  OnDialogTick() override
    {
        HRESULT  hr = S_OK;

        if (m_body != nullptr)
        {
            hr = GrowHeightBy (m_body->TakeOverrunPx());
            IGNORE_RETURN_VALUE (hr, S_OK);
        }
    }

private:
    DxuiMessageBoxBody      * m_body      = nullptr;
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
        size_t  nl     = text.find (L'\n', pos);
        size_t  end    = (nl == std::wstring::npos) ? text.size() : nl;
        int     used   = 0;
        int     pieces = 0;

        //  Wrapped as the renderer wraps, a piece at a time: a word ends at a
        //  space or a hyphen, and a path breaks before each backslash, so a
        //  long path takes more lines than its length alone says.
        for (size_t at = pos; at < end; )
        {
            size_t  stop = at + 1;
            int     len  = 0;

            while (stop < end && text[stop] != L'\\' && text[stop - 1] != L' ' && text[stop - 1] != L'-')
            {
                stop++;
            }

            len = (int) (stop - at);

            if (used > 0 && used + len > cpl)
            {
                lines++;
                used = 0;
            }

            used += len;

            while (used > cpl)
            {
                lines++;
                used -= cpl;
            }

            pieces++;
            at = stop;
        }

        lines += (used > 0 || pieces == 0) ? 1 : 0;

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
//  GetMaxHeightDip
//
//  A long message's box grows to its owner's work area, less a margin, as
//  Windows' own message box does, and never below the fixed cap.
//
////////////////////////////////////////////////////////////////////////////////

static int GetMaxHeightDip (HWND owner)
{
    MONITORINFO  monitor = { sizeof (monitor) };
    UINT         dpi     = (owner != nullptr) ? GetDpiForWindow (owner) : GetDpiForSystem();
    POINT        cursor  = {};
    HMONITOR     handle  = nullptr;
    int          workDip = 0;



    //  With no owner yet, as for a command line refused before Casso's window
    //  opens, the box opens on the monitor under the pointer.
    if (owner != nullptr)
    {
        handle = MonitorFromWindow (owner, MONITOR_DEFAULTTOPRIMARY);
    }
    else if (GetCursorPos (&cursor))
    {
        handle = MonitorFromPoint (cursor, MONITOR_DEFAULTTOPRIMARY);
    }

    if (dpi == 0 || handle == nullptr || !GetMonitorInfoW (handle, &monitor))
    {
        return s_kMaxHeightDip;
    }

    workDip = MulDiv (monitor.rcWork.bottom - monitor.rcWork.top, USER_DEFAULT_SCREEN_DPI, (int) dpi);

    return (std::max) (s_kMaxHeightDip, workDip - s_kWorkAreaMarginDip);
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetTextHeightDip
//
//  The text's real wrap at the content width of a client area that wide,
//  where the theme's font is known; the estimate runs short on long words.
//
////////////////////////////////////////////////////////////////////////////////

static int GetTextHeightDip (const std::wstring & body, DxuiMessageSeverity severity, const IDxuiTheme * theme, int clientWidthDip)
{
    HRESULT  hr       = S_OK;
    bool     hasGlyph = severity != DxuiMessageSeverity::None;
    int      textDip  = DxuiMessageBoxWindow::EstimateTextHeightDip (body, hasGlyph);



    if (theme != nullptr)
    {
        DxuiFontHandle  font     = theme->BodyFont();
        float           widthDip = (float) (clientWidthDip - 2 * DxuiButtonRow::kEdgePadDip - (hasGlyph ? s_kIconColDip + s_kIconTextGapDip : 0));
        float           measured = 0.0f;

        hr = DxuiTextRenderer::MeasureWrappedDip (body.c_str(), font.sizeDip, font.face, font.weight, widthDip, measured);

        if (SUCCEEDED (hr))
        {
            textDip = (int) std::ceil (measured);
        }
    }

    return textDip;
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetBoxHeightDip
//
////////////////////////////////////////////////////////////////////////////////

static int GetBoxHeightDip (HWND owner, DxuiMessageSeverity severity, int textDip)
{
    int  contentDip = (std::max) (textDip, (severity != DxuiMessageSeverity::None) ? s_kGlyphRowDip : 0);



    return std::clamp (s_kChromeHeightDip + contentDip, s_kMinHeightDip, GetMaxHeightDip (owner));
}





////////////////////////////////////////////////////////////////////////////////
//
//  FitToClient
//
//  The box is created at an outer size, and its frame takes width and
//  height from the client area the text is drawn in. The text is measured
//  again at the client's real width, where it can wrap to more lines than
//  at the outer width, and the frame's height is added, so no line is cut
//  off.
//
////////////////////////////////////////////////////////////////////////////////

static HRESULT FitToClient (HWND hwnd, HWND owner, const std::wstring & body, DxuiMessageSeverity severity, const IDxuiTheme * theme)
{
    HRESULT  hr        = S_OK;
    RECT     window    = {};
    RECT     client    = {};
    UINT     dpi       = 0;
    BOOL     placed    = FALSE;
    int      textDip   = 0;
    int      heightPx  = 0;



    CBRAEx (hwnd != nullptr, E_INVALIDARG);

    dpi = GetDpiForWindow (hwnd);
    CBR (dpi != 0);

    placed = GetWindowRect (hwnd, &window);
    CWRA (placed);

    placed = GetClientRect (hwnd, &client);
    CWRA (placed);

    textDip  = GetTextHeightDip (body, severity, theme, MulDiv (client.right, USER_DEFAULT_SCREEN_DPI, (int) dpi));
    heightPx = MulDiv (GetBoxHeightDip (owner, severity, textDip), (int) dpi, USER_DEFAULT_SCREEN_DPI)
             + (window.bottom - window.top) - client.bottom;

    placed = SetWindowPos (hwnd, nullptr, 0, 0, window.right - window.left, heightPx, SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
    CWRA (placed);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  StripRunMarks
//
//  The text without its `**` marks, for sizing the box and for the system
//  box it falls back to.
//
////////////////////////////////////////////////////////////////////////////////

static std::wstring StripRunMarks (const std::wstring & text)
{
    std::vector<std::vector<DxuiTextRun>>  lines;
    std::wstring                           plain;



    DxuiParseTextRuns (text, lines);

    for (size_t i = 0; i < lines.size(); i++)
    {
        for (const DxuiTextRun & run : lines[i])
        {
            plain += run.text;
        }

        plain += (i + 1 < lines.size()) ? L"\n" : L"";
    }

    return plain;
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
    int                       textDip    = 0;
    HRESULT                   hr         = S_OK;
    int                       choice     = 0;
    DxuiMessageBoxWindow      dlg;
    DxuiWindow::CreateParams  params;
    HINSTANCE                 hInst      = nullptr;



    std::vector<ButtonSpec>   buttons;
    std::wstring              body       = (text != nullptr) ? text : L"";
    std::wstring              plain      = StripRunMarks (body);
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

    textDip   = GetTextHeightDip (plain, severity, theme, s_kWidthDip);
    heightDip = GetBoxHeightDip (owner, severity, textDip);

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

    //  Placed by its owner, the box is sized in the owner's DPI rather than
    //  the system's, which differ on a monitor scaled unlike the primary one
    //  or after the scale changes while Windows is running.
    params.placement                = (owner != nullptr) ? DxuiWindowPlacement::CenteredOnOwner : DxuiWindowPlacement::Default;

    hr = dlg.Create (params);

    if (FAILED (hr))
    {
        // The Dxui backend could not be created -- fall back to the system box
        // so the caller still gets a decision rather than a silent default.
        choice = MessageBoxW (owner, plain.c_str(), caption, uType);
    }
    else
    {
        dlg.SetTheme (theme);

        hr = FitToClient (dlg.GetHwnd(), owner, plain, severity, theme);
        IGNORE_RETURN_VALUE (hr, S_OK);

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





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiParseTextRuns
//
////////////////////////////////////////////////////////////////////////////////

void DxuiParseTextRuns (const std::wstring & text, std::vector<std::vector<DxuiTextRun>> & outLines)
{
    size_t  pos = 0;



    outLines.clear();

    for (;;)
    {
        size_t                    nl       = text.find (L'\n', pos);
        size_t                    end      = (nl == std::wstring::npos) ? text.size() : nl;
        bool                      semiBold = false;
        std::vector<DxuiTextRun>  line;

        while (pos < end)
        {
            size_t  mark = text.find (L"**", pos);
            size_t  stop = (mark == std::wstring::npos || mark > end) ? end : mark;

            if (stop > pos)
            {
                line.push_back ({ text.substr (pos, stop - pos), semiBold });
            }

            semiBold = (stop == mark) ? !semiBold : semiBold;
            pos      = (stop == mark) ? stop + 2 : stop;
        }

        outLines.push_back (std::move (line));

        if (nl == std::wstring::npos)
        {
            break;
        }

        pos = nl + 1;
    }
}