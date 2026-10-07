#pragma once

#include "Debugger/HeatBankMap.h"





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapOptions
//
//  How the heat map pane shows its map, kept as text in the user's settings:
//  which accesses it shows, whether heat fades or accumulates, how long a
//  single access takes to fade away, and which bank it shows.
//
//  The banks: Cpu is the 64 KB the CPU addresses. Main and Aux are where
//  accesses land, as the //e's DRAM holds them, the language card's banks in
//  the top 16 KB (HeatSpace); LanguageCard and AuxLanguageCard show that top
//  16 KB alone. Rom is the ROM reads reach.
//
////////////////////////////////////////////////////////////////////////////////

class HeatMapOptions
{
public:
    enum class View
    {
        All,
        Code,
        Data,
    };

    enum class Bank
    {
        Cpu,
        Main,
        Aux,
        LanguageCard,
        AuxLanguageCard,
        Rom,
    };

    static constexpr size_t  kBankCount = 6;

    //  The fade times the pane offers, in seconds of machine time.
    static constexpr std::array<int, 6>  kFadeChoices        = { 2, 5, 10, 20, 30, 60 };
    static constexpr int                 kDefaultFadeSeconds = 10;
    static constexpr int                 kMinFadeSeconds     = 1;
    static constexpr int                 kMaxFadeSeconds     = 600;

    //  Where a language card view starts: bank 1, as the DRAM holds it.
    static constexpr Word                kLanguageCardFirst  = 0xC000;

    bool  cumulative  = false;
    int   fadeSeconds = kDefaultFadeSeconds;
    View  view        = View::All;
    Bank  bank        = Bank::Cpu;

    bool operator== (const HeatMapOptions & other) const = default;

    std::string            ToText              () const;
    static HeatMapOptions  FromText            (const std::string & text);

    //  The space a bank's counts are in, and whether it shows an address of
    //  that space.
    static HeatSpace       GetSpace            (Bank bank);
    static bool            IsShown             (Bank bank, Word address);

    //  Whether a machine whose bank map is given has the bank.
    static bool            IsAvailable         (Bank bank, const HeatBankMap & bankMap);

    //  The bank's name on the pane's bar, "Main RAM"; and where an address of
    //  the bank is, "Aux language card bank 1 $D123", or "$D123" for the
    //  CPU's. A machine without aux RAM has one language card, and its
    //  locations say so without "Main".
    static std::wstring    GetBankLabel        (Bank bank);
    static std::wstring    DescribeLocation    (Bank bank, Word address, bool hasAux);

    //  The bank as the settings' text writes it, "main", and back.
    static std::string     GetBankWord         (Bank bank);
    static bool            TryGetBank          (const std::string & word, Bank & outBank);
};
