#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  SourceDocuments
//
//  Which source file each document slot shows (FR-054, FR-113): a file the
//  PC or a navigation reaches is brought forward in the slot that already
//  holds it, or opened in a free one. With every slot taken, the one used
//  longest ago gives way, but never the one holding the file the caller says
//  to keep -- the PC's.
//
//  The slots are the window's document panes, which exist from the start and
//  show while they hold a file, as the disassembly views and memory windows
//  do. What is open is saved by file name and line, since a file's id belongs
//  to the debug file it came from, and restored once a debug file with those
//  names is loaded.
//
//  No window and no controls: the window asks which slot a file is in and
//  shows it there.
//
////////////////////////////////////////////////////////////////////////////////

class SourceDocuments
{
public:
    static constexpr int  kMaxDocuments = 8;

    //  A saved document: its file's name and the line it showed.
    struct Saved
    {
        std::string  name;
        int          line = 0;

        bool operator== (const Saved &) const = default;
    };

    //  The slot showing `fileId`, opened if no slot shows it yet, and marked
    //  as used now. `keepFileId` is never closed to make room.
    int   Open       (int fileId, int keepFileId = -1);
    void  Close      (int slot);
    void  Clear      ();

    //  The slot showing `fileId`, or -1.
    int   Find       (int fileId) const;
    bool  IsOpen     (int slot) const { return GetFileId (slot) >= 0; }
    int   GetFileId  (int slot) const;
    int   GetCount   () const;

    //  The line a slot's document shows, kept for saving.
    void  SetLine    (int slot, int line);
    int   GetLine    (int slot) const;

    //  " source=NAME:LINE" for each open document, in slot order, a name's
    //  spaces and percent signs escaped so the text splits on spaces.
    std::string  Format (const std::function<std::string (int fileId)> & nameOf) const;

    //  The same text for documents saved and not yet reopened.
    static std::string  FormatSaved (const std::vector<Saved> & saved);

    //  The saved documents in open-views text; anything else is passed over.
    static std::vector<Saved>  Parse (const std::string & text);

private:
    struct Slot
    {
        int       fileId = -1;
        int       line   = 0;
        uint64_t  used   = 0;
    };

    static std::string  Escape   (const std::string & name);
    static std::string  Unescape (const std::string & text);

    std::array<Slot, kMaxDocuments>  m_slots;
    uint64_t                         m_clock = 0;
};
