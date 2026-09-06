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
    string            scenePose;                        // the readout's own format
    CaptureCrtParams  crt;
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

    // The CRT summary value. Public for the same reason.
    static string  FormatCrtParams (const CaptureCrtParams & crt);

    // The scene view written down: orbit in DEGREES, zoom, and pan.
    //
    // SHARED WITH THE ON-SCREEN POSE READOUT, which is the point. A pose read
    // out of a file and one read off an old screenshot have to be the same
    // text or they do not restore the same view, and two format strings in two
    // files is how they stop being. Angles arrive in radians because that is
    // what the scene holds; degrees are what a person can act on.
    static string  FormatScenePose (float yawRad, float pitchRad,
                                    float zoom, float panX, float panY);

private:
    static string  FormatFloat (float value, int decimals);
};
