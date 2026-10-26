#pragma once

#include "Pch.h"

#include "Devices/Disk/Inspector/DiskComparison.h"
#include "Ui/DiskInspector/DiskInspectorPalette.h"
#include "Ui/DiskInspector/EditController.h"
#include "Ui/DiskInspector/FindMarks.h"
#include "Ui/DiskInspector/InspectorViewModel.h"
#include "Shell/InspectorStatusPublisher.h"





////////////////////////////////////////////////////////////////////////////////
//
//  InspectorHead
//
//  The drive's head as the platter and the strip draw it: its quarter
//  track, the turn of the record under it, and what it is doing (FR-064,
//  FR-066).
//
////////////////////////////////////////////////////////////////////////////////

struct InspectorHead
{
    bool          isShown      = false;
    int           quarterTrack = -1;
    double        turn         = 0.0;
    HeadActivity  activity     = HeadActivity::Idle;
};





////////////////////////////////////////////////////////////////////////////////
//
//  InspectorViewContext
//
//  What every view of the inspector window reads: the disk's analysis, the
//  view model it changes, the resolved palette, and the call to make after
//  it changes the selection so the other views follow, and while comparing
//  the comparison and B's analysis.
//
////////////////////////////////////////////////////////////////////////////////

struct InspectorViewContext
{
    const DiskAnalysis        * analysis          = nullptr;
    InspectorViewModel        * model             = nullptr;
    DiskInspectorPalette        palette           = DiskInspectorPalette::MakeFallback (true);
    bool                        hasDisk           = false;
    bool                        isTimingMode      = false;
    double                      timingRange       = 0.05;
    const FileMap             * fileMap           = nullptr;
    int                         selectedFile      = -1;
    bool                        isFilesOverlay    = false;
    bool                        isFileHighlighted = false;
    POINT                       hoverPx           = { SHRT_MIN, SHRT_MIN };
    RECT                        scenePx           = {};
    bool                        isDiffsOverlay    = false;
    InspectorHead               head;
    const DriveHeadState      * drive             = nullptr;
    bool                        isReadsOverlay    = false;
    const DiskComparison      * comparison        = nullptr;
    const DiskAnalysis        * analysisB         = nullptr;
    const TrackAnalysis       * trackB            = nullptr;
    const vector<Difference>  * nibbleDiffs       = nullptr;
    const vector<int>         * diffCells         = nullptr;
    const FindMarks           * findMarks         = nullptr;
    EditController            * edits             = nullptr;
    const IDxuiControl        * selectionSource   = nullptr;
    std::function<void ()>    onSelectionChanged;

    const TrackAnalysis *  GetTrack () const { return (model != nullptr && hasDisk) ? model->GetTrack() : nullptr; }
};





////////////////////////////////////////////////////////////////////////////////
//
//  InspectorView
//
//  A view of the inspector window: a Dxui control laid out in pixels, which
//  gives the tooltip for a point when it has one.
//
////////////////////////////////////////////////////////////////////////////////

class InspectorView : public IDxuiControl
{
public:
    static constexpr float  kTextDip  = 13.0f;
    static constexpr float  kSmallDip = 11.0f;

    explicit InspectorView (InspectorViewContext & context) : m_context (context) {}

    void  Layout (const RECT & boundsDip, const DxuiDpiScaler & scaler) override
    {
        SetBounds (boundsDip);
        m_scaler = scaler;
    }

    //  A swatch beside each tooltip line about a color under a point; false
    //  when the tip has no color lines.
    virtual bool  GetTooltipSwatches (POINT pointPx, vector<DxuiTooltipSwatch> & outSwatches) const
    {
        (void) pointPx;
        (void) outSwatches;
        return false;
    }

    //  The text and the rectangle it describes, for the window's tooltip.
    virtual bool  GetTooltip (POINT pointPx, std::wstring & outText, RECT & outAnchorPx) const
    {
        (void) pointPx;
        (void) outText;
        (void) outAnchorPx;
        return false;
    }

protected:
    //  The context's selectionSource is this view for the call.
    void  NotifySelection() const
    {
        if (m_context.onSelectionChanged)
        {
            m_context.selectionSource = this;
            m_context.onSelectionChanged();
            m_context.selectionSource = nullptr;
        }
    }

    float  GetWidth  () const { return static_cast<float> (m_boundsDip.right - m_boundsDip.left); }
    float  GetHeight () const { return static_cast<float> (m_boundsDip.bottom - m_boundsDip.top); }

    InspectorViewContext &  m_context;
    DxuiDpiScaler           m_scaler;
};
