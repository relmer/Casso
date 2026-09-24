#include "Pch.h"

#include "Ui/Debugger/Panes/SourceDocuments.h"





static constexpr const char * s_kpszToken = "source=";





////////////////////////////////////////////////////////////////////////////////
//
//  SourceDocuments::Open
//
//  The slot already showing the file, else the first free one, else the one
//  used longest ago that is not showing the file to keep.
//
////////////////////////////////////////////////////////////////////////////////

int SourceDocuments::Open (int fileId, int keepFileId)
{
    int  slot = Find (fileId);



    if (fileId < 0)
    {
        return -1;
    }

    for (int i = 0; slot < 0 && i < kMaxDocuments; i++)
    {
        if (m_slots[(size_t) i].fileId < 0)
        {
            slot = i;
        }
    }

    if (slot < 0)
    {
        for (int i = 0; i < kMaxDocuments; i++)
        {
            bool  kept = m_slots[(size_t) i].fileId == keepFileId;

            if (!kept && (slot < 0 || m_slots[(size_t) i].used < m_slots[(size_t) slot].used))
            {
                slot = i;
            }
        }
    }

    if (m_slots[(size_t) slot].fileId != fileId)
    {
        m_slots[(size_t) slot] = Slot { fileId, 0, 0 };
    }

    m_slots[(size_t) slot].used = ++m_clock;

    return slot;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SourceDocuments::Close
//
////////////////////////////////////////////////////////////////////////////////

void SourceDocuments::Close (int slot)
{
    if (slot >= 0 && slot < kMaxDocuments)
    {
        m_slots[(size_t) slot] = Slot();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  SourceDocuments::Clear
//
////////////////////////////////////////////////////////////////////////////////

void SourceDocuments::Clear()
{
    m_slots.fill (Slot());
}





////////////////////////////////////////////////////////////////////////////////
//
//  SourceDocuments::Find
//
////////////////////////////////////////////////////////////////////////////////

int SourceDocuments::Find (int fileId) const
{
    for (int i = 0; fileId >= 0 && i < kMaxDocuments; i++)
    {
        if (m_slots[(size_t) i].fileId == fileId)
        {
            return i;
        }
    }

    return -1;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SourceDocuments::GetFileId
//
////////////////////////////////////////////////////////////////////////////////

int SourceDocuments::GetFileId (int slot) const
{
    return (slot >= 0 && slot < kMaxDocuments) ? m_slots[(size_t) slot].fileId : -1;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SourceDocuments::GetCount
//
////////////////////////////////////////////////////////////////////////////////

int SourceDocuments::GetCount() const
{
    return (int) std::count_if (m_slots.begin(), m_slots.end(), [] (const Slot & slot) { return slot.fileId >= 0; });
}





////////////////////////////////////////////////////////////////////////////////
//
//  SourceDocuments::SetLine
//
////////////////////////////////////////////////////////////////////////////////

void SourceDocuments::SetLine (int slot, int line)
{
    if (IsOpen (slot))
    {
        m_slots[(size_t) slot].line = line;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  SourceDocuments::GetLine
//
////////////////////////////////////////////////////////////////////////////////

int SourceDocuments::GetLine (int slot) const
{
    return IsOpen (slot) ? m_slots[(size_t) slot].line : 0;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SourceDocuments::Format
//
////////////////////////////////////////////////////////////////////////////////

std::string SourceDocuments::Format (const std::function<std::string (int fileId)> & nameOf) const
{
    std::vector<Saved>  saved;
    std::string         name;



    for (const Slot & slot : m_slots)
    {
        name = (slot.fileId >= 0) ? nameOf (slot.fileId) : std::string();

        if (!name.empty())
        {
            saved.push_back ({ name, slot.line });
        }
    }

    return FormatSaved (saved);
}





////////////////////////////////////////////////////////////////////////////////
//
//  SourceDocuments::FormatSaved
//
////////////////////////////////////////////////////////////////////////////////

std::string SourceDocuments::FormatSaved (const std::vector<Saved> & saved)
{
    std::string  text;



    for (const Saved & each : saved)
    {
        text += std::format (" {}{}:{}", s_kpszToken, Escape (each.name), each.line);
    }

    return text;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SourceDocuments::Parse
//
//  The line follows the last colon, so a name may hold colons of its own.
//
////////////////////////////////////////////////////////////////////////////////

std::vector<SourceDocuments::Saved> SourceDocuments::Parse (const std::string & text)
{
    std::vector<Saved>  saved;
    std::istringstream  in (text);
    std::string         token;
    std::string_view    prefix = s_kpszToken;



    while (in >> token)
    {
        size_t       colon = token.rfind (':');
        int          line  = 0;
        std::string  digits;

        if (!token.starts_with (prefix) || colon == std::string::npos || colon <= prefix.size())
        {
            continue;
        }

        digits = token.substr (colon + 1);

        if (std::from_chars (digits.data(), digits.data() + digits.size(), line).ptr != digits.data() + digits.size())
        {
            continue;
        }

        saved.push_back ({ Unescape (token.substr (prefix.size(), colon - prefix.size())), line });
    }

    return saved;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SourceDocuments::Escape
//
////////////////////////////////////////////////////////////////////////////////

std::string SourceDocuments::Escape (const std::string & name)
{
    std::string  text;



    for (char ch : name)
    {
        if      (ch == '%') { text += "%25"; }
        else if (ch == ' ') { text += "%20"; }
        else                { text += ch;    }
    }

    return text;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SourceDocuments::Unescape
//
////////////////////////////////////////////////////////////////////////////////

std::string SourceDocuments::Unescape (const std::string & text)
{
    std::string  name;



    for (size_t i = 0; i < text.size(); i++)
    {
        if      (text.compare (i, 3, "%20") == 0) { name += ' '; i += 2; }
        else if (text.compare (i, 3, "%25") == 0) { name += '%'; i += 2; }
        else                                       { name += text[i];    }
    }

    return name;
}
