#include "Pch.h"

#include "MessageDialog.h"
#include "Ui/Dialogs/DialogBodyContent.h"
#include "Ui/Dialogs/DialogDefinition.h"





////////////////////////////////////////////////////////////////////////////////
//
//  MessageDialog::GetCommandId
//
//  The cancel button maps to IDCANCEL (so Escape / the close-box fire it);
//  every other button gets a synthetic id offset past IDOK / IDCANCEL so
//  it never collides with the window-level cancel result.
//
////////////////////////////////////////////////////////////////////////////////

int MessageDialog::GetCommandId (const Button & button, int index)
{
    return button.isCancel ? IDCANCEL : (s_kCommandBase + index);
}





////////////////////////////////////////////////////////////////////////////////
//
//  MessageDialog::Configure
//
////////////////////////////////////////////////////////////////////////////////

void MessageDialog::Configure (std::unique_ptr<DxuiPanel>  content,
                               std::vector<Button>         buttons,
                               int                         closeBoxResult)
{
    int  i = 0;



    m_pendingContent = std::move (content);
    m_buttons        = std::move (buttons);
    m_closeBoxResult = closeBoxResult;

    for (i = 0; i < (int) m_buttons.size(); ++i)
    {
        if (m_buttons[(size_t) i].isDefault)
        {
            m_defaultCommandId = GetCommandId (m_buttons[(size_t) i], i);
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  MessageDialog::OnCreate
//
//  Installs the pre-built content and one command button per configured
//  Button. Command buttons auto-close via EndDialog(id); the isCancel
//  button doubles as the Escape / close-box target (IDCANCEL).
//
////////////////////////////////////////////////////////////////////////////////

void MessageDialog::OnCreate()
{
    int  i = 0;



    if (m_pendingContent != nullptr)
    {
        SetDialogContentOwned (std::move (m_pendingContent));
    }

    for (i = 0; i < (int) m_buttons.size(); ++i)
    {
        AddDialogButton (m_buttons[(size_t) i].label, GetCommandId (m_buttons[(size_t) i], i));
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  MessageDialog::TranslateResult
//
////////////////////////////////////////////////////////////////////////////////

int MessageDialog::TranslateResult (int dialogResult) const
{
    int     result = m_closeBoxResult;
    size_t  idx    = 0;



    if (dialogResult >= s_kCommandBase && dialogResult < s_kCommandBase + (int) m_buttons.size())
    {
        result = m_buttons[(size_t) (dialogResult - s_kCommandBase)].resultCode;
    }
    else
    {
        for (auto & button : m_buttons)
        {
            if (button.isCancel)
            {
                result = button.resultCode;
                break;
            }
        }
    }

    return result;
}





////////////////////////////////////////////////////////////////////////////////
//
//  MessageDialog::ShowDefinition
//
//  Translates a renderable DialogDefinition into a MessageDialog whose
//  content is a DialogBodyContent (wrapped body labels + hyperlink links)
//  plus the action buttons, and shows it modally via ShowModalDialog. The
//  dialog height is derived from the content's preferred (line-count)
//  height so short messages stay compact and long ones grow (clamped).
//
////////////////////////////////////////////////////////////////////////////////

int MessageDialog::ShowDefinition (
    const DialogDefinition  & def,
    HINSTANCE                 instance,
    HWND                      owner,
    const IDxuiTheme        * theme,
    const AppIconLoader     & loadIcon)
{
    constexpr int       s_kBaseWidthDip       = 440;
    constexpr int       s_kMaxWidthDip        = 760;
    constexpr int       s_kMinContentWidthDip = 280;   // floor for a self-sizing body
    constexpr int       s_kBodyPadDip         = 56;   // content inset, both sides
    constexpr int       s_kChromeHeightDip    = 108;   // caption + content pad*2 + button row
    constexpr int       s_kMinHeightDip       = 120;
    constexpr int       s_kMaxHeightDip       = 620;
    constexpr int       s_kIconSrcPx          = 256;
    constexpr int       s_kDefaultIconDip     = 48;
    constexpr int       s_kGlyphSizeDip       = 32;
    constexpr wchar_t   s_kchGlyphInfo        = L'\uE946';   // MDL2 Info
    constexpr wchar_t   s_kchGlyphWarning     = L'\uE7BA';   // MDL2 Warning
    constexpr wchar_t   s_kchGlyphError       = L'\uEA39';   // MDL2 ErrorBadge
    constexpr uint32_t  s_kGlyphArgbInfo      = 0xFF4A9EDB;
    constexpr uint32_t  s_kGlyphArgbWarning   = 0xFFF5A623;
    constexpr uint32_t  s_kGlyphArgbError     = 0xFFE5424D;



    std::unique_ptr<DialogBodyContent>  content    = std::make_unique<DialogBodyContent>();
    MessageDialog                       dlg;
    DxuiWindow::CreateParams            params;
    std::vector<MessageDialog::Button>  buttons;
    HRESULT                             hr         = S_OK;
    int                                 heightDip  = 0;
    int                                 widthDip   = 0;
    int                                 result     = -1;
    bool                                imageShown = false;


    content->SetRuns (def.body);

    // A picture takes the app icon's place above the body; a severity glyph
    // sits beside the body and is unaffected.
    if (def.image.has_value())
    {
        imageShown = content->SetImage (*def.image);
    }

    if (!imageShown && (def.icon == DialogIcon::AppPhotoreal || def.icon == DialogIcon::AppFlat))
    {
        std::vector<uint32_t>  iconPixels;
        int                    iconW   = 0;
        int                    iconH   = 0;
        int                    iconDip = (def.iconSizeOverrideDp > 0.0f) ? (int) def.iconSizeOverrideDp : s_kDefaultIconDip;
        HRESULT                hrIcon  = E_NOTIMPL;


        if (loadIcon)
        {
            hrIcon = loadIcon (def.icon, s_kIconSrcPx, iconPixels, iconW, iconH);
        }

        if (SUCCEEDED (hrIcon))
        {
            content->SetIcon (std::move (iconPixels), iconW, iconH, iconDip);
        }
    }
    else if (def.icon == DialogIcon::Info)
    {
        content->SetGlyphIcon (s_kchGlyphInfo, s_kGlyphArgbInfo, s_kGlyphSizeDip);
    }
    else if (def.icon == DialogIcon::Warning)
    {
        content->SetGlyphIcon (s_kchGlyphWarning, s_kGlyphArgbWarning, s_kGlyphSizeDip);
    }
    else if (def.icon == DialogIcon::Error)
    {
        content->SetGlyphIcon (s_kchGlyphError, s_kGlyphArgbError, s_kGlyphSizeDip);
    }

    heightDip = std::clamp (s_kChromeHeightDip + content->GetPreferredHeightDip(),
                            s_kMinHeightDip,
                            s_kMaxHeightDip);

    for (const DialogButton & button : def.buttons)
    {
        buttons.push_back ({ button.label, button.resultCode, button.isDefault, button.isCancel });
    }

    //  WIDE ENOUGH FOR ITS OWN BUTTONS. The width was a hard 440 while the
    //  height already grew with the text, so a dialog whose buttons carried a
    //  filename pushed the row past the left margin and hard against the
    //  frame with no gap at all. No label carries a filename any more, but a
    //  measured row is what keeps that from mattering: measured with the same
    //  estimate the row lays itself out with, so the two cannot disagree.
    widthDip = DxuiButtonRow::kEdgePadDip * 2;

    for (const DialogButton & button : def.buttons)
    {
        widthDip += DxuiButtonRow::GetWidthForLabel (button.label) + DxuiButtonRow::kGapDip;
    }

    widthDip -= DxuiButtonRow::kGapDip;

    // A body of aligned column rows knows how wide it wants to be, and that
    // width varies with what the running machine actually has. Let it set the
    // width -- floor included, so a short body gives a small dialog rather
    // than a standard-width one with a column of air down the right. Prose
    // bodies report 0 (they wrap to whatever they are given) and keep the
    // standard width.
    {
        int  bodyDip = content->GetPreferredWidthDip();

        if (bodyDip > 0)
        {
            bodyDip += s_kBodyPadDip;
            widthDip = (widthDip > bodyDip) ? widthDip : bodyDip;
            widthDip = std::clamp (widthDip, s_kMinContentWidthDip, s_kMaxWidthDip);
        }
        else
        {
            widthDip = std::clamp (widthDip, s_kBaseWidthDip, s_kMaxWidthDip);
        }
    }

    dlg.Configure (std::move (content), std::move (buttons), def.closeBoxResult.value_or (-1));

    params.title                    = def.title;
    params.hInstance                = instance;
    params.ownerHwnd                = owner;
    params.initialSizeDip           = { widthDip, heightDip };
    params.resizable                = false;
    params.insetContentBelowCaption = true;
    params.captionStyle             = DxuiCaptionStyle::CloseOnly;

    // Centered on the owner window rather than on the OS cascade, which
    // ignores the owner: the Help modals opened at the cascade's top-left
    // corner, half off the window that raised them. A modal belongs where
    // the user is already looking.
    params.placement                = DxuiWindowPlacement::CenteredOnOwner;

    hr = dlg.Create (params);
    CHRA (hr);

    dlg.SetTheme (theme);

    result = dlg.TranslateResult (dlg.ShowModalDialog (dlg.GetDefaultCommandId()));

Error:
    return result;
}
