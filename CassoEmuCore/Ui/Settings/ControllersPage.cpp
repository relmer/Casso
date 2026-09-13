#include "Pch.h"

#include "ControllersPage.h"

#include "Controllers/ControlLabels.h"
#include "Controllers/ControllerTokens.h"





// Layout metrics (DIP), matching the other settings pages.
static constexpr int  s_kRowHeightDp    = 28;
static constexpr int  s_kLabelWidthDp   = 110;
static constexpr int  s_kSummaryWidthDp = 210;
static constexpr int  s_kAddWidthDp     = 140;
static constexpr int  s_kCaptureWidthDp = 120;
static constexpr int  s_kClearWidthDp   = 60;
static constexpr int  s_kWideWidthDp    = 340;
static constexpr int  s_kButtonWidthDp  = 130;
static constexpr int  s_kChildIndentDp  = 18;
static constexpr int  s_kGapDp          = 8;
static constexpr int  s_kSectionGapDp   = 14;
static constexpr int  s_kPagePadDp      = 16;

static constexpr const wchar_t *  s_kTargetNames[ControllersPage::kTargetCount] =
{
    L"PDL0 (X):", L"PDL1 (Y):", L"PB0:", L"PB1:", L"PB2:",
};





////////////////////////////////////////////////////////////////////////////////
//
//  ControllersPage
//
//  Registers every widget in the page's child tree; Layout positions them and
//  Refresh fills them from the state.
//
////////////////////////////////////////////////////////////////////////////////

ControllersPage::ControllersPage (std::wstring title)
    : DxuiPropertyPage (std::move (title))
{
    size_t  i = 0;



    Adopt (m_controllerLabel);
    Adopt (m_controller);

    for (i = 0; i < kTargetCount; i++)
    {
        Adopt (m_targetLabel[i]);
        Adopt (m_summary[i]);
        Adopt (m_add[i]);
        Adopt (m_capture[i]);
        Adopt (m_clear[i]);
    }

    for (i = 0; i < kAxisCount; i++)
    {
        Adopt (m_invert[i]);
        Adopt (m_response[i]);
        Adopt (m_speed[i]);
    }

    Adopt (m_sharedWarning);
    Adopt (m_deadzoneLabel);
    Adopt (m_deadzone);
    Adopt (m_calibrationLabel);
    Adopt (m_calibrationStatus);
    Adopt (m_calibrate);
    Adopt (m_calibrationCancel);
    Adopt (m_useAutomatic);
    Adopt (m_liveLabel);
    Adopt (m_live);
    Adopt (m_reset);
}





////////////////////////////////////////////////////////////////////////////////
//
//  SetState
//
//  Wires every widget's callback into the state once; Refresh only ever
//  syncs values, so a callback never replaces itself while it runs.
//
////////////////////////////////////////////////////////////////////////////////

void ControllersPage::SetState (ControllersPageState * state)
{
    size_t  i = 0;



    m_state = state;

    m_controller.SetSelect ([this] (int index)
    {
        if (m_state != nullptr && index >= 0)
        {
            m_state->SelectController ((size_t) index);
            m_captureTarget = kTargetCount;
            RebuildChoices();
            Refresh();
        }
    });

    for (i = 0; i < kTargetCount; i++)
    {
        m_add[i].SetSelect   ([this, i] (int index) { AddChoice (i, index); });
        m_capture[i].SetOnClick ([this, i] () { ToggleCapture (i); });
        m_clear[i].SetOnClick   ([this, i] () { ClearTarget (i); });
    }

    for (i = 0; i < kAxisCount; i++)
    {
        m_invert[i].SetOnChange ([this, i] (bool checked) { SetAxisInverted (i, checked); });

        m_response[i].SetSelect ([this, i] (int index)
        {
            SetAxisResponse (i, index == 1 ? AxisResponse::Rate : AxisResponse::Absolute, m_speed[i].GetValue());
        });

        m_speed[i].SetOnChange ([this, i] (float value)
        {
            SetAxisResponse (i, AxisResponse::Rate, value);
        });
    }

    m_deadzone.SetOnChange ([this] (float percent)
    {
        if (m_state != nullptr)
        {
            m_state->SetDeadzone (percent / 100.0f);
            AfterEdit();
        }
    });

    m_calibrate.SetOnClick ([this] () { OnCalibrateClick(); });

    m_calibrationCancel.SetOnClick ([this] ()
    {
        if (m_state != nullptr)
        {
            m_state->CancelCalibration();
            RefreshCalibration();
        }
    });

    m_useAutomatic.SetOnClick ([this] ()
    {
        if (m_state != nullptr)
        {
            m_state->UseAutomaticCalibration();
            AfterEdit();
        }
    });

    m_reset.SetOnClick ([this] ()
    {
        if (m_state != nullptr)
        {
            m_state->ResetToDefaults();
            AfterEdit();
        }
    });

    RebuildChoices();
    Refresh();
}





