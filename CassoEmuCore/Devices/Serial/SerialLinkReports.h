#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  SerialLinkReports
//
//  Problems with the machine's serial attachments, for the infobar: a COM
//  port that is absent, in use or unplugged, a host that is offline, a listen
//  port in use, a baud rate the PC port does not support. One report per
//  port, the newest last. Attachments post on the emulation thread; the UI
//  thread reads the generation each tick and the list when it changes.
//
////////////////////////////////////////////////////////////////////////////////

class SerialLinkReports
{
public:
    struct Report
    {
        std::string  portTitle;
        std::string  text;
    };

    //  An empty text clears the port's report.
    void                 Post          (const std::string & portTitle, const std::string & text);
    void                 Clear         ();
    std::vector<Report>  GetReports    () const;
    uint64_t             GetGeneration () const { return m_generation.load (memory_order_acquire); }

private:
    mutable std::mutex     m_lock;
    std::vector<Report>    m_reports;
    std::atomic<uint64_t>  m_generation = 0;
};
