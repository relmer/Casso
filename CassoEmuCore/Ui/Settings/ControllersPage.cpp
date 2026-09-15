#include "Pch.h"

#include "ControllersPage.h"

#include "Controllers/ControlLabels.h"
#include "Controllers/ControllerTokens.h"





// Layout metrics (DIP), matching the other settings pages.
static constexpr int  s_kRowHeightDp           = 28;
static constexpr int  s_kLabelWidthDp          = 90;
static constexpr int  s_kRowWidthDp            = 220;
static constexpr int  s_kAddWidthDp            = 28;
static constexpr int  s_kStickSizeDp           = 190;
static constexpr int  s_kLightSizeDp           = 14;
static constexpr int  s_kWideWidthDp           = 340;
static constexpr int  s_kButtonWidthDp         = 130;
static constexpr int  s_kProfileButtonWidthDp  = 90;
static constexpr int  s_kOptionWidthDp         = 110;
static constexpr int  s_kChildIndentDp         = 18;
static constexpr int  s_kGapDp                 = 6;
static constexpr int  s_kSectionGapDp          = 14;
static constexpr int  s_kPagePadDp             = 16;

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
    size_t  target = 0;
    size_t  row    = 0;



    Adopt (m_controllerLabel);
    Adopt (m_controller);
    Adopt (m_profileLabel);
    Adopt (m_profile);
    Adopt (m_newProfile);
    Adopt (m_renameProfile);
    Adopt (m_deleteProfile);
    Adopt (m_joystickHeading);
    Adopt (m_stick);
    Adopt (m_buttonsHeading);

    for (target = 0; target < kTargetCount; target++)
    {
        Adopt (m_targetLabel[target]);
        Adopt (m_addRow[target]);

        for (row = 0; row < kMaxRows; row++)
        {
            Adopt (m_rows[target][row]);
        }
    }

    for (target = 0; target < kButtonCount; target++)
    {
        Adopt (m_lights[target]);
    }

    for (target = 0; target < kAxisCount; target++)
    {
        Adopt (m_invert[target]);
        Adopt (m_response[target]);
        Adopt (m_speed[target]);
    }

    Adopt (m_sharedWarning);
    Adopt (m_deadzoneLabel);
    Adopt (m_deadzone);
    Adopt (m_calibrationLabel);
    Adopt (m_calibrationStatus);
    Adopt (m_calibrate);
    Adopt (m_calibrationCancel);
    Adopt (m_useAutomatic);
    Adopt (m_reset);
}





////////////////////////////////////////////////////////////////////////////////
//
//  SetState
//
//  Wires every widget's callback into the state once. Refresh only syncs
//  values, and sets m_isSyncing while it does, so a drop-down whose selection
//  it moves does not read that as the user picking.
//
////////////////////////////////////////////////////////////////////////////////

