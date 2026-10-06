#pragma once





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapOptions
//
//  How the heat map pane shows its map, kept as text in the user's settings:
//  which accesses it shows, whether heat fades or accumulates, and how long a
//  single access takes to fade away.
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

    //  The fade times the pane steps through, in seconds of machine time.
    static constexpr std::array<int, 6>  kFadeChoices        = { 2, 5, 10, 20, 30, 60 };
    static constexpr int                 kDefaultFadeSeconds = 10;
    static constexpr int                 kMinFadeSeconds     = 1;
    static constexpr int                 kMaxFadeSeconds     = 600;

    bool  cumulative  = false;
    int   fadeSeconds = kDefaultFadeSeconds;
    View  view        = View::All;

    bool operator== (const HeatMapOptions & other) const = default;

    std::string            ToText              () const;
    static HeatMapOptions  FromText            (const std::string & text);
    static int             GetNextFadeSeconds  (int seconds);
};
