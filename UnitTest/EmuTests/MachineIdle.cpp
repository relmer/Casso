#include "Pch.h"

#include "Machines/Apple2/Apple2e/Apple2eMmu.h"

#include "MachineIdle.h"
#include "TextScreenScraper.h"
#include "Machines/Apple2/Common/Disk2Controller.h"





////////////////////////////////////////////////////////////////////////////////
//
//  MachineIdle::IsIdle
//
////////////////////////////////////////////////////////////////////////////////

bool MachineIdle::IsIdle (
    MachineHost                    &  host,
    const std::vector<std::string>  &  previous,
    const std::vector<std::string>  &  current)
{
    int     changed   = 0;
    bool    atPrompt  = false;
    size_t  rowCount  = 0;



    // No earlier sample means nothing to compare against yet.
    if (previous.empty() || current.empty())
    {
        return false;
    }

    if (host.GetRefs().diskController != nullptr && host.GetRefs().diskController->IsMotorOn())
    {
        return false;
    }

    // One cell of drift is the flashing cursor. Anything more is the
    // machine still printing.
    rowCount = (std::min) (previous.size(), current.size());

    for (size_t r = 0; r < rowCount; r++)
    {
        size_t  colCount = (std::min) (previous[r].size(), current[r].size());

        for (size_t c = 0; c < colCount; c++)
        {
            if (previous[r][c] != current[r][c])
            {
                changed++;

                if (changed > 1)
                {
                    return false;
                }
            }
        }
    }

    // A bare prompt is the bottom-most non-blank row holding one or two
    // glyphs: the ] plus, on the frames it is lit, the cursor. DOS
    // sometimes leaves only the cursor there, so the ] itself is not
    // required -- the shortness is what separates a prompt from a line of
    // output or a half-typed command.
    for (size_t i = current.size(); i > 0 && !atPrompt; i--)
    {
        std::string  row = current[i - 1];

        while (!row.empty() && row.back() == ' ')
        {
            row.pop_back();
        }

        if (!row.empty())
        {
            atPrompt = row.size() <= 2;
            break;
        }
    }

    return atPrompt;
}





////////////////////////////////////////////////////////////////////////////////
//
//  MachineIdle::RunUntilIdle
//
////////////////////////////////////////////////////////////////////////////////

uint64_t MachineIdle::RunUntilIdle (MachineHost & host, uint64_t cycleCap)
{
    uint64_t                  spent = 0;
    int                       quiet = 0;
    std::vector<std::string>  previous;



    // Nothing to pump, and no screen to scrape either. A host always has a
    // bus; what it may not have is a machine on it.
    if (host.GetCpu() == nullptr || host.GetMmu() == nullptr)
    {
        return 0;
    }

    while (spent < cycleCap)
    {
        uint64_t  slice = (std::min) (kSampleSlice, cycleCap - spent);

        host.RunCycles (slice);
        spent += slice;

        // Scrape rather than Scrape40 so an 80-column //c reads correctly.
        std::vector<std::string>  current = TextScreenScraper::Scrape (host);

        quiet = IsIdle (host, previous, current) ? quiet + 1 : 0;

        previous = std::move (current);

        if (quiet >= kQuietSamples)
        {
            break;
        }
    }

    return spent;
}
