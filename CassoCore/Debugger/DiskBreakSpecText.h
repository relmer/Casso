#pragma once

#include "Debugger/DiskBreakSpec.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DiskBreakSpecText
//
//  A disk breakpoint as text: the arguments BPDISK takes to set it again,
//  which BPSAVE writes and AppleWinParser reads back; the description the
//  replies and the Name column use; and the event's words. Numbers are hex,
//  with no prefix, as BPSAVE's other lines write them.
//
////////////////////////////////////////////////////////////////////////////////

class DiskBreakSpecText
{
public:
    //  An event word BPDISK takes, the event it sets and its syntax line.
    //  WRITE sets write mode, and the parser turns it into a written
    //  prologue or a blocked write by its arguments.
    struct EventWord
    {
        const char  * word;
        DiskEvent     event;
        const char  * syntax;
    };

    //  The event words in FR-163's order, the breakpoint dialog's.
    static std::span<const EventWord>  GetEventWords();

    //  "D1 READ 11 0 PASSED", without the command word.
    static std::string   FormatArguments (const DiskBreakSpec & spec);

    //  "sector read, track $11 sector $00, drive 1".
    static std::string   GetDescription  (const DiskBreakSpec & spec);

    //  The Trigger column's word: "Sector read", "Address field", ...
    static const char *  GetTriggerText  (DiskEvent event);

    //  The event's own words, which the description begins with: "sector read".
    static const char *  GetEventText    (DiskEvent event);

    //  The request's word after RWTS or DRIVER: READ, WRITE, SEEK, STATUS or
    //  FORMAT; null for a command neither has.
    static const char *  GetCommandWord  (DiskEvent event, Byte command);

private:
    static void          AddMarks    (const DiskBreakSpec & spec, std::string & text);
    static void          AddRange    (const DiskBreakSpec & spec, std::string & text);
    static void          AddNumbers  (std::initializer_list<std::optional<Byte>> numbers, std::string & text);
    static void          AddFields   (const DiskBreakSpec & spec, std::string & text);
    static std::string   FormatRange (const DiskBreakSpec & spec);
};
