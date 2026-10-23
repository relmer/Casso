#pragma once

#include "Pch.h"

#include "Machines/Apple2/Common/SscSwitches.h"
#include "Ui/Settings/SscSettingsModel.h"
#include "Ui/Settings/SscSwitchesView.h"

class DxuiHwndSource;
class SettingsPanelState;





////////////////////////////////////////////////////////////////////////////////
//
//  SerialCardPage
//
//  Settings > Serial card: the Super Serial Card's firmware status, its DIP
//  switches as the settings they select (SscSettingsModel) and as a drawing
//  of the switches themselves, and what is plugged into its serial
//  connector. Present only while the staged hardware has the card turned on,
//  as the Storage page follows the disk controller.
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

    // What a connector attachment choice is, in drop-down order. After these
    // come a TCP port on this PC and then each of the PC's COM ports, as a
    // //c's serial port offers.
    static constexpr const char *  kAttachmentDevices[] = { "", "serial-loopback", "serial-file-out", "serial-file-in" };
    static constexpr int           kLoopbackChoice      = 1;
    static constexpr int           kFileOutChoice       = 2;
    static constexpr int           kFileInChoice        = 3;
    static constexpr int           kTcpChoice           = 4;

    // The card's TCP port is this plus its slot, so slot 2's is 6502, as the
    // //c's port 2's is.
    static constexpr uint16_t      kTcpBasePort         = 6500;

    // `forOutput` is true for the file the card sends into, false for the file sent into the card.
    using BrowseFn = std::function<bool (bool forOutput, std::string & inOutPath)>;

    explicit SerialCardPage (std::wstring title = L"Serial card");

    void  SetState          (SettingsPanelState * state);
    void  SetPopupHost      (DxuiHwndSource * host);
    void  SetFirmwareStatus (const std::wstring & text, bool offerDownload);
    void  SetOnDownload     (std::function<void()> fn) { m_onDownload = std::move (fn); }
    void  SetOnBrowse       (BrowseFn fn)              { m_onBrowse   = std::move (fn); }

    // Reports a change a switch made on another tab, such as the ImageWriter
    // II disconnected when the card is switched out of printer mode.
    void  SetOnNotice       (std::function<void (const std::wstring &)> fn) { m_onNotice = std::move (fn); }

    // The PC's COM ports, offered for the connector.
    void  SetComPorts       (std::vector<std::wstring> ports) { m_comPorts = std::move (ports); }

    // The attachments offered for a card in `slot`, as devices and as the
    // drop-down's labels. A current device that is none of them is added at
    // the end, so it is not lost.
    static void  BuildAttachments (int slot, const std::vector<std::wstring> & comPorts, const std::string & current,
                                   std::vector<std::wstring> & outLabels, std::vector<std::string> & outDevices);

    void  Layout   (const RECT & rect, const DxuiDpiScaler & scaler) override;
    void  Rebuild  ();

    // Test accessors.
    int                   GetRowCount       () const { return static_cast<int> (m_rowSettings.size()); }
    SscSetting            GetRowSetting     (int row) const { return m_rowSettings[static_cast<size_t> (row)]; }
    DxuiComboBox        & GetRowDropdown    (int row) { return m_rowCombos[row]; }
    DxuiComboBox        & GetAttachDropdown ()        { return m_attach; }
    SscSwitchesView     & GetSwitchesView   ()        { return m_switchesView; }
    const std::wstring  & GetRowNote        (int row) const { return m_rowNoteText[row]; }
    const std::wstring  & GetAttachNote     () const  { return m_attachNoteText; }
    const std::vector<string> & GetAttachDevices () const { return m_devices; }
    DxuiInfoTip         & GetRowInfoTip     (int row) { return m_rowTips[row]; }
    DxuiButton          & GetRestoreButton  ()        { return m_restore; }

private:
    static RECT          MakeRect          (int l, int t, int w, int h);
    static std::wstring  GetAttachmentNote (const std::string & device);

    std::string          GetAttachedDevice (int choice) const;

    IDxuiTextRenderer  * GetMeasuringRenderer () const;
    int                  GetLabelColumnPx     (const DxuiDpiScaler & scaler) const;
    int                  GetDropdownWidthPx   (const DxuiDpiScaler & scaler, int maxPx) const;

    SscSwitches  ReadStagedSwitches () const;
    void         StageSwitches      (const SscSwitches & switches);
    void         StageAttachment    (int choice, const std::string & path);
    void         SyncSwitches       (const SscSwitches & switches);
    void         Relayout           ();
    void         OnRowPicked        (int row, int choice);
    void         OnAttachPicked     (int choice);
    void         OnBrowse           ();
    void         OnSwitchToggled    (SscBank bank, int number);
    void         OnRestoreDefaults  ();

    SettingsPanelState *     m_state     = nullptr;
    DxuiHwndSource *         m_popupHost = nullptr;
    std::function<void()>    m_onDownload;
    BrowseFn                 m_onBrowse;
    std::function<void (const std::wstring &)>  m_onNotice;

    DxuiInfoBanner           m_firmwareBanner;
    DxuiButton               m_download;
    bool                     m_offerDownload = false;

    DxuiLabel                m_switchesHeading;
    DxuiLabel                m_rowLabels[kMaxRows];
    DxuiComboBox             m_rowCombos[kMaxRows];
    DxuiInfoTip              m_rowTips[kMaxRows];
    std::vector<SscSetting>  m_rowSettings;
    DxuiLabel                m_rowNotes[kMaxRows];
    std::wstring             m_rowNoteText[kMaxRows];
    SscSwitchesView          m_switchesView;

    DxuiLabel                m_attachLabel;
    DxuiComboBox             m_attach;
    std::vector<string>      m_devices;
    std::vector<wstring>     m_comPorts;
    DxuiLabel                m_attachNote;
    std::wstring             m_attachNoteText;
    DxuiLabel                m_pathLabel;
    DxuiLabel                m_pathValue;
    DxuiButton               m_browse;
    std::string              m_path;

    DxuiButton               m_restore;

    bool                     m_hasLayout = false;
    RECT                     m_lastRect  = {};
    DxuiDpiScaler            m_lastScaler;
};
