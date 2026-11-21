#pragma once

#include "Pch.h"

#include "SettingsPanelState.h"
#include "Ui/Settings/SerialCardPage.h"



class DxuiHwndSource;





////////////////////////////////////////////////////////////////////////////////
//
//  SlotsPage
//
//  The settings sheet's "Slots" tab, on a machine with slots. The slots are
//  listed down the left, each with its card. The selected slot's card and
//  that card's options are on the right: the drives connected to a storage
//  card, a Super Serial Card's switches and connection, the Disk II's write
//  protection. Images are inserted and ejected from the drives themselves,
//  not here. Each edit takes a machine reset to apply.
//
////////////////////////////////////////////////////////////////////////////////

class SlotsPage : public DxuiPropertyPage
{
public:
    explicit SlotsPage (std::wstring title = L"Slots");

    static constexpr int     kFirstSlot = 1;
    static constexpr int     kLastSlot  = 7;
    static constexpr size_t  kMaxPorts  = 8;

    // Called with a card's device once the card is put in a slot.
    using CardChosenFn = std::function<void (const std::string & device)>;

    void  Layout          (const RECT & rect, const DxuiDpiScaler & scaler) override;
    void  SetState        (SettingsPanelState * state);
    void  SetPopupHost    (DxuiHwndSource * host);
    void  SetOnCardChosen (CardChosenFn fn) { m_onCardChosen = std::move (fn); }
    void  Rebuild         ();

    // The Super Serial Card's switches and connection, on the card's slot.
    SerialCardPage  & GetSerialPage () { return m_serial; }

    // The edits, exposed for unit tests.
    void                  SelectSlot       (int slot);
    void                  ChooseCard       (int choice);
    void                  ConfirmReplace   ();
    void                  CancelReplace    ();
    void                  ChoosePortDrive  (size_t port, int choice);
    void                  SetChainDrive    (size_t port, bool connected);
    int                   GetSelectedSlot  () const { return m_selectedSlot; }
    bool                  IsReplacePending () const { return m_replacePending; }
    const std::wstring  & GetStoragePrompt () const { return m_prompt; }
    DxuiComboBox        & GetCardDropdown  ()       { return m_cardDropdown; }
    size_t                GetPortRowCount  () const { return m_portRows; }
    DxuiComboBox        & GetPortDropdown  (size_t port) { return m_portDropdowns[port]; }
    const DxuiToggle    & GetPortToggle    (size_t port) const { return m_portToggles[port]; }

    // A slot's line in the list, "Slot 5: Apple II SCSI Card" or "Slot 2:
    // Empty". Exposed for unit tests.
    static std::wstring  MakeSlotLabel (const std::vector<HardwareEntry> & entries, int slot);

private:
    static RECT          MakeRect         (int l, int t, int w, int h);
    static std::wstring  Widen            (const std::string & narrow);
    static std::wstring  GetCardName      (const HardwareEntry & entry);
    int                  GetSelectedEntry () const;
    bool                 IsDiskIiSelected () const;
    bool                 IsSerialSelected () const;
    void                 RebuildList      ();
    void                 RebuildDetail    ();
    void                 ApplyCard        (const std::string & device);
    void                 OnEdited         (HRESULT hr, const std::wstring & error = std::wstring());
    void                 Relayout         ();

    SettingsPanelState            * m_state          = nullptr;
    CardChosenFn                    m_onCardChosen;
    RECT                            m_baseRect       = {};
    DxuiDpiScaler                   m_scaler;

    DxuiListView                          m_list;
    DxuiLabel                             m_cardLabel;
    DxuiComboBox                          m_cardDropdown;
    DxuiLabel                             m_promptLabel;
    DxuiButton                            m_replaceButton;
    DxuiButton                            m_cancelButton;
    DxuiCheckbox                          m_firmware;
    DxuiLabel                             m_drivesHeading;
    std::array<DxuiLabel,    kMaxPorts>   m_portLabels;
    std::array<DxuiComboBox, kMaxPorts>   m_portDropdowns;
    std::array<DxuiToggle,   kMaxPorts>   m_portToggles;
    std::array<DxuiCheckbox, kMaxPorts>   m_timingBoxes;
    DxuiLabel                             m_protectLabel;
    std::array<DxuiCheckbox, 2>           m_writeProtect;
    SerialCardPage                        m_serial;

    std::vector<SlotCardChoice>     m_cardChoices;
    std::wstring                    m_prompt;
    std::string                     m_pendingDevice;
    int                             m_selectedSlot   = kFirstSlot;
    size_t                          m_portRows       = 0;
    bool                            m_isChain        = false;
    bool                            m_replacePending = false;
};
