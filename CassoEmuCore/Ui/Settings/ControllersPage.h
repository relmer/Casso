#pragma once

#include "Pch.h"

#include "Ui/Settings/ControllersPageState.h"

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
//      * PDL0 .. PB2        (one row each: what drives it, an "Add control"
//                            list, Press to assign, Clear)
//      * Invert / response  (per paddle axis: position or paddle speed, and
//                            the speed)
//      * Deadzone           (DxuiSlider)
//      * Calibration        (DirectInput only: Calibrate, then Next, then
//                            Finish; or Use automatic)
//      * Live               (what the game port reads under the edits)
//      * Restore defaults
//
//  The page is polled each dialog tick with the controller's latest reading,
//  which is what drives press-to-assign, the Calibrate steps and the live
//  readout.
//
////////////////////////////////////////////////////////////////////////////////

class ControllersPage : public DxuiPropertyPage
{
public:

    static constexpr size_t  kTargetCount = 5;
    static constexpr size_t  kAxisCount   = 2;

    using SampleSource = std::function<std::optional<ControllerSample> (const ControllerUnitKey &)>;
    using InspectFn    = std::function<void (const std::optional<ControllerUnitKey> &)>;

    explicit ControllersPage (std::wstring title = L"Controllers");

    void  SetState         (ControllersPageState * state);
    void  SetSampleSource  (SampleSource source);
    void  SetOnInspect     (InspectFn onInspect);
    void  SetPopupHost     (DxuiHwndSource * host);

    void  Layout           (const RECT & rect, const DxuiDpiScaler & scaler) override;

    // Each dialog tick: feed the controller's latest reading to the capture,
    // the calibration and the live readout.
    void  Poll             ();

    // Re-sync every widget from the state.
    void  Refresh          ();

private:

    // One entry of a target's "Add control" list.
    struct ControlChoice
    {
        bool       isPair = false;
        ControlId  control;
        ControlId  positive;
    };

    static RECT          MakeRect          (int l, int t, int w, int h);
    static PaddleTarget  TargetAt          (size_t index);
    static std::wstring  DescribeAxis      (ControllerKind kind, const AxisBinding & binding);
    static std::wstring  DescribeButton    (ControllerKind kind, const ButtonBinding & binding);

    void                 RebuildChoices    ();
    void                 RefreshTargets    ();
    void                 RefreshAxisOptions ();
    void                 RefreshCalibration ();
    void                 AddChoice         (size_t target, int comboIndex);
    void                 ToggleCapture     (size_t target);
    void                 ClearTarget       (size_t target);
    void                 SetAxisInverted   (size_t axis, bool inverted);
    void                 SetAxisResponse   (size_t axis, AxisResponse response, float maxSpeed);
    void                 OnCalibrateClick  ();
    void                 AfterEdit         ();
    ControllerKind       GetSelectedKind   () const;

    ControllersPageState              * m_state               = nullptr;
    SampleSource                        m_sampleSource;
    InspectFn                           m_onInspect;
    std::optional<ControllerUnitKey>    m_inspected;
    size_t                              m_lastControllerCount = 0;
    size_t                              m_captureTarget       = kTargetCount;

    DxuiLabel     m_controllerLabel;
    DxuiComboBox  m_controller;

    std::array<DxuiLabel, kTargetCount>                   m_targetLabel;
    std::array<DxuiLabel, kTargetCount>                   m_summary;
    std::array<DxuiComboBox, kTargetCount>                m_add;
    std::array<DxuiButton, kTargetCount>                  m_capture;
    std::array<DxuiButton, kTargetCount>                  m_clear;
    std::array<std::vector<ControlChoice>, kTargetCount>  m_choices;

    std::array<DxuiCheckbox, kAxisCount>    m_invert;
    std::array<DxuiComboBox, kAxisCount>    m_response;
    std::array<DxuiSlider,   kAxisCount>    m_speed;

    DxuiLabel     m_sharedWarning;

    DxuiLabel     m_deadzoneLabel;
    DxuiSlider    m_deadzone;

    DxuiLabel     m_calibrationLabel;
    DxuiLabel     m_calibrationStatus;
    DxuiButton    m_calibrate;
    DxuiButton    m_calibrationCancel;
    DxuiButton    m_useAutomatic;

    DxuiLabel     m_liveLabel;
    DxuiLabel     m_live;

    DxuiButton    m_reset;
};
