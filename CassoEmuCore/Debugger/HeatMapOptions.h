#pragma once

#include "Debugger/HeatBankMap.h"





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapOptions
//
//  How the heat map pane shows its map, kept as text in the user's settings:
//  which accesses it shows, whether heat fades or accumulates, how long a
//  single access takes to fade away, which bank it shows, whether its writes
//  leave out those that stored the value already there, and the set of
//  ranges whose reads before written it leaves out.
//
//  The views: All, Code and Data, and Changed, the writes that changed the
//  byte they stored to, alone.
//
//  The banks: Cpu is the 64 KB the CPU addresses. Main and Aux are where
//  accesses land, as the //e's DRAM holds them, the language card's banks in
//  the top 16 KB (HeatSpace); LanguageCard and AuxLanguageCard show that top
//  16 KB alone. Rom is the ROM reads reach.
//
//  Blend mixes the colors of an address touched more than one way, and gives
//  one both run as code and written its own color, rather than showing the
//  kind that touched it most.
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
        Changed,
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
    bool  blend       = false;

    //  Writes that stored the value already there are left out of the
    //  writes shown; the set of ranges whose reads before written are left
    //  out, none while empty.
    bool         ignoreSameWrites = false;
    std::string  ignoreSet;

    bool operator== (const HeatMapOptions & other) const = default;

    std::string            ToText              () const;
    static HeatMapOptions  FromText            (const std::string & text);

    //  The space a bank's counts are in, and whether it shows an address of
    //  that space.
    static HeatSpace       GetSpace            (Bank bank);
    static bool            IsShown             (Bank bank, Word address);

    //  The address the CPU reaches an address of the bank at, as the
    //  language card maps it in: bank 1 of a RAM space, held at $C000, is
    //  reached at $D000. A breakpoint or a symbol is on that address.
    static Word            GetCpuAddress       (Bank bank, Word address);

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

private:
    static constexpr std::string_view  kIgnoreKey = "ignore=";
};
