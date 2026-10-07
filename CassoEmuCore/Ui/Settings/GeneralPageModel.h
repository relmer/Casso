#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  GeneralPageModel
//
//  The text and pref values behind Settings > General: when the last update
//  check ran, the skipped release, and the download-offer checkboxes over
//  their "ask" / "allow" / "decline" consent strings.
//
////////////////////////////////////////////////////////////////////////////////

class GeneralPageModel
{
public:
    static constexpr const char * kpszConsentAsk     = "ask";
    static constexpr const char * kpszConsentDecline = "decline";

    static bool          IsOfferChecked          (std::string_view consent);
    static std::string   MakeConsentFromChecked  (bool checked);

    static std::wstring  MakeLastCheckedText     (const SYSTEMTIME * checkedLocal, const SYSTEMTIME & nowLocal);
    static std::wstring  MakeLastCheckedTextNow  (std::int64_t checkedUtc);
    static std::wstring  MakeSkippedText         (std::string_view skippedVersion);
    static bool          TryGetLocalTime         (std::int64_t utcSeconds, SYSTEMTIME & outLocal);

private:
    static std::int64_t  GetDayNumber            (const SYSTEMTIME & date);
    static std::wstring  MakeClockText           (const SYSTEMTIME & time);
};
