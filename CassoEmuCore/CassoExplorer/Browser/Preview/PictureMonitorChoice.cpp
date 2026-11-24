#include "Pch.h"

#include "CassoExplorer/Browser/Preview/PictureMonitorChoice.h"
#include "CassoExplorer/Model/PreviewDecoder.h"





////////////////////////////////////////////////////////////////////////////////
//
//  PictureMonitorChoice::PictureMonitorChoice
//
////////////////////////////////////////////////////////////////////////////////

PictureMonitorChoice::PictureMonitorChoice (PreviewContent & preview, MemoryBus & bus)
    : m_preview (preview),
      m_bus     (bus)
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  PictureMonitorChoice::SetPreviewMonitor
//
////////////////////////////////////////////////////////////////////////////////

void PictureMonitorChoice::SetPreviewMonitor (PicturePreview::Monitor monitor, uint32_t customArgb)
{
    HRESULT  hr = S_OK;



    if (monitor == m_previewMonitor && customArgb == m_customArgb)
    {
        return;
    }

    m_previewMonitor = monitor;
    m_customArgb     = customArgb;

    if (m_preview.kind == PreviewContent::Kind::Picture && m_preview.pictureMode != PicturePreview::Mode::None)
    {
        hr = PicturePreview::Render (m_preview.pictureMode, m_preview.bytes, m_preview.origin, m_bus,
                                     m_preview.bgra, m_preview.width, m_preview.height, GetPictureMonitor(), m_customArgb);
        IGNORE_RETURN_VALUE (hr, S_OK);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  PictureMonitorChoice::GetPictureMonitor
//
////////////////////////////////////////////////////////////////////////////////

PicturePreview::Monitor PictureMonitorChoice::GetPictureMonitor() const
{
    return CanTogglePictureColor() ? ChooseMonitor (m_previewMonitor, m_preview.pictureMode) : m_previewMonitor;
}





////////////////////////////////////////////////////////////////////////////////
//
//  PictureMonitorChoice::ChooseMonitor
//
////////////////////////////////////////////////////////////////////////////////

PicturePreview::Monitor PictureMonitorChoice::ChooseMonitor (PicturePreview::Monitor chosen, PicturePreview::Mode mode)
{
    PicturePreview::Monitor  monitor = chosen;



    if (mode == PicturePreview::Mode::DoubleHiRes || mode == PicturePreview::Mode::DoubleHiResWindowed || mode == PicturePreview::Mode::DoubleHiResLatched)
    {
        monitor = PicturePreview::Monitor::Color;
    }
    else if (mode == PicturePreview::Mode::DoubleHiResMono && chosen == PicturePreview::Monitor::Color)
    {
        monitor = PicturePreview::Monitor::White;
    }

    return monitor;
}





////////////////////////////////////////////////////////////////////////////////
//
//  PictureMonitorChoice::CanTogglePictureColor
//
////////////////////////////////////////////////////////////////////////////////

bool PictureMonitorChoice::CanTogglePictureColor() const
{
    return m_preview.kind == PreviewContent::Kind::Picture && PicturePreview::IsDoubleHiRes (m_preview.pictureChosen);
}





////////////////////////////////////////////////////////////////////////////////
//
//  PictureMonitorChoice::TogglePictureColor
//
//  Lasts for this picture: the next picture's mode is chosen afresh.
//
////////////////////////////////////////////////////////////////////////////////

bool PictureMonitorChoice::TogglePictureColor()
{
    HRESULT  hr = S_OK;



    if (!CanTogglePictureColor())
    {
        return false;
    }

    m_preview.pictureMode = (m_preview.pictureMode != PicturePreview::Mode::DoubleHiResMono) ? PicturePreview::Mode::DoubleHiResMono
                                                                                             : PicturePreview::Mode::DoubleHiRes;

    hr = PicturePreview::Render (m_preview.pictureMode, m_preview.bytes, m_preview.origin, m_bus,
                                 m_preview.bgra, m_preview.width, m_preview.height, GetPictureMonitor(), m_customArgb);

    return SUCCEEDED (hr);
}





////////////////////////////////////////////////////////////////////////////////
//
//  PictureMonitorChoice::RedrawForForm
//
//  A double hi-res picture's monitor follows its form, which is known once it
//  is decoded.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT PictureMonitorChoice::RedrawForForm()
{
    HRESULT  hr = S_OK;



    if (GetPictureMonitor() != m_previewMonitor)
    {
        hr = PicturePreview::Render (m_preview.pictureMode, m_preview.bytes, m_preview.origin, m_bus,
                                     m_preview.bgra, m_preview.width, m_preview.height, GetPictureMonitor(), m_customArgb);
    }

    return hr;
}
