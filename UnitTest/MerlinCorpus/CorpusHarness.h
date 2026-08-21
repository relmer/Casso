#pragma once

#include "AssemblerTypes.h"





////////////////////////////////////////////////////////////////////////////////
//
//  CorpusEntryClass
//
//  Where an entry's expectation came from. Kept distinct so it is never unclear
//  whether bytes were captured from real Merlin or authored by hand.
//
////////////////////////////////////////////////////////////////////////////////

enum class CorpusEntryClass
{
    Captured,               // bytes came from real Merlin; carries a version
    NegativeHandAuthored,   // Merlin produces no bytes for it; expectations are diagnostics
};





////////////////////////////////////////////////////////////////////////////////
//
//  CorpusSource
//
//  One named source text. Multi-source entries exercise file inclusion, served
//  through the injected reader rather than from disk.
//
////////////////////////////////////////////////////////////////////////////////

struct CorpusSource
{
    const char *  name;
    const char *  text;
};





////////////////////////////////////////////////////////////////////////////////
//
//  CorpusEntry
//
//  One unit of correctness evidence: source authored here, paired with the
//  bytes real Merlin produced from it.
//
//  `discriminates` is what stops the corpus being vacuous. Labels, origin,
//  literals and the expression evaluator are SHARED between dialects, so an
//  entry built only from those assembles identically whether the Merlin profile
//  works or is never consulted. An entry claiming a Merlin construct must also
//  FAIL under AS65; one that passes under both is either not exercising what it
//  claims or is not reaching the profile at all.
//
////////////////////////////////////////////////////////////////////////////////

struct CorpusEntry
{
    const char *                 name           = nullptr;
    std::vector<CorpusSource>    sources;
    const char *                 entryPoint     = nullptr;   // which source is assembled
    std::vector<Byte>            expectedBytes;
    const char *                 merlinVersion  = nullptr;
    CorpusEntryClass             entryClass     = CorpusEntryClass::Captured;
    bool                         discriminates  = false;
};





////////////////////////////////////////////////////////////////////////////////
//
//  CorpusVerdict
//
////////////////////////////////////////////////////////////////////////////////

enum class CorpusVerdict
{
    Match,
    ByteMismatch,        // same length, differing content
    LengthMismatch,      // different lengths
    EmptyExpectation,    // nothing to compare against -- an error, never a pass
};





////////////////////////////////////////////////////////////////////////////////
//
//  CorpusComparison
//
////////////////////////////////////////////////////////////////////////////////

struct CorpusComparison
{
    CorpusVerdict  verdict            = CorpusVerdict::EmptyExpectation;
    size_t         expectedLength     = 0;
    size_t         actualLength       = 0;
    size_t         firstDifference    = 0;      // index; meaningful unless Match
    bool           hasFirstDifference = false;
};





////////////////////////////////////////////////////////////////////////////////
//
//  CorpusHarness
//
//  The comparison the whole corpus rests on.
//
//  Guarded on its own, because it sits DOWNSTREAM of every guard protecting the
//  corpus inputs. A harness that compares lengths only, stops early, or treats
//  a size mismatch as a skip makes every entry pass regardless of how carefully
//  the entries themselves were captured -- success reported, nothing checked,
//  and aimed at the one component nobody suspects because it IS the check.
//
//  So it reports the FIRST DIFFERING INDEX rather than a bare bool. A
//  length-only comparison passes a same-length mismatch, which is the likeliest
//  real bug, and an index is what makes that failure visible in the assertion
//  message rather than merely counted.
//
//  An empty expectation is an ERROR rather than a trivial match. Nothing to
//  compare against is indistinguishable from a passing comparison unless it is
//  called out.
//
////////////////////////////////////////////////////////////////////////////////

class CorpusHarness
{
public:
    static CorpusComparison Compare (const std::vector<Byte> & expected, const std::vector<Byte> & actual)
    {
        CorpusComparison  result   = {};
        size_t            shared   = 0;
        bool              isEmpty  = expected.empty();



        result.expectedLength = expected.size();
        result.actualLength   = actual.size();

        if (isEmpty)
        {
            result.verdict = CorpusVerdict::EmptyExpectation;
            return result;
        }

        // Scan the overlap FIRST, so a length mismatch still reports where the
        // content diverged rather than only that the sizes differ.
        shared = (expected.size() < actual.size()) ? expected.size() : actual.size();

        for (size_t i = 0; i < shared; i++)
        {
            if (expected[i] != actual[i])
            {
                result.firstDifference    = i;
                result.hasFirstDifference = true;
                break;
            }
        }

        if (expected.size() != actual.size())
        {
            result.verdict = CorpusVerdict::LengthMismatch;

            // Content agreed as far as it went; the divergence IS the length.
            if (!result.hasFirstDifference)
            {
                result.firstDifference    = shared;
                result.hasFirstDifference = true;
            }

            return result;
        }

        result.verdict = result.hasFirstDifference ? CorpusVerdict::ByteMismatch : CorpusVerdict::Match;

        return result;
    }



    // A human-readable account of a comparison, so a failing entry says where it
    // went wrong rather than only that it did.
    static std::string Describe (const std::string & entryName, const CorpusComparison & comparison)
    {
        std::string  result;



        switch (comparison.verdict)
        {
            case CorpusVerdict::Match:
                result = entryName + ": matched";
                break;

            case CorpusVerdict::EmptyExpectation:
                result = entryName + ": EMPTY EXPECTATION -- nothing was compared, which is an error rather than a pass";
                break;

            case CorpusVerdict::LengthMismatch:
                result = entryName + ": length mismatch -- expected " + std::to_string (comparison.expectedLength)
                       + " bytes, got " + std::to_string (comparison.actualLength)
                       + "; first difference at offset " + std::to_string (comparison.firstDifference);
                break;

            case CorpusVerdict::ByteMismatch:
                result = entryName + ": byte mismatch at offset " + std::to_string (comparison.firstDifference)
                       + " (both " + std::to_string (comparison.expectedLength) + " bytes)";
                break;
        }

        return result;
    }
};
