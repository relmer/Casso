#pragma once

#include "Pch.h"





class DiskImage;





////////////////////////////////////////////////////////////////////////////////
//
//  FluxSectorWriter
//
//  Sector writes to a flux track, spliced into the flux at the controller's
//  cell.
//
////////////////////////////////////////////////////////////////////////////////

class FluxSectorWriter
{
public:
    //  A sector write on a flux track: each sector whose bytes changed has
    //  its data field spliced into the flux at the controller's cell. A blank
    //  flux track gets a whole standard track the same way.
    static HRESULT  WriteFluxTrackSectors (const vector<Byte>  &  sectors,
                                           const int           *  interleave,
                                           int                    track,
                                           DiskImage           &  inOutImage);

private:
    static void     WriteBlankFluxTrack   (const vector<Byte>  &  sectors,
                                           const int           *  interleave,
                                           int                    track,
                                           DiskImage           &  inOutImage);
};