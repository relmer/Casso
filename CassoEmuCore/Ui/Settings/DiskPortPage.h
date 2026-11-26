#pragma once

#include "Pch.h"

#include "Core/MachineConfig.h"
#include "SettingsPanelState.h"



class DxuiHwndSource;





////////////////////////////////////////////////////////////////////////////////
//
//  DiskPortPage
//
//  The settings sheet's "Disk port" tab, on a //c or //c Plus: the drives
//  plugged into the back of the machine, one row each, in chain order. Each
//  row's dropdown offers its drive's kind and the kinds that would fit in its
//  place, and its Remove button unplugs it. A last row adds a drive, and is
//  left out when the port is full. The list is sorted into chain order after
//  every edit. Images are inserted from the drives themselves, not here.
//
////////////////////////////////////////////////////////////////////////////////

class DiskPortPage : public DxuiPropertyPage
{
public:
    explicit DiskPortPage (std::wstring title = L"Disk port");

    static constexpr size_t  kMaxRows = MachineConfigLoader::kMaxChainDrives;

    void  Layout       (const RECT & rect, const DxuiDpiScaler & scaler) override;
    void  SetState     (SettingsPanelState * state);
    void  SetPopupHost (DxuiHwndSource * host);
    void  Rebuild      ();

    // The edits, exposed for unit tests.
    void                  ChooseDrive    (size_t row, int choice);
    void                  RemoveDrive    (size_t row);
    void                  AddDrive       (int choice);
    size_t                GetRowCount    () const { return m_drives.size(); }
    bool                  IsAddRowShown  () const { return m_addDropdown.IsVisible(); }
    DxuiComboBox        & GetRowDropdown (size_t row) { return m_rowDropdowns[row]; }
    DxuiComboBox        & GetAddDropdown ()           { return m_addDropdown; }
    const std::wstring  & GetSubtitle    () const { return m_subtitle.GetText(); }

    // A drive kind's name on the page, "UniDisk 3.5". Exposed for unit tests.
    static std::wstring  GetDriveLabel (const std::string & device);

private:
    static RECT               MakeRect      (int l, int t, int w, int h);
    std::vector<std::string>  GetRowChoices (size_t row) const;
    std::vector<std::string>  GetAddChoices () const;
    void                      SetDrives     (std::vector<std::string> drives);
    void                      Relayout      ();

    SettingsPanelState            * m_state    = nullptr;
    RECT                            m_baseRect = {};
    DxuiDpiScaler                   m_scaler;

    DxuiLabel                                 m_heading;
    DxuiLabel                                 m_subtitle;
    std::array<DxuiComboBox, kMaxRows>        m_rowDropdowns;
    std::array<DxuiButton,   kMaxRows>        m_removeButtons;
    DxuiComboBox                              m_addDropdown;

    std::vector<std::string>                  m_drives;
};
