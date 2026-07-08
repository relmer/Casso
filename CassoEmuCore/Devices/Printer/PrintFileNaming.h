#pragma once

#include "Pch.h"




////////////////////////////////////////////////////////////////////////////////
//
//  PrintFileNaming
//
//  Collision-free output filename policy for the PNG sink (FR-012). Both the
//  clock and the "does this path exist" check are injected, so the policy is
//  pure and unit-testable -- the shell supplies GetLocalTime and fs::exists at
//  the call site, and does the actual write.
//
////////////////////////////////////////////////////////////////////////////////

class PrintFileNaming
{
public:
    // A collision-free ".png" path in `folder` for a print captured at `when`.
    // `taken` reports whether a candidate path already exists; a numeric suffix
    // is appended until a free name is found.
    static fs::path ComposePngPath (const fs::path &                          folder,
                                    const SYSTEMTIME &                        when,
                                    const function<bool (const fs::path &)> & taken);

private:
    static wstring  CandidateName (const wstring & base, int ordinal);
};
