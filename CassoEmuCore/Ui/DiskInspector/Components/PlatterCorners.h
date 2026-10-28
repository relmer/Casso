#pragma once

#include "Pch.h"

#include "Ui/DiskInspector/Components/IInspectorComponent.h"

class FloatingGroup;
class IInspectorComponentHost;
class InspectorSession;
class OverlayChip;
class PlatterLegendView;
class PlatterView;
class TiltButton;





//  The platter's overlays, in the order of their chips (FR-031).
enum class PlatterOverlay
{
    Files,
    Reads,
    Writes,
    Scans,
    Alignment,
    Differences,
};





////////////////////////////////////////////////////////////////////////////////
//
//  PlatterCorners
//
//  The platter's controls, in translucent groups in the corners of its
//  column (FR-163): the legend at the top left, the tilt button and its
//  slider at the top right, the overlay chips at the bottom left, and the
//  zoom group at the bottom right.
//
////////////////////////////////////////////////////////////////////////////////

class PlatterCorners : public IInspectorComponent
{
public:
    PlatterCorners (InspectorSession & session, IInspectorComponentHost & host, PlatterView & platter);

    void          Create        () override;
    void          Update        () override;
    bool          OnMouse       (const DxuiMouseEvent & ev) override;
    std::wstring  GetTip        (POINT pointPx, RECT & outAnchorPx) const override;
    void          GetPrefs      (DiskInspectorPrefs & inOut) const override;
    void          ApplyPrefs    (const DiskInspectorPrefs & prefs) override;
    void          OnDiskStarted (const DiskChange & change) override;

    void    Layout         (const RECT & area, const DxuiDpiScaler & scaler);
    int     GetMinWidthPx  (const DxuiDpiScaler & scaler) const;
    bool    Contains       (POINT pointPx) const;
    bool    IsOverlayOn    (PlatterOverlay overlay) const;
    void    SetOverlay     (PlatterOverlay overlay, bool isOn);
    void    ShowLegend     (bool isPinned);
    void    HideLegend     ();
    void    ToggleTilt     ();
    double  GetLastTilt    () const { return m_lastTilt; }

private:
    void    Place          ();
    void    TrackLegendHover (POINT pointPx);
    bool    IsSliderWanted () const;
    int     GetChipsWidthPx (const DxuiDpiScaler & scaler) const;
    int     GetZoomWidthPx  (const DxuiDpiScaler & scaler) const;

    InspectorSession         & m_session;
    IInspectorComponentHost  & m_host;
    PlatterView              & m_platter;
    FloatingGroup            * m_legendGroup      = nullptr;
    FloatingGroup            * m_tiltGroup        = nullptr;
    FloatingGroup            * m_chipsGroup       = nullptr;
    FloatingGroup            * m_zoomGroup        = nullptr;
    DxuiIconButton           * m_legendButton     = nullptr;
    PlatterLegendView        * m_legend           = nullptr;
    DxuiPopupHost            * m_legendPopup      = nullptr;
    TiltButton               * m_tilt             = nullptr;
    DxuiSlider               * m_tiltSlider       = nullptr;
    OverlayChip              * m_filesChip        = nullptr;
    OverlayChip              * m_readsChip        = nullptr;
    OverlayChip              * m_writesChip       = nullptr;
    OverlayChip              * m_scansChip        = nullptr;
    OverlayChip              * m_alignChip        = nullptr;
    OverlayChip              * m_diffsChip        = nullptr;
    DxuiIconButton           * m_zoomOut          = nullptr;
    DxuiIconButton           * m_zoomIn           = nullptr;
    DxuiDpiScaler              m_scaler;
    RECT                       m_areaPx           = {};
    bool                       m_isLegendPinned   = false;
    int64_t                    m_legendClosedMs   = 0;
    bool                       m_isOverTilt       = false;
    bool                       m_isSliderOut      = false;
    bool                       m_isAlignment      = false;
    bool                       m_isFilesDefaulted = false;
    bool                       m_isFilesSaved     = false;
    double                     m_lastTilt         = 0.5235987755982988;
};
