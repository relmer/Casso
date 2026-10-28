#pragma once

#include "Pch.h"

#include "Machines/Apple2/Common/SscSwitches.h"
#include "Ui/Settings/SerialAttachmentView.h"
#include "Ui/Settings/SscSettingsModel.h"
#include "Ui/Settings/SscSwitchesView.h"

class DxuiHwndSource;
class IHostSerialPorts;
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

    // `forOutput` is true for the file the card sends into, false for the file sent into the card.
    using BrowseFn = SerialAttachmentView::BrowseFn;

    explicit SerialCardPage (std::wstring title = L"Serial card");

    void  SetState          (SettingsPanelState * state);
    void  SetPopupHost      (DxuiHwndSource * host);
    void  SetFirmwareStatus (const std::wstring & text, bool offerDownload);
    void  SetOnDownload     (std::function<void()> fn) { m_onDownload = std::move (fn); }
    void  SetOnBrowse       (BrowseFn fn)              { m_attachment.SetOnBrowse (std::move (fn)); }
    void  SetSerialPorts    (IHostSerialPorts * ports) { m_attachment.SetSerialPorts (ports); }

    void  Layout   (const RECT & rect, const DxuiDpiScaler & scaler) override;
    void  Rebuild  ();

    // Test accessors.
    int                   GetRowCount       () const { return static_cast<int> (m_rowSettings.size()); }
    SscSetting            GetRowSetting     (int row) const { return m_rowSettings[static_cast<size_t> (row)]; }
    DxuiComboBox        & GetRowDropdown    (int row) { return m_rowCombos[row]; }
    DxuiComboBox        & GetAttachDropdown ()        { return m_attachment.GetAttachDropdown(); }
    SerialAttachmentView & GetAttachment    ()        { return m_attachment; }
    SscSwitchesView     & GetSwitchesView   ()        { return m_switchesView; }
    const std::wstring  & GetRowNote        (int row) const { return m_rowNoteText[row]; }
    const std::wstring  & GetAttachNote     () const  { return m_attachment.GetNoteText(); }
    DxuiInfoTip         & GetRowInfoTip     (int row) { return m_rowTips[row]; }
    DxuiButton          & GetRestoreButton  ()        { return m_restore; }

private:
    static RECT          MakeRect          (int l, int t, int w, int h);

    IDxuiTextRenderer  * GetMeasuringRenderer () const;
    int                  GetLabelColumnPx     (const DxuiDpiScaler & scaler) const;
    int                  GetDropdownWidthPx   (const DxuiDpiScaler & scaler, int maxPx) const;

    SscSwitches  ReadStagedSwitches () const;
    void         StageSwitches      (const SscSwitches & switches);
    void         SyncSwitches       (const SscSwitches & switches);
    void         Relayout           ();
    void         OnRowPicked        (int row, int choice);
    void         OnAttachmentChanged (const SerialAttachmentChoice & choice);
    void         OnSwitchToggled    (SscBank bank, int number);
    void         OnRestoreDefaults  ();

    SettingsPanelState *     m_state     = nullptr;
    DxuiHwndSource *         m_popupHost = nullptr;
    std::function<void()>    m_onDownload;

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

    SerialAttachmentView     m_attachment { SerialAttachmentView::Kind::Card, L"Serial connector:" };

    DxuiButton               m_restore;

    bool                     m_hasLayout = false;
    RECT                     m_lastRect  = {};
    DxuiDpiScaler            m_lastScaler;
};
