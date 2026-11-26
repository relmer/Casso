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
};





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiWindowSizer
//
//  The window's size limits and its fit to its content.
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
    bool  GetMinMax       (MINMAXINFO & info) const;

private:
    IDxuiWindowSizerHost  & m_host;
    SIZE                    m_minSizeDip = { 0, 0 };
};
