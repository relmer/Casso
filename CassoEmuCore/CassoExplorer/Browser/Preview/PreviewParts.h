#pragma once

#include "Pch.h"

class MemoryBus;
class PictureMonitorChoice;
class PreviewViewState;
struct PreviewContent;





////////////////////////////////////////////////////////////////////////////////
//
//  PreviewParts
//
//  The preview's content, the memory bus that pictures are decoded on, the
//  view choices, and the picture monitor.
//
////////////////////////////////////////////////////////////////////////////////

struct PreviewParts
{
    PreviewContent        & preview;
    MemoryBus             & bus;
    PreviewViewState      & viewState;
    PictureMonitorChoice  & monitor;
};
