#pragma once

#include "Pch.h"

#include "CassoExplorer/Model/CassoExplorerPrefs.h"
#include "Config/CrtTypes.h"
#include "Ui/Settings/ColorPickerOverlay.h"
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
//  CassoExplorerCassoLinkPage
//
//  Whether CassoLink serves ADTPro's client, and the folder it serves.
//
////////////////////////////////////////////////////////////////////////////////

class CassoExplorerCassoLinkPage : public DxuiPropertyPage
{
public:
    explicit CassoExplorerCassoLinkPage (std::wstring title) : DxuiPropertyPage (std::move (title)) {}

    void  Init    (DxuiCheckbox & emulate, DxuiLabel & folderLabel, DxuiTextInput & folder, DxuiLabel & note);
    void  Layout  (const RECT & boundsDip, const DxuiDpiScaler & scaler) override;
    bool  OnMouse (const DxuiMouseEvent & ev) override;

    static constexpr int  kNoteHeightDip = 60;

private:
    DxuiCheckbox   * m_emulate     = nullptr;
    DxuiLabel      * m_folderLabel = nullptr;
    DxuiTextInput  * m_folder      = nullptr;
    DxuiLabel      * m_note        = nullptr;
};





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerOptionsDialog
//
//  CassoExplorer's settings: the form a copy out of an image takes on the
//  host, Casso's file types, and, on Casso's own Display page, the monitor
//  and effects picture previews are drawn with, and the custom monitor's
//  color, picked with Casso's color picker. A Display change is drawn in
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

        //  Whether CassoLink serves ADTPro's client, and from which folder;
        //  an empty one is Documents\ADTPro.
        bool          emulateAdtPro = true;
        std::wstring  adtProFolder;

        //  The preview's monitor, as a PicturePreview::Monitor, the custom
        //  monitor's color, and the effects changed on each monitor, by its
        //  name in CassoExplorerPrefs::kMonitorNames.
        int                                  monitor    = 1;
        uint32_t                             customArgb = PicturePreview::kDefaultCustomArgb;
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


    //  Runs the dialog modally over `owner`. False when it was canceled or
    //  could not open; `inOutChoices` changes only on OK.
    static bool  Ask (HWND owner, const IDxuiTheme * theme, const DisplayHost & display, Choices & inOutChoices);

protected:
    void     OnBuildPages () override;
    HRESULT  OnOk         () override;
    void     OnCancel     () override;

    //  The color picker, modal over the sheet while it is open.
    void     Layout            (const RECT & boundsPx, const DxuiDpiScaler & scaler) override;
    bool     HasModalOverlay   () const override;
    void     PaintModalOverlay (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme) override;
    bool     OnOverlayMouse    (const DxuiMouseEvent & ev) override;
    bool     OnOverlayChar     (wchar_t ch) override;
    bool     OnOverlayKey      (WPARAM vk) override;
    LPCWSTR  GetCursorForPoint (POINT clientPx) const override;

private:
    CrtOverrides &  GetOverrides    ();
    void            SeedDisplay     ();
    void            WireDisplay     ();
    void            WireColorPicker ();
    void            SetCustomColor  (uint32_t argb);
    void            PreviewChoices  ();

    const IDxuiTheme            * m_theme         = nullptr;
    DisplayHost                   m_display;
    Choices                       m_original;
    Choices                       m_choices;
    bool                          m_confirmed     = false;
    DxuiLabel                     m_namingLabel;
    DxuiComboBox                  m_naming;
    DxuiCheckbox                  m_register;
    DxuiCheckbox                  m_emulateAdtPro;
    DxuiLabel                     m_adtProLabel;
    DxuiTextInput                 m_adtProFolder;
    DxuiLabel                     m_adtProNote;
    CassoExplorerCassoLinkPage  * m_linkPage      = nullptr;
    ColorPickerOverlay            m_colorPicker;
    CassoExplorerGeneralPage    * m_general       = nullptr;
    DisplayPage                 * m_displayPage   = nullptr;
};
