#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  ConsoleLinks
//
//  Links to files in the console's lines. A link is a run of a line's text
//  and the full path of a file. The console draws the run in the link color,
//  the tip over it is the path, and a click on it opens the file's folder
//  with the file selected.
//
//  A LINK IS MARKED INSIDE THE LINE, so a console line remains one string
//  from the CPU thread to the pane and in the text the project saves. The
//  mark is kOpen, the path, kSplit, the link's text, then kClose; none of
//  them is a character a reply or a file name has. Only the console's lines
//  are marked: a reply's text, which the debug channel and batch runs write,
//  has the full path written out.
//
////////////////////////////////////////////////////////////////////////////////

class ConsoleLinks
{
public:
    //  A link in a line's text as drawn in the console: its first character
    //  and its length in that text, and the file's full path.
    struct Link
    {
        int           start  = 0;
        int           length = 0;
        std::wstring  path;
    };

    static constexpr char  kOpen  = '\x01';
    static constexpr char  kSplit = '\x02';
    static constexpr char  kClose = '\x03';

    //  `label` linked to the file at `path`, and a file's name linked to it.
    static std::string   MakeLink      (const std::string & label, const std::string & path);
    static std::string   MakeFileLink  (const std::string & path);

    //  Each full path of `paths` in the text replaced by its file's name,
    //  linked to it; text already linked is left alone.
    static std::string   LinkFiles     (const std::string & line, const std::vector<std::string> & paths);
    static void          LinkFiles     (std::vector<std::string> & lines, const std::vector<std::string> & paths);

    //  Every full path in the text, quoted or not, from a drive letter or a
    //  share's `\\`, replaced by its file's name linked to it; the quotes
    //  around a quoted one go too. Text already linked is left alone.
    static std::string   LinkPaths     (const std::string & line);

    //  The file's name at the end of `label`, as a scope's label ends with
    //  it, linked to the file; the label as it is when it doesn't end so.
    static std::string   LinkFileName  (const std::string & label, const std::string & path);

    //  The line's text as drawn in the console, wide, with its links; and
    //  the same text narrow, for laying a line out.
    static std::wstring  Parse         (const std::string & line, std::vector<Link> & outLinks);
    static std::string   GetShownText  (const std::string & line);

    //  A file's name: the text after a path's last slash.
    static std::string   GetFileName   (const std::string & path);

private:
    //  Whether a path that matched up to `at` ends there, rather than going
    //  on to a longer path.
    static bool          IsPathEnd     (const std::string & line, size_t at);
    static bool          IsPathStart   (const std::string & line, size_t at);
};
