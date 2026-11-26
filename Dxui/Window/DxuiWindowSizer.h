#pragma once

#include "Pch.h"


class DxuiHwndSource;





////////////////////////////////////////////////////////////////////////////////
//
//  IDxuiWindowSizerHost
//
//  What DxuiWindowSizer reads from its window: the host, and the largest
//  client sizes the window's content reports.
//
////////////////////////////////////////////////////////////////////////////////

class IDxuiWindowSizerHost
{
public:
    virtual ~IDxuiWindowSizerHost() = default;

    virtual DxuiHwndSource *  GetSizerSource       () const                      = 0;
    virtual bool              GetMaxClientSizePx   (SIZE & outSizePx) const      = 0;
    virtual bool              GetMaxTrackSizePx    (SIZE & outSizePx) const      = 0;
    virtual void              NotifyWindowPlaced   ()                            = 0;
};





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiWindowSizer
//
//  The window's size limits, its fit to its content, and when a change of
//  its position or size is a placement: a change outside the OS drag loop
//  is one, and inside it only the end of the loop is.
//
////////////////////////////////////////////////////////////////////////////////

class DxuiWindowSizer
{
public:
    //  Fit grows or shrinks the window to the content's largest client size,
    //  for a window opened before its content was laid out. Grow never
    //  shrinks it, for content that grows while the window is open.
    enum class MaxSizeMode
    {
        Fit,
        Grow,
    };

    explicit DxuiWindowSizer (IDxuiWindowSizerHost & host) : m_host (host) {}

    //  The smallest size the user can resize the window to.
    void  SetMinSizeDip   (const SIZE & sizeDip) { m_minSizeDip = sizeDip; }

    void  ResizeToMaxSize (MaxSizeMode mode);
    SIZE  AddFrameSizePx  (const SIZE & clientPx) const;
    bool  GetMinMax       (MINMAXINFO & info) const;

    void  BeginSizeMove   ()       { m_inSizeMove = true; }
    void  EndSizeMove     ()       { m_inSizeMove = false; m_host.NotifyWindowPlaced(); }
    void  NoteGeometry    () const { if (!m_inSizeMove) { m_host.NotifyWindowPlaced(); } }

private:
    IDxuiWindowSizerHost  & m_host;
    SIZE                    m_minSizeDip = { 0, 0 };
    bool                    m_inSizeMove = false;   // between WM_ENTERSIZEMOVE and WM_EXITSIZEMOVE
};
