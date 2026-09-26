#pragma once





////////////////////////////////////////////////////////////////////////////////
//
//  ConsoleHistory
//
//  The lines run from the console's command box, most recent last. Running a
//  line already in the history moves it to the most recent place rather than
//  keeping two copies. Up and Down walk it from the most recent line back; the
//  text being typed when the walk began comes back after the most recent line.
//
////////////////////////////////////////////////////////////////////////////////

class ConsoleHistory
{
public:
    static constexpr size_t  kLimit = 200;

    void                            Add       (const std::wstring & line);
    std::optional<std::wstring>     GetOlder  (const std::wstring & typed);
    std::optional<std::wstring>     GetNewer  ();
    void                            EndBrowse ()       { m_index.reset(); }

    const std::vector<std::wstring> & GetLines () const { return m_lines; }

private:
    std::vector<std::wstring>  m_lines;
    std::optional<size_t>      m_index;
    std::wstring               m_draft;
};
