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
//  Automatic preview, its description below its box, then the disk image
//  types.
//
////////////////////////////////////////////////////////////////////////////////

class CassoExplorerGeneralPage : public DxuiPropertyPage
{
public:
    explicit CassoExplorerGeneralPage (std::wstring title) : DxuiPropertyPage (std::move (title)) {}

    void  Init    (DxuiLabel & previewNote, DxuiCheckbox & autoPreview, DxuiCheckbox & registration);
    void  Layout  (const RECT & boundsDip, const DxuiDpiScaler & scaler) override;
    bool  OnMouse (const DxuiMouseEvent & ev) override;

    static constexpr int  kNoteHeightDip = 40;
    static constexpr int  kNoteIndentDip = 22;   // under the checkbox's label

private:
    DxuiLabel     * m_previewNote = nullptr;
    DxuiCheckbox  * m_autoPreview = nullptr;
    DxuiCheckbox  * m_register    = nullptr;
};





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerHostNamesPage
//
//  The host file name style: its description above its choices.
//
////////////////////////////////////////////////////////////////////////////////

class CassoExplorerHostNamesPage : public DxuiPropertyPage
{
public:
    explicit CassoExplorerHostNamesPage (std::wstring title) : DxuiPropertyPage (std::move (title)) {}

    void  Init    (DxuiLabel & namingNote, DxuiRadioGroup & naming);
    void  Layout  (const RECT & boundsDip, const DxuiDpiScaler & scaler) override;
    bool  OnMouse (const DxuiMouseEvent & ev) override;

    static constexpr int  kNoteHeightDip         = 124;
    static constexpr int  kOptionLabelHeightDip  = 20;
    static constexpr int  kOptionDescHeightDip   = 18;
    static constexpr int  kOptionGapDip          = 10;

private:
    DxuiRadioGroup  * m_naming     = nullptr;
    DxuiLabel       * m_namingNote = nullptr;
};





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerFullSendPage
//
//  A full send's folder, with its Browse button, and its image format. A full
//  send starts from the Apple's keyboard, so these are set here ahead of time.
//
////////////////////////////////////////////////////////////////////////////////

class CassoExplorerFullSendPage : public DxuiPropertyPage
{
public:
    explicit CassoExplorerFullSendPage (std::wstring title) : DxuiPropertyPage (std::move (title)) {}

    void  Init    (DxuiLabel & note, DxuiLabel & folder, DxuiButton & browse, DxuiRadioGroup & format);
    void  Layout  (const RECT & boundsDip, const DxuiDpiScaler & scaler) override;
    bool  OnMouse (const DxuiMouseEvent & ev) override;

    static constexpr int  kNoteHeightDip   = 60;
    static constexpr int  kBrowseWidthDip  = 100;
    static constexpr int  kOptionHeightDip = 26;
    static constexpr int  kOptionCount     = 6;

private:
    DxuiLabel       * m_note   = nullptr;
    DxuiLabel       * m_folder = nullptr;
    DxuiButton      * m_browse = nullptr;
    DxuiRadioGroup  * m_format = nullptr;
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
        //  An index into GetNamingOptions.
        int   hostNaming = 0;

        //  Whether Casso is the program for its disk image types, and whether
        //  it runs from an installed package, whose manifest declares them.
        bool  registered = false;
        bool  packaged   = false;

        //  Whether the preview pane opens and closes for the selected file.
        bool  autoPreview = true;

        //  A full send's folder, and its extension: empty for the default.
        std::wstring  fullSendFolder;
        std::string   fullSendExtension;


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

        //  The folder picker over the dialog; ioFolder changes when one is picked.
        std::function<void (HWND owner, std::wstring & ioFolder)>  pickFolder;
    };

    //  The host file name forms, in the order the setting stores them.
    static std::vector<DxuiRadioOption>  GetNamingOptions ();


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
    void            WireFullSend    ();
    void            SetCustomColor  (uint32_t argb);
    void            PreviewChoices  ();

    const IDxuiTheme            * m_theme         = nullptr;
    DisplayHost                   m_display;
    Choices                       m_original;
    Choices                       m_choices;
    bool                          m_confirmed     = false;
    DxuiRadioGroup                m_naming;
    DxuiLabel                     m_namingNote;
    DxuiLabel                     m_previewNote;
    DxuiCheckbox                  m_autoPreview;
    DxuiCheckbox                  m_register;
    DxuiLabel                     m_fullSendNote;
    DxuiLabel                     m_fullSendFolder;
    DxuiButton                    m_fullSendBrowse;
    DxuiRadioGroup                m_fullSendFormat;
    CassoExplorerFullSendPage   * m_fullSend      = nullptr;
    ColorPickerOverlay            m_colorPicker;
    CassoExplorerGeneralPage    * m_general       = nullptr;
    CassoExplorerHostNamesPage  * m_hostNames     = nullptr;
    DisplayPage                 * m_displayPage   = nullptr;
};
