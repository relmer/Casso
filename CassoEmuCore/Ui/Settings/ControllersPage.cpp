#include "Pch.h"

#include "ControllersPage.h"

#include "Controllers/ControlLabels.h"
#include "Controllers/ControllerTokens.h"
#include "Controllers/PlayerModeRules.h"

#include "Widgets/DxuiTreeView.h"
#include "Window/DxuiHwndSource.h"





// Layout metrics (DIP), matching the other settings pages.
static constexpr int  s_kRowHeightDp            = 28;
static constexpr int  s_kLabelWidthDp           = 120;
static constexpr int  s_kRowWidthDp             = 220;
static constexpr int  s_kAddWidthDp             = 28;
static constexpr int  s_kStickSizeDp            = 190;
static constexpr int  s_kLightSizeDp            = 22;
static constexpr int  s_kWideWidthDp            = 340;
static constexpr int  s_kButtonWidthDp          = 130;
static constexpr int  s_kProfileButtonWidthDp   = 90;
static constexpr int  s_kOptionWidthDp          = 110;
static constexpr int  s_kPlayerModeWidthDp      = 170;
static constexpr int  s_kGapDp                  = 6;
static constexpr int  s_kMessageGapDp           = 4;
static constexpr int  s_kSectionGapDp           = 14;
static constexpr int  s_kPagePadDp              = 16;
static constexpr int  s_kWarningPadYDp          = 4;

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



    for (target = 0; target < kPlayerCount; target++)
    {
        Adopt (m_playerLabel[target]);
        Adopt (m_playerEntry[target]);
        Adopt (m_playerMode[target]);
        Adopt (m_playerWarning[target]);

        // A description or a mode too long for its drop-down keeps its end,
        // where two units of a model differ.
        m_playerEntry[target].SetElide (DxuiElide::Middle);
        m_playerMode[target].SetElide  (DxuiElide::Middle);

        m_playerWarning[target].SetSeverity (DxuiInfoBanner::Severity::Info);
        m_playerWarning[target].SetVisible  (false);

        // A one-line notice among the player rows, so it hugs its text.
        m_playerWarning[target].SetVerticalPaddingDip ((float) s_kWarningPadYDp);
    }

    Adopt (m_controllerLabel);
    Adopt (m_controller);

    // A device description keeps both ends, as the players' entries do.
    m_controller.SetElide (DxuiElide::Middle);
    Adopt (m_profileLabel);
    Adopt (m_profile);
    Adopt (m_newProfile);
    Adopt (m_renameProfile);
    Adopt (m_deleteProfile);
    Adopt (m_joystickHeading);
    Adopt (m_stick);
    Adopt (m_buttonsHeading);

    // Each target's rows sit in its table, which makes them as they are
    // needed; every target has its first.
    for (target = 0; target < kTargetCount; target++)
    {
        Adopt (m_targetLabel[target]);
        Adopt (m_tables[target]);
        Adopt (m_addRow[target]);

        EnsureRows (target, 1);
    }

    for (target = 0; target < kButtonCount; target++)
    {
        Adopt (m_lights[target]);
    }


    Adopt (m_switchView);

    for (target = 0; target < kAxisCount; target++)
    {
        Adopt (m_paddleBars[target]);
        Adopt (m_invert[target]);
        Adopt (m_response[target]);
        Adopt (m_speedLabel[target]);
        Adopt (m_speed[target]);
    }

    // A warning under each target's rows, compact like the players', with the
    // outlined MDL2 warning glyph the app's other warnings show.
    for (target = 0; target < kTargetCount; target++)
    {
        Adopt (m_sharedWarning[target]);

        m_sharedWarning[target].SetSeverity           (DxuiInfoBanner::Severity::Warning);
        m_sharedWarning[target].SetIconGlyph          (s_kpszMdl2Warning);
        m_sharedWarning[target].SetVisible            (false);
        m_sharedWarning[target].SetVerticalPaddingDip ((float) s_kWarningPadYDp);
    }

    Adopt (m_deadZoneLabel);
    Adopt (m_deadZone);
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
    // Each loop below scopes its own index. The three walk arrays of three
    // different lengths, and one index shared across them reads to the
    // analyzer as a single range wide enough to leave the shortest array.
    m_state = state;

    m_controller.SetSelect ([this] (int item)
    {
        size_t  index = 0;



        if (m_isSyncing || m_state == nullptr || item < 0 || (size_t) item >= m_editingIndices.size())
        {
            return;
        }

        index = m_editingIndices[(size_t) item];

        if (m_state->GetSelectedIndex() == std::optional<size_t> (index))
        {
            return;
        }

        AskToSaveProfileEdits ([this, index] () { SwitchController (index); });
    });

    m_profile.SetSelect ([this] (int index)
    {
        if (!m_isSyncing)
        {
            OnProfileSelect (index);
        }
    });


    for (size_t target = 0; target < kPlayerCount; target++)
    {
        m_playerEntry[target].SetSelect ([this, target] (int item)
        {
            if (!m_isSyncing)
            {
                OnPlayerEntrySelect (target, item);
            }
        });

        m_playerMode[target].SetSelect ([this, target] (int item)
        {
            if (!m_isSyncing)
            {
                OnPlayerModeSelect (target, item);
            }
        });
    }

    m_newProfile.SetOnClick    ([this] () { OnNewProfile(); });
    m_renameProfile.SetOnClick ([this] () { OnRenameProfile(); });
    m_deleteProfile.SetOnClick ([this] () { OnDeleteProfile(); });

    for (size_t target = 0; target < kTargetCount; target++)
    {
        // Bounded again, although the loop already bounds it. The analyzer
        // widens the index across the loop body and reads this as a write
        // past the end.
        if (target < m_addRow.size())
        {
            m_addRow[target].SetOnClick ([this, target] () { AddRow (target); });
        }
    }

    for (size_t target = 0; target < kAxisCount; target++)
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

    m_deadZone.SetOnChange ([this] (float percent)
    {
        if (!m_isSyncing && m_state != nullptr)
        {
            m_state->SetDeadZone (percent / 100.0f);
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



    m_popupHost = host;

    m_controller.SetPopupHost    (host);
    m_profile.SetPopupHost       (host);
    m_profileDialog.SetPopupHost (host);

    for (target = 0; target < kPlayerCount; target++)
    {
        m_playerEntry[target].SetPopupHost (host);
        m_playerMode[target].SetPopupHost  (host);
    }

    for (target = 0; target < kTargetCount; target++)
    {
        for (const std::unique_ptr<DxuiComboBox> & row : m_rows[target])
        {
            row->SetPopupHost (host);
        }
    }

    for (target = 0; target < kAxisCount; target++)
    {
        m_response[target].SetPopupHost (host);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetMeasuringRenderer
//
//  The renderer Layout measures text with: one set for the page, else the
//  popup host's, asked each time because the host rebuilds it when the
//  device is lost. Null before the host has one.
//
////////////////////////////////////////////////////////////////////////////////

IDxuiTextRenderer * ControllersPage::GetMeasuringRenderer() const
{
    if (m_textRenderer != nullptr)
    {
        return m_textRenderer;
    }

    return (m_popupHost != nullptr) ? m_popupHost->GetTextRenderer() : nullptr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Layout
//
//  The controller picker across the top, and under it the profile picker
//  with New, Rename and Delete. Below them the joystick: under its heading,
//  indented, each paddle axis's rows followed by its options, and to their
//  right the picture of the controller -- the stick circle, the Joyport's
//  switches or the paddle bars. Then the buttons, each row indented the same
//  and its light in the picture's column, then the dead zone, calibration
//  and Reset profile. The mapping drop-downs, Invert, the paddle speed
//  slider's track and the dead zone slider's track share one left edge.
//
//  Row counts change as mappings are added and removed, so everything below
//  a target's rows moves with them; Relayout reruns this with the last
//  rectangle whenever they change.
//
//  The page is taller than the sheet has room for once the players sit above
//  the rest, so it reports its content height -- down to Reset profile and
//  the page padding under it -- and the sheet scrolls it. The height is
//  reported last, after the page is fully laid out, since the sheet may lay
//  the page out again in response.
//
//  Each player's row is its entry and its mode, with a warning under it
//  while the Joyport has taken the player's buttons. Both rows are always
//  there: Player 2's Disabled entry is how two-player play is turned off.
//  The mode drop-down is as wide as the widest mode and ends at the right
//  edge of the Profile row at the design width, moving right with a wider
//  sheet; the entry, and Editing below it, take the rest of the row. What each player drives is the heading above the input
//  picture, for the controller in Editing.
//
////////////////////////////////////////////////////////////////////////////////

void ControllersPage::Layout (const RECT & rect, const DxuiDpiScaler & scaler)
{
    UINT                 dpi         = scaler.GetDpi();
    int                  pad         = scaler.ToPx (s_kPagePadDp);
    int                  rowH        = scaler.ToPx (s_kRowHeightDp);
    int                  labelWidth  = scaler.ToPx (s_kLabelWidthDp);
    int                  rowWidth    = scaler.ToPx (s_kRowWidthDp);
    int                  addWidth    = scaler.ToPx (s_kAddWidthDp);
    int                  stickSize   = scaler.ToPx (s_kStickSizeDp);
    int                  lightSize   = scaler.ToPx (s_kLightSizeDp);
    int                  wideWidth   = scaler.ToPx (s_kWideWidthDp);
    int                  buttonWidth = scaler.ToPx (s_kButtonWidthDp);
    int                  profileBtnW = scaler.ToPx (s_kProfileButtonWidthDp);
    int                  optionWidth = scaler.ToPx (s_kOptionWidthDp);
    int                  indent      = scaler.ToPx (DxuiTreeView::kIndentDip);
    int                  gap         = scaler.ToPx (s_kGapDp);
    int                  sectionGap  = scaler.ToPx (s_kSectionGapDp);
    int                  trackInset  = scaler.ToPx (DxuiSlider::kTrackInsetDip);
    int                  x           = rect.left + pad;
    int                  y           = rect.top  + pad;
    int                  rowsX       = x + indent;
    int                  columnX     = x + labelWidth;
    int                  columnEnd   = columnX + rowWidth + gap + addWidth;
    int                  labelW      = columnX - gap - rowsX;
    int                  profileEnd  = x + labelWidth + rowWidth + gap + (profileBtnW + gap) * 2 + profileBtnW;
    int                  pictureX    = profileEnd - stickSize;
    int                  stickTop    = 0;
    int                  axesBottom  = 0;
    int                  contentH    = 0;
    int                  playerStep  = rowH + gap;
    int                  warnW       = columnEnd - rowsX;
    int                  stretch     = GetDesignWidthPx() > 0 ? std::max (0, (int) (rect.right - rect.left) - GetDesignWidthPx()) : 0;
    bool                 isTwoPlayer = m_state != nullptr && m_state->IsMultiplayerEnabled();
    bool                 isJoyport   = IsJoyportMode();
    bool                 isPaddles   = !isJoyport && IsPaddlesMode();
    int                  responseW   = 0;
    int                  responseX   = 0;
    int                  messageX    = pictureX + lightSize + scaler.ToPx (s_kMessageGapDp);
    size_t               target      = 0;
    size_t               player      = 0;
    IDxuiTextRenderer  * text        = GetMeasuringRenderer();



    m_lastRect   = rect;
    m_lastScaler = scaler;
    m_hasLayout  = true;

    m_revealedWarning.reset();

    // The players lead the page, since who is playing decides what
    // everything below edits. A player whose buttons the Joyport has taken
    // has a warning under its row, as wide as the row's drop-downs.
    for (player = 0; player < kPlayerCount; player++)
    {
        int           warnW   = profileEnd - (x + labelWidth);
        int           warnH   = 0;
        std::wstring  warning = m_state != nullptr ? m_state->GetButtonsCutNotice (player) : std::wstring();

        m_playerLabel[player].SetRect (MakeRect (x, y, labelWidth, rowH));
        m_playerLabel[player].SetText (player == 0 ? L"Player 1:" : L"Player 2:");
        m_playerEntry[player].SetRect (MakeRect (x + labelWidth, y, 0, rowH));
        m_playerMode[player].SetRect  (MakeRect (x + labelWidth, y, 0, rowH));

        y += playerStep;

        m_isWarningShown[player] = !warning.empty();
        m_playerWarning[player].SetText    (warning);
        m_playerWarning[player].SetVisible (m_isWarningShown[player]);
        m_playerWarning[player].SetDpi     (dpi);

        if (!m_isWarningShown[player])
        {
            continue;
        }

        // Measured whenever there is a renderer to measure with: the estimate
        // is generous, and a line it reserves that the text does not use
        // shows as a blank band inside the border.
        if (text != nullptr)
        {
            warnH = (int) std::ceil (m_playerWarning[player].GetMeasuredHeightPx (*text, (float) warnW, scaler));
        }
        else
        {
            warnH = (int) std::ceil (m_playerWarning[player].GetPreferredHeightPx ((float) warnW, scaler));
        }

        m_playerWarning[player].SetRect (MakeRect (x + labelWidth, y, warnW, warnH));

        y += warnH + gap;
    }

    y += sectionGap - gap;
    m_controllerLabel.SetRect (MakeRect (x, y, labelWidth, rowH));

    // While two people play, this drop-down chooses whose mappings the rest
    // of the page edits rather than who drives the game port, which the
    // slots above decide.
    m_controllerLabel.SetText (isTwoPlayer ? L"Editing:" : L"Controller:");
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

    // The heading gives what this controller drives: its joystick, paddle,
    // paddles or Joyport jack. In a jack the stick's square shows the
    // switches it closes instead, and in a paddle mode a bar per paddle.
    m_isJoyportShown = isJoyport;
    m_isPaddlesShown = isPaddles;

    m_joystickHeading.SetRect (MakeRect (x, y, wideWidth, rowH));
    m_joystickHeading.SetText (m_state != nullptr ? m_state->GetEditedHeading() : std::wstring (L"Joystick"));
    y += rowH;

    stickTop   = y;
    axesBottom = y;

    // The stick and the Joyport's switches end at the Delete button's right
    // edge, as the players' rows do; a paddle's bar goes in its own row below.
    m_stick.SetVisible (!isJoyport && !isPaddles);
    m_stick.Layout (MakeRect (pictureX, stickTop, stickSize, stickSize), scaler);
    m_switchView.SetVisible (isJoyport);
    m_switchView.Layout     (MakeRect (pictureX, stickTop, stickSize, stickSize), scaler);

    // The two axes, stacked under the heading, their labels indented as a
    // child setting's are, with the picture to their right. An axis this
    // controller's player does not drive is GONE rather than grayed: a row
    // that cannot do anything is one more thing to read past.
    for (target = 0; target < kAxisCount; target++)
    {
        int           tableH    = 0;
        bool          isInPlay  = IsTargetShown (target);
        bool          isSpeed   = isInPlay && m_state != nullptr && m_state->IsPaddleSpeedShown (TargetAt (target));
        std::wstring  playLabel = m_state != nullptr ? m_state->GetTargetPlayLabel (TargetAt (target)) : std::wstring();

        m_targetLabel[target].SetVisible   (isInPlay);
        m_targetLabel[target].SetRect      (MakeRect (rowsX, axesBottom, labelW, rowH));
        m_targetLabel[target].SetTextAlign (DxuiTextHAlign::Left, DxuiTextVAlign::Center);

        // While two play, a row is labeled with the paddle the guest reads it
        // on: a player holding the second joystick drives PDL2 and PDL3. With
        // the Joyport attached its label is the switches it closes.
        m_targetLabel[target].SetText (GetRowLabel (target, playLabel, isJoyport, isPaddles));

        // A paddle's bar is on its first row, level with its label, ending
        // where the stick would.
        m_paddleBars[target].SetVisible (isPaddles && isInPlay);
        m_paddleBars[target].Layout     (MakeRect (pictureX, axesBottom, profileEnd - pictureX, rowH), scaler);

        tableH = LayOutTable (target, columnX, axesBottom, rowWidth, isInPlay, scaler);

        m_addRow[target].SetLabel   (L"+");
        m_addRow[target].SetVisible (isInPlay);
        m_addRow[target].Layout     (MakeRect (columnX + rowWidth + gap, axesBottom, addWidth, rowH));

        if (!isInPlay)
        {
            m_invert[target].SetVisible     (false);
            m_response[target].SetVisible   (false);
            m_speedLabel[target].SetVisible (false);
            m_speed[target].SetVisible      (false);
            LayOutSharedWarning (target, rowsX, axesBottom, warnW, text, scaler);
            continue;
        }

        axesBottom += tableH;

        // Right-aligned with the mapping drop-down above it: the two option
        // widths and the row width are the same span in DIPs, but each is
        // scaled to pixels on its own, so the rounding left the edges a pixel
        // or two apart. Taking the remainder of the row lands it exactly.
        // It is widened, leftward, to show its longest item whole beside its
        // arrow, and Invert takes what is left. Only a Paddle profile has a
        // paddle speed, and only a knob's offers Position beside it: a
        // Joystick profile's axes always give position, and a Joyport switch
        // follows the stick's deflection either way.
        m_response[target].SetItems ({ L"Position", L"Paddle speed" });
        m_response[target].SetDpi   (dpi);

        responseW = rowWidth - optionWidth;

        if (text != nullptr)
        {
            responseW = std::max (responseW, (int) std::ceil (m_response[target].GetFitWidthPx (*text)));
        }

        responseX = columnX + rowWidth - responseW;

        m_invert[target].SetVisible (true);
        m_invert[target].SetRect    (MakeRect (columnX, axesBottom, responseX - columnX, rowH));
        m_invert[target].SetLabel   (L"Invert");

        m_response[target].SetVisible (m_state != nullptr && m_state->IsPositionOffered (TargetAt (target)));
        m_response[target].SetRect    (MakeRect (responseX, axesBottom, responseW, rowH));

        // The speed slider takes the row under Invert, with its label in the
        // target labels' column, while the axis plays at paddle speed: its
        // track, not the puck's room left of it, starts at the drop-down
        // column's edge and runs to the end of the "+" column, as the dead
        // zone's does.
        m_speedLabel[target].SetVisible (isSpeed);
        m_speed[target].SetVisible      (isSpeed);

        if (isSpeed)
        {
            axesBottom += rowH + gap;
        }

        m_speedLabel[target].SetRect (MakeRect (rowsX, axesBottom, columnX - trackInset - rowsX, rowH));
        m_speedLabel[target].SetText (L"Paddle speed:");

        m_speed[target].SetRect          (MakeRect (columnX - trackInset, axesBottom, columnEnd - (columnX - trackInset), rowH));
        m_speed[target].SetRange         (ControllerProfileStore::kMinMaxSpeed, ControllerProfileStore::kMaxMaxSpeed);
        m_speed[target].SetStep          (16.0f);
        m_speed[target].SetDecimalPlaces (0);
        m_speed[target].SetSuffix        (L"/s");
        m_speed[target].SetTickInterval  (256.0f);

        axesBottom += rowH + gap;
        axesBottom += LayOutSharedWarning (target, rowsX, axesBottom, warnW, text, scaler);
        axesBottom += sectionGap - gap;
    }

    PollPaddleBars (nullptr);

    y = std::max (isPaddles ? stickTop : stickTop + stickSize, axesBottom) + sectionGap;

    // The buttons, each with a light that fills while it reads pressed, in
    // the picture's column beside its row. A lit light's message runs from
    // the light to the page padding, past the pictures' right edge, so it
    // is not counted in the page's content width.
    m_buttonsHeading.SetRect (MakeRect (x, y, wideWidth, rowH));
    m_buttonsHeading.SetText (L"Buttons");
    y += rowH;

    for (target = kAxisCount; target < kTargetCount; target++)
    {
        int           tableH      = 0;
        int           rowMessageX = isJoyport ? pictureX : messageX;
        size_t        light       = target - kAxisCount;
        bool          isInPlay    = IsTargetShown (target);
        std::wstring  playLabel   = m_state != nullptr ? m_state->GetTargetPlayLabel (TargetAt (target)) : std::wstring();

        // The Joyport's art shows fire itself, so there the light is not
        // drawn and its message starts at the art's left edge instead.
        m_lights[light].SetVisible       (isInPlay);
        m_lights[light].SetCircleShown   (!isJoyport);
        m_lights[light].Layout           (MakeRect (pictureX, y + (rowH - lightSize) / 2, lightSize, lightSize), scaler);
        m_lights[light].SetMessageBounds (MakeRect (rowMessageX, y, std::max (0, (int) rect.right - pad - rowMessageX), rowH));

        m_targetLabel[target].SetVisible   (isInPlay);
        m_targetLabel[target].SetRect      (MakeRect (rowsX, y, labelW, rowH));
        m_targetLabel[target].SetTextAlign (DxuiTextHAlign::Left, DxuiTextVAlign::Center);
        m_targetLabel[target].SetText      (GetRowLabel (target, playLabel, isJoyport, isPaddles));

        tableH = LayOutTable (target, columnX, y, rowWidth, isInPlay, scaler);

        m_addRow[target].SetLabel   (L"+");
        m_addRow[target].SetVisible (isInPlay);
        m_addRow[target].Layout     (MakeRect (columnX + rowWidth + gap, y, addWidth, rowH));

        if (isInPlay)
        {
            y += tableH;
        }

        y += LayOutSharedWarning (target, rowsX, y, warnW, text, scaler);
    }

    // A section gap above and below, and the track, not the puck's room
    // left of it, starting at the drop-down column above.
    y += sectionGap - gap;

    m_deadZoneLabel.SetRect (MakeRect (x, y, columnX - trackInset - x, rowH));
    m_deadZoneLabel.SetText (L"Dead zone:");
    m_deadZone.SetRect          (MakeRect (columnX - trackInset, y, columnEnd - (columnX - trackInset), rowH));
    m_deadZone.SetRange         (0.0f, 90.0f);
    m_deadZone.SetStep          (1.0f);
    m_deadZone.SetDecimalPlaces (0);
    m_deadZone.SetSuffix        (L"%");
    m_deadZone.SetTickInterval  (10.0f);
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
    contentH = y + rowH + pad - rect.top;

    for (player = 0; player < kPlayerCount; player++)
    {
        m_playerLabel[player].SetDpi (dpi);
        m_playerEntry[player].SetDpi (dpi);
        m_playerMode[player].SetDpi  (dpi);
    }

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

        for (const std::unique_ptr<DxuiComboBox> & row : m_rows[target])
        {
            row->SetDpi (dpi);
        }
    }

    for (target = 0; target < kAxisCount; target++)
    {
        m_invert[target].SetDpi     (dpi);
        m_response[target].SetDpi   (dpi);
        m_speedLabel[target].SetDpi (dpi);
        m_speed[target].SetDpi      (dpi);
    }

    m_deadZoneLabel.SetDpi     (dpi);
    m_deadZone.SetDpi          (dpi);
    m_calibrationLabel.SetDpi  (dpi);
    m_calibrationStatus.SetDpi (dpi);
    m_calibrate.SetDpi         (dpi);
    m_calibrationCancel.SetDpi (dpi);
    m_useAutomatic.SetDpi      (dpi);
    m_reset.SetDpi             (dpi);

    RebuildChoices();
    Refresh();

    // The content width is taken with the player rows at their design
    // extent, so stretching them never raises the width the sheet may grow
    // to.
    StretchPlayerRows    (x + labelWidth, profileEnd, gap, scaler);
    DxuiPanel::SetBounds (rect);
    SetContentWidthPx    (GetRightmostChildEdgePx() + pad - rect.left);
    StretchPlayerRows    (x + labelWidth, profileEnd + stretch, gap, scaler);
    SetContentHeightPx   (contentH);
}





////////////////////////////////////////////////////////////////////////////////
//
//  StretchPlayerRows
//
//  The entry column and the mode column each at least as wide as the longest
//  string it can show, with any room left over shared between them in
//  proportion to those widths; the mode drop-downs end at `right`. When both
//  cannot fit, the mode column keeps its width and the entries elide. The
//  Editing drop-down is as wide as the entries, and the warning under a
//  player spans both. Only the widths change; Layout has placed the rows.
//
////////////////////////////////////////////////////////////////////////////////

void ControllersPage::StretchPlayerRows (int left, int right, int gap, const DxuiDpiScaler & scaler)
{
    int     modeWidth  = GetModeWidthPx (scaler);
    int     entryFit   = GetEntryFitWidthPx();
    int     available  = right - left - gap;
    int     entryWidth = available - modeWidth;
    RECT    editing    = m_controller.GetRect();
    size_t  player     = 0;



    if (entryFit > 0 && available > entryFit + modeWidth)
    {
        modeWidth  = MulDiv (available, modeWidth, entryFit + modeWidth);
        entryWidth = available - modeWidth;
    }

    m_controller.SetRect (MakeRect (left, editing.top, entryWidth, editing.bottom - editing.top));

    for (player = 0; player < kPlayerCount; player++)
    {
        RECT  entry   = m_playerEntry[player].GetRect();
        RECT  warning = m_playerWarning[player].GetBounds();

        m_playerEntry[player].SetRect (MakeRect (left, entry.top, entryWidth, entry.bottom - entry.top));
        m_playerMode[player].SetRect  (MakeRect (right - modeWidth, entry.top, modeWidth, entry.bottom - entry.top));

        if (m_isWarningShown[player])
        {
            m_playerWarning[player].SetRect (MakeRect (left, warning.top, right - left, warning.bottom - warning.top));
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetModeWidthPx
//
//  Wide enough for the longest label a mode drop-down can show -- every
//  mode, and Player 2's Automatic with each mode it can resolve to --
//  beside its arrow, measured in the drop-down's font. With nothing to
//  measure with, the design width.
//
////////////////////////////////////////////////////////////////////////////////

int ControllersPage::GetModeWidthPx (const DxuiDpiScaler & scaler) const
{
    constexpr PlayerMode         kModes[] = { PlayerMode::Joystick, PlayerMode::JoyportLeft, PlayerMode::JoyportRight, PlayerMode::Paddle, PlayerMode::TwoPaddles, PlayerMode::SameAsPlayer1 };
    IDxuiTextRenderer          * text     = GetMeasuringRenderer();
    DxuiComboBox                 measure;
    std::vector<std::wstring>    labels;



    if (text == nullptr)
    {
        return scaler.ToPx (s_kPlayerModeWidthDp);
    }

    for (PlayerMode mode : kModes)
    {
        labels.push_back (PlayerModeRules::GetModeLabel (mode));
        labels.push_back (PlayerModeRules::GetAutomaticModeLabel (mode));
    }

    measure.SetDpi   (scaler.GetDpi());
    measure.SetItems (labels);

    return (int) std::ceil (measure.GetFitWidthPx (*text));
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetEntryFitWidthPx
//
//  Wide enough for the longest entry the player and Editing drop-downs now
//  list, beside their arrow. Zero with nothing to measure with.
//
////////////////////////////////////////////////////////////////////////////////

int ControllersPage::GetEntryFitWidthPx() const
{
    IDxuiTextRenderer  * text   = GetMeasuringRenderer();
    float                width  = 0.0f;
    size_t               player = 0;



    if (text == nullptr)
    {
        return 0;
    }

    width = m_controller.GetFitWidthPx (*text);

    for (player = 0; player < kPlayerCount; player++)
    {
        width = std::max (width, m_playerEntry[player].GetFitWidthPx (*text));
    }

    return (int) std::ceil (width);
}





////////////////////////////////////////////////////////////////////////////////
//
//  SetAnimationsEnabled
//
////////////////////////////////////////////////////////////////////////////////

void ControllersPage::SetAnimationsEnabled (bool isEnabled)
{
    for (ButtonLightView & light : m_lights)
    {
        light.SetAnimationsEnabled (isEnabled);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetPollIntervalMs
//
////////////////////////////////////////////////////////////////////////////////

UINT ControllersPage::GetPollIntervalMs() const
{
    return IsVisible() ? kLivePollMs : 0;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Poll
//
////////////////////////////////////////////////////////////////////////////////

void ControllersPage::Poll()
{
    Poll ((int64_t) GetTickCount64());
}





////////////////////////////////////////////////////////////////////////////////
//
//  Poll
//
//  The page asks the service to read the controller it shows, whether or not
//  that controller is the one selected, and then hands the latest reading to
//  whatever is waiting on it. Each button's light takes every reading since
//  the last poll, so a press that came and went in between still lights it.
//
//  A hidden page reads nothing, and lets the controller go so the service
//  stops reading it for the page.
//
////////////////////////////////////////////////////////////////////////////////

void ControllersPage::Poll (int64_t nowMs)
{
    constexpr float                                  kMsPerSecond = 1000.0f;
    std::optional<size_t>                            selected;
    std::optional<ControllerUnitKey>                 unit;
    std::optional<ControllerSample>                  sample;
    std::vector<ControllerSample>                    history;
    GamePortContribution                             reading;
    std::bitset<GamePortContribution::kButtonCount>  wasPressed;
    float                                            elapsed      = m_lastPollMs > 0 ? (float) (nowMs - m_lastPollMs) / kMsPerSecond : 0.0f;
    bool                                             isLive       = false;
    size_t                                           light        = 0;



    m_lastPollMs = nowMs;

    if (m_state == nullptr)
    {
        return;
    }

    if (!IsVisible())
    {
        if (m_inspected.has_value() && m_onInspect)
        {
            m_inspected.reset();
            m_onInspect (std::nullopt);
        }

        return;
    }

    if (m_state->GetControllers().size() != m_lastControllerCount || m_state->IsEditedControllerConnected() != m_wasEditedConnected)
    {
        m_capturing.reset();
        RebuildChoices();
        Refresh();
    }

    // A player can move into or out of a Joyport jack from the command bar
    // while the page is open; the readout above the rows and the warnings
    // under the players follow.
    SyncJoyportLayout();

    selected = m_state->GetSelectedIndex();

    // An unplugged controller in Editing has nothing to show: the views idle.
    if (selected.has_value() && m_state->IsEditedControllerConnected())
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

    for (PaddleBarView & bar : m_paddleBars)
    {
        bar.SetActive (sample.has_value());
    }

    if (!sample.has_value())
    {
        m_stick.SetValues (127, 127);
        PollPaddleBars    (nullptr);

        for (light = 0; light < kButtonCount; light++)
        {
            m_lights[light].Clear();
        }

        PollSwitchLights (nullptr);
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

    // The readings between polls count only for their buttons, so they move
    // no rate paddle; the latest moves it for the time since the last poll.
    if (m_historySource)
    {
        history = m_historySource (unit.value());
    }

    for (const ControllerSample & between : history)
    {
        wasPressed |= m_state->ComputeLiveReading (between, 0.0f).buttons;
    }

    reading = m_state->ComputeLiveReading (sample.value(), elapsed);

    // A target this controller does not drive reads as if it were not bound:
    // the row is grayed out, and a dot or a light that still moved with the
    // stick said the binding was live when it is not.
    if (!m_state->IsTargetInPlay (PaddleTarget::Pdl1))
    {
        reading.paddle[1].reset();
    }

    if (!m_state->IsTargetInPlay (PaddleTarget::Pdl0))
    {
        reading.paddle[0].reset();
    }

    for (light = 0; light < kButtonCount; light++)
    {
        if (!m_state->IsTargetInPlay (TargetAt (kAxisCount + light)))
        {
            reading.buttons.reset (light);
        }
    }

    m_stick.SetValues (reading.paddle[0].value_or (GamePortState::kPaddleCenter),
                       reading.paddle[1].value_or (GamePortState::kPaddleCenter));

    // The circle names the paddles the guest reads these axes on, which are
    // the player's own two while two people play.
    {
        std::wstring  horizontal = m_state->GetTargetPlayLabel (PaddleTarget::Pdl0);
        std::wstring  vertical   = m_state->GetTargetPlayLabel (PaddleTarget::Pdl1);

        if (!horizontal.empty()) { horizontal.pop_back(); }
        if (!vertical.empty())   { vertical.pop_back(); }

        m_stick.SetAxisLabels (horizontal.empty() ? std::wstring (L"PDL0") : horizontal,
                               m_state->IsTargetInPlay (PaddleTarget::Pdl1)
                                   ? (vertical.empty() ? std::wstring (L"PDL1") : vertical)
                                   : std::wstring());
    }

    for (light = 0; light < kButtonCount; light++)
    {
        // A button the machine lacks, or this controller does not drive,
        // stays dark whatever is pressed.
        isLive = m_state->IsTargetInPlay (TargetAt (kAxisCount + light)) && m_state->IsTargetAvailable (TargetAt (kAxisCount + light));

        m_lights[light].Update (isLive && wasPressed[light], isLive && reading.buttons.test (light), nowMs);
    }

    PollPaddleBars   (&reading);
    PollSwitchLights (&reading);
}





////////////////////////////////////////////////////////////////////////////////
//
//  IsJoyportMode
//
//  The page shows the Joyport's switches where the stick is while the
//  controller in Editing plays for a player in one of its jacks.
//
////////////////////////////////////////////////////////////////////////////////

bool ControllersPage::IsJoyportMode() const
{
    return m_state != nullptr && m_state->IsEditedOnJoyport();
}





////////////////////////////////////////////////////////////////////////////////
//
//  IsPaddlesMode
//
//  The page shows a bar per paddle where the stick is while the controller
//  in Editing plays for a player in Paddle or Two paddles mode.
//
////////////////////////////////////////////////////////////////////////////////

bool ControllersPage::IsPaddlesMode() const
{
    return m_state != nullptr && m_state->IsEditedOnPaddles();
}





////////////////////////////////////////////////////////////////////////////////
//
//  PollPaddleBars
//
//  Each paddle row's bar, for the paddle the guest reads it on, at the value
//  this reading gives it; at center with no reading. Layout shows only the
//  rows in play.
//
////////////////////////////////////////////////////////////////////////////////

void ControllersPage::PollPaddleBars (const GamePortContribution * reading)
{
    HRESULT  hr        = S_OK;
    size_t   axis      = 0;
    bool     isShowing = m_isPaddlesShown && m_state != nullptr;



    BAIL_OUT_IF (!isShowing, S_OK);

    for (axis = 0; axis < kAxisCount; axis++)
    {
        PaddleBar     bar;
        std::wstring  name = m_state->GetTargetPlayLabel (TargetAt (axis));

        if (!name.empty())
        {
            name.pop_back();
        }

        bar.name  = name.empty() ? std::format (L"PDL{}", axis) : name;
        bar.value = (reading != nullptr) ? reading->paddle[axis].value_or (PaddleBar::kCenter) : PaddleBar::kCenter;

        m_paddleBars[axis].SetBar (bar);
    }

Error:
    return;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SyncJoyportLayout
//
//  Each poll: the page is laid out again when the controller in Editing
//  moves into or out of a Joyport jack or a paddle mode, or a warning under
//  a player comes or goes, since each changes what the page shows, not just
//  its values.
//
////////////////////////////////////////////////////////////////////////////////

void ControllersPage::SyncJoyportLayout()
{
    bool    isPaddles = !IsJoyportMode() && IsPaddlesMode();
    bool    isChanged = IsJoyportMode() != m_isJoyportShown || isPaddles != m_isPaddlesShown;
    size_t  player    = 0;



    for (player = 0; player < kPlayerCount && m_state != nullptr; player++)
    {
        isChanged = isChanged || m_state->GetButtonsCutNotice (player).empty() == m_isWarningShown[player];
    }

    if (isChanged)
    {
        Relayout();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  PollSwitchLights
//
//  Lit exactly when the switch would read closed, from the page's own reading
//  of the edited mapping, so a profile can be checked against the Joyport
//  without booting a game. The heading follows Editing, which can move to
//  another player's controller while the page is open. No reading: all dark.
//
////////////////////////////////////////////////////////////////////////////////

void ControllersPage::PollSwitchLights (const GamePortContribution * reading)
{
    HRESULT  hr        = S_OK;
    bool     isShowing = m_isJoyportShown && m_state != nullptr;



    BAIL_OUT_IF (!isShowing, S_OK);

    m_joystickHeading.SetText (m_state->GetEditedHeading());

    m_switchView.SetActive   (reading != nullptr);
    m_switchView.SetSwitches (reading != nullptr ? reading->switches : JoystickSwitches());

Error:
    return;
}





////////////////////////////////////////////////////////////////////////////////
//
//  IsTargetShown
//
//  Whether a target's rows are on the page: one this controller's player
//  drives, and, with the Joyport attached, one the Joyport reads. Layout and
//  the row re-sync both ask here, so they cannot disagree about a row.
//
////////////////////////////////////////////////////////////////////////////////

bool ControllersPage::IsTargetShown (size_t target) const
{
    PaddleTarget  paddleTarget = TargetAt (target);
    bool          isInPlay     = m_state == nullptr || m_state->IsTargetInPlay (paddleTarget);
    bool          isUsed       = !m_isJoyportShown || ControllersPageState::IsJoyportTarget (paddleTarget);



    return isInPlay && isUsed;
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetRowLabel
//
//  With the Joyport attached, what the row closes; otherwise the paddle or
//  button the guest reads it on, which while two play is the player's own.
//  A paddle has one axis, so in a paddle mode its row carries no axis letter.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring ControllersPage::GetRowLabel (size_t target, const std::wstring & playLabel, bool isJoyport, bool isPaddles)
{
    std::wstring  label = isJoyport ? ControllersPageState::GetJoyportRowLabel (TargetAt (target)) : std::wstring();



    if (label.empty() && playLabel.empty() && isPaddles && target < kAxisCount)
    {
        label = std::format (L"PDL{}:", target);
    }

    if (label.empty())
    {
        label = playLabel.empty() ? std::wstring (s_kTargetNames[target]) : playLabel;
    }

    return label;
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetTargetLabel
//
//  A target as its row's label shows it, without the trailing colon, for
//  use inside a sentence: "Fire", "PDL0 (X)", "PB2".
//
////////////////////////////////////////////////////////////////////////////////

std::wstring ControllersPage::GetTargetLabel (PaddleTarget target) const
{
    std::wstring  label;
    std::wstring  playLabel;
    size_t        index     = 0;
    size_t        i         = 0;



    for (i = 0; i < kTargetCount; i++)
    {
        if (TargetAt (i) == target)
        {
            index = i;
        }
    }

    playLabel = m_state != nullptr ? m_state->GetTargetPlayLabel (target) : std::wstring();
    label     = GetRowLabel (index, playLabel, IsJoyportMode(), !IsJoyportMode() && IsPaddlesMode());

    if (!label.empty() && label.back() == L':')
    {
        label.pop_back();
    }

    return label;
}





////////////////////////////////////////////////////////////////////////////////
//
//  MakeSharedNotice
//
//  One sentence, a line each, for every control whose sharing warning goes
//  under this target: the control as its rows show it, then every other
//  target it is on, as in "B is also assigned to Fire and PB1." Empty when
//  no warning goes here.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring ControllersPage::MakeSharedNotice (PaddleTarget target) const
{
    ControllerKind             kind   = GetSelectedKind();
    std::wstring               notice;
    std::vector<std::wstring>  others;



    if (m_state == nullptr)
    {
        return notice;
    }

    for (const ControlId & control : m_state->GetSharedControls())
    {
        if (m_state->GetSharedWarningTarget (control) != target)
        {
            continue;
        }

        others.clear();

        for (PaddleTarget other : m_state->GetControlTargets (control))
        {
            if (other != target)
            {
                others.push_back (GetTargetLabel (other));
            }
        }

        notice += notice.empty() ? L"" : L"\n";
        notice += ControlLabels::For (kind, control) + L" is also assigned to " + ControllersPageState::JoinWithAnd (others) + L".";
    }

    return notice;
}





////////////////////////////////////////////////////////////////////////////////
//
//  LayOutSharedWarning
//
//  The sharing warning under one target's rows, at (x, y), shown only while
//  it has sentences and as tall as they are. Returns the height it takes,
//  with the gap below it, or 0 while it is hidden. A warning whose text is
//  new is the one the sheet scrolls to after an edit.
//
////////////////////////////////////////////////////////////////////////////////

int ControllersPage::LayOutSharedWarning (
    size_t                  target,
    int                     x,
    int                     y,
    int                     width,
    IDxuiTextRenderer     * text,
    const DxuiDpiScaler   & scaler)
{
    DxuiInfoBanner  & banner   = m_sharedWarning[target];
    std::wstring      notice   = IsTargetShown (target) ? MakeSharedNotice (TargetAt (target)) : std::wstring();
    bool              isNew    = !notice.empty() && notice != banner.GetText();
    int               heightPx = 0;



    banner.SetText    (notice);
    banner.SetVisible (!notice.empty());
    banner.SetDpi     (scaler.GetDpi());

    if (notice.empty())
    {
        return 0;
    }

    // Measured whenever there is a renderer to measure with, as the players'
    // warnings are.
    if (text != nullptr)
    {
        heightPx = (int) std::ceil (banner.GetMeasuredHeightPx (*text, (float) width, scaler));
    }
    else
    {
        heightPx = (int) std::ceil (banner.GetPreferredHeightPx ((float) width, scaler));
    }

    banner.SetRect (MakeRect (x, y, width, heightPx));

    if (isNew)
    {
        m_revealedWarning = target;
    }

    return heightPx + scaler.ToPx (s_kGapDp);
}





////////////////////////////////////////////////////////////////////////////////
//
//  HasSharedNoticeChanged
//
//  Whether any target's sharing warning would now read differently from the
//  one laid out.
//
////////////////////////////////////////////////////////////////////////////////

bool ControllersPage::HasSharedNoticeChanged() const
{
    size_t  target  = 0;
    bool    changed = false;



    for (target = 0; target < kTargetCount && !changed; target++)
    {
        std::wstring  notice = IsTargetShown (target) ? MakeSharedNotice (TargetAt (target)) : std::wstring();

        changed = notice != m_sharedWarning[target].GetText();
    }

    return changed;
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
    int                        selectedItem  = 0;
    size_t                     index         = 0;
    bool                       isPlayersOnly = false;
    bool                       isEditable    = false;



    if (m_state == nullptr)
    {
        return;
    }

    // IN MULTIPLAYER, ONLY THE PLAYERS' CONTROLLERS ARE EDITED. The page shows
    // the mode being played, and in multiplayer a controller in neither slot
    // drives nothing, so its page had no rows at all. It is edited from single
    // player, where its page is whole.
    isPlayersOnly = m_state->IsMultiplayerEnabled();
    m_editingIndices.clear();

    for (index = 0; index < m_state->GetControllers().size(); index++)
    {
        const ControllersPageState::ControllerEntry &  entry = m_state->GetControllers()[index];

        if (isPlayersOnly && m_state->GetPlayerUnit (0) != entry.unit && m_state->GetPlayerUnit (kPlayerTwo) != entry.unit)
        {
            continue;
        }

        m_editingIndices.push_back (index);
        names.push_back (entry.isConnected ? entry.description : entry.description + L" (not connected)");
    }

    if (names.empty())
    {
        names.push_back (L"No controller is attached");
    }

    selected              = m_state->GetSelectedIndex();
    isEditable            = m_state->IsEditedControllerConnected();
    m_lastControllerCount = m_state->GetControllers().size();
    m_wasEditedConnected  = isEditable;
    m_isSyncing           = true;

    for (index = 0; selected.has_value() && index < m_editingIndices.size(); index++)
    {
        if (m_editingIndices[index] == selected.value())
        {
            selectedItem = (int) index;
        }
    }

    m_controller.SetItems    (names);
    m_controller.SetSelected (selectedItem);
    m_controller.SetEnabled  (selected.has_value() && !m_editingIndices.empty());

    RefreshPlayers();
    RefreshProfiles();
    RefreshRows();
    RefreshAxisOptions();
    RefreshCalibration();

    m_deadZone.SetValue   (m_state->GetDeadZone() * 100.0f);
    m_deadZone.SetEnabled (isEditable);
    m_reset.SetEnabled    (isEditable);

    m_isSyncing = false;

    // A sharing warning's height follows its sentences, so a change to them
    // is a change to the page's layout. Layout sets the text before it
    // refreshes, so this cannot loop.
    if (m_hasLayout && HasSharedNoticeChanged())
    {
        Relayout();
    }
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
    bool                       hasUnit  = m_state->IsEditedControllerConnected();
    bool                       canEdit  = hasUnit && !m_state->IsEditingBuiltInProfile();
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
    if (m_state == nullptr || !m_state->IsEditedControllerConnected())
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
        [this, proceed] (const std::wstring &, ProfileSource, const std::wstring &)
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
//  The starting points of the mode in effect, and the profiles of that mode
//  a copy can start from, opening on the edited profile when it is one of
//  them.
//
////////////////////////////////////////////////////////////////////////////////

void ControllersPage::OpenNewProfileDialog()
{
    std::vector<std::string>   names    = m_state->GetCopySourceNames();
    std::string                edited   = m_state->GetEditedProfileName();
    std::vector<std::wstring>  copies;
    size_t                     selected = 0;
    size_t                     i        = 0;



    for (i = 0; i < names.size(); i++)
    {
        copies.push_back (Utf8ToWide (names[i]));

        if (names[i] == edited)
        {
            selected = i;
        }
    }

    m_profileDialog.OpenNew (ControllersPageState::GetStartingPoints (m_state->GetProfileMode(), !names.empty()),
                             copies,
                             selected,
        [this] (const std::wstring & name, ProfileSource source, const std::wstring & copySource)
        {
            ProfileEditResult  result = m_state->CreateProfile (WideToUtf8 (name), source, WideToUtf8 (copySource));

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
    if (m_state == nullptr || m_state->IsEditingBuiltInProfile())
    {
        return;
    }

    m_profileDialog.OpenRename (Utf8ToWide (m_state->GetEditedProfileName()),
        [this] (const std::wstring & name, ProfileSource, const std::wstring &)
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
    if (m_state == nullptr || m_state->IsEditingBuiltInProfile())
    {
        return;
    }

    m_profileDialog.OpenConfirmDelete (Utf8ToWide (m_state->GetEditedProfileName()),
        [this] (const std::wstring &, ProfileSource, const std::wstring &)
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

        if (m_state == nullptr || !selected.has_value())
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
    ControllerKind  kind    = GetSelectedKind();
    bool            hasUnit = m_state->IsEditedControllerConnected();
    size_t          target  = 0;
    size_t          row     = 0;



    for (target = 0; target < kTargetCount; target++)
    {
        PaddleTarget  paddleTarget = TargetAt (target);
        bool          available    = hasUnit && m_state->IsTargetAvailable (paddleTarget);

        // A target this controller's player does not drive is not on the page
        // at all while two people play. Layout leaves its rows out; this has
        // to agree, or a re-sync puts them back on top of the rows below.
        bool          isInPlay     = IsTargetShown (target);

        // A player beside the Joyport keeps its button rows, disabled: the
        // Joyport has its buttons, and the bindings wait in the profile.
        bool          isCut        = target >= kAxisCount && m_state->AreEditedButtonsCut();
        bool          isEditable   = available && isInPlay && !isCut;
        size_t        count        = GetBindingCount (target);
        size_t        shown        = GetShownRows (target);

        EnsureRows (target, shown);

        for (row = 0; row < m_rows[target].size(); row++)
        {
            std::vector<std::wstring>  items;
            std::vector<std::wstring>  glyphs;
            bool                       isCapturing = m_capturing.has_value() && m_capturing->first == target && m_capturing->second == row;
            int                        choice      = FindChoice (target, row);

            items.push_back (isCapturing ? L"Press a control..." : L"Press to assign...");
            items.push_back (m_state->GetUnassignedLabel (paddleTarget));
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

            m_rows[target][row]->SetItems      (items);
            m_rows[target][row]->SetItemGlyphs (glyphs);
            // A target the machine lacks says so, rather than showing a
            // binding saved from a machine that has it as though it applied.
            m_rows[target][row]->SetSelected (!available  ? kNoneItem
                                              : isCapturing ? kPressToAssignItem
                                              : (row < count ? choice : kNoneItem));
            m_rows[target][row]->SetEnabled  (isEditable);
            m_rows[target][row]->SetVisible  (isInPlay && row < shown);
        }

        m_addRow[target].SetVisible (isInPlay);
        // Never unavailable for the number of rows: the table scrolls.
        m_addRow[target].SetEnabled (isEditable && count > 0 && !m_hasExtraRow[target]);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  RefreshPlayers
//
//  Each player's two drop-downs and the note of what it drives. The entries
//  are the picker's own, the other player's controller included: picking
//  that one returns the other player to Automatic. The modes are the
//  picker's too, a Joyport jack the other player holds listed and disabled.
//
////////////////////////////////////////////////////////////////////////////////

void ControllersPage::RefreshPlayers()
{
    size_t  player = 0;
    size_t  i      = 0;



    for (player = 0; player < kPlayerCount; player++)
    {
        std::vector<InputModeRules::PlayerChoice>      choices      = m_state->GetEntryChoices (player);
        std::vector<InputModeRules::PlayerModeChoice>  modes        = m_state->GetModeChoices (player);
        std::vector<std::wstring>                      items;
        std::vector<std::wstring>                      modeItems;
        std::vector<bool>                              modeEnabled;
        int                                            selected     = 0;
        int                                            selectedMode = 0;

        m_playerEntries[player].clear();

        for (i = 0; i < choices.size(); i++)
        {
            if (choices[i].isChecked)
            {
                selected = (int) i;
            }

            m_playerEntries[player].push_back (choices[i].entry);
            items.push_back (choices[i].label);
        }

        m_playerEntry[player].SetItems    (items);
        m_playerEntry[player].SetSelected (selected);

        m_playerModes[player].clear();

        for (i = 0; i < modes.size(); i++)
        {
            if (modes[i].isChecked)
            {
                selectedMode = (int) i;
            }

            m_playerModes[player].push_back (modes[i].mode);
            modeItems.push_back   (modes[i].label);
            modeEnabled.push_back (modes[i].isEnabled);
        }

        m_playerMode[player].SetItems        (modeItems);
        m_playerMode[player].SetItemsEnabled (modeEnabled);
        m_playerMode[player].SetSelected     (selectedMode);
    }

    m_joystickHeading.SetText (m_state->GetEditedHeading());
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnPlayerEntrySelect
//
////////////////////////////////////////////////////////////////////////////////

void ControllersPage::OnPlayerEntrySelect (size_t player, int item)
{
    if (m_state == nullptr || player >= kPlayerCount || item < 0 || (size_t) item >= m_playerEntries[player].size())
    {
        return;
    }

    PlayerEntry                       pick      = m_playerEntries[player][(size_t) item];
    std::optional<ControllerUnitKey>  playerOne = m_state->GetPlayerUnit (0);
    bool                              movesOne  = false;



    // Player one changes when its own drop-down picks something else, or
    // when player two takes player one's controller.
    movesOne = (player == 0) ? !(pick == m_state->GetPlayerEntries()[0])
                             : pick.unit.has_value() && pick.unit == playerOne;

    if (!movesOne)
    {
        ApplyPlayerEntry (player, pick);
        return;
    }

    // ASKED BEFORE THE PICK IS APPLIED, not after. Editing follows player one,
    // so a pick that moves player one leaves the edited profile; asking first
    // means Cancel takes back the whole gesture -- the pick as well as the
    // move -- rather than leaving the players changed and Editing on a
    // controller that is no longer player one's. The prompt re-syncs the
    // drop-downs as it opens, so a canceled pick shows as never made.
    AskToSaveProfileEdits ([this, player, pick] ()
    {
        ApplyPlayerEntry (player, pick);
        FollowPlayerOne();
    });
}





////////////////////////////////////////////////////////////////////////////////
//
//  ApplyPlayerEntry
//
//  Who plays decides which rows below are in play, so the whole page follows
//  a pick here.
//
////////////////////////////////////////////////////////////////////////////////

void ControllersPage::ApplyPlayerEntry (size_t player, const PlayerEntry & entry)
{
    m_state->PickPlayerEntry (player, entry);
    Relayout();
}





////////////////////////////////////////////////////////////////////////////////
//
//  FollowPlayerOne
//
//  In multiplayer the controller worth editing is player one's, so Editing
//  moves to it -- or to player two's when player one's slot is empty, since
//  Editing lists only the players' controllers and must land on one of them.
//  The move asks about unsaved profile edits first, exactly as a pick from the
//  Editing drop-down does.
//
////////////////////////////////////////////////////////////////////////////////

void ControllersPage::FollowPlayerOne()
{
    std::optional<ControllerUnitKey>  unit;
    std::optional<size_t>             index;



    if (m_state == nullptr || !m_state->IsMultiplayerEnabled())
    {
        return;
    }

    unit = m_state->GetPlayerUnit (0);

    if (!unit.has_value())
    {
        unit = m_state->GetPlayerUnit (kPlayerTwo);
    }

    if (!unit.has_value())
    {
        return;
    }

    index = m_state->FindController (unit.value());

    if (!index.has_value() || m_state->GetSelectedIndex() == index)
    {
        return;
    }

    AskToSaveProfileEdits ([this, index] () { SwitchController (index.value()); });
}





////////////////////////////////////////////////////////////////////////////////
//
//  StartNewProfile
//
//  New... from a player's profile section in the picker: Editing moves to
//  that player's controller and the New profile dialog opens for it. Leaving
//  a profile with unapplied edits asks once, before either, exactly as New
//  on the page does; Cancel leaves the page as it was.
//
////////////////////////////////////////////////////////////////////////////////

void ControllersPage::StartNewProfile (const ControllerUnitKey & unit)
{
    std::optional<size_t>  index;



    if (m_state == nullptr)
    {
        return;
    }

    index = m_state->FindController (unit);

    if (!index.has_value())
    {
        return;
    }

    AskToSaveProfileEdits ([this, index] ()
    {
        if (m_state->GetSelectedIndex() != index)
        {
            SwitchController (index.value());
        }

        OpenNewProfileDialog();
    });
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnPlayerModeSelect
//
//  The player's mode applies at once, and changes which rows the controller
//  in Editing drives and which kind of profile the page lists, so the page
//  is laid out again.
//
////////////////////////////////////////////////////////////////////////////////

void ControllersPage::OnPlayerModeSelect (size_t player, int item)
{
    if (m_state == nullptr || player >= kPlayerCount || item < 0 || (size_t) item >= m_playerModes[player].size())
    {
        return;
    }

    m_state->SetPlayerMode (player, m_playerModes[player][(size_t) item]);
    m_capturing.reset();
    m_hasExtraRow = {};
    Relayout();
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
    ControlMapping  mapping = m_state->GetMapping();   // a copy: IsPositionOffered rebuilds what GetMapping returns
    size_t          axis    = 0;



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

        // An axis this controller's player does not drive takes no options
        // either: the row it belongs to is not editable, so neither is what
        // shapes it.
        bool  isEditable = m_state->IsEditedControllerConnected() && m_state->IsTargetInPlay (TargetAt (axis));

        m_response[axis].SetVisible   (IsTargetShown (axis) && m_state->IsPositionOffered (TargetAt (axis)));
        m_speed[axis].SetVisible      (IsTargetShown (axis) && m_state->IsPaddleSpeedShown (TargetAt (axis)));
        m_speedLabel[axis].SetVisible (IsTargetShown (axis) && m_state->IsPaddleSpeedShown (TargetAt (axis)));

        m_invert[axis].SetEnabled   (analog != nullptr && isEditable);
        m_response[axis].SetEnabled (analog != nullptr && isEditable);
        m_speed[axis].SetEnabled    (analog != nullptr && isEditable && analog->response == AxisResponse::Rate);

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
    m_calibrate.SetEnabled         (m_state->IsEditedControllerConnected());
    m_useAutomatic.SetEnabled      (m_state->IsEditedControllerConnected());

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
//  never added. A target takes any number of rows; the new one is scrolled
//  into view in the target's table.
//
////////////////////////////////////////////////////////////////////////////////

void ControllersPage::AddRow (size_t target)
{
    std::optional<size_t>            selected;
    std::optional<ControllerSample>  baseline;
    size_t                           count    = GetBindingCount (target);



    if (m_state == nullptr)
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

    // A sharing warning that just appeared or changed can land below the
    // part of the page in view; bring it in.
    if (m_revealedWarning.has_value())
    {
        RequestReveal (m_sharedWarning[m_revealedWarning.value()].GetBounds());
        m_revealedWarning.reset();
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

    if (m_state == nullptr || !selected.has_value())
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
//  on press-to-assign past the end; never fewer than one. Past kTableRows the
//  target's table scrolls.
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

    return std::max (shown, (size_t) 1);
}





////////////////////////////////////////////////////////////////////////////////
//
//  EnsureRows
//
//  A target's table holds at least `count` drop-downs, each wired to its row
//  and to the popup host, at the page's DPI. Rows are never taken away; the
//  ones past what the target shows are hidden.
//
////////////////////////////////////////////////////////////////////////////////

void ControllersPage::EnsureRows (size_t target, size_t count)
{
    RowList  & rows = m_rows[target];



    while (rows.size() < count)
    {
        size_t  row = rows.size();

        rows.push_back (std::make_unique<DxuiComboBox>());

        rows[row]->SetSelect ([this, target, row] (int item)
        {
            if (!m_isSyncing)
            {
                OnRowSelect (target, row, item);
            }
        });

        rows[row]->SetPopupHost (m_popupHost);
        rows[row]->SetDpi       (m_lastScaler.GetDpi());
        m_tables[target].Adopt  (*rows[row]);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  LayOutTable
//
//  A target's rows at (x, y), `width` wide, in its table: as tall as its
//  rows up to kTableRows, and scrolling past that, with the scrollbar at the
//  right of the width and the rows narrowed to leave it room. The viewport
//  reaches half a gap above the first row and below the last shown, so a
//  row's focus rectangle is not cut off. A row waiting on press-to-assign,
//  a new one included, is scrolled into view. Returns the height the table
//  takes on the page, the gap below it included.
//
////////////////////////////////////////////////////////////////////////////////

int ControllersPage::LayOutTable (
    size_t                  target,
    int                     x,
    int                     y,
    int                     width,
    bool                    isInPlay,
    const DxuiDpiScaler   & scaler)
{
    DxuiScrollPanel  & table     = m_tables[target];
    RowList          & rows      = m_rows[target];
    int                rowH      = scaler.ToPx (s_kRowHeightDp);
    int                gap       = scaler.ToPx (s_kGapDp);
    int                step      = rowH + gap;
    int                inset     = gap / 2;
    size_t             shown     = GetShownRows (target);
    size_t             visible   = std::min (shown, kTableRows);
    bool               canScroll = shown > kTableRows;
    int                rowWidth  = canScroll ? width - scaler.ToPx (DxuiScrollPanel::kScrollbarWidthDip) - inset : width;
    size_t             row       = 0;



    EnsureRows (target, shown);

    table.SetVisible    (isInPlay);
    table.SetLineStepPx (step);

    for (row = 0; row < rows.size(); row++)
    {
        rows[row]->SetVisible (isInPlay && row < shown);
        table.PlaceChild      (*rows[row], MakeRect (x, y + (int) row * step, rowWidth, rowH));
    }

    table.Layout (MakeRect (x, y - inset, width, (int) visible * step - gap + 2 * inset), scaler);

    if (m_capturing.has_value() && m_capturing->first == target && m_capturing->second < rows.size())
    {
        table.RevealDescendant (*rows[m_capturing->second]);
    }

    return (int) visible * step;
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



    if (m_state == nullptr || !selected.has_value())
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
