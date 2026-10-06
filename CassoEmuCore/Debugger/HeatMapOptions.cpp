#include "Pch.h"

#include "Debugger/HeatMapOptions.h"





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapOptions::ToText
//
//  Never empty, so a setting written once reads as written, not as unset.
//
////////////////////////////////////////////////////////////////////////////////

std::string HeatMapOptions::ToText() const
{
    std::string  text = std::format ("fade={}", fadeSeconds);



    switch (view)
    {
    case View::Code: text += " view=code"; break;
    case View::Data: text += " view=data"; break;
    default:         text += " view=all";  break;
    }

    if (cumulative)
    {
        text += " cumulative";
    }

    return text;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapOptions::FromText
//
//  Words it does not know are passed over, and a fade out of range is held
//  to the range, so a hand-edited setting still gives a usable map.
//
////////////////////////////////////////////////////////////////////////////////

HeatMapOptions HeatMapOptions::FromText (const std::string & text)
{
    static constexpr std::string_view  kFadeKey       = "fade=";
    static constexpr size_t            kMaxFadeDigits = 4;
    HeatMapOptions                     options;
    std::istringstream                 words (text);
    std::string                        word;
    std::string                        digits;
    int                                seconds        = 0;



    while (words >> word)
    {
        if (word == "cumulative")       { options.cumulative = true;       }
        else if (word == "view=code")   { options.view       = View::Code; }
        else if (word == "view=data")   { options.view       = View::Data; }
        else if (word == "view=all")    { options.view       = View::All;  }

        if (!word.starts_with (kFadeKey))
        {
            continue;
        }

        digits = word.substr (kFadeKey.size());

        //  Enough digits to pass the longest fade, and no more, so the number
        //  cannot overflow.
        if (digits.empty() || digits.size() > kMaxFadeDigits || digits.find_first_not_of ("0123456789") != std::string::npos)
        {
            continue;
        }

        seconds             = std::stoi (digits);
        options.fadeSeconds = std::clamp (seconds, kMinFadeSeconds, kMaxFadeSeconds);
    }

    return options;
}





