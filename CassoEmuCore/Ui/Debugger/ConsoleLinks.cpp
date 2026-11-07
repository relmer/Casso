#include "Pch.h"

#include "Ui/Debugger/ConsoleLinks.h"

#include "Core/TextEncoding.h"





////////////////////////////////////////////////////////////////////////////////
//
//  ConsoleLinks::MakeLink
//
////////////////////////////////////////////////////////////////////////////////

std::string ConsoleLinks::MakeLink (const std::string & label, const std::string & path)
{
    return std::string (1, kOpen) + path + kSplit + label + kClose;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ConsoleLinks::MakeFileLink
//
//  A folder's path, with no file name after its last slash, is linked
//  whole.
//
////////////////////////////////////////////////////////////////////////////////

std::string ConsoleLinks::MakeFileLink (const std::string & path)
{
    std::string  name = GetFileName (path);



    return MakeLink (name.empty() ? path : name, path);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ConsoleLinks::GetFileName
//
////////////////////////////////////////////////////////////////////////////////

std::string ConsoleLinks::GetFileName (const std::string & path)
{
    size_t  slash = path.find_last_of ("/\\");



    return (slash == std::string::npos) ? path : path.substr (slash + 1);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ConsoleLinks::LinkFiles
//
//  Longer paths are compared first, so a path that starts another is not
//  linked inside it, and a path is linked only where the text has it whole:
//  a longer path that starts with it, as a copy set aside has, is a
//  different file.
//
////////////////////////////////////////////////////////////////////////////////

std::string ConsoleLinks::LinkFiles (const std::string & line, const std::vector<std::string> & paths)
{
    std::vector<std::string>  sorted;
    std::string               linked;
    size_t                    at     = 0;



    for (const std::string & path : paths)
    {
        if (!path.empty())
        {
            sorted.push_back (path);
        }
    }

    std::ranges::sort (sorted, [] (const std::string & a, const std::string & b) { return a.size() > b.size(); });

    while (at < line.size())
    {
        size_t  close = (line[at] == kOpen) ? line.find (kClose, at) : std::string::npos;
        auto    path  = std::ranges::find_if (sorted, [&line, at] (const std::string & each)
                        {
                            return line.compare (at, each.size(), each) == 0 && IsPathEnd (line, at + each.size());
                        });

        if (line[at] == kOpen)
        {
            close   = (close == std::string::npos) ? line.size() : close + 1;
            linked += line.substr (at, close - at);
            at      = close;
            continue;
        }

        if (path == sorted.end())
        {
            linked += line[at];
            at++;
            continue;
        }

        linked += MakeFileLink (*path);
        at     += path->size();
    }

    return linked;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ConsoleLinks::IsPathEnd
//
//  The end of the text, a space, a mark, or a closing quote, parenthesis,
//  comma or semicolon. A period or a colon ends a path only when it ends a
//  sentence or a clause, followed by the end or a space; one followed by
//  more text, as an extension is, continues the path.
//
////////////////////////////////////////////////////////////////////////////////

bool ConsoleLinks::IsPathEnd (const std::string & line, size_t at)
{
    static constexpr std::string_view  kClosers   = ",;)]\"'";
    static constexpr std::string_view  kStops     = ".:!?";
    bool                               isLast     = at + 1 >= line.size();
    bool                               isSpaced   = !isLast && (line[at + 1] == ' ' || line[at + 1] == '\t' || line[at + 1] == kOpen);



    if (at >= line.size() || line[at] == ' ' || line[at] == '\t' || line[at] == kOpen || kClosers.find (line[at]) != std::string_view::npos)
    {
        return true;
    }

    return kStops.find (line[at]) != std::string_view::npos && (isLast || isSpaced);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ConsoleLinks::LinkFiles
//
////////////////////////////////////////////////////////////////////////////////

void ConsoleLinks::LinkFiles (std::vector<std::string> & lines, const std::vector<std::string> & paths)
{
    if (paths.empty())
    {
        return;
    }

    for (std::string & line : lines)
    {
        line = LinkFiles (line, paths);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ConsoleLinks::LinkPaths
//
//  A quoted path runs to its closing quote, so it may have spaces in it; a
//  bare one runs to a space, a quote, a comma, a semicolon or a closing
//  parenthesis, less a period or colon that ends a sentence after it.
//
////////////////////////////////////////////////////////////////////////////////

std::string ConsoleLinks::LinkPaths (const std::string & line)
{
    std::string  out;
    size_t       at = 0;



    out.reserve (line.size());

    while (at < line.size())
    {
        size_t       end    = std::string::npos;
        std::string  path;

        if (line[at] == kOpen)
        {
            end = line.find (kClose, at);
            end = (end == std::string::npos) ? line.size() : end + 1;
            out.append (line, at, end - at);
            at  = end;
            continue;
        }

        if (line[at] == '"' && IsPathStart (line, at + 1))
        {
            end = line.find ('"', at + 1);
        }

        if (end != std::string::npos)
        {
            path = line.substr (at + 1, end - at - 1);
            out += MakeFileLink (path);
            at   = end + 1;
            continue;
        }

        if ((at == 0 || strchr (" \t(=,'", line[at - 1]) != nullptr) && IsPathStart (line, at))
        {
            end = line.find_first_of (" \t\",;)", at);
            end = (end == std::string::npos) ? line.size() : end;

            while (end > at && (line[end - 1] == '.' || line[end - 1] == ':'))
            {
                end--;
            }

            path = line.substr (at, end - at);

            if (!GetFileName (path).empty())
            {
                out += MakeFileLink (path);
                at   = end;
                continue;
            }
        }

        out += line[at];
        at++;
    }

    return out;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ConsoleLinks::IsPathStart
//
//  A drive letter, a colon and a slash, or a share's two backslashes.
//
////////////////////////////////////////////////////////////////////////////////

bool ConsoleLinks::IsPathStart (const std::string & line, size_t at)
{
    bool  isDrive = at + 2 < line.size() && isalpha ((unsigned char) line[at]) && line[at + 1] == ':' && (line[at + 2] == '\\' || line[at + 2] == '/');
    bool  isShare = at + 2 < line.size() && line[at] == '\\' && line[at + 1] == '\\' && isalnum ((unsigned char) line[at + 2]);



    return isDrive || isShare;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ConsoleLinks::LinkFileName
//
////////////////////////////////////////////////////////////////////////////////

std::string ConsoleLinks::LinkFileName (const std::string & label, const std::string & path)
{
    std::string  name = GetFileName (path);



    if (path.empty() || name.empty() || !label.ends_with (name))
    {
        return label;
    }

    return label.substr (0, label.size() - name.size()) + MakeLink (name, path);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ConsoleLinks::Parse
//
//  A mark left open, with no kSplit or kClose after it, is not a link; the
//  marks are dropped from its text.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring ConsoleLinks::Parse (const std::string & line, std::vector<Link> & outLinks)
{
    std::wstring  shown;
    size_t        at    = 0;



    outLinks.clear();

    while (at < line.size())
    {
        size_t       open  = line.find (kOpen, at);
        size_t       split = (open == std::string::npos)  ? std::string::npos : line.find (kSplit, open + 1);
        size_t       close = (split == std::string::npos) ? std::string::npos : line.find (kClose, split + 1);
        std::string  rest;
        Link         link;

        if (close == std::string::npos)
        {
            rest = line.substr (at);
            std::erase_if (rest, [] (char ch) { return ch == kOpen || ch == kSplit || ch == kClose; });
            shown += TextEncoding::NarrowToWide (rest);
            break;
        }

        shown      += TextEncoding::NarrowToWide (line.substr (at, open - at));
        link.start  = (int) shown.size();
        link.path   = TextEncoding::NarrowToWide (line.substr (open + 1, split - open - 1));
        shown      += TextEncoding::NarrowToWide (line.substr (split + 1, close - split - 1));
        link.length = (int) shown.size() - link.start;
        at          = close + 1;

        if (link.length > 0)
        {
            outLinks.push_back (std::move (link));
        }
    }

    return shown;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ConsoleLinks::GetShownText
//
////////////////////////////////////////////////////////////////////////////////

std::string ConsoleLinks::GetShownText (const std::string & line)
{
    std::string  shown;
    size_t       at    = 0;



    if (line.find (kOpen) == std::string::npos)
    {
        return line;
    }

    while (at < line.size())
    {
        size_t       open  = line.find (kOpen, at);
        size_t       split = (open == std::string::npos)  ? std::string::npos : line.find (kSplit, open + 1);
        size_t       close = (split == std::string::npos) ? std::string::npos : line.find (kClose, split + 1);
        std::string  rest;

        if (close == std::string::npos)
        {
            rest = line.substr (at);
            std::erase_if (rest, [] (char ch) { return ch == kOpen || ch == kSplit || ch == kClose; });
            shown += rest;
            break;
        }

        shown += line.substr (at, open - at);
        shown += line.substr (split + 1, close - split - 1);
        at     = close + 1;
    }

    return shown;
}
