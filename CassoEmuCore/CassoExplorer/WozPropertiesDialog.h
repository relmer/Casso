#pragma once

#include "Pch.h"

#include "CassoExplorer/AppleFilePropertiesDialog.h"
#include "CassoExplorer/Model/WozProperties.h"
#include "Machines/Apple2/Common/WozLoader.h"

class WozPropertiesPage;





////////////////////////////////////////////////////////////////////////////////
//
//  WozPropertiesDialog
//
//  Disk properties: a WOZ image's INFO fields on one tab and its META on the
//  other. Both are saved together, by OK or Apply, into the image in place.
//
////////////////////////////////////////////////////////////////////////////////

class WozPropertiesDialog : public DxuiPropertySheet
{
public:
    //  Saves the edits, returning the reason it failed, or nothing.
    using ApplyFn = std::function<std::wstring (const WozLoader::Edits & edits)>;

    static constexpr int  kInfoTab = 0;
    static constexpr int  kMetaTab = 1;

    static void  Show (HWND owner, const IDxuiTheme * theme, const std::wstring & leafName, const WozLoader::Description & woz, int tab, ApplyFn apply);

protected:
    void  OnBuildPages () override;

private:
    //  A META key typed as text: a standard key with no list of values, or
    //  any key the image has that the reference doesn't define. A list's
    //  values have a field each, with a blank one after them to add another.
    struct MetaText
    {
        std::string                                   key;
        DxuiLabel                                     label;
        std::vector<std::unique_ptr<DxuiTextInput>>   values;
    };

    using Machines = std::array<DxuiCheckbox, WozProperties::kMachineCount>;

    static constexpr int     kWidthDip       = 640;
    static constexpr int     kHeightDip      = 620;
    static constexpr int     kMinHeightDip   = 360;
    static constexpr int     kComboWidthDip  = 260;
    static constexpr int     kMachineDip     = 64;
    static constexpr int     kWideMachineDip = 128;
    static constexpr int     kDateWidthDip   = 260;
    static constexpr int     kPickWidthDip   = 32;

    static constexpr const wchar_t *  kNotGiven     = L"Not given";
    static constexpr size_t           kMinListFields = 2;

    void                 BuildInfoPage   ();
    void                 BuildMetaPage   ();
    void                 AddMachineRows  (std::vector<AppleFilePropertiesPanel::Row> & rows, DxuiLabel & label, Machines & boxes, uint16_t checked);
    void                 SetUpLabel      (DxuiLabel & label, const std::wstring & text);
    void                 AddMetaText     (std::vector<AppleFilePropertiesPanel::Row> & rows, const std::string & key, const std::wstring & label, bool isList);
    void                 AddImageDate    (std::vector<AppleFilePropertiesPanel::Row> & rows);
    void                 PickImageDate   ();
    bool                 TryGetImageDate (std::string & outValue) const;
    void                 SetUpInput      (DxuiTextInput & input, const std::wstring & text);
    void                 SetUpChoices    (DxuiComboBox & combo, std::vector<std::wstring> & items, const std::wstring & value);
    void                 MarkEdited      ();
    bool                 TrySave         ();
    std::wstring         GetMetaValue    (const std::string & key) const;
    static uint16_t      GetChecked      (const Machines & boxes);
    static std::wstring  ToLines         (std::wstring value);

    const IDxuiTheme                        * m_theme          = nullptr;
    WozLoader::Description                    m_woz;
    ApplyFn                                   m_apply;
    WozPropertiesPage                       * m_infoPage       = nullptr;
    WozPropertiesPage                       * m_metaPage       = nullptr;
    std::vector<uint16_t>                     m_ramSizes;
    std::vector<std::wstring>                 m_languages;
    std::vector<std::wstring>                 m_metaRams;
    bool                                      m_machinesEdited = false;
    std::vector<std::unique_ptr<MetaText>>    m_metaTexts;

    DxuiLabel                     m_formatLabel;
    DxuiLabel                     m_format;
    DxuiLabel                     m_creatorLabel;
    DxuiLabel                     m_creator;
    DxuiCheckbox                  m_writeProtected { L"Write-protected" };
    DxuiLabel                     m_bootLabel;
    DxuiComboBox                  m_boot;
    DxuiLabel                     m_hardwareLabel;
    Machines                      m_hardware;
    DxuiLabel                     m_ramLabel;
    DxuiComboBox                  m_ram;
    DxuiLabel                     m_version1Note;
    DxuiLabel                     m_languageLabel;
    DxuiComboBox                  m_language;
    DxuiLabel                     m_metaRamLabel;
    DxuiComboBox                  m_metaRam;
    DxuiLabel                     m_machinesLabel;
    Machines                      m_machines;
    DxuiLabel                     m_imageDateLabel;
    DxuiTextInput                 m_imageDate;
    DxuiIconButton                m_imageDatePick;
    DxuiCalendar                  m_calendar;
    std::wstring                  m_imageDateShown;   // the image's date as the field first had it
};
