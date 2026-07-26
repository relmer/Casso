#pragma once

#include "Pch.h"

class PrintRaster;




////////////////////////////////////////////////////////////////////////////////
//
//  PrintJobStore
//
//  Per-machine pending-strip persistence (FR-026): the irreducible file-I/O
//  edge around the unit-tested core serializer (PrintJobPersistence). Reads and
//  writes <machine-state>/PendingPrint/{strip.png,strip.json}; the caller owns
//  the directory path (<assetBase>/Machines/<name>/PendingPrint). The strip.png
//  is a lossless indexed-colour native-grid image, strip.json its feed/boundary
//  metadata. COM must be live on the calling thread (the codec uses WIC).
//
////////////////////////////////////////////////////////////////////////////////

class PrintJobStore
{
public:
    // Rebuilds the strip from the sidecar. Returns S_FALSE (outRaster left
    // empty) when no pending strip exists, so a first-run open is not an error.
    static HRESULT  Load  (const fs::path & dir, PrintRaster & outRaster);

    // Writes the strip as PendingPrint/{strip.png,strip.json}, creating the
    // directory. Overwrites any prior pending copy.
    static HRESULT  Save  (const fs::path & dir, const PrintRaster & raster);

    // Deletes the pending sidecar (eject/discard). Best-effort, never throws.
    static void     Clear (const fs::path & dir);
};
