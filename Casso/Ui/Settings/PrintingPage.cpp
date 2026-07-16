#include "Pch.h"

#include "PrintingPage.h"




////////////////////////////////////////////////////////////////////////////////
//
//  Anonymous helpers
//
////////////////////////////////////////////////////////////////////////////////

namespace
{
    constexpr int    s_kRowHeightDp     = 28;
    constexpr int    s_kLabelWidthDp    = 130;
    constexpr int    s_kDropdownWidthDp = 220;
    constexpr int    s_kCheckWidthDp    = 140;
    constexpr int    s_kChildIndentDp   = 18;    // one nesting step (matches DxuiTreeView)
    constexpr int    s_kSectionGapDp    = 14;
    constexpr int    s_kPagePadDp       = 16;


    RECT MakeRect (int l, int t, int w, int h)
    {
        RECT  rc = { l, t, l + w, t + h };
        return rc;
    }

    // Dot-style token <-> dropdown index. The dpi dropdown maps index 0 -> 288,
    // 1 -> 576.
    int  DotStyleToIndex (const std::string & s) { return s == "plain" ? 1 : 0; }
}




////////////////////////////////////////////////////////////////////////////////
//
//  PrintingPage::PrintingPage
//
//  Registers each member widget in the page's child tree (non-owning Adopt),
//  like DiskPage; Layout positions them and Rebuild wires their callbacks.
//
////////////////////////////////////////////////////////////////////////////////

PrintingPage::PrintingPage (std::wstring title)
    : DxuiPropertyPage (std::move (title))
{
    Adopt (m_dpiLabel);
    Adopt (m_dpi);
    Adopt (m_styleLabel);
    Adopt (m_dotStyle);
    Adopt (m_audioLabel);
    Adopt (m_soundsToggle);
    Adopt (m_volumeLabel);
    Adopt (m_volume);
    Adopt (m_panOverride);
    Adopt (m_panLabel);
    Adopt (m_pan);
}




////////////////////////////////////////////////////////////////////////////////
//
//  PrintingPage::SetPrefs
//
////////////////////////////////////////////////////////////////////////////////

void PrintingPage::SetPrefs (GlobalUserPrefs * prefs)
{
    m_prefs = prefs;
    Rebuild ();
}




////////////////////////////////////////////////////////////////////////////////
//
//  PrintingPage::SetPopupHost
//
////////////////////////////////////////////////////////////////////////////////

void PrintingPage::SetPopupHost (DxuiHwndSource * host)
{
    m_dpi.SetPopupHost      (host);
    m_dotStyle.SetPopupHost (host);
}




////////////////////////////////////////////////////////////////////////////////
//
//  PrintingPage::Layout
//
////////////////////////////////////////////////////////////////////////////////

void PrintingPage::Layout (const RECT & rect, const DxuiDpiScaler & scaler)
{
    UINT dpi         = scaler.Dpi ();
    int  pad         = scaler.Px (s_kPagePadDp);
    int  rowHeight   = scaler.Px (s_kRowHeightDp);
    int  labelWidth  = scaler.Px (s_kLabelWidthDp);
    int  dropWidth   = scaler.Px (s_kDropdownWidthDp);
    int  checkWidth  = scaler.Px (s_kCheckWidthDp);
    int  childIndent = scaler.Px (s_kChildIndentDp);
    int  sectionGap  = scaler.Px (s_kSectionGapDp);
    int  x           = rect.left + pad;
    int  y           = rect.top  + pad;
    int  controlsX   = x + labelWidth;


    m_dpiLabel.SetRect  (MakeRect (x, y, labelWidth, rowHeight));
    m_dpiLabel.SetText  (L"Output resolution:");
    m_dpi.SetRect  (MakeRect (controlsX, y, dropWidth, rowHeight));
    m_dpi.SetItems ({ L"288 dpi (draft)", L"576 dpi (high)" });
    y += rowHeight + sectionGap;

    m_styleLabel.SetRect  (MakeRect (x, y, labelWidth, rowHeight));
    m_styleLabel.SetText  (L"Dot style:");
    m_dotStyle.SetRect  (MakeRect (controlsX, y, dropWidth, rowHeight));
    m_dotStyle.SetItems ({ L"Ink (round dots)", L"Plain (square)" });
    y += rowHeight + sectionGap;

    // Printer sound: a master toggle whose children (volume, manual pan) indent
    // one step; the pan slider indents a second step under the manual-pan
    // checkbox. Labels indent; the controls stay in one column.
    m_audioLabel.SetRect (MakeRect (x, y, labelWidth, rowHeight));
    m_audioLabel.SetText (L"Printer sound:");
    m_soundsToggle.SetRect (MakeRect (controlsX, y, checkWidth, rowHeight));
    y += rowHeight + sectionGap;

    m_volumeLabel.SetRect (MakeRect (x + childIndent, y, labelWidth - childIndent, rowHeight));
    m_volumeLabel.SetText (L"Volume:");
    ConfigureVolumeSlider (m_volume, MakeRect (controlsX, y, dropWidth, rowHeight));
    y += rowHeight + sectionGap;

    m_panOverride.SetRect  (MakeRect (x + childIndent, y, labelWidth + dropWidth - childIndent, rowHeight));
    m_panOverride.SetLabel (L"Manual stereo pan");
    y += rowHeight + sectionGap;

    m_panLabel.SetRect (MakeRect (x + childIndent * 2, y, labelWidth - childIndent * 2, rowHeight));
    m_panLabel.SetText (L"Pan:");
    ConfigurePanSlider (m_pan, MakeRect (controlsX, y, dropWidth, rowHeight));
    y += rowHeight + sectionGap;

    m_dpiLabel.SetDpi      (dpi);
    m_dpi.SetDpi           (dpi);
    m_styleLabel.SetDpi    (dpi);
    m_dotStyle.SetDpi      (dpi);
    m_audioLabel.SetDpi    (dpi);
    m_soundsToggle.SetDpi  (dpi);
    m_volumeLabel.SetDpi   (dpi);
    m_volume.SetDpi        (dpi);
    m_panOverride.SetDpi   (dpi);
    m_panLabel.SetDpi      (dpi);
    m_pan.SetDpi           (dpi);

    DxuiPanel::SetBounds (rect);
}




