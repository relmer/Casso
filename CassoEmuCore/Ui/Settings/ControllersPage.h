#pragma once

#include "Pch.h"

#include "Ui/Settings/ControllerReadoutViews.h"
#include "Ui/Settings/ControllersPageState.h"
#include "Ui/Settings/JoyportSwitchView.h"
#include "Ui/Settings/ProfileDialogOverlay.h"

#include "Core/DxuiSlide.h"
#include "Window/DxuiPropertyPage.h"
#include "Widgets/DxuiButton.h"
#include "Widgets/DxuiCheckbox.h"
#include "Widgets/DxuiComboBox.h"
#include "Widgets/DxuiLabel.h"
#include "Widgets/DxuiSlider.h"
#include "Widgets/DxuiToggle.h"


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
//      * Joyport            (DxuiToggle: the unit's Apple / Atari switch, on a
//                            machine that can take one)
//      * Players            (Player 1's entry and target; a Multiplayer
//                            checkbox; Player 2's, while it is ticked)
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
    static constexpr size_t  kPlayerTwo   = ControllersPageState::kPlayerTwo;

    using SampleSource = std::function<std::optional<ControllerSample> (const ControllerUnitKey &)>;
    using InspectFn    = std::function<void (const std::optional<ControllerUnitKey> &)>;

    explicit ControllersPage (std::wstring title = L"Controllers");

    void  SetState         (ControllersPageState * state);
    void  SetSampleSource  (SampleSource source);
    void  SetOnInspect     (InspectFn onInspect);
    void  SetPopupHost     (DxuiHwndSource * host);

    // Where Save on the profile-switch prompt commits the edited model.
    void  SetOnCommitProfile (ControllersPageState::CommitFn onCommit) { m_onCommitProfile = std::move (onCommit); }

    // The Joyport's Apple / Atari switch at the top of the page: whether the
    // Joyport is in effect, whether the running machine can take one, and how
    // to turn it on or off. While it is on, the stick and the button lights
    // give way to the five switch lights the Joyport reads.
    void  SetJoyportFns (std::function<bool()>      isOn,
                         std::function<bool()>      isOffered,
                         std::function<void (bool)> set);

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

    // New... from a profile section in the paddle picker: the New Profile dialog
    // for that section's controller, after asking about unsaved edits.
    void  StartNewProfile  (const ControllerUnitKey & unit);

    // Press-to-assign in progress, for the sheet's prompt over the page: the
    // sentence it shows, and a way to call the wait off.
    bool          IsCapturing        () const;
    std::wstring  GetCapturePrompt   () const;
    void          CancelCapture      ();

    // The page's rows came or went, which changes what Tab reaches.
    void          SetOnLayoutChanged (std::function<void ()> onLayoutChanged);

    // A slide started or finished, for the sheet to poll the page often
    // enough to draw it.
    void          SetOnSlideChanged  (std::function<void (bool isSliding)> onSlideChanged);

    // What the target drop-down lists first: no target of the player's own,
    // so the slot follows the profile.
    static constexpr const wchar_t *  kpszFollowProfile = L"Follow the profile";

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
    void                 RefreshPlayers     ();
    void                 OnPlayerEntrySelect  (size_t player, int item);
    void                 OnPlayerTargetSelect (size_t player, int item);
    void                 OnMultiplayerCheck   (bool isChecked);
    void                 ApplyPlayerEntry     (size_t player, const PlayerEntry & entry);
    void                 StartPlayerTwoSlide  (bool isShowing, int distancePx);
    void                 AdvanceSlide         ();
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

    void                 AskToSaveProfileEdits (std::function<void ()> proceed);
    void                 OnRenameProfile    ();
    void                 OnDeleteProfile    ();
    void                 ShowDialog         ();
    void                 AfterEdit          ();
    bool                 IsJoyportOffered   () const;
    bool                 IsJoyportMode      () const;
    void                 OnJoyportSwitch    (bool isAtariMode);
    void                 ApplyJoyportMode   (bool isAtariMode);
    void                 SyncJoyportSwitch  ();
    void                 LayOutJoyportSection (int x, int y, bool isOffered, const DxuiDpiScaler & scaler);
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

    // The Joyport's Apple / Atari switch, shown where the machine can take
    // one. The last value read from isOn is kept apart from the switch, so a
    // flip on the page is not taken for a change from the picker before the
    // command it posts has been handled. "Joyport" sits to the left of the
    // switch, and each position is labeled to its right, level with the
    // knob's place there.
    DxuiLabel                   m_joyportHeading;
    DxuiToggle                  m_joyportSwitch;
    DxuiLabel                   m_joyportAppleLabel;
    DxuiLabel                   m_joyportAtariLabel;
    std::function<bool()>       m_isJoyportOn;
    std::function<bool()>       m_isJoyportOffered;
    std::function<void (bool)>  m_setJoyport;
    bool                        m_lastJoyportOn         = false;
    bool                        m_isJoyportSectionShown = false;

    // The two players: Player 1's row, the Multiplayer checkbox, and Player
    // 2's row while it is ticked. Each row's drop-downs carry what they
    // offer, so a pick resolves to an entry and a target rather than to an
    // index into a list that may have been rebuilt since.
    //
    // TICKING SLIDES THE ROWS BELOW DOWN to make room for Player 2's row,
    // which appears once they have, and unticking takes the row away and
    // slides them back, over the time and curve a menu opens with.
    DxuiCheckbox                                                             m_multiplayerCheck;
    std::array<DxuiLabel, kPlayerCount>                                      m_playerLabel;
    std::array<DxuiComboBox, kPlayerCount>                                   m_playerEntry;
    std::array<DxuiLabel, kPlayerCount>                                      m_playerMapsLabel;
    std::array<DxuiComboBox, kPlayerCount>                                   m_playerTarget;
    std::array<std::vector<PlayerEntry>, kPlayerCount>                       m_playerEntries;
    std::array<std::vector<std::optional<PlayerAxisTarget>>, kPlayerCount>   m_playerTargets;
    DxuiSlide                                                                m_slide;
    bool                                                                     m_isSliding        = false;
    bool                                                                     m_isPlayerTwoShown = false;
    std::function<void (bool)>                                               m_onSlideChanged;

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

    // The Joyport's switches, drawn where the stick is while it is on.
    JoyportSwitchView      m_switchView;
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
