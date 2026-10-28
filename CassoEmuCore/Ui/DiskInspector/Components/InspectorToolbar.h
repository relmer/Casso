#pragma once

#include "Pch.h"

#include "Ui/DiskInspector/Components/IInspectorComponent.h"
#include "Ui/DiskInspector/Components/InspectorCommands.h"

class IInspectorComponentHost;
class InspectorSession;





////////////////////////////////////////////////////////////////////////////////
//
//  InspectorToolbar
//
//  The one toolbar row: in Casso the drive selector, the mode selector and,
//  in Timing mode, the timing range on the left; at the right end the
//  common commands, Follow head in Casso, Go to and Find (FR-007, FR-023,
//  FR-024, FR-061, FR-067).
//
////////////////////////////////////////////////////////////////////////////////

class InspectorToolbar : public IInspectorComponent
{
public:
    InspectorToolbar (InspectorSession & session, IInspectorComponentHost & host, const InspectorCommands & commands);

    void          Create     () override;
    void          Update     () override;
    void          Tick       (int64_t nowMs) override;
    void          PaintUnder (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme) override;
    bool          OnMouse    (const DxuiMouseEvent & ev) override;
    std::wstring  GetTip     (POINT pointPx, RECT & outAnchorPx) const override;
    void          GetPrefs   (DiskInspectorPrefs & inOut) const override;
    void          ApplyPrefs (const DiskInspectorPrefs & prefs) override;

    void  Layout        (const RECT & area, const DxuiDpiScaler & scaler);
    int   GetHeightPx   (const DxuiDpiScaler & scaler) const;
    void  StepRange     (int delta);
    bool  CanStepRange  (int delta) const;
    void  SetTimingMode (bool isTiming);

private:
    bool  IsDriveShown () const;

    InspectorSession           & m_session;
    IInspectorComponentHost    & m_host;
    const InspectorCommands    & m_commands;
    DxuiSegmentedControl       * m_drives     = nullptr;
    DxuiSegmentedControl       * m_modes      = nullptr;
    DxuiButton                 * m_rangeDown  = nullptr;
    DxuiButton                 * m_rangeUp    = nullptr;
    DxuiLabel                  * m_rangeLabel = nullptr;
    DxuiToolbar                * m_tools      = nullptr;
    RECT                         m_rowPx      = {};
    int                          m_rangeStep  = 3;
    int                          m_driveCount = 0;
};
