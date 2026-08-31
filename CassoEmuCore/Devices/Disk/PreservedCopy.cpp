#include "Pch.h"

#include "PreservedCopy.h"





////////////////////////////////////////////////////////////////////////////////
//
//  PreservedCopy::MakePath
//
//  Where a preserved version goes.
//
//  BESIDE THE ORIGINAL, so the pairing is obvious in a folder listing and the
//  copy is somewhere the user is already looking. The alternative -- a
//  application-owned backup folder -- hides the one thing they need to find at
//  the moment they need it most.
//
//  ATTEMPT ZERO IS `-01` RATHER THAN NOTHING. A bare name sorts after the
//  numbered ones that follow it in the same second -- the comparison reaches
//  `.dsk` where they have `-02` -- so leaving the first copy unnumbered would
//  put it last in the listing it is supposed to lead.
//
////////////////////////////////////////////////////////////////////////////////

std::string PreservedCopy::MakePath (const std::string & imagePath,
                                     const std::string & stamp,
                                     int                 attempt)
{
    fs::path     original (imagePath);
    fs::path     folder    = original.parent_path();
    std::string  stem      = original.stem().string();
    std::string  extension = original.extension().string();
    std::string  name;



    if (stem.empty())
    {
        stem = "disk";
    }

    if (extension.empty())
    {
        extension = ".dsk";
    }

    //  Zero-padded, because `-10` sorting before `-2` would break the same
    //  promise the counter exists to keep.
    {
        std::string  counter = std::to_string (attempt + 1);

        if (counter.size() < 2)
        {
            counter = "0" + counter;
        }

        name = stem + "." + stamp + "-" + counter + extension;
    }

    return (folder.empty() ? fs::path (name) : (folder / name)).string();
}





////////////////////////////////////////////////////////////////////////////////
//
//  PreservedCopy::MakeStamp
//
//  `20260830-014233` for a point in time.
//
//  SORTABLE AS TEXT, which is what makes a directory listing readable in the
//  order things happened without anything sorting it by date.
//
////////////////////////////////////////////////////////////////////////////////

std::string PreservedCopy::MakeStamp (time_t when)
{
    tm      parts    = {};
    char    text[32] = {};
    size_t  written  = 0;



    if (localtime_s (&parts, &when) != 0)
    {
        return "unknown-time";
    }

    written = strftime (text, sizeof (text), "%Y%m%d-%H%M%S", &parts);

    //  A buffer this size cannot be too small for that format, so a zero here
    //  would mean something is wrong with the time itself rather than with the
    //  formatting -- and a name is still needed either way.
    if (written == 0)
    {
        return "unknown-time";
    }

    return std::string (text);
}
