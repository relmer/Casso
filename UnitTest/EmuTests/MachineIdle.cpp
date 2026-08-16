#include "Pch.h"

#include "MachineIdle.h"
#include "TextScreenScraper.h"
#include "Devices/Disk2Controller.h"





////////////////////////////////////////////////////////////////////////////////
//
//  MachineIdle::IsIdle
//
////////////////////////////////////////////////////////////////////////////////

bool MachineIdle::IsIdle (
    EmulatorCore                    &  core,
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

    if (core.diskController != nullptr && core.diskController->IsMotorOn())
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

uint64_t MachineIdle::RunUntilIdle (EmulatorCore & core, uint64_t cycleCap)
{
    uint64_t                  spent = 0;
    int                       quiet = 0;
    std::vector<std::string>  previous;



    // Nothing to pump, and no screen to scrape either.
    if (!core.HasApple2e() || core.bus == nullptr)
    {
        return 0;
    }

    while (spent < cycleCap)
    {
        uint64_t  slice = (std::min) (kSampleSlice, cycleCap - spent);

        core.RunCycles (slice);
        spent += slice;

        // Scrape rather than Scrape40 so an 80-column //c reads correctly.
        std::vector<std::string>  current = TextScreenScraper::Scrape (core);

        quiet = IsIdle (core, previous, current) ? quiet + 1 : 0;

        previous = std::move (current);

        if (quiet >= kQuietSamples)
        {
            break;
        }
    }

    return spent;
}