////////////////////////////////////////////////////////////////////////////////
//
//  SetSampleSource
//
////////////////////////////////////////////////////////////////////////////////

void ControllersPage::SetSampleSource (SampleSource source)
{
    m_sampleSource = std::move (source);
}





////////////////////////////////////////////////////////////////////////////////
//
//  SetOnInspect
//
////////////////////////////////////////////////////////////////////////////////

void ControllersPage::SetOnInspect (InspectFn onInspect)
{
    m_onInspect = std::move (onInspect);
}





////////////////////////////////////////////////////////////////////////////////
//
//  SetPopupHost
//
////////////////////////////////////////////////////////////////////////////////

void ControllersPage::SetPopupHost (DxuiHwndSource * host)
{
    size_t  i = 0;



    m_controller.SetPopupHost (host);

    for (i = 0; i < kTargetCount; i++)
    {
        m_add[i].SetPopupHost (host);
    }

    for (i = 0; i < kAxisCount; i++)
    {
        m_response[i].SetPopupHost (host);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  Layout
//
//  One aligned form: a label column, then each row's controls left to right.
//  The two paddle axes carry an indented second row for how the stick drives
//  them.
//
////////////////////////////////////////////////////////////////////////////////

void ControllersPage::Layout (const RECT & rect, const DxuiDpiScaler & scaler)
{
    UINT    dpi          = scaler.GetDpi();
    int     pad          = scaler.ToPx (s_kPagePadDp);
    int     row          = scaler.ToPx (s_kRowHeightDp);
    int     labelWidth   = scaler.ToPx (s_kLabelWidthDp);
    int     summaryWidth = scaler.ToPx (s_kSummaryWidthDp);
    int     addWidth     = scaler.ToPx (s_kAddWidthDp);
    int     captureWidth = scaler.ToPx (s_kCaptureWidthDp);
    int     clearWidth   = scaler.ToPx (s_kClearWidthDp);
    int     wideWidth    = scaler.ToPx (s_kWideWidthDp);
    int     buttonWidth  = scaler.ToPx (s_kButtonWidthDp);
    int     indent       = scaler.ToPx (s_kChildIndentDp);
    int     gap          = scaler.ToPx (s_kGapDp);
    int     sectionGap   = scaler.ToPx (s_kSectionGapDp);
    int     x            = rect.left + pad;
    int     y            = rect.top  + pad;
    int     controlsX    = x + labelWidth;
    size_t  i            = 0;



    m_controllerLabel.SetRect (MakeRect (x, y, labelWidth, row));
    m_controllerLabel.SetText (L"Controller:");
    m_controller.SetRect      (MakeRect (controlsX, y, wideWidth, row));
    y += row + sectionGap;

    for (i = 0; i < kTargetCount; i++)
    {
        int  cx = controlsX;

        m_targetLabel[i].SetRect (MakeRect (x, y, labelWidth, row));
        m_targetLabel[i].SetText (s_kTargetNames[i]);

        m_summary[i].SetRect (MakeRect (cx, y, summaryWidth, row));
        cx += summaryWidth + gap;

        m_add[i].SetRect (MakeRect (cx, y, addWidth, row));
        cx += addWidth + gap;

        m_capture[i].Layout (MakeRect (cx, y, captureWidth, row));
        cx += captureWidth + gap;

        m_clear[i].SetLabel (L"Clear");
        m_clear[i].Layout   (MakeRect (cx, y, clearWidth, row));

        y += row + gap;

        if (i < kAxisCount)
        {
            int  ox = controlsX + indent;

            m_invert[i].SetRect  (MakeRect (ox, y, addWidth - indent, row));
            m_invert[i].SetLabel (L"Invert");
            ox += addWidth - indent + gap;

            m_response[i].SetRect  (MakeRect (ox, y, addWidth, row));
            m_response[i].SetItems ({ L"Position", L"Paddle speed" });
            ox += addWidth + gap;

            m_speed[i].SetRect         (MakeRect (ox, y, captureWidth + clearWidth + gap, row));
            m_speed[i].SetRange        (ControllerProfileStore::kMinMaxSpeed, ControllerProfileStore::kMaxMaxSpeed);
            m_speed[i].SetStep         (16.0f);
            m_speed[i].SetDecimalPlaces (0);
            m_speed[i].SetSuffix       (L"/s");

            y += row + gap;
        }
    }

    m_sharedWarning.SetRect  (MakeRect (x, y, wideWidth + labelWidth, row));
    m_sharedWarning.SetColor (0xFFF0A030);   // amber caution, as the sheet's restart notice
    y += row + sectionGap;

    m_deadzoneLabel.SetRect (MakeRect (x, y, labelWidth, row));
    m_deadzoneLabel.SetText (L"Deadzone:");
    m_deadzone.SetRect         (MakeRect (controlsX, y, wideWidth, row));
    m_deadzone.SetRange        (0.0f, 90.0f);
    m_deadzone.SetStep         (1.0f);
    m_deadzone.SetDecimalPlaces (0);
    m_deadzone.SetSuffix       (L"%");
    y += row + sectionGap;

    m_calibrationLabel.SetRect (MakeRect (x, y, labelWidth, row));
    m_calibrationLabel.SetText (L"Calibration:");
    m_calibrationStatus.SetRect (MakeRect (controlsX, y, wideWidth + buttonWidth, row));
    y += row + gap;

    m_calibrate.Layout (MakeRect (controlsX, y, buttonWidth, row));
    m_calibrationCancel.SetLabel (L"Cancel");
    m_calibrationCancel.Layout (MakeRect (controlsX + buttonWidth + gap, y, buttonWidth, row));
    m_useAutomatic.SetLabel (L"Use automatic");
    m_useAutomatic.Layout (MakeRect (controlsX + (buttonWidth + gap) * 2, y, buttonWidth, row));
    y += row + sectionGap;

    m_liveLabel.SetRect (MakeRect (x, y, labelWidth, row));
    m_liveLabel.SetText (L"Live:");
    m_live.SetRect (MakeRect (controlsX, y, wideWidth + buttonWidth, row));
    y += row + sectionGap;

    m_reset.SetLabel (L"Restore defaults");
    m_reset.Layout (MakeRect (controlsX, y, buttonWidth, row));

    m_controllerLabel.SetDpi (dpi);
    m_controller.SetDpi      (dpi);

    for (i = 0; i < kTargetCount; i++)
    {
        m_targetLabel[i].SetDpi (dpi);
        m_summary[i].SetDpi     (dpi);
        m_add[i].SetDpi         (dpi);
        m_capture[i].SetDpi     (dpi);
        m_clear[i].SetDpi       (dpi);
    }

    for (i = 0; i < kAxisCount; i++)
    {
        m_invert[i].SetDpi   (dpi);
        m_response[i].SetDpi (dpi);
        m_speed[i].SetDpi    (dpi);
    }

    m_sharedWarning.SetDpi     (dpi);
    m_deadzoneLabel.SetDpi     (dpi);
    m_deadzone.SetDpi          (dpi);
    m_calibrationLabel.SetDpi  (dpi);
    m_calibrationStatus.SetDpi (dpi);
    m_calibrate.SetDpi         (dpi);
    m_calibrationCancel.SetDpi (dpi);
    m_useAutomatic.SetDpi      (dpi);
    m_liveLabel.SetDpi         (dpi);
    m_live.SetDpi              (dpi);
    m_reset.SetDpi             (dpi);

    RebuildChoices();
    Refresh();

    DxuiPanel::SetBounds (rect);
}





////////////////////////////////////////////////////////////////////////////////
//
//  Poll
//
//  The page asks the service to read the controller it shows, whether or not
//  that controller is the one selected, and then hands the latest reading to
//  whatever is waiting on it.
//
////////////////////////////////////////////////////////////////////////////////

void ControllersPage::Poll()
{
    std::optional<size_t>             selected;
    std::optional<ControllerUnitKey>  unit;
    std::optional<ControllerSample>   sample;
    GamePortContribution              reading;



    if (m_state == nullptr)
    {
        return;
    }

    if (m_state->GetControllers().size() != m_lastControllerCount)
    {
        RebuildChoices();
        Refresh();
    }

    selected = m_state->GetSelectedIndex();

    if (selected.has_value())
    {
        unit = m_state->GetControllers()[selected.value()].unit;
    }

    if (unit != m_inspected && m_onInspect)
    {
        m_inspected = unit;
        m_onInspect (unit);
    }

    if (!unit.has_value())
    {
        m_live.SetText (L"No controller is attached.");
        return;
    }

    if (m_sampleSource)
    {
        sample = m_sampleSource (unit.value());
    }

    if (!sample.has_value())
    {
        m_live.SetText (L"Not connected.");
        return;
    }

    if (m_state->IsCapturing() && m_state->FeedCapture (sample.value()))
    {
        m_captureTarget = kTargetCount;
        AfterEdit();
    }

    if (m_state->GetCalibrationStep() != CalibrationStep::None)
    {
        m_state->FeedCalibration (sample.value());
    }

    reading = m_state->ComputeLiveReading (sample.value());

    m_live.SetText (std::format (L"PDL0 {}    PDL1 {}    PB0 {}    PB1 {}    PB2 {}",
                                 reading.paddle.has_value() ? reading.paddle.value()[0] : 127,
                                 reading.paddle.has_value() ? reading.paddle.value()[1] : 127,
                                 reading.buttons.test (0) ? L"down" : L"up",
                                 reading.buttons.test (1) ? L"down" : L"up",
                                 m_state->IsTargetAvailable (PaddleTarget::Pb2) ? (reading.buttons.test (2) ? L"down" : L"up") : L"n/a"));
}





////////////////////////////////////////////////////////////////////////////////
//
//  Refresh
//
////////////////////////////////////////////////////////////////////////////////

void ControllersPage::Refresh()
{
    std::vector<std::wstring>  names;
    std::optional<size_t>      selected;



    if (m_state == nullptr)
    {
        return;
    }

    for (const ControllersPageState::ControllerEntry & entry : m_state->GetControllers())
    {
        names.push_back (entry.isConnected ? entry.description : entry.description + L" (not connected)");
    }

    if (names.empty())
    {
        names.push_back (L"No controller is attached");
    }

    selected              = m_state->GetSelectedIndex();
    m_lastControllerCount = m_state->GetControllers().size();

    m_controller.SetItems    (names);
    m_controller.SetSelected (selected.has_value() ? (int) selected.value() : 0);
    m_controller.SetEnabled  (selected.has_value());

    RefreshTargets();
    RefreshAxisOptions();
    RefreshCalibration();

    m_deadzone.SetValue   (m_state->GetDeadzone() * 100.0f);
    m_deadzone.SetEnabled (selected.has_value());
    m_reset.SetEnabled    (selected.has_value());
}





////////////////////////////////////////////////////////////////////////////////
//
//  RebuildChoices
//
//  Each target's "Add control" list, from the selected controller's
//  controls. An axis target offers the analog controls and each D-pad as a
//  left/right or up/down pair; a button target offers every control.
//
////////////////////////////////////////////////////////////////////////////////

void ControllersPage::RebuildChoices()
{
    std::optional<size_t>  selected = m_state != nullptr ? m_state->GetSelectedIndex() : std::nullopt;
    ControllerKind         kind     = GetSelectedKind();
    size_t                 i        = 0;



    for (i = 0; i < kTargetCount; i++)
    {
        std::vector<std::wstring>  items = { L"Add control..." };

        m_choices[i].clear();

        if (selected.has_value())
        {
            for (const ControlId & control : m_state->GetControllers()[selected.value()].controls)
            {
                bool  isAnalog = control.kind == ControlKind::Axis || control.kind == ControlKind::Trigger;

                if (i >= kAxisCount)
                {
                    m_choices[i].push_back ({ false, control, {} });
                    items.push_back (ControlLabels::For (kind, control));
                }
                else if (isAnalog)
                {
                    m_choices[i].push_back ({ false, control, {} });
                    items.push_back (ControlLabels::For (kind, control));
                }
                else if (control.kind == ControlKind::DpadLeft)
                {
                    ControlId  right = { ControlKind::DpadRight, control.index };

                    m_choices[i].push_back ({ true, control, right });
                    items.push_back (ControlLabels::For (kind, control) + L" / right");
                }
                else if (control.kind == ControlKind::DpadUp)
                {
                    ControlId  down = { ControlKind::DpadDown, control.index };

                    m_choices[i].push_back ({ true, control, down });
                    items.push_back (ControlLabels::For (kind, control) + L" / down");
                }
            }
        }

        m_add[i].SetItems    (items);
        m_add[i].SetSelected (0);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  RefreshTargets
//
////////////////////////////////////////////////////////////////////////////////

void ControllersPage::RefreshTargets()
{
    const ControlMapping &  mapping  = m_state->GetMapping();
    ControllerKind          kind     = GetSelectedKind();
    bool                    hasUnit  = m_state->GetSelectedIndex().has_value();
    std::vector<ControlId>  shared   = m_state->GetSharedControls();
    size_t                  i        = 0;



    for (i = 0; i < kTargetCount; i++)
    {
        PaddleTarget  target    = TargetAt (i);
        bool          available = m_state->IsTargetAvailable (target);
        std::wstring  summary;

        if (i < kAxisCount)
        {
            const std::vector<AxisBinding> &  bindings = (i == 0) ? mapping.pdl0 : mapping.pdl1;

            for (const AxisBinding & binding : bindings)
            {
                summary += (summary.empty() ? L"" : L", ") + DescribeAxis (kind, binding);
            }
        }
        else
        {
            const std::vector<ButtonBinding> &  bindings = (i == 2) ? mapping.pb0 : (i == 3) ? mapping.pb1 : mapping.pb2;

            for (const ButtonBinding & binding : bindings)
            {
                summary += (summary.empty() ? L"" : L", ") + DescribeButton (kind, binding);
            }
        }

        if (!available)
        {
            summary = L"Not on this machine";
        }
        else if (summary.empty())
        {
            summary = L"Not assigned";
        }

        m_summary[i].SetText  (summary);
        m_add[i].SetEnabled     (hasUnit && available);
        m_capture[i].SetEnabled (hasUnit && available);
        m_clear[i].SetEnabled   (hasUnit && available);
        m_capture[i].SetLabel   (m_captureTarget == i && m_state->IsCapturing() ? L"Press a control..." : L"Press to assign");
    }

    if (shared.empty())
    {
        m_sharedWarning.SetText (L"");
    }
    else
    {
        std::wstring  names;

        for (const ControlId & control : shared)
        {
            names += (names.empty() ? L"" : L", ") + ControlLabels::For (kind, control);
        }

        m_sharedWarning.SetText (L"Assigned to more than one target: " + names);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  RefreshAxisOptions
//
//  The first analog binding on each axis speaks for the row, since the
//  options set every analog binding on it at once.
//
////////////////////////////////////////////////////////////////////////////////

void ControllersPage::RefreshAxisOptions()
{
    const ControlMapping &  mapping = m_state->GetMapping();
    size_t                  i       = 0;



    for (i = 0; i < kAxisCount; i++)
    {
        const std::vector<AxisBinding> &  bindings = (i == 0) ? mapping.pdl0 : mapping.pdl1;
        const AxisBinding *               analog   = nullptr;

        for (const AxisBinding & binding : bindings)
        {
            if (binding.kind == AxisBindingKind::Analog)
            {
                analog = &binding;
                break;
            }
        }

        m_invert[i].SetEnabled   (analog != nullptr);
        m_response[i].SetEnabled (analog != nullptr);
        m_speed[i].SetEnabled    (analog != nullptr && analog->response == AxisResponse::Rate);

        if (analog != nullptr)
        {
            m_invert[i].SetChecked    (analog->inverted);
            m_response[i].SetSelected (analog->response == AxisResponse::Rate ? 1 : 0);
            m_speed[i].SetValue       (analog->maxSpeed);
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  RefreshCalibration
//
////////////////////////////////////////////////////////////////////////////////

void ControllersPage::RefreshCalibration()
{
    bool                   canCalibrate = m_state->IsCalibratable();
    CalibrationStep        step         = m_state->GetCalibrationStep();
    std::optional<size_t>  selected     = m_state->GetSelectedIndex();
    bool                   hasUser      = false;



    if (selected.has_value() && canCalibrate)
    {
        auto  found = m_state->GetCalibrations().find (ControllerTokens::UnitToToken (m_state->GetControllers()[selected.value()].unit));

        hasUser = found != m_state->GetCalibrations().end() && found->second.mode == CalibrationMode::User;
    }

    m_calibrate.SetVisible         (canCalibrate);
    m_calibrationCancel.SetVisible (canCalibrate && step != CalibrationStep::None);
    m_useAutomatic.SetVisible      (canCalibrate && step == CalibrationStep::None && hasUser);

    if (!selected.has_value())
    {
        m_calibrationStatus.SetText (L"");
    }
    else if (!canCalibrate)
    {
        m_calibrationStatus.SetText (L"Xbox controllers are calibrated at the factory.");
    }
    else if (step == CalibrationStep::Center)
    {
        m_calibrationStatus.SetText (L"Leave the stick at rest, then click Next.");
        m_calibrate.SetLabel (L"Next");
    }
    else if (step == CalibrationStep::Travel)
    {
        m_calibrationStatus.SetText (L"Move the stick through its full travel, then click Finish.");
        m_calibrate.SetLabel (L"Finish");
    }
    else
    {
        m_calibrationStatus.SetText (hasUser ? L"Calibrated." : L"Automatic.");
        m_calibrate.SetLabel (L"Calibrate");
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  AddChoice
//
////////////////////////////////////////////////////////////////////////////////

void ControllersPage::AddChoice (size_t target, int comboIndex)
{
    const ControlChoice *  choice = nullptr;



    if (m_state == nullptr || comboIndex <= 0 || (size_t) comboIndex > m_choices[target].size())
    {
        return;
    }

    choice = &m_choices[target][(size_t) comboIndex - 1];

    if (target < kAxisCount)
    {
        AxisBinding  binding;

        if (choice->isPair)
        {
            binding.kind     = AxisBindingKind::DigitalPair;
            binding.negative = choice->control;
            binding.positive = choice->positive;
        }
        else
        {
            binding.analog = choice->control;
        }

        m_state->AddAxisBinding (TargetAt (target), binding);
    }
    else
    {
        ButtonBinding  binding;

        binding.control = choice->control;

        if (choice->control.kind == ControlKind::Trigger)
        {
            binding.threshold = ButtonBinding::kTriggerThreshold;
        }

        m_state->AddButtonBinding (TargetAt (target), binding);
    }

    m_add[target].SetSelected (0);
    AfterEdit();
}





////////////////////////////////////////////////////////////////////////////////
//
//  ToggleCapture
//
//  A second click on the waiting button cancels the wait (FR-022). The
//  baseline is the controller's reading at the click, so whatever was
//  already held then is ignored until it is let go.
//
////////////////////////////////////////////////////////////////////////////////

void ControllersPage::ToggleCapture (size_t target)
{
    std::optional<size_t>            selected;
    std::optional<ControllerSample>  baseline;



    if (m_state == nullptr)
    {
        return;
    }

    if (m_state->IsCapturing())
    {
        bool  wasThis = (m_captureTarget == target);

        m_state->CancelCapture();
        m_captureTarget = kTargetCount;

        if (wasThis)
        {
            RefreshTargets();
            return;
        }
    }

    selected = m_state->GetSelectedIndex();

    if (!selected.has_value())
    {
        return;
    }

    if (m_sampleSource)
    {
        baseline = m_sampleSource (m_state->GetControllers()[selected.value()].unit);
    }

    m_state->BeginCapture (TargetAt (target), baseline.value_or (ControllerSample()));
    m_captureTarget = target;
    RefreshTargets();
}





////////////////////////////////////////////////////////////////////////////////
//
//  ClearTarget
//
////////////////////////////////////////////////////////////////////////////////

void ControllersPage::ClearTarget (size_t target)
{
    size_t  count = 0;



    if (m_state == nullptr)
    {
        return;
    }

    count = (target == 0) ? m_state->GetMapping().pdl0.size()
          : (target == 1) ? m_state->GetMapping().pdl1.size()
          : (target == 2) ? m_state->GetMapping().pb0.size()
          : (target == 3) ? m_state->GetMapping().pb1.size()
          :                 m_state->GetMapping().pb2.size();

    while (count > 0)
    {
        count--;
        m_state->RemoveBinding (TargetAt (target), count);
    }

    AfterEdit();
}





////////////////////////////////////////////////////////////////////////////////
//
//  SetAxisInverted
//
////////////////////////////////////////////////////////////////////////////////

void ControllersPage::SetAxisInverted (size_t axis, bool inverted)
{
    size_t  count = 0;
    size_t  i     = 0;



    if (m_state == nullptr)
    {
        return;
    }

    count = (axis == 0) ? m_state->GetMapping().pdl0.size() : m_state->GetMapping().pdl1.size();

    for (i = 0; i < count; i++)
    {
        m_state->SetInverted (TargetAt (axis), i, inverted);
    }

    AfterEdit();
}





////////////////////////////////////////////////////////////////////////////////
//
//  SetAxisResponse
//
////////////////////////////////////////////////////////////////////////////////

void ControllersPage::SetAxisResponse (size_t axis, AxisResponse response, float maxSpeed)
{
    size_t  count = 0;
    size_t  i     = 0;



    if (m_state == nullptr)
    {
        return;
    }

    count = (axis == 0) ? m_state->GetMapping().pdl0.size() : m_state->GetMapping().pdl1.size();

    for (i = 0; i < count; i++)
    {
        m_state->SetResponse (TargetAt (axis), i, response, maxSpeed);
    }

    AfterEdit();
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnCalibrateClick
//
//  One button walks the three steps: Calibrate, Next, Finish.
//
////////////////////////////////////////////////////////////////////////////////

void ControllersPage::OnCalibrateClick()
{
    if (m_state == nullptr)
    {
        return;
    }

    if (m_state->GetCalibrationStep() == CalibrationStep::None)
    {
        m_state->BeginCalibration();
        RefreshCalibration();
        return;
    }

    m_state->AdvanceCalibration();
    AfterEdit();
}





////////////////////////////////////////////////////////////////////////////////
//
//  AfterEdit
//
////////////////////////////////////////////////////////////////////////////////

void ControllersPage::AfterEdit()
{
    Refresh();
    MarkDirty (m_state != nullptr && m_state->IsDirty());
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetSelectedKind
//
////////////////////////////////////////////////////////////////////////////////

ControllerKind ControllersPage::GetSelectedKind() const
{
    std::optional<size_t>  selected = m_state != nullptr ? m_state->GetSelectedIndex() : std::nullopt;



    if (!selected.has_value())
    {
        return ControllerKind::DirectInput;
    }

    return m_state->GetControllers()[selected.value()].unit.model.kind;
}





////////////////////////////////////////////////////////////////////////////////
//
//  MakeRect
//
////////////////////////////////////////////////////////////////////////////////

RECT ControllersPage::MakeRect (int l, int t, int w, int h)
{
    RECT  rc = { l, t, l + w, t + h };



    return rc;
}





////////////////////////////////////////////////////////////////////////////////
//
//  TargetAt
//
////////////////////////////////////////////////////////////////////////////////

PaddleTarget ControllersPage::TargetAt (size_t index)
{
    static constexpr PaddleTarget  s_kTargets[kTargetCount] =
    {
        PaddleTarget::Pdl0, PaddleTarget::Pdl1, PaddleTarget::Pb0, PaddleTarget::Pb1, PaddleTarget::Pb2,
    };



    return s_kTargets[index < kTargetCount ? index : 0];
}





////////////////////////////////////////////////////////////////////////////////
//
//  DescribeAxis
//
////////////////////////////////////////////////////////////////////////////////

std::wstring ControllersPage::DescribeAxis (ControllerKind kind, const AxisBinding & binding)
{
    std::wstring  text;



    if (binding.kind == AxisBindingKind::DigitalPair)
    {
        return ControlLabels::For (kind, binding.negative) + L" / " + ControlLabels::For (kind, binding.positive);
    }

    text = ControlLabels::For (kind, binding.analog);

    if (binding.inverted)
    {
        text += L" (inverted)";
    }

    if (binding.response == AxisResponse::Rate)
    {
        text += L" (paddle speed)";
    }

    return text;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DescribeButton
//
////////////////////////////////////////////////////////////////////////////////

std::wstring ControllersPage::DescribeButton (ControllerKind kind, const ButtonBinding & binding)
{
    std::wstring  text = ControlLabels::For (kind, binding.control);



    if (binding.control.kind == ControlKind::Axis)
    {
        text += binding.negativeDirection ? L" (negative)" : L" (positive)";
    }

    return text;
}
