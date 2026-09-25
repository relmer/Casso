#pragma once

#include "Pch.h"

#include "Ui/Settings/ControllerReadoutViews.h"
#include "Ui/Settings/ControllersPageState.h"
#include "Ui/Settings/JoyportSwitchView.h"
#include "Ui/Settings/ProfileDialogOverlay.h"

#include "Window/DxuiPropertyPage.h"
#include "Widgets/DxuiButton.h"
#include "Widgets/DxuiCheckbox.h"
#include "Widgets/DxuiComboBox.h"
#include "Widgets/DxuiLabel.h"
#include "Widgets/DxuiSlider.h"


class DxuiHwndSource;





////////////////////////////////////////////////////////////////////////////////
//
//  ControllersPage
//
//  Settings > Controllers: how each attached controller drives the game port.
//  The page holds no settings of its own. It shows a ControllersPageState and
//  forwards every edit to it; the sheet's apply pipeline commits that state on
//  OK and reverts it on Cancel.
//
//      * Controller         (DxuiComboBox: every attached controller)
//      * Profile            (DxuiComboBox: the model's profiles, with New,
//                            Rename and Delete)
//      * Joystick           (a circle with a dot where the stick is, PDL0 and
//                            PDL1 labeled, beside a row per mapping for each)
//      * Buttons            (PB0 .. PB2: a light, and a row per mapping)
//      * Deadzone, Calibration, Reset profile
//
//  ONE DROP-DOWN PER MAPPING. It shows the control assigned, and lists "Press
//  to assign...", "None" and the controller's controls. A target takes more
//  than one control through "+", which adds a row; "None" on an added row
//  removes it. The first row always stays, showing "None" when the target is
//  unassigned, so every target keeps its place on the page.
//
//  The page is polled each dialog tick with the controller's latest reading,
//  which is what drives press-to-assign, the Calibrate steps, the stick and
//  the lights.
//
////////////////////////////////////////////////////////////////////////////////

class ControllersPage : public DxuiPropertyPage
{
public:

    static constexpr size_t  kTargetCount = 5;
    static constexpr size_t  kAxisCount   = 2;
    static constexpr size_t  kButtonCount = 3;
    static constexpr size_t  kMaxRows     = 4;
    static constexpr size_t  kPlayerCount = MultiplayerSetup::kPlayerCount;

    using SampleSource = std::function<std::optional<ControllerSample> (const ControllerUnitKey &)>;
    using InspectFn    = std::function<void (const std::optional<ControllerUnitKey> &)>;

    explicit ControllersPage (std::wstring title = L"Controllers");

    void  SetState         (ControllersPageState * state);
    void  SetSampleSource  (SampleSource source);
    void  SetOnInspect     (InspectFn onInspect);
    void  SetPopupHost     (DxuiHwndSource * host);

    // Where Save on the profile-switch prompt commits the edited model.
    void  SetOnCommitProfile (ControllersPageState::CommitFn onCommit) { m_onCommitProfile = std::move (onCommit); }

    // Whether the running machine has the Sirius Joyport attached. While it
    // does, the stick and the button lights give way to the five switch
    // lights the Joyport reads.
    void  SetJoyportAttachedFn (std::function<bool()> isAttached) { m_isJoyportAttached = std::move (isAttached); }

    void  Layout           (const RECT & rect, const DxuiDpiScaler & scaler) override;

    // Each dialog tick: feed the controller's latest reading to the capture,
    // the calibration, the stick and the lights.
    void  Poll             ();

    // Re-sync every widget from the state.
    void  Refresh          ();

    // Lay the page out again and rebuild the tab order: what the page shows
    // changed shape, not just its values -- the multiplayer section coming or
    // going with the machine's mode, say.
    void  Relayout         ();

    // In multiplayer, moves Editing to player one's controller, asking about
    // unsaved profile edits first. Nothing happens outside multiplayer or
    // with player one's slot empty.
    void  FollowPlayerOne  ();

    // New... from the paddle picker's Profiles submenu: the New Profile dialog
    // for the controller being edited, after asking about unsaved edits.
    void  StartNewProfile  () { OnNewProfile(); }

    // Press-to-assign in progress, for the sheet's prompt over the page: the
    // sentence it shows, and a way to call the wait off.
    bool          IsCapturing        () const;
    std::wstring  GetCapturePrompt   () const;
    void          CancelCapture      ();

    // The page's rows came or went, which changes what Tab reaches.
    void          SetOnLayoutChanged (std::function<void ()> onLayoutChanged);

    // The profile dialog, for the sheet to show over the page and route input
    // to while it is open.
    bool                    IsProfileDialogOpen () const { return m_profileDialog.IsOpen(); }
    ProfileDialogOverlay &  GetProfileDialog    ()       { return m_profileDialog; }

private:

    // One control a row's drop-down offers, after "Press to assign..." and
    // "None".
    struct ControlChoice
    {
        bool       isPair = false;
        ControlId  control;
        ControlId  positive;
    };

    static constexpr int  kPressToAssignItem = 0;
    static constexpr int  kNoneItem          = 1;
    static constexpr int  kFirstControlItem  = 2;

    static RECT          MakeRect           (int l, int t, int w, int h);
    static PaddleTarget  TargetAt           (size_t index);
    static std::wstring  DescribeAxis       (ControllerKind kind, const AxisBinding & binding);
    static std::wstring  DescribeButton     (ControllerKind kind, const ButtonBinding & binding);
    static std::wstring  Utf8ToWide         (const std::string & text);
    static std::string   WideToUtf8         (const std::wstring & text);

