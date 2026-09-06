#pragma once

#include "Pch.h"

#include "Capture/ScreenshotMode.h"
#include "Devices/Printer/PngMetadata.h"





////////////////////////////////////////////////////////////////////////////////
//
//  CaptureCrtParams
//
//  The CRT settings in force for a capture, as data.
//
//  A COPY OF THE SHELL'S CrtParams RATHER THAN THE THING ITSELF. That type
//  lives with the post-process chain in the executable, and core cannot see
//  it; taking a preformatted string instead would move the decision of WHICH
//  parameters a screenshot records out of reach of a test. So the shell copies
//  seven numbers across and the composing stays here.
//
////////////////////////////////////////////////////////////////////////////////

struct CaptureCrtParams
{
    float  brightness        = 1.0f;
    float  contrast          = 1.0f;
    float  gamma             = 1.0f;
    float  scanlineIntensity = 0.0f;
    float  bloomStrength     = 0.0f;
    float  bloomRadius       = 0.0f;
    float  colorBleedWidth   = 0.0f;
    float  persistence       = 0.0f;
};





////////////////////////////////////////////////////////////////////////////////
//
//  ScreenshotFacts
//
//  Everything a screenshot can say about itself.
//
//  Already-resolved strings and plain numbers, deliberately: the composer's
//  job is deciding WHICH entries a mode emits, not how to spell a monitor key
//  or a scene pose. Those have owners elsewhere and one spelling each.
//
////////////////////////////////////////////////////////////////////////////////

struct ScreenshotFacts
{
    ScreenshotMode    mode              = ScreenshotMode::Scene;
    string            versionString;                    // "Casso 1.22.0"
    string            machineDisplayName;               // "Apple //e"
    SYSTEMTIME        when              = {};
    int               utcOffsetMinutes  = 0;
    string            monitorKey;                       // CrtResolver::MakeKey output
    CaptureCrtParams  crt;

    // The scene view as NUMBERS, not as the readout's sentence. Each one
    // becomes its own entry, so a reader restoring a view reads five values
    // instead of parsing a line.
    bool              hasScenePose      = false;
    float             orbitYawRad       = 0.0f;
    float             orbitPitchRad     = 0.0f;
    float             zoom              = 1.0f;
    float             panX              = 0.0f;
    float             panY              = 0.0f;
};





////////////////////////////////////////////////////////////////////////////////
//
//  ScreenshotMetadata
//
//  THE SINGLE AUTHORITY for what a screenshot says. The per-mode entry set is
//  the published contract in specs/030-screenshot-capture/contracts; nothing
//  else decides it, and the codec below simply writes what it is handed.
//
//  Entries come back in the contract's order so a round-trip test can compare
//  sequences rather than sets, and so files written by successive builds diff
//  cleanly.
//
////////////////////////////////////////////////////////////////////////////////

class ScreenshotMetadata
{
public:
    static vector<MetadataEntry>  Compose (const ScreenshotFacts & facts);

    // RFC 1123 with the UTC offset, e.g. "Sat, 05 Sep 2026 14:32:07 -0700".
    // Public because it is a format with rules of its own worth pinning apart
    // from the entry set that carries it.
    static string  FormatCreationTime (const SYSTEMTIME & when, int utcOffsetMinutes);

    // The scene view as one line, for the ON-SCREEN READOUT only. The file
    // records the same view as separate numeric entries instead -- a reader
    // restoring a pose wants five values, not a sentence to parse -- so this
    // no longer has a second caller and is not what a screenshot carries.
    // Angles arrive in radians because that is what the scene holds; degrees
    // are what a person can act on.
    static string  FormatScenePose (float yawRad, float pitchRad,
                                    float zoom, float panX, float panY);

    static float   RadiansToDegrees (float radians);

private:
    static string  FormatFloat (float value, int decimals);
};