void ControllersPage::SetState (ControllersPageState * state)
{
    size_t  target = 0;
    size_t  row    = 0;



    m_state = state;

    m_controller.SetSelect ([this] (int index)
    {
        if (m_isSyncing || m_state == nullptr || index < 0 || m_state->GetSelectedIndex() == std::optional<size_t> ((size_t) index))
        {
            return;
        }

        AskToSaveProfileEdits ([this, index] () { SwitchController ((size_t) index); });
    });

    m_profile.SetSelect ([this] (int index)
    {
        if (!m_isSyncing)
        {
            OnProfileSelect (index);
        }
    });

    m_newProfile.SetOnClick    ([this] () { OnNewProfile(); });
    m_renameProfile.SetOnClick ([this] () { OnRenameProfile(); });
    m_deleteProfile.SetOnClick ([this] () { OnDeleteProfile(); });

    for (target = 0; target < kTargetCount; target++)
    {
        for (row = 0; row < kMaxRows; row++)
        {
            m_rows[target][row].SetSelect ([this, target, row] (int item)
            {
                if (!m_isSyncing)
                {
                    OnRowSelect (target, row, item);
                }
            });
        }

        m_addRow[target].SetOnClick ([this, target] () { AddRow (target); });
    }

    for (target = 0; target < kAxisCount; target++)
    {
        m_invert[target].SetOnChange ([this, target] (bool checked)
        {
            if (!m_isSyncing)
            {
                SetAxisInverted (target, checked);
            }
        });

        m_response[target].SetSelect ([this, target] (int index)
        {
            if (!m_isSyncing)
            {
                SetAxisResponse (target, index == 1 ? AxisResponse::Rate : AxisResponse::Absolute, m_speed[target].GetValue());
            }
        });

        m_speed[target].SetOnChange ([this, target] (float value)
        {
            if (!m_isSyncing)
            {
                SetAxisResponse (target, AxisResponse::Rate, value);
            }
        });
    }

    m_deadzone.SetOnChange ([this] (float percent)
    {
        if (!m_isSyncing && m_state != nullptr)
        {
            m_state->SetDeadzone (percent / 100.0f);
            MarkDirty (m_state->IsDirty());
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
            m_state->ResetProfile();
            m_hasExtraRow = {};
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
    size_t  target = 0;
    size_t  row    = 0;



    m_controller.SetPopupHost (host);
    m_profile.SetPopupHost    (host);

    for (target = 0; target < kTargetCount; target++)
    {
        for (row = 0; row < kMaxRows; row++)
        {
            m_rows[target][row].SetPopupHost (host);
        }
    }

    for (target = 0; target < kAxisCount; target++)
    {
        m_response[target].SetPopupHost (host);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  Layout
//
//  The controller picker across the top, and under it the profile picker
//  with New, Rename and Delete. Below them the joystick: the stick
//  circle on the left, and to its right each paddle axis's rows followed by
//  its options. Then the buttons, each with its light, then the deadzone,
//  calibration and Reset profile.
//
//  Row counts change as mappings are added and removed, so everything below
//  a target's rows moves with them; Relayout reruns this with the last
//  rectangle whenever they change.
//
////////////////////////////////////////////////////////////////////////////////

void ControllersPage::Layout (const RECT & rect, const DxuiDpiScaler & scaler)
{
    UINT    dpi         = scaler.GetDpi();
    int     pad         = scaler.ToPx (s_kPagePadDp);
    int     rowH        = scaler.ToPx (s_kRowHeightDp);
    int     labelWidth  = scaler.ToPx (s_kLabelWidthDp);
    int     rowWidth    = scaler.ToPx (s_kRowWidthDp);
    int     addWidth    = scaler.ToPx (s_kAddWidthDp);
    int     stickSize   = scaler.ToPx (s_kStickSizeDp);
    int     lightSize   = scaler.ToPx (s_kLightSizeDp);
    int     wideWidth   = scaler.ToPx (s_kWideWidthDp);
    int     buttonWidth = scaler.ToPx (s_kButtonWidthDp);
    int     profileBtnW = scaler.ToPx (s_kProfileButtonWidthDp);
    int     optionWidth = scaler.ToPx (s_kOptionWidthDp);
    int     indent      = scaler.ToPx (s_kChildIndentDp);
    int     gap         = scaler.ToPx (s_kGapDp);
    int     sectionGap  = scaler.ToPx (s_kSectionGapDp);
    int     x           = rect.left + pad;
    int     y           = rect.top  + pad;
    int     axesX       = x + stickSize + sectionGap;
    int     stickTop    = 0;
    int     axesBottom  = 0;
    size_t  target      = 0;
    size_t  row         = 0;



    m_lastRect   = rect;
    m_lastScaler = scaler;
    m_hasLayout  = true;

    m_controllerLabel.SetRect (MakeRect (x, y, labelWidth, rowH));
    m_controllerLabel.SetText (L"Controller:");
    m_controller.SetRect      (MakeRect (x + labelWidth, y, wideWidth, rowH));
    y += rowH + gap;

    m_profileLabel.SetRect (MakeRect (x, y, labelWidth, rowH));
    m_profileLabel.SetText (L"Profile:");
    m_profile.SetRect      (MakeRect (x + labelWidth, y, rowWidth, rowH));
    m_newProfile.SetLabel    (L"New...");
    m_newProfile.Layout      (MakeRect (x + labelWidth + rowWidth + gap, y, profileBtnW, rowH));
    m_renameProfile.SetLabel (L"Rename...");
    m_renameProfile.Layout   (MakeRect (x + labelWidth + rowWidth + gap + (profileBtnW + gap), y, profileBtnW, rowH));
    m_deleteProfile.SetLabel (L"Delete...");
    m_deleteProfile.Layout   (MakeRect (x + labelWidth + rowWidth + gap + (profileBtnW + gap) * 2, y, profileBtnW, rowH));
    y += rowH + sectionGap;

    m_joystickHeading.SetRect (MakeRect (x, y, wideWidth, rowH));
    m_joystickHeading.SetText (L"Joystick");
    y += rowH;

    stickTop   = y;
    axesBottom = y;

    m_stick.Layout (MakeRect (x, stickTop, stickSize, stickSize), scaler);

    // The two axes, stacked to the right of the stick.
    for (target = 0; target < kAxisCount; target++)
    {
        size_t  shown = GetShownRows (target);

        m_targetLabel[target].SetRect (MakeRect (axesX, axesBottom, labelWidth, rowH));
        m_targetLabel[target].SetText (s_kTargetNames[target]);

        for (row = 0; row < kMaxRows; row++)
        {
            m_rows[target][row].SetVisible (row < shown);
            m_rows[target][row].SetRect    (MakeRect (axesX + labelWidth, axesBottom + (int) row * (rowH + gap), rowWidth, rowH));
        }

        m_addRow[target].SetLabel (L"+");
        m_addRow[target].Layout   (MakeRect (axesX + labelWidth + rowWidth + gap, axesBottom + (int) (shown - 1) * (rowH + gap), addWidth, rowH));

        axesBottom += (int) shown * (rowH + gap);

        m_invert[target].SetRect  (MakeRect (axesX + labelWidth + indent, axesBottom, optionWidth - indent, rowH));
        m_invert[target].SetLabel (L"Invert");

        m_response[target].SetRect  (MakeRect (axesX + labelWidth + optionWidth, axesBottom, optionWidth, rowH));
        m_response[target].SetItems ({ L"Position", L"Paddle speed" });

        m_speed[target].SetRect          (MakeRect (axesX + labelWidth + optionWidth * 2 + gap, axesBottom, optionWidth, rowH));
        m_speed[target].SetRange         (ControllerProfileStore::kMinMaxSpeed, ControllerProfileStore::kMaxMaxSpeed);
        m_speed[target].SetStep          (16.0f);
        m_speed[target].SetDecimalPlaces (0);
        m_speed[target].SetSuffix        (L"/s");
        m_speed[target].SetTickInterval  (256.0f);

        axesBottom += rowH + sectionGap;
    }

    y = std::max (stickTop + stickSize, axesBottom) + sectionGap;

    // The buttons, each with a light that fills while it reads pressed.
    m_buttonsHeading.SetRect (MakeRect (x, y, wideWidth, rowH));
    m_buttonsHeading.SetText (L"Buttons");
    y += rowH;

    for (target = kAxisCount; target < kTargetCount; target++)
    {
        size_t  shown = GetShownRows (target);
        size_t  light = target - kAxisCount;

        m_lights[light].Layout (MakeRect (x, y + (rowH - lightSize) / 2, lightSize, lightSize), scaler);

        m_targetLabel[target].SetRect (MakeRect (x + lightSize + gap, y, labelWidth - lightSize - gap, rowH));
        m_targetLabel[target].SetText (s_kTargetNames[target]);

        for (row = 0; row < kMaxRows; row++)
        {
            m_rows[target][row].SetVisible (row < shown);
            m_rows[target][row].SetRect    (MakeRect (x + labelWidth, y + (int) row * (rowH + gap), rowWidth, rowH));
        }

        m_addRow[target].SetLabel (L"+");
        m_addRow[target].Layout   (MakeRect (x + labelWidth + rowWidth + gap, y + (int) (shown - 1) * (rowH + gap), addWidth, rowH));

        y += (int) shown * (rowH + gap);
    }

    m_sharedWarning.SetRect  (MakeRect (x, y, wideWidth + labelWidth + buttonWidth, rowH));
    m_sharedWarning.SetColor (0xFFF0A030);   // amber caution, as the sheet's restart notice
    y += rowH + gap;

    m_deadzoneLabel.SetRect (MakeRect (x, y, labelWidth, rowH));
    m_deadzoneLabel.SetText (L"Deadzone:");
    m_deadzone.SetRect          (MakeRect (x + labelWidth, y, wideWidth, rowH));
    m_deadzone.SetRange         (0.0f, 90.0f);
    m_deadzone.SetStep          (1.0f);
    m_deadzone.SetDecimalPlaces (0);
    m_deadzone.SetSuffix        (L"%");
    m_deadzone.SetTickInterval  (10.0f);
    y += rowH + sectionGap;

    m_calibrationLabel.SetRect  (MakeRect (x, y, labelWidth, rowH));
    m_calibrationLabel.SetText  (L"Calibration:");
    m_calibrationStatus.SetRect (MakeRect (x + labelWidth, y, wideWidth + buttonWidth, rowH));
    y += rowH + gap;

    m_calibrate.Layout (MakeRect (x + labelWidth, y, buttonWidth, rowH));
    m_calibrationCancel.SetLabel (L"Cancel");
    m_calibrationCancel.Layout (MakeRect (x + labelWidth + buttonWidth + gap, y, buttonWidth, rowH));
    m_useAutomatic.SetLabel (L"Use automatic");
    m_useAutomatic.Layout (MakeRect (x + labelWidth + (buttonWidth + gap) * 2, y, buttonWidth, rowH));
    y += rowH + sectionGap;

    m_reset.SetLabel (L"Reset profile");
    m_reset.Layout (MakeRect (x + labelWidth, y, buttonWidth, rowH));

    m_controllerLabel.SetDpi (dpi);
    m_controller.SetDpi      (dpi);
    m_profileLabel.SetDpi    (dpi);
    m_profile.SetDpi         (dpi);
    m_newProfile.SetDpi      (dpi);
    m_renameProfile.SetDpi   (dpi);
    m_deleteProfile.SetDpi   (dpi);
    m_joystickHeading.SetDpi (dpi);
    m_buttonsHeading.SetDpi  (dpi);

    for (target = 0; target < kTargetCount; target++)
    {
        m_targetLabel[target].SetDpi (dpi);
        m_addRow[target].SetDpi      (dpi);

        for (row = 0; row < kMaxRows; row++)
        {
            m_rows[target][row].SetDpi (dpi);
        }
    }

    for (target = 0; target < kAxisCount; target++)
    {
        m_invert[target].SetDpi   (dpi);
        m_response[target].SetDpi (dpi);
        m_speed[target].SetDpi    (dpi);
    }

    m_sharedWarning.SetDpi     (dpi);
    m_deadzoneLabel.SetDpi     (dpi);
    m_deadzone.SetDpi          (dpi);
    m_calibrationLabel.SetDpi  (dpi);
    m_calibrationStatus.SetDpi (dpi);
    m_calibrate.SetDpi         (dpi);
    m_calibrationCancel.SetDpi (dpi);
    m_useAutomatic.SetDpi      (dpi);
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
    size_t                            light    = 0;



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

    if (unit.has_value() && m_sampleSource)
    {
        sample = m_sampleSource (unit.value());
    }

    // No reading yet for the controller shown: ask again. The service may
    // not have read it since the request, or something else cleared the
    // request (a closing sheet does), and asking wakes the controller thread.
    if (unit.has_value() && !sample.has_value() && m_onInspect)
    {
        m_onInspect (unit);
    }

    m_stick.SetActive (sample.has_value());

    if (!sample.has_value())
    {
        m_stick.SetValues (127, 127);

        for (light = 0; light < kButtonCount; light++)
        {
            m_lights[light].SetLit (false);
        }

        return;
    }

    if (m_state->IsCapturing() && m_state->FeedCapture (sample.value()))
    {
        m_capturing.reset();
        m_hasExtraRow = {};
        AfterEdit();
    }

    if (m_state->GetCalibrationStep() != CalibrationStep::None)
    {
        m_state->FeedCalibration (sample.value());
    }

    reading = m_state->ComputeLiveReading (sample.value());

    m_stick.SetValues (reading.paddle.has_value() ? reading.paddle.value()[0] : 127,
                       reading.paddle.has_value() ? reading.paddle.value()[1] : 127);

    for (light = 0; light < kButtonCount; light++)
    {
        // A button the machine lacks stays dark whatever is pressed.
        m_lights[light].SetLit (reading.buttons.test (light) && m_state->IsTargetAvailable (TargetAt (kAxisCount + light)));
    }
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
    m_isSyncing           = true;

    m_controller.SetItems    (names);
    m_controller.SetSelected (selected.has_value() ? (int) selected.value() : 0);
    m_controller.SetEnabled  (selected.has_value());

    RefreshProfiles();
    RefreshRows();
    RefreshAxisOptions();
    RefreshCalibration();

    m_deadzone.SetValue   (m_state->GetDeadzone() * 100.0f);
    m_deadzone.SetEnabled (selected.has_value());
    m_reset.SetEnabled    (selected.has_value());

    m_isSyncing = false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  RefreshProfiles
//
//  The Default can be edited and reset but not renamed or deleted, so those
//  two are unavailable while it is the profile shown.
//
////////////////////////////////////////////////////////////////////////////////

void ControllersPage::RefreshProfiles()
{
    std::vector<std::wstring>  items;
    std::string                edited   = m_state->GetEditedProfileName();
    bool                       hasUnit  = m_state->GetSelectedIndex().has_value();
    bool                       canEdit  = hasUnit && !m_state->IsEditingDefaultProfile();
    size_t                     i        = 0;
    int                        selected = 0;



    m_profileNames = m_state->GetProfileNames();

    for (i = 0; i < m_profileNames.size(); i++)
    {
        items.push_back (Utf8ToWide (m_profileNames[i]));

        if (m_profileNames[i] == edited)
        {
            selected = (int) i;
        }
    }

    m_profile.SetItems    (items);
    m_profile.SetSelected (selected);
    m_profile.SetEnabled  (hasUnit);

    m_newProfile.SetEnabled    (hasUnit);
    m_renameProfile.SetEnabled (canEdit);
    m_deleteProfile.SetEnabled (canEdit);
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnProfileSelect
//
//  Leaving a profile with unapplied edits asks first (AskToSaveProfileEdits).
//
////////////////////////////////////////////////////////////////////////////////

void ControllersPage::OnProfileSelect (int index)
{
    std::string  name;



    if (m_state == nullptr || index < 0 || (size_t) index >= m_profileNames.size())
    {
        return;
    }

    name = m_profileNames[(size_t) index];

    if (name == m_state->GetEditedProfileName())
    {
        return;
    }

    AskToSaveProfileEdits ([this, name] () { SwitchProfile (name); });
}





////////////////////////////////////////////////////////////////////////////////
//
//  SwitchProfile
//
////////////////////////////////////////////////////////////////////////////////

void ControllersPage::SwitchProfile (const std::string & name)
{
    m_state->SelectProfile (name);
    m_capturing.reset();
    m_hasExtraRow = {};
    AfterEdit();
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnNewProfile
//
//  A new profile becomes the one being edited, so leaving a profile with
//  unapplied edits asks first, exactly as switching profiles does. The New
//  dialog opens only once the answer is Save (and the save went through) or
//  Discard; Cancel leaves the page as it was.
//
////////////////////////////////////////////////////////////////////////////////

void ControllersPage::OnNewProfile()
{
    if (m_state == nullptr || !m_state->GetSelectedIndex().has_value())
    {
        return;
    }

    AskToSaveProfileEdits ([this] () { OpenNewProfileDialog(); });
}





////////////////////////////////////////////////////////////////////////////////
//
//  SwitchController
//
////////////////////////////////////////////////////////////////////////////////

void ControllersPage::SwitchController (size_t index)
{
    m_state->SelectController (index);
    m_capturing.reset();
    m_hasExtraRow = {};
    RebuildChoices();
    AfterEdit();
}





////////////////////////////////////////////////////////////////////////////////
//
//  AskToSaveProfileEdits
//
//  Leaving the edited profile -- for another profile, a new one, or another
//  controller -- asks first when it has unapplied edits. Save commits the
//  model now, so Cancel on the sheet no longer undoes it; Discard puts the
//  profile back as last committed; Cancel stays put with the edits pending.
//  Until the user answers, the drop-downs go on showing what is being left,
//  and a save that fails stays there too.
//
////////////////////////////////////////////////////////////////////////////////

void ControllersPage::AskToSaveProfileEdits (std::function<void()> proceed)
{
    if (!m_state->HasUnappliedProfileEdits())
    {
        proceed();
        return;
    }

    Refresh();

    m_profileDialog.OpenSaveOrDiscard (Utf8ToWide (m_state->GetEditedProfileName()),
        [this, proceed] (const std::wstring &, ProfileSource)
        {
            HRESULT  hr = m_onCommitProfile ? m_state->SaveProfileEdits (m_onCommitProfile) : E_FAIL;

            if (SUCCEEDED (hr))
            {
                proceed();
            }
            else
            {
                Refresh();
            }

            return ProfileEditResult::Ok;
        },
        [this, proceed] ()
        {
            m_state->DiscardProfileEdits();
            AfterEdit();
            proceed();
        },
        [this] ()
        {
            Refresh();
        });

    ShowDialog();
}





////////////////////////////////////////////////////////////////////////////////
//
//  OpenNewProfileDialog
//
////////////////////////////////////////////////////////////////////////////////

void ControllersPage::OpenNewProfileDialog()
{
    std::string  current = m_state->GetEditedProfileName();



    m_profileDialog.OpenNew (Utf8ToWide (current),
        [this, current] (const std::wstring & name, ProfileSource source)
        {
            ProfileEditResult  result = m_state->CreateProfile (WideToUtf8 (name), source, current);

            if (result == ProfileEditResult::Ok)
            {
                m_capturing.reset();
                m_hasExtraRow = {};
                AfterEdit();
            }

            return result;
        });

    ShowDialog();
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnRenameProfile
//
////////////////////////////////////////////////////////////////////////////////

void ControllersPage::OnRenameProfile()
{
    if (m_state == nullptr || m_state->IsEditingDefaultProfile())
    {
        return;
    }

    m_profileDialog.OpenRename (Utf8ToWide (m_state->GetEditedProfileName()),
        [this] (const std::wstring & name, ProfileSource)
        {
            ProfileEditResult  result = m_state->RenameProfile (WideToUtf8 (name));

            if (result == ProfileEditResult::Ok)
            {
                AfterEdit();
            }

            return result;
        });

    ShowDialog();
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnDeleteProfile
//
////////////////////////////////////////////////////////////////////////////////

void ControllersPage::OnDeleteProfile()
{
    if (m_state == nullptr || m_state->IsEditingDefaultProfile())
    {
        return;
    }

    m_profileDialog.OpenConfirmDelete (Utf8ToWide (m_state->GetEditedProfileName()),
        [this] (const std::wstring &, ProfileSource)
        {
            ProfileEditResult  result = m_state->DeleteProfile();

            m_capturing.reset();
            m_hasExtraRow = {};
            AfterEdit();

            return result;
        });

    ShowDialog();
}





////////////////////////////////////////////////////////////////////////////////
//
//  ShowDialog
//
//  The sheet repaints so the dialog appears at once.
//
////////////////////////////////////////////////////////////////////////////////

void ControllersPage::ShowDialog()
{
    if (m_onLayoutChanged)
    {
        m_onLayoutChanged();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  RebuildChoices
//
//  Each target's controls, from the selected controller. An axis target
//  offers the analog controls and each D-pad as a left/right or up/down pair;
//  a button target offers every control.
//
////////////////////////////////////////////////////////////////////////////////

void ControllersPage::RebuildChoices()
{
    std::optional<size_t>  selected = m_state != nullptr ? m_state->GetSelectedIndex() : std::nullopt;
    size_t                 target   = 0;



    for (target = 0; target < kTargetCount; target++)
    {
        m_choices[target].clear();

        if (!selected.has_value())
        {
            continue;
        }

        for (const ControlId & control : m_state->GetControllers()[selected.value()].controls)
        {
            bool  isAnalog = control.kind == ControlKind::Axis || control.kind == ControlKind::Trigger;

            if (target >= kAxisCount || isAnalog)
            {
                m_choices[target].push_back ({ false, control, {} });
            }
            else if (control.kind == ControlKind::DpadLeft)
            {
                m_choices[target].push_back ({ true, control, { ControlKind::DpadRight, control.index } });
            }
            else if (control.kind == ControlKind::DpadUp)
            {
                m_choices[target].push_back ({ true, control, { ControlKind::DpadDown, control.index } });
            }
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  RefreshRows
//
//  Every drop-down's items and selection, and which rows and "+" buttons
//  show. A binding the list does not offer -- a pair built some other way,
//  say -- is added to its own row's list so the row still names it.
//
////////////////////////////////////////////////////////////////////////////////

void ControllersPage::RefreshRows()
{
    ControllerKind          kind    = GetSelectedKind();
    bool                    hasUnit = m_state->GetSelectedIndex().has_value();
    std::vector<ControlId>  shared  = m_state->GetSharedControls();
    size_t                  target  = 0;
    size_t                  row     = 0;



    for (target = 0; target < kTargetCount; target++)
    {
        PaddleTarget  paddleTarget = TargetAt (target);
        bool          available    = hasUnit && m_state->IsTargetAvailable (paddleTarget);
        size_t        count        = GetBindingCount (target);
        size_t        shown        = GetShownRows (target);

        for (row = 0; row < kMaxRows; row++)
        {
            std::vector<std::wstring>  items;
            std::vector<std::wstring>  glyphs;
            bool                       isCapturing = m_capturing.has_value() && m_capturing->first == target && m_capturing->second == row;
            int                        choice      = FindChoice (target, row);

            items.push_back (isCapturing ? L"Press a control..." : L"Press to assign...");
            items.push_back (available ? L"None"
                                       : L"Not supported on " + (m_state->GetMachineName().empty() ? std::wstring (L"this machine") : m_state->GetMachineName()));
            glyphs.resize   (items.size());

            for (const ControlChoice & entry : m_choices[target])
            {
                items.push_back  (entry.isPair ? ControlLabels::For (kind, entry.control) + L" / " + ControlLabels::For (kind, entry.positive)
                                               : ControlLabels::For (kind, entry.control));
                glyphs.push_back (ControlLabels::GlyphFor (kind, entry.control));
            }

            if (row < count && choice < 0)
            {
                const ControlMapping &  mapping = m_state->GetMapping();

                items.push_back (target < kAxisCount ? DescribeAxis   (kind, (target == 0 ? mapping.pdl0 : mapping.pdl1)[row])
                                                     : DescribeButton (kind, (target == 2 ? mapping.pb0 : target == 3 ? mapping.pb1 : mapping.pb2)[row]));
                choice = (int) items.size() - 1;
            }

            m_rows[target][row].SetItems      (items);
            m_rows[target][row].SetItemGlyphs (glyphs);
            // A target the machine lacks says so, rather than showing a
            // binding saved from a machine that has it as though it applied.
            m_rows[target][row].SetSelected (!available  ? kNoneItem
                                             : isCapturing ? kPressToAssignItem
                                             : (row < count ? choice : kNoneItem));
            m_rows[target][row].SetEnabled  (available);
            m_rows[target][row].SetVisible  (row < shown);
        }

        m_addRow[target].SetEnabled (available && count > 0 && shown < kMaxRows && !m_hasExtraRow[target]);
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
//  The first analog binding on each axis speaks for its options, since the
//  options set every analog binding on the axis at once.
//
////////////////////////////////////////////////////////////////////////////////

void ControllersPage::RefreshAxisOptions()
{
    const ControlMapping &  mapping = m_state->GetMapping();
    size_t                  axis    = 0;



    for (axis = 0; axis < kAxisCount; axis++)
    {
        const std::vector<AxisBinding> &  bindings = (axis == 0) ? mapping.pdl0 : mapping.pdl1;
        const AxisBinding *               analog   = nullptr;

        for (const AxisBinding & binding : bindings)
        {
            if (binding.kind == AxisBindingKind::Analog)
            {
                analog = &binding;
                break;
            }
        }

        m_invert[axis].SetEnabled   (analog != nullptr);
        m_response[axis].SetEnabled (analog != nullptr);
        m_speed[axis].SetEnabled    (analog != nullptr && analog->response == AxisResponse::Rate);

        if (analog != nullptr)
        {
            m_invert[axis].SetChecked    (analog->inverted);
            m_response[axis].SetSelected (analog->response == AxisResponse::Rate ? 1 : 0);
            m_speed[axis].SetValue       (analog->maxSpeed);
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
    std::optional<size_t>  selected     = m_state->GetSelectedIndex();
    bool                   canCalibrate = m_state->IsCalibratable();
    CalibrationStep        step         = m_state->GetCalibrationStep();
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
        m_calibrationStatus.SetText (hasUser ? L"Calibrated" : L"Automatic");
        m_calibrate.SetLabel (L"Calibrate");
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnRowSelect
//
//  "Press to assign..." waits for a control to replace the row's, or to fill
//  an empty row; "None" removes the row's control, which removes an added
//  row outright; a control replaces the row's or fills it.
//
////////////////////////////////////////////////////////////////////////////////

void ControllersPage::OnRowSelect (size_t target, size_t row, int item)
{
    std::optional<size_t>            selected;
    std::optional<ControllerSample>  baseline;
    size_t                           count    = 0;
    size_t                           choice   = 0;



    if (m_state == nullptr)
    {
        return;
    }

    selected = m_state->GetSelectedIndex();
    count    = GetBindingCount (target);

    if (!selected.has_value())
    {
        return;
    }

    if (item == kPressToAssignItem)
    {
        if (m_sampleSource)
        {
            baseline = m_sampleSource (m_state->GetControllers()[selected.value()].unit);
        }

        m_state->BeginCapture (TargetAt (target), baseline.value_or (ControllerSample()),
                               row < count ? std::optional<size_t> (row) : std::nullopt);
        m_capturing = std::make_pair (target, row);
        Refresh();
        return;
    }

    if (m_capturing.has_value())
    {
        m_state->CancelCapture();
        m_capturing.reset();
    }

    if (item == kNoneItem)
    {
        if (row < count)
        {
            m_state->RemoveBinding (TargetAt (target), row);
        }

        m_hasExtraRow[target] = false;
        AfterEdit();
        return;
    }

    choice = (size_t) (item - kFirstControlItem);

    if (choice >= m_choices[target].size())
    {
        return;
    }

    if (target < kAxisCount)
    {
        AxisBinding  binding;

        if (m_choices[target][choice].isPair)
        {
            binding.kind     = AxisBindingKind::DigitalPair;
            binding.negative = m_choices[target][choice].control;
            binding.positive = m_choices[target][choice].positive;
        }
        else
        {
            binding.analog = m_choices[target][choice].control;
        }

        if (row < count)
        {
            m_state->ReplaceAxisBinding (TargetAt (target), row, binding);
        }
        else
        {
            m_state->AddAxisBinding (TargetAt (target), binding);
        }
    }
    else
    {
        ButtonBinding  binding;

        binding.control = m_choices[target][choice].control;

        if (binding.control.kind == ControlKind::Trigger)
        {
            binding.threshold = ButtonBinding::kTriggerThreshold;
        }

        if (row < count)
        {
            m_state->ReplaceButtonBinding (TargetAt (target), row, binding);
        }
        else
        {
            m_state->AddButtonBinding (TargetAt (target), binding);
        }
    }

    m_hasExtraRow[target] = false;
    AfterEdit();
}





////////////////////////////////////////////////////////////////////////////////
//
//  AddRow
//
//  Waits at once for the control the new row will hold. The sheet shows a
//  prompt over the page while it waits, so the user is not left wondering
//  what a click on "+" did; Escape or a click calls it off, and the row is
//  never added.
//
////////////////////////////////////////////////////////////////////////////////

void ControllersPage::AddRow (size_t target)
{
    std::optional<size_t>            selected;
    std::optional<ControllerSample>  baseline;
    size_t                           count    = GetBindingCount (target);



    if (m_state == nullptr || count >= kMaxRows)
    {
        return;
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

    m_state->BeginCapture (TargetAt (target), baseline.value_or (ControllerSample()), std::nullopt);
    m_capturing = std::make_pair (target, count);
    Relayout();
}





////////////////////////////////////////////////////////////////////////////////
//
//  SetAxisInverted
//
////////////////////////////////////////////////////////////////////////////////

void ControllersPage::SetAxisInverted (size_t axis, bool inverted)
{
    size_t  count = GetBindingCount (axis);
    size_t  i     = 0;



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
    size_t  count = GetBindingCount (axis);
    size_t  i     = 0;



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
    Relayout();
    MarkDirty (m_state != nullptr && m_state->IsDirty());
}





////////////////////////////////////////////////////////////////////////////////
//
//  Relayout
//
//  Rows came or went, which moves everything below them and changes what Tab
//  reaches; the sheet is told so it can rebuild its tab order.
//
////////////////////////////////////////////////////////////////////////////////

void ControllersPage::Relayout()
{
    if (m_hasLayout)
    {
        Layout (m_lastRect, m_lastScaler);
    }
    else
    {
        Refresh();
    }

    if (m_onLayoutChanged)
    {
        m_onLayoutChanged();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  IsCapturing
//
////////////////////////////////////////////////////////////////////////////////

bool ControllersPage::IsCapturing() const
{
    return m_state != nullptr && m_state->IsCapturing();
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetCapturePrompt
//
////////////////////////////////////////////////////////////////////////////////

std::wstring ControllersPage::GetCapturePrompt() const
{
    std::optional<size_t>  selected = m_state != nullptr ? m_state->GetSelectedIndex() : std::nullopt;
    std::wstring           target   = m_capturing.has_value() ? std::wstring (s_kTargetNames[m_capturing->first]) : std::wstring();



    if (!target.empty() && target.back() == L':')
    {
        target.pop_back();
    }

    if (!selected.has_value())
    {
        return L"Press a control.";
    }

    return L"Press a control on " + m_state->GetControllers()[selected.value()].description + L" for " + target + L".";
}





////////////////////////////////////////////////////////////////////////////////
//
//  CancelCapture
//
////////////////////////////////////////////////////////////////////////////////

void ControllersPage::CancelCapture()
{
    if (m_state != nullptr)
    {
        m_state->CancelCapture();
    }

    m_capturing.reset();
    Relayout();
}





////////////////////////////////////////////////////////////////////////////////
//
//  SetOnLayoutChanged
//
////////////////////////////////////////////////////////////////////////////////

void ControllersPage::SetOnLayoutChanged (std::function<void()> onLayoutChanged)
{
    m_onLayoutChanged = std::move (onLayoutChanged);
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetBindingCount
//
////////////////////////////////////////////////////////////////////////////////

size_t ControllersPage::GetBindingCount (size_t target) const
{
    static const ControlMapping  s_kEmpty;
    const ControlMapping &       mapping = m_state != nullptr ? m_state->GetMapping() : s_kEmpty;



    switch (target)
    {
        case 0:  return mapping.pdl0.size();
        case 1:  return mapping.pdl1.size();
        case 2:  return mapping.pb0.size();
        case 3:  return mapping.pb1.size();
        default: return mapping.pb2.size();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetShownRows
//
//  One row per control, plus an empty row "+" asked for, plus a row waiting
//  on press-to-assign past the end; never fewer than one, never more than
//  kMaxRows.
//
////////////////////////////////////////////////////////////////////////////////

size_t ControllersPage::GetShownRows (size_t target) const
{
    size_t  count = GetBindingCount (target);
    size_t  shown = count;



    if (m_hasExtraRow[target])
    {
        shown++;
    }

    if (m_capturing.has_value() && m_capturing->first == target && m_capturing->second >= count)
    {
        shown = std::max (shown, m_capturing->second + 1);
    }

    return std::clamp (shown, (size_t) 1, kMaxRows);
}





////////////////////////////////////////////////////////////////////////////////
//
//  FindChoice
//
//  The drop-down item for the control on a row, or -1 when the row has no
//  control or the list does not offer it.
//
////////////////////////////////////////////////////////////////////////////////

int ControllersPage::FindChoice (size_t target, size_t row) const
{
    const ControlMapping &  mapping = m_state->GetMapping();
    size_t                  i       = 0;



    if (row >= GetBindingCount (target))
    {
        return -1;
    }

    for (i = 0; i < m_choices[target].size(); i++)
    {
        const ControlChoice &  choice  = m_choices[target][i];
        bool                   isMatch = false;

        if (target < kAxisCount)
        {
            const AxisBinding &  binding = (target == 0 ? mapping.pdl0 : mapping.pdl1)[row];

            isMatch = choice.isPair ? (binding.kind == AxisBindingKind::DigitalPair && binding.negative == choice.control && binding.positive == choice.positive)
                                    : (binding.kind == AxisBindingKind::Analog && binding.analog == choice.control);
        }
        else
        {
            const ButtonBinding &  binding = (target == 2 ? mapping.pb0 : target == 3 ? mapping.pb1 : mapping.pb2)[row];

            isMatch = binding.control == choice.control;
        }

        if (isMatch)
        {
            return kFirstControlItem + (int) i;
        }
    }

    return -1;
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
    if (binding.kind == AxisBindingKind::DigitalPair)
    {
        return ControlLabels::For (kind, binding.negative) + L" / " + ControlLabels::For (kind, binding.positive);
    }

    return ControlLabels::For (kind, binding.analog);
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





////////////////////////////////////////////////////////////////////////////////
//
//  Utf8ToWide
//
//  Profile names are kept as UTF-8, as the prefs file stores them.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring ControllersPage::Utf8ToWide (const std::string & text)
{
    int           length = 0;
    std::wstring  wide;



    if (text.empty())
    {
        return wide;
    }

    length = MultiByteToWideChar (CP_UTF8, 0, text.data(), (int) text.size(), nullptr, 0);

    if (length <= 0)
    {
        return wide;
    }

    wide.resize ((size_t) length);
    MultiByteToWideChar (CP_UTF8, 0, text.data(), (int) text.size(), wide.data(), length);

    return wide;
}





////////////////////////////////////////////////////////////////////////////////
//
//  WideToUtf8
//
////////////////////////////////////////////////////////////////////////////////

std::string ControllersPage::WideToUtf8 (const std::wstring & text)
{
    int          length = 0;
    std::string  narrow;



    if (text.empty())
    {
        return narrow;
    }

    length = WideCharToMultiByte (CP_UTF8, 0, text.data(), (int) text.size(), nullptr, 0, nullptr, nullptr);

    if (length <= 0)
    {
        return narrow;
    }

    narrow.resize ((size_t) length);
    WideCharToMultiByte (CP_UTF8, 0, text.data(), (int) text.size(), narrow.data(), length, nullptr, nullptr);

    return narrow;
}
