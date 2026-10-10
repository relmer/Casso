#pragma once

#include "Pch.h"

#include "Ui/DiskInspector/DiskInspectorPalette.h"
#include "Ui/DiskInspector/InspectorViewModel.h"





////////////////////////////////////////////////////////////////////////////////
//
//  InspectorViewContext
//
//  What every view of the inspector window reads: the disk's analysis, the
//  view model it changes, the resolved palette, and the call to make after
//  it changes the selection so the other views follow.
//
////////////////////////////////////////////////////////////////////////////////

struct InspectorViewContext
{
    const DiskAnalysis    * analysis     = nullptr;
    InspectorViewModel    * model        = nullptr;
    DiskInspectorPalette    palette      = DiskInspectorPalette::MakeFallback (true);
    bool                    hasDisk      = false;
    bool                    isTimingMode = false;
    double                  timingRange  = 0.05;
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

    //  The text and the rectangle it describes, for the window's tooltip.
    virtual bool  GetTooltip (POINT pointPx, std::wstring & outText, RECT & outAnchorPx) const
    {
        (void) pointPx;
        (void) outText;
        (void) outAnchorPx;
        return false;
    }

protected:
    void  NotifySelection() const
    {
        if (m_context.onSelectionChanged)
        {
            m_context.onSelectionChanged();
        }
    }

    float  GetWidth  () const { return static_cast<float> (m_boundsDip.right - m_boundsDip.left); }
    float  GetHeight () const { return static_cast<float> (m_boundsDip.bottom - m_boundsDip.top); }

    InspectorViewContext &  m_context;
    DxuiDpiScaler           m_scaler;
};
