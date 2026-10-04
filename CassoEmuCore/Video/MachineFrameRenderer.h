#pragma once

#include "Pch.h"

class MachineBuilder;
class MachineHost;





////////////////////////////////////////////////////////////////////////////////
//
//  MachineFrameRenderer
//
//  One frame of a machine's screen as a color monitor shows it, drawn by the
//  machine's own video modes: the mode its soft switches select, with the
//  mixed-mode text rows over it. For a machine other than the one on
//  screen, such as one loaded from a snapshot to draw a picture of it; the
//  emulator's own frame also carries the monitor's tint and the debugger's
//  beam mark, which this leaves out.
//
////////////////////////////////////////////////////////////////////////////////

class MachineFrameRenderer
{
public:
    static constexpr int  kWidth  = 560;
    static constexpr int  kHeight = 384;

    //  `outBgra` becomes kWidth by kHeight, top row first.
    static HRESULT  Render (MachineHost & machine, MachineBuilder & builder, bool flashOn, std::vector<uint32_t> & outBgra);

private:
    static constexpr uint32_t  kBlackArgb     = 0xFF000000u;
    static constexpr uint32_t  kWhiteArgb     = 0xFFFFFFFFu;
    static constexpr int       kMixedFirstRow = 20;
    static constexpr int       kMixedLastRow  = 24;
};
