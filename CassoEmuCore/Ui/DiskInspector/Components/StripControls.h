#pragma once

#include "Pch.h"

#include "Ui/DiskInspector/Components/IInspectorComponent.h"

class IInspectorComponentHost;
class InspectorSession;
class TrackStripView;





////////////////////////////////////////////////////////////////////////////////
//
//  StripControls
//
//  The row under the strip: at its left end the strip's label, "Track 17.25
//  · one turn from the index", then the readout of the zoom, the cells in
//  view and the selection, and at its right end "−", "+" and "Whole track"
//  (FR-033, FR-034).
//
////////////////////////////////////////////////////////////////////////////////

class StripControls : public IInspectorComponent
{
public:
    StripControls (InspectorSession & session, IInspectorComponentHost & host, TrackStripView & strip);

    void          Create  () override;
    void          Update  () override;
    std::wstring  GetTip  (POINT pointPx, RECT & outAnchorPx) const override;

    void  Layout   (const RECT & area, const DxuiDpiScaler & scaler);
    bool  Contains (POINT pointPx) const;

private:
    InspectorSession         & m_session;
    IInspectorComponentHost  & m_host;
    TrackStripView           & m_strip;
    DxuiLabel                * m_label   = nullptr;
    DxuiLabel                * m_readout = nullptr;
    DxuiButton               * m_out     = nullptr;
    DxuiButton               * m_in      = nullptr;
    DxuiButton               * m_whole   = nullptr;
    RECT                       m_rowPx   = {};
};
