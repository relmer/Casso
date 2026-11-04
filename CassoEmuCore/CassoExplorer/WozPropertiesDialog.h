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
    //  A META key typed as text: a standard key with no list of values, or a
    //  custom key, one the reference doesn't define, with its key in a
    //  separate field and a button that deletes it. A standard list's values have
    //  a field each, with a blank one after them to add another. The message
    //  under them is why they can't be saved.
    struct MetaText
    {
        std::string                                   key;
        bool                                          custom = false;
        DxuiLabel                                     label;
        DxuiTextInput                                 keyInput;
        DxuiIconButton                                remove;
        std::vector<std::unique_ptr<DxuiTextInput>>   values;
        DxuiFieldError                                error;
    };

    using Machines = std::array<DxuiCheckbox, WozProperties::kMachineCount>;

    static constexpr int     kWidthDip       = 640;
    static constexpr int     kHeightDip      = 620;
    static constexpr int     kMinHeightDip   = 360;
    static constexpr int     kComboWidthDip  = 260;
    static constexpr int     kMachineDip     = 64;
    static constexpr int     kWideMachineDip = 128;
    static constexpr int     kDateWidthDip   = 150;
    static constexpr int     kPickWidthDip   = 32;
    static constexpr int     kClockWidthDip  = 60;
    static constexpr int     kKeyWidthDip    = AppleFilePropertiesPanel::kLabelWidthDip - AppleFilePropertiesPanel::kFieldGapDip;
    static constexpr int     kValueWidthDip  = 420;
    static constexpr int     kErrorDip       = 22;
    static constexpr int     kMinutesPerHour = 60;
    static constexpr int     kHoursPerHalf   = 12;
    static constexpr int     kHoursPerDay    = 24;

    static constexpr const wchar_t *  kNotGiven      = L"Not given";
    static constexpr size_t           kMinListFields = 2;

    void                 BuildInfoPage   ();
    void                 BuildMetaPage   ();
    void                 AddMachineRows  (std::vector<AppleFilePropertiesPanel::Row> & rows, DxuiLabel & label, Machines & boxes, uint16_t checked);
    void                 SetUpLabel      (DxuiLabel & label, const std::wstring & text);
    void                 AddMetaText     (std::vector<AppleFilePropertiesPanel::Row> & rows, const std::string & key, const std::wstring & label, bool isList);
    MetaText           & AddCustomText   (const std::string & key);
    void                 AddImageDate    (std::vector<AppleFilePropertiesPanel::Row> & rows);
    void                 SetUpClock      (DxuiComboBox & combo, std::vector<std::wstring> items, int selected);
    void                 PickImageDate   ();
    bool                 TryGetImageDate (std::string & outValue) const;
    bool                 TryGetTypedTime (SYSTEMTIME & outLocal) const;
    void                 SetMetaRows     ();
    void                 AddCustomField  ();
    void                 RemoveCustom    (MetaText * text);
    void                 Validate        ();
    std::wstring         CheckCustom     (const MetaText & text) const;
    static std::wstring  JoinLines       (const MetaText & text);
    static std::wstring  CheckValues     (const MetaText & text);
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
    DxuiComboBox                  m_hour;
    DxuiComboBox                  m_minute;
    DxuiComboBox                  m_second;
    DxuiComboBox                  m_dayHalf;
    DxuiFieldError                m_imageDateError;
    DxuiCalendar                  m_calendar;
    bool                          m_clock24       = false;
    bool                          m_hadDate       = false;   // the image had a date, at m_shownLocal
    SYSTEMTIME                    m_shownLocal    = {};

    std::vector<AppleFilePropertiesPanel::Row>    m_standardRows;
    std::vector<std::unique_ptr<MetaText>>        m_removedTexts;   // deleted custom fields, destroyed when the window closes
    DxuiLabel                                     m_customHeading;
    DxuiButton                                    m_addField { L"Add field" };
    DxuiFieldValidator                            m_validator;
};