    size_t               GetBindingCount    (size_t target) const;
    size_t               GetShownRows       (size_t target) const;
    int                  FindChoice         (size_t target, size_t row) const;

    void                 RebuildChoices     ();
    void                 RefreshRows        ();
    void                 RefreshMultiplayer ();
    void                 OnPlayerControllerSelect (size_t player, int item);
    void                 OnPlayerTargetSelect     (size_t player, int item);
    void                 RefreshAxisOptions ();
    void                 RefreshCalibration ();
    void                 OnRowSelect        (size_t target, size_t row, int item);
    void                 AddRow             (size_t target);
    void                 SetAxisInverted    (size_t axis, bool inverted);
    void                 SetAxisResponse    (size_t axis, AxisResponse response, float maxSpeed);
    void                 OnCalibrateClick   ();
    void                 RefreshProfiles    ();
    void                 OnProfileSelect    (int index);
    void                 SwitchProfile      (const std::string & name);
    void                 OnNewProfile       ();
    void                 OpenNewProfileDialog ();
    void                 SwitchController   (size_t index);
    void                 ApplyPlayerController (size_t player, const std::optional<ControllerUnitKey> & unit);
    void                 AskToSaveProfileEdits (std::function<void ()> proceed);
    void                 OnRenameProfile    ();
    void                 OnDeleteProfile    ();
    void                 ShowDialog         ();
    void                 AfterEdit          ();
    bool                 IsJoyportAttached  () const;
    bool                 IsTargetShown      (size_t target) const;
    static std::wstring  GetRowLabel        (size_t target, const std::wstring & playLabel, bool isJoyport);
    void                 PollSwitchLights   (const GamePortContribution * reading);
    ControllerKind       GetSelectedKind    () const;

    ControllersPageState                      * m_state               = nullptr;
    SampleSource                                m_sampleSource;
    InspectFn                                   m_onInspect;
    ControllersPageState::CommitFn              m_onCommitProfile;
    std::optional<ControllerUnitKey>            m_inspected;
    size_t                                      m_lastControllerCount = 0;

    // The controller each row of the Editing drop-down stands for. In
    // multiplayer the list holds only the players' controllers, so a row's
    // position is not the controller's index.
    std::vector<size_t>                         m_editingIndices;
    std::optional<std::pair<size_t, size_t>>    m_capturing;
    std::array<bool, kTargetCount>              m_hasExtraRow         = {};
    RECT                                        m_lastRect            = {};
    DxuiDpiScaler                               m_lastScaler;
    bool                                        m_hasLayout           = false;
    bool                                        m_isSyncing           = false;
    std::function<void ()>                      m_onLayoutChanged;

    // The two player slots, shown only while the machine is in multiplayer
    // mode. Each row's drop-downs carry what they offer, so a pick resolves
    // to a controller and a target rather than to an index into a list that
    // may have been rebuilt since.
    DxuiLabel                                                                m_multiplayerHeading;
    std::array<DxuiLabel, kPlayerCount>                                      m_playerLabel;
    std::array<DxuiComboBox, kPlayerCount>                                   m_playerController;
    std::array<DxuiLabel, kPlayerCount>                                      m_playerMapsLabel;
    std::array<DxuiComboBox, kPlayerCount>                                   m_playerTarget;
    std::array<std::vector<std::optional<ControllerUnitKey>>, kPlayerCount>  m_playerUnits;
    std::array<std::vector<PlayerAxisTarget>, kPlayerCount>                  m_playerTargets;

    DxuiLabel          m_controllerLabel;
    DxuiComboBox       m_controller;

    DxuiLabel                 m_profileLabel;
    DxuiComboBox              m_profile;
    DxuiButton                m_newProfile;
    DxuiButton                m_renameProfile;
    DxuiButton                m_deleteProfile;
    std::vector<std::string>  m_profileNames;
    ProfileDialogOverlay      m_profileDialog;

    DxuiLabel          m_joystickHeading;
    StickPositionView  m_stick;
    DxuiLabel          m_buttonsHeading;

    // The Joyport's switches, drawn where the stick is while it is attached.
    JoyportSwitchView      m_switchView;
    std::function<bool()>  m_isJoyportAttached;
    bool                   m_isJoyportShown = false;

    std::array<DxuiLabel, kTargetCount>                                 m_targetLabel;
    std::array<ButtonLightView, kButtonCount>                           m_lights;
    std::array<std::array<DxuiComboBox, kMaxRows>, kTargetCount>        m_rows;
    std::array<DxuiButton, kTargetCount>                                m_addRow;
    std::array<std::vector<ControlChoice>, kTargetCount>                m_choices;

    std::array<DxuiCheckbox, kAxisCount>  m_invert;
    std::array<DxuiComboBox, kAxisCount>  m_response;
    std::array<DxuiSlider, kAxisCount>    m_speed;

    DxuiLabel          m_sharedWarning;

    DxuiLabel          m_deadzoneLabel;
    DxuiSlider         m_deadzone;

    DxuiLabel          m_calibrationLabel;
    DxuiLabel          m_calibrationStatus;
    DxuiButton         m_calibrate;
    DxuiButton         m_calibrationCancel;
    DxuiButton         m_useAutomatic;

    DxuiButton         m_reset;
};
