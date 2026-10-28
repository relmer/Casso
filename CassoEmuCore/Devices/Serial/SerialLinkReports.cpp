#include "Pch.h"

#include "Devices/Serial/SerialLinkReports.h"





////////////////////////////////////////////////////////////////////////////////
//
//  SerialLinkReports::Post
//
//  The port's old report is removed and the new one added last. Posting what
//  the port already has, or clearing a port with no report, does not change
//  the generation, so an attachment can post its state on every poll.
//
////////////////////////////////////////////////////////////////////////////////

void SerialLinkReports::Post (
    const std::string  & portTitle,
    const std::string  & text)
{
    std::lock_guard<std::mutex>  held  (m_lock);
    auto                         found = std::find_if (m_reports.begin(), m_reports.end(), [&portTitle] (const Report & report) { return report.portTitle == portTitle; });
    bool                         isNew = found == m_reports.end();



    if ((isNew && text.empty()) || (!isNew && found->text == text))
    {
        return;
    }

    if (!isNew)
    {
        m_reports.erase (found);
    }

    if (!text.empty())
    {
        m_reports.push_back ({ portTitle, text });
    }

    m_generation.fetch_add (1, memory_order_release);
}





////////////////////////////////////////////////////////////////////////////////
//
//  SerialLinkReports::Clear
//
////////////////////////////////////////////////////////////////////////////////

void SerialLinkReports::Clear()
{
    std::lock_guard<std::mutex>  held (m_lock);



    m_reports.clear();
    m_generation.fetch_add (1, memory_order_release);
}





////////////////////////////////////////////////////////////////////////////////
//
//  SerialLinkReports::GetReports
//
////////////////////////////////////////////////////////////////////////////////

std::vector<SerialLinkReports::Report> SerialLinkReports::GetReports() const
{
    std::lock_guard<std::mutex>  held (m_lock);



    return m_reports;
}
