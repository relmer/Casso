#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  PrintFileNaming
//
//  Collision-free output filename policy, shared by every file Casso writes
//  for the user: printouts (FR-012) and screenshots. Both the clock and the
//  "does this path exist" check are injected, so the policy is pure and
//  unit-testable -- the shell supplies GetLocalTime and fs::exists at the call
//  site, and does the actual write.
//
//  ONE OWNER FOR COLLISION HANDLING. Printouts and screenshots land in
//  adjacent folders under Pictures and a user sees them side by side, so they
//  share a naming grammar rather than each inventing one. The caller varies
//  only the leading words and the extension.
//
////////////////////////////////////////////////////////////////////////////////

class PrintFileNaming
{
public:
    // A collision-free path in `folder` for an artifact captured at `when`,
    // named "<baseName> YYYY-MM-DD HHMMSS<extension>". `extension` carries its
    // own leading dot (".png"), so it reads the way fs::path::extension does
    // and a caller cannot forget the separator. `taken` reports whether a
    // candidate already exists; a numeric suffix is appended until a free name
    // is found.
    static fs::path ComposeTimestampedPath (const fs::path &                          folder,
                                            const wstring &                           baseName,
                                            const wstring &                           extension,
                                            const SYSTEMTIME &                        when,
                                            const function<bool (const fs::path &)> & taken);

private:
    static wstring  ComposeCandidateName (const wstring & base,
                                          const wstring & extension,
                                          int             ordinal);
};
