#include "Pch.h"

#include "CassoLink/Session/TrackVerifier.h"
#include "CassoLink/CassoLinkCodec.h"





////////////////////////////////////////////////////////////////////////////////
//
//  TrackVerifier::Verify
//
////////////////////////////////////////////////////////////////////////////////

HRESULT TrackVerifier::Verify (ITrackVerifierIo & io, Result & outResult)
{
    constexpr size_t                     kSectorBytes = 256;
    HRESULT                              hr           = S_OK;
    std::array<SectorState, kSectors>    states;
    std::vector<Byte>                    pending;
    std::vector<Byte>                    data;
    std::vector<Word>                    crcs;
    std::vector<bool>                    read;
    bool                                 anyRead      = false;



    outResult = Result();
    outResult.data.assign ((size_t) kSectors * kSectorBytes, 0);

    for (int sector = 0; sector < kSectors; sector++)
    {
        pending.push_back ((Byte) sector);
    }

    hr = io.ReadData (pending, data, read);
    CHR (hr);

    for (size_t i = 0; i < pending.size(); i++)
    {
        states[i].tries++;

        if (read[i])
        {
            std::span<const Byte>  sector (data.data() + i * kSectorBytes, kSectorBytes);

            Note (states[i], CassoLinkCodec::ComputeCrc (sector), sector);
            anyRead = true;
        }
    }

    while (!pending.empty())
    {
        std::vector<Byte>  next;

        for (Byte sector : pending)
        {
            if (!IsSettled (states[sector]))
            {
                next.push_back (sector);
            }
        }

        pending = std::move (next);

        if (pending.empty())
        {
            break;
        }

        //  A track with nothing on it that reads isn't worth nine tries.
        if (!anyRead && states[0].tries >= kEmptyTrackReads)
        {
            outResult.noSectors = true;
            break;
        }

        //  The base reads run in place, and retries from a moved head.
        if (states[pending[0]].tries >= kBaseReads)
        {
            hr = io.MoveHead();
            CHR (hr);
        }

        hr = io.ReadCrcs (pending, crcs, read);
        CHR (hr);

        for (size_t i = 0; i < pending.size(); i++)
        {
            states[pending[i]].tries++;

            if (read[i])
            {
                Note (states[pending[i]], crcs[i], {});
                anyRead = true;
            }
        }
    }

    for (int sector = 0; sector < kSectors; sector++)
    {
        SectorState  & state = states[(size_t) sector];
        Variant      * top   = GetTop (state);

        outResult.outcomes[(size_t) sector] = Judge (state);

        if (top != nullptr)
        {
            hr = FetchData (io, (Byte) sector, state, *top);
            CHR (hr);

            //  The data kept is another read's when this one's can't be read again.
            if (top->data.empty())
            {
                outResult.outcomes[(size_t) sector] = Outcome::Unreliable;

                auto  other = std::find_if (state.variants.begin(), state.variants.end(), [] (const Variant & variant) { return !variant.data.empty(); });

                top = other == state.variants.end() ? nullptr : &*other;
            }
        }

        if (top != nullptr)
        {
            std::copy (top->data.begin(), top->data.end(), outResult.data.begin() + (ptrdiff_t) ((size_t) sector * kSectorBytes));
        }
        else
        {
            outResult.outcomes[(size_t) sector] = Outcome::Unreadable;
        }
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  TrackVerifier::Note
//
////////////////////////////////////////////////////////////////////////////////

void TrackVerifier::Note (SectorState & state, Word crc, std::span<const Byte> data)
{
    auto  found = std::find_if (state.variants.begin(), state.variants.end(), [crc] (const Variant & variant) { return variant.crc == crc; });



    state.good++;

    if (found == state.variants.end())
    {
        state.variants.push_back (Variant { crc, 0, {} });
        found = state.variants.end() - 1;
    }

    found->count++;

    if (found->data.empty() && !data.empty())
    {
        found->data.assign (data.begin(), data.end());
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  TrackVerifier::IsSettled
//
//  Three matching reads, two matching among failures, a majority of the most
//  reads there can be, or no reads left.
//
////////////////////////////////////////////////////////////////////////////////

bool TrackVerifier::IsSettled (const SectorState & state)
{
    int  top = 0;



    for (const Variant & variant : state.variants)
    {
        top = std::max (top, variant.count);
    }

    if (state.tries >= kMaxReads || top > kMaxReads / 2)
    {
        return true;
    }

    if (state.tries < kBaseReads)
    {
        return false;
    }

    return state.variants.size() == 1 && top >= 2;
}





////////////////////////////////////////////////////////////////////////////////
//
//  TrackVerifier::GetTop
//
////////////////////////////////////////////////////////////////////////////////

TrackVerifier::Variant * TrackVerifier::GetTop (SectorState & state)
{
    auto  top = std::max_element (state.variants.begin(), state.variants.end(), [] (const Variant & a, const Variant & b) { return a.count < b.count; });



    return top == state.variants.end() ? nullptr : &*top;
}





////////////////////////////////////////////////////////////////////////////////
//
//  TrackVerifier::Judge
//
////////////////////////////////////////////////////////////////////////////////

TrackVerifier::Outcome TrackVerifier::Judge (SectorState & state)
{
    Variant  * top = GetTop (state);



    if (top == nullptr)
    {
        return Outcome::Unreadable;
    }

    if (state.variants.size() == 1 && state.good == state.tries)
    {
        return Outcome::Clean;
    }

    if (state.variants.size() == 1)
    {
        return top->count >= 2 ? Outcome::Recovered : Outcome::Unreliable;
    }

    return top->count * 2 > state.good ? Outcome::Recovered : Outcome::Unreliable;
}





////////////////////////////////////////////////////////////////////////////////
//
//  TrackVerifier::FetchData
//
//  The data of a CRC that only later reads saw, a few tries from the track as it
//  now reads.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT TrackVerifier::FetchData (ITrackVerifierIo & io, Byte sector, SectorState & state, Variant & top)
{
    constexpr int      kFetchTries = 4;
    HRESULT            hr          = S_OK;
    const Byte         one[]       = { sector };
    std::vector<Byte>  data;
    std::vector<bool>  read;



    UNREFERENCED_PARAMETER (state);

    for (int attempt = 0; attempt < kFetchTries && top.data.empty(); attempt++)
    {
        hr = io.ReadData (one, data, read);
        CHR (hr);

        if (read[0] && CassoLinkCodec::ComputeCrc (data) == top.crc)
        {
            top.data = data;
        }
    }

Error:
    return hr;
}
