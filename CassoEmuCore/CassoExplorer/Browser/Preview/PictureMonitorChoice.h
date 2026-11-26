#pragma once

#include "Pch.h"

#include "CassoExplorer/Model/PicturePreview.h"

class MemoryBus;
struct PreviewContent;





////////////////////////////////////////////////////////////////////////////////
//
//  PictureMonitorChoice
//
//  The monitor a picture preview is drawn for, with the custom monitor's
//  color, and the color form of a double hi-res picture. A change draws the
//  current picture again.
//
////////////////////////////////////////////////////////////////////////////////

class PictureMonitorChoice
{
public:
    PictureMonitorChoice (PreviewContent & preview, MemoryBus & bus);

    void                     SetPreviewMonitor (PicturePreview::Monitor monitor, uint32_t customArgb = PicturePreview::kDefaultCustomArgb);
    PicturePreview::Monitor  GetPreviewMonitor () const { return m_previewMonitor; }
    uint32_t                 GetCustomArgb     () const { return m_customArgb; }

    //  The monitor the current picture is drawn on: the chosen one, except
    //  for a double hi-res picture, whose monitor follows its form. Its color
    //  form is drawn on the color monitor, and its monochrome form on the
    //  chosen monochrome monitor, or the white one when the color monitor is
    //  chosen.
    PicturePreview::Monitor  GetPictureMonitor () const;

    static PicturePreview::Monitor  ChooseMonitor (PicturePreview::Monitor chosen, PicturePreview::Mode mode);

    //  A double hi-res picture drawn in color or as its bits, whichever the
    //  automatic choice was not; again to go back. False for any other preview.
    bool  TogglePictureColor    ();
    bool  CanTogglePictureColor () const;

    //  The current picture drawn again when its monitor is not the chosen one,
    //  as it is once its form is known. S_OK, with nothing drawn, when the monitor
    //  is the chosen one.
    HRESULT  RedrawForForm ();

private:
    PreviewContent           & m_preview;
    MemoryBus                & m_bus;
    PicturePreview::Monitor    m_previewMonitor = PicturePreview::Monitor::Color;
    uint32_t                   m_customArgb     = PicturePreview::kDefaultCustomArgb;
};
