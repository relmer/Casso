#include "Pch.h"
#include "Core/GamePatcher.h"

#include "Core/MemoryBus.h"




////////////////////////////////////////////////////////////////////////////////
//
//  GamePatcher::AddRule
//
////////////////////////////////////////////////////////////////////////////////

void GamePatcher::AddRule (const Rule & rule)
{
    m_rules.push_back (rule);
}




////////////////////////////////////////////////////////////////////////////////
//
//  GamePatcher::Clear
//
////////////////////////////////////////////////////////////////////////////////

void GamePatcher::Clear ()
{
    m_rules.clear ();
    m_totalApplied = 0;
}




////////////////////////////////////////////////////////////////////////////////
//
//  GamePatcher::AddApple2cVblSpin
//
//  The //e-style VBL sync `LDA $C019 / BMI *-3` == bytes AD 19 C0 30 FB.
//  Replace the two-byte BMI (offset 3) with two NOPs so the read falls through
//  to the routine's RTS.
//
////////////////////////////////////////////////////////////////////////////////

void GamePatcher::AddApple2cVblSpin ()
{
    Rule   rule;

    rule.signature   = { 0xAD, 0x19, 0xC0, 0x30, 0xFB };
    rule.patchOffset = 3;
    rule.replacement = { 0xEA, 0xEA };
    rule.label       = "apple2c-vbl-spin";

    AddRule (rule);
}




////////////////////////////////////////////////////////////////////////////////
//
//  GamePatcher::Matches
//
////////////////////////////////////////////////////////////////////////////////

bool GamePatcher::Matches (MemoryBus & bus, Word at, const Rule & rule) const
{
    size_t   i;

    for (i = 0; i < rule.signature.size (); i++)
    {
        if (bus.ReadByte (static_cast<Word> (at + i)) != rule.signature[i])
        {
            return false;
        }
    }

    return true;
}




////////////////////////////////////////////////////////////////////////////////
//
//  GamePatcher::Scan
//
////////////////////////////////////////////////////////////////////////////////

int GamePatcher::Scan (MemoryBus & bus, Word lo, Word hi)
{
    int   applied = 0;

    for (const Rule & rule : m_rules)
    {
        size_t     sigLen = rule.signature.size ();
        uint32_t   end;
        uint32_t   at;

        if (sigLen == 0 || hi < lo || static_cast<uint32_t> (hi - lo + 1) < sigLen)
        {
            continue;
        }

        // uint32_t iteration so `at++` cannot wrap a 16-bit address.
        end = static_cast<uint32_t> (hi) - static_cast<uint32_t> (sigLen - 1);

        for (at = lo; at <= end; at++)
        {
            size_t   k;

            if (!Matches (bus, static_cast<Word> (at), rule))
            {
                continue;
            }

            for (k = 0; k < rule.replacement.size (); k++)
            {
                bus.WriteByte (
                    static_cast<Word> (at + rule.patchOffset + k),
                    rule.replacement[k]);
            }

            applied++;
            m_totalApplied++;
        }
    }

    return applied;
}




////////////////////////////////////////////////////////////////////////////////
//
//  GamePatcher::ScanRam
//
////////////////////////////////////////////////////////////////////////////////

int GamePatcher::ScanRam (MemoryBus & bus)
{
    return Scan (bus, s_kRamScanLo, s_kRamScanHi);
}
