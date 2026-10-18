#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  ReleaseVersion
//
//  A release's MAJOR.MINOR.PATCH, as a release tag ("v1.30.0") or a bare
//  version ("1.30.0") writes it. Ordered on all three parts; the minor line
//  is (major, minor) alone, which is what a README release highlight covers.
//
////////////////////////////////////////////////////////////////////////////////

struct ReleaseVersion
{
    int  major = 0;
    int  minor = 0;
    int  patch = 0;

    static HRESULT  Parse             (std::string_view text, ReleaseVersion & outVersion);
    static bool     TryParse          (std::string_view text, ReleaseVersion & outVersion);

    std::string     ToString          () const;
    int             CompareMinorLine  (const ReleaseVersion & other) const;

    auto operator<=> (const ReleaseVersion &) const = default;
    bool operator==  (const ReleaseVersion &) const = default;

private:
    static bool     TryParseNumber    (std::string_view & text, int & outNumber);
    static bool     TryConsumeDot     (std::string_view & text);
};
