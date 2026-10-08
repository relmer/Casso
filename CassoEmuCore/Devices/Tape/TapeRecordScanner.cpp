#include "Pch.h"

#include "Devices/Tape/TapeRecordScanner.h"





////////////////////////////////////////////////////////////////////////////////
//
//  TapeRecordScanner::Scan
//
//  Walks the half-cycles: a long enough run of leader-length ones is a
//  leader; the two half-cycles after it are the sync; then each pair is one
//  bit, until a cycle too long to be a bit ends the record. The scan picks
//  up again after it, so a tape's every record is found in one pass.
//
////////////////////////////////////////////////////////////////////////////////

void TapeRecordScanner::Scan (const std::vector<double> & transitions, uint32_t sampleRate, std::vector<TapeRecord> & records)
{
    double  usPerSample = 1.0e6 / (double) (sampleRate == 0 ? 1 : sampleRate);
    size_t  count       = transitions.size();
    size_t  i           = 0;



    records.clear();

    auto  halfUs = [&] (size_t k) { return (transitions[k + 1] - transitions[k]) * usPerSample; };

    while (count >= 2 && i + 1 < count)
    {
        size_t      runStart = i;
        size_t      run      = 0;
        TapeRecord  record;
        size_t      j        = 0;
        size_t      bits     = 0;
        Byte        current  = 0;
        Byte        checksum = kChecksumSeed;
        Byte        last     = 0;



        // A short burst of odd half-cycles between leader ones -- a click,
        // a dropout -- does not end the leader: damage there costs the guest
        // nothing, since the ROM only waits through it. The sync ends it,
        // being followed by data, not leader.
        while (i + 1 < count)
        {
            size_t  skip = 0;



            if (IsLeaderHalf (halfUs (i)))
            {
                run++;
                i++;
                continue;
            }

            while (run > 0 && skip < kLeaderMaxBurst && i + skip + 2 < count && !IsLeaderHalf (halfUs (i + skip + 1)))
            {
                skip++;
            }

            if (run == 0 || skip >= kLeaderMaxBurst || i + skip + 2 >= count)
            {
                break;
            }

            i += skip + 1;
        }

        if (run < kLeaderMinRun)
        {
            i++;
            continue;
        }

        record.leaderStart = transitions[runStart];
        record.dataStart   = transitions[i];

        // Past the sync's two half-cycles, one bit per full cycle.
        for (j = i + 2; j + 2 < count; j += 2)
        {
            double  cycleUs = halfUs (j) + halfUs (j + 1);

            if (cycleUs > kGapCycleUs)
            {
                break;
            }

            current = (Byte) ((current << 1) | (cycleUs > kOneMinCycleUs ? 1 : 0));
            bits++;

            if (bits % 8 == 0)
            {
                checksum ^= last;
                last      = current;
                record.byteCount++;
                current   = 0;
            }
        }

        // The checksum covers every byte but the last, which is the checksum.
        // The seed was folded in before the first byte, so the first fold
        // above XORed the zero that `last` started as -- a no-op.
        record.dataEnd = transitions[j < count ? j : count - 1];
        record.isValid = record.byteCount >= 2 && checksum == last;

        records.push_back (record);
        i = j;
    }
}