////////////////////////////////////////////////////////////////////////////////
//
//  PrintingPage::Rebuild
//
//  Syncs widgets to GlobalUserPrefs and wires each change back into it. Edits
//  have no live effect; the apply controller persists / reverts them.
//
////////////////////////////////////////////////////////////////////////////////

void PrintingPage::Rebuild ()
{
    GlobalUserPrefs *  prefs = m_prefs;


    if (prefs == nullptr)
    {
        return;
    }

    m_dpi.SetSelected      (prefs->printOutputDpi == 576 ? 1 : 0);
    m_dotStyle.SetSelected (DotStyleToIndex (prefs->printDotStyle));
    m_soundsToggle.SetChecked (prefs->printerAudioEnabled);
    m_volume.SetValue      (prefs->printerAudioVolume * 100.0f);
    m_panOverride.SetChecked (prefs->printerAudioPanOverride);
    m_pan.SetValue         (prefs->printerAudioPan * 100.0f);
    ApplyEnabledState      ();

    m_dpi.SetSelect ([this, prefs] (int idx)
    {
        prefs->printOutputDpi = (idx == 1) ? 576 : 288;
        MarkDirty ();
    });

    m_dotStyle.SetSelect ([this, prefs] (int idx)
    {
        prefs->printDotStyle = (idx == 1) ? "plain" : "ink";
        MarkDirty ();
    });

    m_soundsToggle.SetOnChange ([this, prefs] (bool checked)
    {
        prefs->printerAudioEnabled = checked;
        ApplyEnabledState ();
        MarkDirty ();
    });

    m_volume.SetOnChange ([this, prefs] (float v)
    {
        prefs->printerAudioVolume = v / 100.0f;
        MarkDirty ();
    });

    m_panOverride.SetOnChange ([this, prefs] (bool checked)
    {
        prefs->printerAudioPanOverride = checked;
        ApplyEnabledState ();
        MarkDirty ();
    });

    m_pan.SetOnChange ([this, prefs] (float v)
    {
        prefs->printerAudioPan = v / 100.0f;
        MarkDirty ();
    });
}




////////////////////////////////////////////////////////////////////////////////
//
//  PrintingPage::ConfigureVolumeSlider
//
//  0-100% linear volume slider with a "%" readout (matches DiskPage).
//
////////////////////////////////////////////////////////////////////////////////

void PrintingPage::ConfigureVolumeSlider (DxuiSlider & slider, const RECT & rect)
{
    constexpr float  s_kVolumeMax = 100.0f;


    slider.SetRect      (rect);
    slider.SetRange     (0.0f, s_kVolumeMax);
    slider.SetStep      (1.0f);
    slider.SetSuffix    (L"%");
    slider.SetDecimalPlaces (0);
    slider.SetShowTicks (true);
    slider.SetTickInterval (10.0f);
}




////////////////////////////////////////////////////////////////////////////////
//
//  PrintingPage::ConfigurePanSlider
//
//  Bipolar Left..Center..Right pan slider (matches DiskPage's pan sliders).
//
////////////////////////////////////////////////////////////////////////////////

void PrintingPage::ConfigurePanSlider (DxuiSlider & slider, const RECT & rect)
{
    constexpr float  s_kPanMax = 100.0f;


    slider.SetRect      (rect);
    slider.SetRange     (-s_kPanMax, s_kPanMax);
    slider.SetStep      (5.0f);
    slider.SetShowTicks (true);
    slider.SetTickInterval (25.0f);
    slider.SetCenterOriginFill (true);
    slider.SetValueFormatter ([] (float v) -> std::wstring
    {
        std::wstring  result;
        int           pct = (int) std::lround (v);

        if (pct == 0)
        {
            result = L"Center";
        }
        else if (pct < 0)
        {
            result = std::to_wstring (-pct) + L"% L";
        }
        else
        {
            result = std::to_wstring (pct) + L"% R";
        }
        return result;
    });
}




////////////////////////////////////////////////////////////////////////////////
//
//  PrintingPage::ApplyEnabledState
//
//  Cascade the two nested gates: the master "Printer sound" toggle enables the
//  volume slider + manual-pan checkbox, and the pan slider is additionally
//  gated by the manual-pan checkbox (it auto-follows the preview when off).
//  Disabled controls dim their labels to match.
//
////////////////////////////////////////////////////////////////////////////////

void PrintingPage::ApplyEnabledState ()
{
    bool  soundsOn = (m_prefs != nullptr) && m_prefs->printerAudioEnabled;
    bool  panLive  = soundsOn && (m_prefs != nullptr) && m_prefs->printerAudioPanOverride;

    DxuiTextRole  childRole = soundsOn ? DxuiTextRole::Body : DxuiTextRole::Disabled;
    DxuiTextRole  panRole   = panLive  ? DxuiTextRole::Body : DxuiTextRole::Disabled;

    m_volume.SetEnabled      (soundsOn);
    m_panOverride.SetEnabled (soundsOn);
    m_pan.SetEnabled         (panLive);

    m_volumeLabel.SetTextRole (childRole);
    m_panLabel.SetTextRole    (panRole);
}
