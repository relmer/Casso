#pragma once





////////////////////////////////////////////////////////////////////////////////
//
//  DiskMarkPattern
//
//  The marks that open or close a Disk II field: one to three nibbles, each
//  either an exact value or "any" (written ?? in text). D5 AA 96 is a pattern;
//  so is D5 AA ??.
//
//  Pure, and only the pattern and its match. Whether a custom mark replaces
//  the standard one or is matched beside it is the caller's rule, not the
//  pattern's: the inspector's decode settings and the debugger's breakpoints
//  answer it differently.
//
////////////////////////////////////////////////////////////////////////////////

class DiskMarkPattern
{
public:
    static constexpr int  kMaxNibbles = 3;

    DiskMarkPattern() = default;

    static DiskMarkPattern  MakeExact (Byte first, Byte second);
    static DiskMarkPattern  MakeExact (Byte first, Byte second, Byte third);
    static bool             TryParse  (std::string_view text, DiskMarkPattern & outPattern, std::string & outError);

    int          GetLength () const { return m_length; }
    bool         IsAny     (int index) const { return m_isAny[index]; }
    Byte         GetValue  (int index) const { return m_values[index]; }
    bool         Matches   (std::span<const Byte> nibbles) const;
    std::string  ToText    () const;

    bool  operator== (const DiskMarkPattern & other) const;

private:
    static bool  TryParseToken (std::string_view token, Byte & outValue, bool & outIsAny);

    std::array<Byte, kMaxNibbles>  m_values = {};
    std::array<bool, kMaxNibbles>  m_isAny  = {};
    int                            m_length = 0;
};
