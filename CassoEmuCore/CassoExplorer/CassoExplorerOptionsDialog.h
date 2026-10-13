#pragma once

#include "Pch.h"

#include "Config/CrtTypes.h"
#include "Ui/ThemeLoader.h"


class DisplayPage;





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerGeneralPage
//
//  Each setting a label over its control, down the page.
//
////////////////////////////////////////////////////////////////////////////////

class CassoExplorerGeneralPage : public DxuiPropertyPage
{
public:
    explicit CassoExplorerGeneralPage (std::wstring title) : DxuiPropertyPage (std::move (title)) {}

    void  Init    (DxuiLabel & namingLabel, DxuiComboBox & naming, DxuiCheckbox & registration);
    void  Layout  (const RECT & boundsDip, const DxuiDpiScaler & scaler) override;
    bool  OnMouse (const DxuiMouseEvent & ev) override;

    static constexpr int  kLabelHeightDip = 22;
    static constexpr int  kGapDip         = 6;
    static constexpr int  kFieldHeightDip = 30;

private:
    DxuiLabel     * m_namingLabel = nullptr;
    DxuiComboBox  * m_naming      = nullptr;
    DxuiCheckbox  * m_register    = nullptr;
};





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerOptionsDialog
//
//  CassoExplorer's settings: the form a copy out of an image takes on the
//  host, Casso's file types, and, on Casso's own Display page, the monitor
//  and effects picture previews are drawn with. A Display change is drawn in
//  the preview as it is made; OK saves every choice, and Cancel, Escape and
//  the close box put them all back.
//
////////////////////////////////////////////////////////////////////////////////

class CassoExplorerOptionsDialog : public DxuiPropertySheet
{
public:
    struct Choices
    {
        //  An index into GetNamingLabels.
        int   hostNaming = 0;

        //  Whether Casso is the program for its disk image types.
        bool  registered = false;

        //  The preview's monitor, as an index in Casso's order, and the
        //  effects changed on each monitor, by its name.
        int                                  monitor = 1;
        std::map<std::string, CrtOverrides>  crtOverrides;
    };

    //  What the Display page draws on: the theme's effects, which may be
    //  null, and where each change goes to be drawn in the preview.
    struct DisplayHost
    {
        const ThemeCrtDefaults *                themeDefaults = nullptr;
        std::function<void (const Choices &)>  preview;
    };

    //  The host file name forms, in the order the setting stores them.
    static std::vector<std::wstring>  GetNamingLabels ();

    //  Each monitor's name as Casso writes it, in Casso's order.
    static constexpr const char *  kMonitorNames[] = { "color", "green", "amber", "white" };

    //  Runs the dialog modally over `owner`. False when it was canceled or
    //  could not open; `inOutChoices` changes only on OK.
    static bool  Ask (HWND owner, const IDxuiTheme * theme, const DisplayHost & display, Choices & inOutChoices);

protected:
    void     OnBuildPages () override;
    HRESULT  OnOk         () override;
    void     OnCancel     () override;

private:
    CrtOverrides &  GetOverrides   ();
    void            SeedDisplay    ();
    void            WireDisplay    ();
    void            PreviewChoices ();

    const IDxuiTheme          * m_theme       = nullptr;
    DisplayHost                 m_display;
    Choices                     m_original;
    Choices                     m_choices;
    bool                        m_confirmed   = false;
    DxuiLabel                   m_namingLabel;
    DxuiComboBox                m_naming;
    DxuiCheckbox                m_register;
    CassoExplorerGeneralPage  * m_general     = nullptr;
    DisplayPage               * m_displayPage = nullptr;
};
