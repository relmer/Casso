#include "Pch.h"

#include "Debugger/HeatMapOptions.h"





static constexpr std::pair<HeatMapOptions::Bank, const char *>  s_kBankWords[] =
{
    { HeatMapOptions::Bank::Cpu,             "cpu"   },
    { HeatMapOptions::Bank::Main,            "main"  },
    { HeatMapOptions::Bank::Aux,             "aux"   },
    { HeatMapOptions::Bank::LanguageCard,    "lc"    },
    { HeatMapOptions::Bank::AuxLanguageCard, "auxlc" },
    { HeatMapOptions::Bank::Rom,             "rom"   },
};





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapOptions::ToText
//
//  Never empty, so a setting written once reads as written, not as unset.
//  The bank is written only when it is not the CPU's. The set of ranges left
//  out comes last, since its name may hold spaces and runs to the end.
//
////////////////////////////////////////////////////////////////////////////////

std::string HeatMapOptions::ToText() const
{
    std::string  text = std::format ("fade={}", fadeSeconds);



    switch (view)
    {
    case View::Code:    text += " view=code";    break;
    case View::Data:    text += " view=data";    break;
    case View::Changed: text += " view=changed"; break;
    default:            text += " view=all";     break;
    }

    if (cumulative)
    {
        text += " cumulative";
    }

    for (const auto & [each, word] : s_kBankWords)
    {
        if (each == bank && bank != Bank::Cpu)
        {
            text += " bank=";
            text += word;
        }
    }

    if (ignoreSameWrites)
    {
        text += " changesonly";
    }

    if (!ignoreSet.empty())
    {
        text += " ";
        text += kIgnoreKey;
        text += ignoreSet;
    }

    return text;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapOptions::FromText
//
//  Words it does not know are passed over, and a fade out of range is held
//  to the range, so a hand-edited setting still gives a usable map. The set
//  of ranges left out is the rest of the text.
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
    std::string                        rest;
    int                                seconds        = 0;



    while (words >> word)
    {
        if (word == "cumulative")       { options.cumulative = true;       }
        else if (word == "view=code")   { options.view       = View::Code; }
        else if (word == "view=data")   { options.view       = View::Data; }
        else if (word == "view=all")    { options.view       = View::All;  }

        if (word == "view=changed")
        {
            options.view = View::Changed;
        }

        if (word == "changesonly")
        {
            options.ignoreSameWrites = true;
        }

        for (const auto & [each, name] : s_kBankWords)
        {
            if (word == std::string ("bank=") + name)
            {
                options.bank = each;
            }
        }

        if (word.starts_with (kIgnoreKey))
        {
            rest              = text.substr (text.find (kIgnoreKey) + kIgnoreKey.size());
            options.ignoreSet = rest.substr (0, rest.find_last_not_of (" \t\r\n") + 1);
            break;
        }

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





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapOptions::GetSpace
//
////////////////////////////////////////////////////////////////////////////////

HeatSpace HeatMapOptions::GetSpace (Bank bank)
{
    switch (bank)
    {
    case Bank::Main:
    case Bank::LanguageCard:    return HeatSpace::Main;
    case Bank::Aux:
    case Bank::AuxLanguageCard: return HeatSpace::Aux;
    case Bank::Rom:             return HeatSpace::Rom;
    default:                    return HeatSpace::Cpu;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapOptions::IsShown
//
//  A language card view shows the top 16 KB of its RAM's space alone.
//
////////////////////////////////////////////////////////////////////////////////

bool HeatMapOptions::IsShown (Bank bank, Word address)
{
    bool  isCardView = bank == Bank::LanguageCard || bank == Bank::AuxLanguageCard;



    return !isCardView || address >= kLanguageCardFirst;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapOptions::GetCpuAddress
//
//  In a RAM bank's space, bank 1 of the language card is kept at $C000 and
//  reached at $D000; every other cell is at the address the CPU uses.
//
////////////////////////////////////////////////////////////////////////////////

Word HeatMapOptions::GetCpuAddress (Bank bank, Word address)
{
    constexpr Word  kBank1Last = 0xCFFF;
    constexpr Word  kBank1Step = 0x1000;
    HeatSpace       space      = GetSpace (bank);
    bool            isRam      = space == HeatSpace::Main || space == HeatSpace::Aux;



    if (isRam && address >= kLanguageCardFirst && address <= kBank1Last)
    {
        return (Word) (address + kBank1Step);
    }

    return address;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapOptions::IsAvailable
//
////////////////////////////////////////////////////////////////////////////////

bool HeatMapOptions::IsAvailable (
    Bank                  bank,
    const HeatBankMap   & bankMap)
{
    switch (bank)
    {
    case Bank::LanguageCard:    return bankMap.HasLanguageCard();
    case Bank::AuxLanguageCard: return bankMap.HasLanguageCard() && bankMap.HasSpace (HeatSpace::Aux);
    default:                    return bankMap.HasSpace (GetSpace (bank));
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapOptions::GetBankLabel
//
////////////////////////////////////////////////////////////////////////////////

std::wstring HeatMapOptions::GetBankLabel (Bank bank)
{
    switch (bank)
    {
    case Bank::Main:            return L"Main RAM";
    case Bank::Aux:             return L"Aux RAM";
    case Bank::LanguageCard:    return L"Language card";
    case Bank::AuxLanguageCard: return L"Aux language card";
    case Bank::Rom:             return L"ROM";
    default:                    return L"CPU";
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapOptions::DescribeLocation
//
//  In a RAM bank's space, the top 16 KB is the language card's: bank 1 of
//  $D000 at $C000, bank 2 at $D000 and the high RAM at $E000, each given at
//  the address the CPU reaches it at.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring HeatMapOptions::DescribeLocation (
    Bank   bank,
    Word   address,
    bool   hasAux)
{
    constexpr Word  kBank1Last = 0xCFFF;
    constexpr Word  kBank2Last = 0xDFFF;
    constexpr Word  kBank1Step = 0x1000;
    HeatSpace       space      = GetSpace (bank);
    bool            isAux      = space == HeatSpace::Aux;
    std::wstring    side       = isAux ? L"Aux " : (hasAux ? L"Main " : L"");
    std::wstring    card       = side.empty() ? std::wstring (L"Language card") : side + L"language card";
    std::wstring    high       = side.empty() ? std::wstring (L"High RAM")      : side + L"high RAM";



    if (space == HeatSpace::Cpu)
    {
        return std::format (L"${:04X}", address);
    }

    if (space == HeatSpace::Rom)
    {
        return std::format (L"ROM ${:04X}", address);
    }

    if (address < kLanguageCardFirst)
    {
        return std::format (L"{} RAM ${:04X}", isAux ? L"Aux" : L"Main", address);
    }

    if (address <= kBank1Last)
    {
        return std::format (L"{} bank 1 ${:04X}", card, (Word) (address + kBank1Step));
    }

    if (address <= kBank2Last)
    {
        return std::format (L"{} bank 2 ${:04X}", card, address);
    }

    return std::format (L"{} ${:04X}", high, address);
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapOptions::GetBankWord
//
////////////////////////////////////////////////////////////////////////////////

std::string HeatMapOptions::GetBankWord (Bank bank)
{
    std::string  word = "cpu";



    for (const auto & [each, name] : s_kBankWords)
    {
        if (each == bank)
        {
            word = name;
        }
    }

    return word;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapOptions::TryGetBank
//
////////////////////////////////////////////////////////////////////////////////

bool HeatMapOptions::TryGetBank (
    const std::string  & word,
    Bank               & outBank)
{
    for (const auto & [each, name] : s_kBankWords)
    {
        if (word == name)
        {
            outBank = each;
            return true;
        }
    }

    return false;
}





