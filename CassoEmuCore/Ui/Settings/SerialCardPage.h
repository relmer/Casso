#pragma once

#include "Pch.h"

#include "Machines/Apple2/Common/SscSwitches.h"
#include "Ui/Settings/SscSettingsModel.h"

class DxuiHwndSource;
class SettingsPanelState;





////////////////////////////////////////////////////////////////////////////////
//
//  SerialCardPage
//
//  Settings > Serial card: the Super Serial Card's firmware status, its DIP
//  switches as the settings they select (SscSettingsModel), and what is
//  plugged into its serial connector. Present only while the staged hardware
//  has the card turned on, as the Storage page follows the disk controller.
//
//  Edits are staged in SettingsPanelState. On OK the switches are applied to the
//  running card without a reset; a change of attachment rebuilds the
//  machine, since the attachment is opened when the machine is built.
//
//  The firmware download and the file pickers are the shell's, so the
//  sheet supplies them as callbacks.
//
////////////////////////////////////////////////////////////////////////////////

class SerialCardPage : public DxuiPropertyPage
{
public:
    static constexpr int  kMaxRows = 8;

    // What a connector attachment choice is, in drop-down order.
    static constexpr const char *  kAttachmentDevices[] = { "", "serial-loopback", "serial-file-out", "serial-file-in" };
    static constexpr int           kFileOutChoice       = 2;
    static constexpr int           kFileInChoice        = 3;

    // `forOutput` is true for the file the card sends into, false for the file sent into the card.
    using BrowseFn = std::function<bool (bool forOutput, std::string & inOutPath)>;

    explicit SerialCardPage (std::wstring title = L"Serial card");

    void  SetState          (SettingsPanelState * state);
    void  SetPopupHost      (DxuiHwndSource * host);
    void  SetFirmwareStatus (const std::wstring & text, bool offerDownload);
    void  SetOnDownload     (std::function<void()> fn) { m_onDownload = std::move (fn); }
    void  SetOnBrowse       (BrowseFn fn)              { m_onBrowse   = std::move (fn); }

    void  Layout   (const RECT & rect, const DxuiDpiScaler & scaler) override;
    void  Rebuild  ();

    // Test accessors.
    int                   GetRowCount       () const { return static_cast<int> (m_rowSettings.size()); }
    SscSetting            GetRowSetting     (int row) const { return m_rowSettings[static_cast<size_t> (row)]; }
    DxuiComboBox        & GetRowDropdown    (int row) { return m_rowCombos[row]; }
    DxuiComboBox        & GetAttachDropdown ()        { return m_attach; }
    const std::wstring  & GetReadoutText    (SscBank bank) const;

private:
    static RECT  MakeRect (int l, int t, int w, int h);

    SscSwitches  ReadStagedSwitches () const;
    void         StageSwitches      (const SscSwitches & switches);
    void         StageAttachment    (int choice, const std::string & path);
    void         SyncReadout        (const SscSwitches & switches);
    void         Relayout           ();
    void         OnRowPicked        (int row, int choice);
    void         OnAttachPicked     (int choice);
    void         OnBrowse           ();

    SettingsPanelState *     m_state = nullptr;
    std::function<void()>    m_onDownload;
    BrowseFn                 m_onBrowse;

    DxuiInfoBanner           m_firmwareBanner;
    DxuiButton               m_download;
    bool                     m_offerDownload = false;

    DxuiLabel                m_switchesHeading;
    DxuiLabel                m_rowLabels[kMaxRows];
    DxuiComboBox             m_rowCombos[kMaxRows];
    std::vector<SscSetting>  m_rowSettings;
    DxuiLabel                m_readoutSw1;
    DxuiLabel                m_readoutSw2;
    std::wstring             m_readoutTextSw1;
    std::wstring             m_readoutTextSw2;

    DxuiLabel                m_attachLabel;
    DxuiComboBox             m_attach;
    DxuiLabel                m_pathLabel;
    DxuiLabel                m_pathValue;
    DxuiButton               m_browse;
    std::string              m_path;

    bool                     m_hasLayout = false;
    RECT                     m_lastRect  = {};
    DxuiDpiScaler            m_lastScaler;
};
