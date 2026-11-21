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
//  that card's options are on the right: a storage card's drives and their
//  images, a Super Serial Card's switches and connection, the Disk II's
//  write protection. Each edit takes a machine reset to apply.
//
////////////////////////////////////////////////////////////////////////////////

class SlotsPage : public DxuiPropertyPage
{
public:
    explicit SlotsPage (std::wstring title = L"Slots");

    static constexpr int     kFirstSlot = 1;
    static constexpr int     kLastSlot  = 7;
    static constexpr size_t  kMaxDrives = 8;

    // Opens a file dialog for the image of the drive `device` on a port,
    // starting at `inOutPath`; returns false when the dialog is canceled.
    using BrowseImageFn = std::function<bool (const std::string & device, std::string & inOutPath)>;

    // Called with a card's device once the card is put in a slot.
    using CardChosenFn  = std::function<void (const std::string & device)>;

    void  Layout           (const RECT & rect, const DxuiDpiScaler & scaler) override;
    void  SetState         (SettingsPanelState * state);
    void  SetPopupHost     (DxuiHwndSource * host);
    void  SetOnBrowseImage (BrowseImageFn fn) { m_onBrowseImage = std::move (fn); }
    void  SetOnCardChosen  (CardChosenFn fn)  { m_onCardChosen  = std::move (fn); }
    void  Rebuild          ();

    // The Super Serial Card's switches and connection, on the card's slot.
    SerialCardPage  & GetSerialPage () { return m_serial; }

    // The edits, exposed for unit tests.
    void                  SelectSlot       (int slot);
    void                  ChooseCard       (int choice);
    void                  ConfirmReplace   ();
    void                  CancelReplace    ();
    void                  AttachImage      (size_t port);
    void                  EjectImage       (size_t port);
    void                  AddDrive         ();
    void                  RemoveDrive      (size_t port);
    int                   GetSelectedSlot  () const { return m_selectedSlot; }
    bool                  IsReplacePending () const { return m_replacePending; }
    const std::wstring  & GetStoragePrompt () const { return m_prompt; }
    DxuiComboBox        & GetCardDropdown  ()       { return m_cardDropdown; }
    const DxuiTreeView  & GetSlotList      () const { return m_list; }
    size_t                GetDriveRowCount () const { return m_driveRows; }
    const DxuiButton    & GetEjectButton   (size_t port) const { return m_ejectButtons[port]; }
    const DxuiLabel     & GetDriveLabel    (size_t port) const { return m_driveLabels[port]; }

    // A slot's line in the list, "Slot 5: Apple II SCSI Card" or "Slot 2:
    // Empty". Exposed for unit tests.
    static std::wstring  MakeSlotLabel (const std::vector<HardwareEntry> & entries, int slot);

private:
    static RECT          MakeRect         (int l, int t, int w, int h);
    static std::wstring  Widen            (const std::string & narrow);
    static std::wstring  GetCardName      (const HardwareEntry & entry);
    int                  GetSelectedEntry () const;
    bool                 IsChainSelected  () const;
    bool                 IsDiskIiSelected () const;
    bool                 IsSerialSelected () const;
    void                 RebuildList      ();
    void                 RebuildDetail    ();
    void                 ApplyCard        (const std::string & device);
    void                 OnEdited         (HRESULT hr, const std::wstring & error = std::wstring());
    void                 Relayout         ();

    SettingsPanelState            * m_state          = nullptr;
    BrowseImageFn                   m_onBrowseImage;
    CardChosenFn                    m_onCardChosen;
    RECT                            m_baseRect       = {};
    DxuiDpiScaler                   m_scaler;

    DxuiTreeView                          m_list;
    DxuiLabel                             m_cardLabel;
    DxuiComboBox                          m_cardDropdown;
    DxuiLabel                             m_promptLabel;
    DxuiButton                            m_replaceButton;
    DxuiButton                            m_cancelButton;
    DxuiCheckbox                          m_firmware;
    DxuiLabel                             m_drivesHeading;
    std::array<DxuiLabel,    kMaxDrives>  m_driveLabels;
    std::array<DxuiButton,   kMaxDrives>  m_changeButtons;
    std::array<DxuiButton,   kMaxDrives>  m_ejectButtons;
    std::array<DxuiButton,   kMaxDrives>  m_removeButtons;
    std::array<DxuiCheckbox, kMaxDrives>  m_timingBoxes;
    DxuiButton                            m_addButton;
    DxuiLabel                             m_protectLabel;
    std::array<DxuiCheckbox, 2>           m_writeProtect;
    SerialCardPage                        m_serial;

    std::vector<SlotCardChoice>     m_cardChoices;
    std::wstring                    m_prompt;
    std::string                     m_pendingDevice;
    int                             m_selectedSlot   = kFirstSlot;
    size_t                          m_driveRows      = 0;
    bool                            m_replacePending = false;
};
