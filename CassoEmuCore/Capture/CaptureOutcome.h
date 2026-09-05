#pragma once

#include "Pch.h"

#include "Capture/ScreenshotPlan.h"





////////////////////////////////////////////////////////////////////////////////
//
//  CaptureOutcome
//
//  What actually happened, and the words the user is shown about it.
//
//  THE TWO SINKS ARE INDEPENDENT. The clipboard copy and the file write each
//  succeed or fail on their own, and neither failure suppresses the other --
//  a clipboard held by another application must not cost the file, and a full
//  disk must not cost the paste the user was about to do.
//
//  DescribeResult lives here rather than in the shell so every branch of that
//  reporting is reachable by a test. The shell shows the string it is handed
//  and chooses no wording; a notice path with its own conditionals is a notice
//  path nobody ever proves right.
//
////////////////////////////////////////////////////////////////////////////////

struct CaptureOutcome
{
    CaptureRefusal  refusal        = CaptureRefusal::None;
    bool            clipboardOk    = false;
    bool            fileWritten    = false;

    // Separates "saving is switched off" from "the write was tried and
    // failed". Without it both look like fileWritten == false, and the user
    // gets told a file failed when they had asked for no file.
    bool            writeAttempted = false;

    fs::path        path;

    // The notice text for this outcome. Never empty: every reachable state
    // has something worth saying, including the wholly successful one.
    static wstring  DescribeResult (const CaptureOutcome & outcome);
};
