#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  EventLogText
//
//  Text both event log panes put in their cells: a cycle count with
//  thousands separators, a host clock reading as local time to the
//  millisecond, a byte as hex with its character, and the rows that are not
//  events.
//
////////////////////////////////////////////////////////////////////////////////

class EventLogText
{
public:
    //  1,234,567.
    static std::wstring  FormatCycle    (uint64_t cycle);

    //  A UTC FILETIME as local HH:MM:SS.mmm; empty for 0.
    static std::wstring  FormatWallTime (uint64_t fileTime);

    //  $C1 'A', the character from the low seven bits, or just $8D for a
    //  control character.
    static std::wstring  FormatByteChar (uint8_t value);

    //  The printable character of a key's low seven bits, or '.' for a
    //  control character.
    static wchar_t       GetPrintable   (uint8_t value);

    //  The label and text of a row that is not an event: a gap, an
    //  unavailable stretch, a reset or a power cycle.
    static const wchar_t *  GetGapLabel          () { return L"Gap"; }
    static const wchar_t *  GetGapText           () { return L"Recording paused until here"; }
    static const wchar_t *  GetUnavailableLabel  () { return L"Unavailable"; }
    static const wchar_t *  GetUnavailableText   () { return L"Other disks were in the drives here: events until the next keyframe can't be rebuilt"; }
    static const wchar_t *  GetResetLabel        () { return L"Reset"; }
    static const wchar_t *  GetPowerCycleLabel   () { return L"Power cycle"; }

    //  The row above the oldest: where history begins, the rebuild going
    //  on, or the note that scrolling up rebuilds more.
    static const wchar_t *  GetStartText         () { return L"History begins here"; }
    static const wchar_t *  GetRebuildingText    () { return L"Rebuilding earlier events from history..."; }
    static const wchar_t *  GetEarlierText       () { return L"Scroll up to rebuild earlier events from history"; }
};
