#pragma once

#include "Pch.h"

#include "Ui/Settings/ControllerReadoutViews.h"
#include "Ui/Settings/ControllersPageState.h"
#include "Ui/Settings/JoyportSwitchView.h"
#include "Ui/Settings/PaddleBarView.h"
#include "Ui/Settings/ProfileDialogOverlay.h"

#include "Window/DxuiPropertyPage.h"
#include "Widgets/DxuiButton.h"
#include "Widgets/DxuiCheckbox.h"
#include "Widgets/DxuiComboBox.h"
#include "Widgets/DxuiInfoBanner.h"
#include "Widgets/DxuiLabel.h"
#include "Widgets/DxuiScrollPanel.h"
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
//      * Players            (each player's entry and its mode -- Joystick, a
//                            Joyport jack, Paddle or Two paddles, and
//                            Automatic for Player 2 -- and a warning under a
//                            player whose buttons the Joyport has taken)
//      * Controller         (DxuiComboBox: every attached controller)
//      * Profile            (DxuiComboBox: the model's profiles, with New,
//                            Rename and Delete)
//      * Joystick           (a circle with a dot where the stick is, PDL0 and
//                            PDL1 labeled, beside a row per mapping for each;
//                            in Paddle and Two paddles mode a bar per paddle,
//                            labeled with its value)
//      * Buttons            (PB0 .. PB2: a light, and a row per mapping)
//      * Dead zone, Calibration, Reset profile
//
//  ONE DROP-DOWN PER MAPPING. It shows the control assigned, and lists "Press
//  to assign...", "None" and the controller's controls. A target takes more
//  than one control through "+", which adds a row; "None" on an added row
//  removes it. The first row always stays, showing "None" when the target is
//  unassigned, so every target keeps its place on the page. A target's rows
//  form a table as tall as its rows up to kTableRows, which scrolls past that;
//  "+" sits beside the table's first row, and the axis options and the
//  sharing warning sit under the table.
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
    static constexpr size_t  kTableRows   = 4;     // rows a target's table shows before it scrolls
    static constexpr size_t  kPlayerCount = MultiplayerSetup::kPlayerCount;
    static constexpr size_t  kPlayerTwo   = ControllersPageState::kPlayerTwo;

    // How often the page wants polling while it is shown: one frame at 60 Hz,
    // about as fine as a Win32 timer runs.
    static constexpr UINT    kLivePollMs  = 16;

    using SampleSource  = std::function<std::optional<ControllerSample> (const ControllerUnitKey &)>;
    using HistorySource = std::function<std::vector<ControllerSample> (const ControllerUnitKey &)>;
    using InspectFn     = std::function<void (const std::optional<ControllerUnitKey> &)>;

    explicit ControllersPage (std::wstring title = L"Controllers");

    void  SetState         (ControllersPageState * state);
    void  SetSampleSource  (SampleSource source);
    void  SetOnInspect     (InspectFn onInspect);
    void  SetPopupHost     (DxuiHwndSource * host);

    // Every reading of the controller shown since the last poll, so a press
    // too quick for one poll to see still lights its button.
    void  SetHistorySource (HistorySource source) { m_historySource = std::move (source); }

    // Whether the button lights animate: the system's animation setting,
    // which the sheet passes in.
    void  SetAnimationsEnabled (bool isEnabled);

    // What Layout measures the player warnings with, in place of the popup
    // host's. With neither they are sized from the banner's estimate.
    void  SetTextRenderer  (IDxuiTextRenderer * renderer) { m_textRenderer = renderer; }

    // Where Save on the profile-switch prompt commits the edited model.
    void  SetOnCommitProfile (ControllersPageState::CommitFn onCommit) { m_onCommitProfile = std::move (onCommit); }


    void  Layout           (const RECT & rect, const DxuiDpiScaler & scaler) override;

    // Each dialog tick: feed the controller's latest reading to the capture,
    // the calibration, the stick and the lights. `nowMs` is the clock the
    // lights time their minimum and their fade on; Poll() reads the tick count.
    void  Poll             ();
    void  Poll             (int64_t nowMs);

    // kLivePollMs while the page is shown, else 0: a hidden page reads
    // nothing and wants no polls.
    UINT  GetPollIntervalMs () const;

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

    // A target's mapping drop-downs, one per row, made as rows are needed.
    using RowList = std::vector<std::unique_ptr<DxuiComboBox>>;

    static RECT          MakeRect           (int l, int t, int w, int h);
    static PaddleTarget  TargetAt           (size_t index);
    static std::wstring  DescribeAxis       (ControllerKind kind, const AxisBinding & binding);
    static std::wstring  DescribeButton     (ControllerKind kind, const ButtonBinding & binding);
    static std::wstring  Utf8ToWide         (const std::string & text);
    static std::string   WideToUtf8         (const std::wstring & text);

    size_t               GetBindingCount    (size_t target) const;
    size_t               GetShownRows       (size_t target) const;
    void                 EnsureRows         (size_t target, size_t count);
    int                  LayOutTable        (size_t target, int x, int y, int width, bool isInPlay, const DxuiDpiScaler & scaler);
    int                  FindChoice         (size_t target, size_t row) const;

    void                 RebuildChoices     ();
    void                 RefreshRows        ();
    void                 RefreshPlayers     ();
    void                 StretchPlayerRows  (int left, int right, int gap, const DxuiDpiScaler & scaler);
    int                  GetModeWidthPx     (const DxuiDpiScaler & scaler) const;
    void                 OnPlayerEntrySelect  (size_t player, int item);
    void                 OnPlayerModeSelect   (size_t player, int item);
    void                 ApplyPlayerEntry     (size_t player, const PlayerEntry & entry);
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
    bool                 IsJoyportMode      () const;
    bool                 IsPaddlesMode      () const;
    void                 PollPaddleBars     (const GamePortContribution * reading);
    void                 SyncJoyportLayout  ();
    bool                 IsTargetShown      (size_t target) const;
    static std::wstring  GetRowLabel        (size_t target, const std::wstring & playLabel, bool isJoyport, bool isPaddles);
    std::wstring         GetTargetLabel     (PaddleTarget target) const;
    std::wstring         MakeSharedNotice   (PaddleTarget target) const;
    int                  LayOutSharedWarning (size_t target, int x, int y, int width, IDxuiTextRenderer * text, const DxuiDpiScaler & scaler);
    bool                 HasSharedNoticeChanged () const;
    void                 PollSwitchLights   (const GamePortContribution * reading);
    ControllerKind       GetSelectedKind    () const;
    IDxuiTextRenderer  * GetMeasuringRenderer () const;

    ControllersPageState                      * m_state               = nullptr;
    SampleSource                                m_sampleSource;
    HistorySource                               m_historySource;
    int64_t                                     m_lastPollMs          = 0;
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
    IDxuiTextRenderer                         * m_textRenderer        = nullptr;
    DxuiHwndSource                            * m_popupHost           = nullptr;

    // The two players, each always shown: the entry and the mode, and under
    // a player whose buttons the Joyport has taken, a warning saying so. The entry and mode drop-downs carry
    // what they offer, so a pick resolves to an entry or a mode rather than
    // to an index into a list that may have been rebuilt since.
    std::array<DxuiLabel, kPlayerCount>                 m_playerLabel;
    std::array<DxuiComboBox, kPlayerCount>              m_playerEntry;
    std::array<DxuiComboBox, kPlayerCount>              m_playerMode;
    std::array<DxuiInfoBanner, kPlayerCount>            m_playerWarning;
    std::array<std::vector<PlayerEntry>, kPlayerCount>  m_playerEntries;
    std::array<std::vector<PlayerMode>, kPlayerCount>   m_playerModes;
    std::array<bool, kPlayerCount>                      m_isWarningShown = {};

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

    // In the paddle modes, a bar for each paddle in its own row, in place of
    // the stick.
    std::array<PaddleBarView, kAxisCount>  m_paddleBars;
    bool                                   m_isPaddlesShown = false;

    std::array<DxuiLabel, kTargetCount>                                 m_targetLabel;
    std::array<ButtonLightView, kButtonCount>                           m_lights;
    std::array<RowList, kTargetCount>                                   m_rows;
    std::array<DxuiScrollPanel, kTargetCount>                           m_tables;
    std::array<DxuiButton, kTargetCount>                                m_addRow;
    std::array<std::vector<ControlChoice>, kTargetCount>                m_choices;

    std::array<DxuiCheckbox, kAxisCount>  m_invert;
    std::array<DxuiComboBox, kAxisCount>  m_response;
    std::array<DxuiLabel, kAxisCount>     m_speedLabel;
    std::array<DxuiSlider, kAxisCount>    m_speed;

    // Under each target's rows, a warning for every control there that is also
    // on another target; the one that last came into view, for the sheet to
    // scroll to.
    std::array<DxuiInfoBanner, kTargetCount>  m_sharedWarning;
    std::optional<size_t>                     m_revealedWarning;

    DxuiLabel          m_deadZoneLabel;
    DxuiSlider         m_deadZone;

    DxuiLabel          m_calibrationLabel;
    DxuiLabel          m_calibrationStatus;
    DxuiButton         m_calibrate;
    DxuiButton         m_calibrationCancel;
    DxuiButton         m_useAutomatic;

    DxuiButton         m_reset;
};
