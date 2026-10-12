#include "Pch.h"

#include "Devices/Disk/SectorFieldWriter.h"
#include "Devices/Disk/FluxTrack.h"





////////////////////////////////////////////////////////////////////////////////
//
//  SectorFieldWriter::Write
//
//  Groups the writes by record, plans each record on a copy (find the field,
//  encode, lay the new nibbles into the old ones' cells, decode the result
//  again), and changes the image only when every record's plan passed.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT SectorFieldWriter::Write (
    DiskImage                    &  image,
    std::span<const SectorWrite>    writes,
    SectorWritePolicy               policy,
    vector<SectorWriteError>     &  outErrors)
{
    HRESULT                     hr       = S_OK;
    vector<SlotPlan>            plans;
    vector<SlotPlan>::iterator  it;
    int                         slot     = 0;
    bool                        isClean  = false;



    outErrors.clear();

    for (const SectorWrite & write : writes)
    {
        slot = ResolveSlot (image, write);

        if (slot < 0)
        {
            AddError (write, slot, SectorWriteFailure::NoRecord, outErrors);
            continue;
        }

        it = std::find_if (plans.begin(), plans.end(), [slot] (const SlotPlan & p) { return p.slot == slot; });

        if (it == plans.end())
        {
            plans.push_back ({ slot, {}, nullptr, nullptr });
            it = plans.end() - 1;
        }

        it->writes.push_back (&write);
    }

    for (SlotPlan & plan : plans)
    {
        PlanSlot (image, policy, plan, outErrors);
    }

    isClean = outErrors.empty();
    CBREx (isClean, HRESULT_FROM_WIN32 (ERROR_ACCESS_DENIED));

    for (const SlotPlan & plan : plans)
    {
        if (plan.after->kind == TrackKind::Flux)
        {
            image.SetFluxTrack (plan.slot, plan.after->fluxBytes);
        }
        else
        {
            image.GetTrackBitsForWrite (plan.slot) = plan.after->bits;
        }

        image.MarkTrackChangedByWriter (plan.slot);
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SectorFieldWriter::ResolveSlot
//
//  The record a write changes: the one it gives, or the one the map gives its
//  whole track.
//
////////////////////////////////////////////////////////////////////////////////

int SectorFieldWriter::ResolveSlot (const DiskImage & image, const SectorWrite & write)
{
    return (write.slot >= 0) ? write.slot : image.ResolveWholeTrack (write.track);
}





////////////////////////////////////////////////////////////////////////////////
//
//  SectorFieldWriter::IsSlotDamaged
//
////////////////////////////////////////////////////////////////////////////////

bool SectorFieldWriter::IsSlotDamaged (const DiskImage & image, int slot)
{
    const vector<DamagedTrack> &  damaged = image.GetDamagedTracks();



    return std::any_of (damaged.begin(), damaged.end(), [slot] (const DamagedTrack & d) { return d.trkIndex == slot; });
}





////////////////////////////////////////////////////////////////////////////////
//
//  SectorFieldWriter::IsTrackComplete
//
//  The Strict policy's test: sixteen 16-sector address fields numbered 0 to
//  15, each once, each with a good checksum and a data field whose checksum is
//  good, and no other address field.
//
////////////////////////////////////////////////////////////////////////////////

bool SectorFieldWriter::IsTrackComplete (const vector<LocatedField> & fields)
{
    static constexpr int  kSectors = 16;



    std::array<int, kSectors>  seen       = {};
    bool                       isComplete = true;
    int                        addresses  = 0;



    for (const LocatedField & f : fields)
    {
        if (f.role != FieldRole::Address)
        {
            continue;
        }

        addresses++;

        isComplete = isComplete
                  && f.kind == DiskFieldKind::Sixteen
                  && f.isAddressChecksumGood
                  && f.sector < kSectors
                  && f.pairedField >= 0
                  && fields[f.pairedField].data.isChecksumGood;

        if (isComplete)
        {
            seen[f.sector]++;
        }
    }

    for (int count : seen)
    {
        isComplete = isComplete && count == 1;
    }

    return isComplete && addresses == kSectors;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SectorFieldWriter::FindDataField
//
//  The data field a write changes: the one starting at the cell it gives, or
//  the one paired with the address field holding its sector number, which the
//  Strict policy has already made sure is unique. Returns -1 and the reason
//  when the field cannot be written.
//
////////////////////////////////////////////////////////////////////////////////

int SectorFieldWriter::FindDataField (const vector<LocatedField> & fields, const SectorWrite & write, SectorWriteFailure & outFailure)
{
    int                   found   = -1;
    int                   address = -1;
    size_t                i       = 0;



    outFailure = SectorWriteFailure::SectorNotFound;

    for (i = 0; i < fields.size() && found < 0; i++)
    {
        bool  isByCell   = write.dataFieldCell >= 0
                        && fields[i].role == FieldRole::Data
                        && fields[i].startCell == static_cast<uint32_t> (write.dataFieldCell);
        bool  isByNumber = write.dataFieldCell < 0
                        && fields[i].role == FieldRole::Address
                        && fields[i].sector == write.sector;

        if (isByCell)
        {
            found   = static_cast<int> (i);
            address = fields[i].pairedField;
        }
        else if (isByNumber)
        {
            address = static_cast<int> (i);
            found   = fields[i].pairedField;

            if (found < 0)
            {
                outFailure = SectorWriteFailure::NoDataField;
                break;
            }
        }
    }

    if (found >= 0 && address < 0)
    {
        outFailure = SectorWriteFailure::NoDataField;
        found      = -1;
    }
    else if (found >= 0 && !fields[address].isAddressChecksumGood && !write.isAddressCheckOff)
    {
        outFailure = SectorWriteFailure::AddressChecksumFailed;
        found      = -1;
    }
    else if (found >= 0 && fields[found].hasNoise)
    {
        outFailure = SectorWriteFailure::NoiseInDataField;
        found      = -1;
    }
    else if (found >= 0 && !fields[found].data.invalidNibbles.empty())
    {
        outFailure = SectorWriteFailure::NibblesOutsideTable;
        found      = -1;
    }

    return found;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SectorFieldWriter::PlanSlot
//
//  Everything for one record, on copies: the fields as they are, each write's
//  new nibbles laid into the old cells, the new record built and decoded
//  again, and the result checked against what was asked.
//
////////////////////////////////////////////////////////////////////////////////

void SectorFieldWriter::PlanSlot (const DiskImage & image, SectorWritePolicy policy, SlotPlan & inOutPlan, vector<SectorWriteError> & inOutErrors)
{
    FramedTrack           track;
    FramedTrack           afterTrack;
    vector<LocatedField>  fields;
    vector<LocatedField>  afterFields;
    vector<Byte>          cells;
    vector<Byte>          body;
    vector<int>           written;
    Byte                  checksum   = 0;
    int                   data       = 0;
    int                   bodyLength = 0;
    int                   i          = 0;
    size_t                errorsIn   = inOutErrors.size();
    SectorWriteFailure    failure    = SectorWriteFailure::NoRecord;



    inOutPlan.before = TrackCopy::MakeFromImage (image, inOutPlan.slot);

    LatchFramer::Frame   (*inOutPlan.before, track);
    FieldLocator::Locate (track, FieldMarks::MakeStandard(), fields);

    for (const SectorWrite * write : inOutPlan.writes)
    {
        if (track.cellCount == 0)
        {
            AddError (*write, inOutPlan.slot, SectorWriteFailure::NoRecord, inOutErrors);
            continue;
        }

        if (IsSlotDamaged (image, inOutPlan.slot))
        {
            AddError (*write, inOutPlan.slot, SectorWriteFailure::DamagedRecord, inOutErrors);
            continue;
        }

        if (policy == SectorWritePolicy::Strict && !IsTrackComplete (fields))
        {
            AddError (*write, inOutPlan.slot, SectorWriteFailure::TrackIncomplete, inOutErrors);
            continue;
        }

        data = FindDataField (fields, *write, failure);

        if (data < 0)
        {
            AddError (*write, inOutPlan.slot, failure, inOutErrors);
            continue;
        }

        DiskFieldFormat::EncodeData (fields[data].kind, write->bytes, body, checksum);

        bodyLength = DiskFieldFormat::GetBodyLength (fields[data].kind);

        if (static_cast<int> (body.size()) != bodyLength)
        {
            AddError (*write, inOutPlan.slot, SectorWriteFailure::NibbleCountDiffers, inOutErrors);
            continue;
        }

        if (write->checksumMode == ChecksumMode::KeepStored)
        {
            checksum = track.nibbles[(fields[data].bodyNibble + bodyLength) % track.nibbles.size()].value;
        }

        if (cells.empty())
        {
            cells = track.cells;
        }

        for (i = 0; i < bodyLength; i++)
        {
            WriteNibble (track, fields[data].bodyNibble + i, body[i], cells);
        }

        WriteNibble (track, fields[data].bodyNibble + bodyLength, checksum, cells);
        written.push_back (data);
    }

    if (inOutErrors.size() == errorsIn && !cells.empty())
    {
        BuildAfter (track, cells, inOutPlan);

        LatchFramer::Frame   (*inOutPlan.after, afterTrack);
        FieldLocator::Locate (afterTrack, FieldMarks::MakeStandard(), afterFields);

        if (!IsVerified (fields, afterFields, written, inOutPlan))
        {
            AddError (*inOutPlan.writes.front(), inOutPlan.slot, SectorWriteFailure::VerifyFailed, inOutErrors);
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  SectorFieldWriter::WriteNibble
//
//  A nibble's eight cells, from the cell its leading 1 sits in. The zero cells
//  after it are left as they are.
//
////////////////////////////////////////////////////////////////////////////////

void SectorFieldWriter::WriteNibble (const FramedTrack & track, int nibbleIndex, Byte value, vector<Byte> & inOutCells)
{
    const FramedNibble &  nibble = track.nibbles[nibbleIndex % track.nibbles.size()];
    int                   bit    = 0;



    for (bit = 0; bit < LatchFramer::kNibbleCells; bit++)
    {
        inOutCells[(nibble.startCell + bit) % track.cellCount] = static_cast<Byte> ((value >> ((LatchFramer::kNibbleCells - 1) - bit)) & 1);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  SectorFieldWriter::BuildAfter
//
//  The new record. A bit track packs the cells back into a buffer of the same
//  size. A flux track places each transition at the end of its cell, adding up
//  every cell's recorded time from the index, so a transition outside the
//  changed fields lands exactly where it was and each cell of a field takes
//  the time the cell at the same place took.
//
////////////////////////////////////////////////////////////////////////////////

void SectorFieldWriter::BuildAfter (const FramedTrack & track, const vector<Byte> & cells, SlotPlan & inOutPlan)
{
    static constexpr int  kBitsPerByte = 8;



    FluxTrack         flux;
    vector<uint64_t>  ticks;
    double            elapsed = 0;
    size_t            k       = 0;



    inOutPlan.after = std::make_shared<TrackCopy> (*inOutPlan.before);

    if (inOutPlan.before->kind == TrackKind::Flux)
    {
        for (k = 0; k < cells.size(); k++)
        {
            elapsed += track.cellTicks[k];

            if (cells[k] != 0)
            {
                ticks.push_back (static_cast<uint64_t> (std::llround (elapsed)));
            }
        }

        flux.AssignTransitionTicks (ticks, static_cast<uint64_t> (std::llround (track.turnTicks)));
        inOutPlan.after->fluxBytes = flux.GetBytes();
    }
    else
    {
        std::fill (inOutPlan.after->bits.begin(), inOutPlan.after->bits.end(), static_cast<Byte> (0));

        for (k = 0; k < cells.size(); k++)
        {
            inOutPlan.after->bits[k / kBitsPerByte] = static_cast<Byte> (inOutPlan.after->bits[k / kBitsPerByte] | (cells[k] << ((kBitsPerByte - 1) - (k % kBitsPerByte))));
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  SectorFieldWriter::IsVerified
//
//  The new record decoded again: the same fields in the same order, each
//  written data field holding its new bytes (with a good checksum when it was
//  recomputed), and every other field decoding exactly as it did before.
//
////////////////////////////////////////////////////////////////////////////////

bool SectorFieldWriter::IsVerified (const vector<LocatedField> & before, const vector<LocatedField> & after, const vector<int> & written, const SlotPlan & plan)
{
    bool    isSame  = before.size() == after.size();
    bool    isWrite = false;
    size_t  i       = 0;
    size_t  w       = 0;



    for (i = 0; isSame && i < before.size(); i++)
    {
        isSame = before[i].role == after[i].role
              && before[i].kind == after[i].kind
              && before[i].startCell == after[i].startCell
              && before[i].sector == after[i].sector
              && before[i].volume == after[i].volume
              && before[i].isAddressChecksumGood == after[i].isAddressChecksumGood
              && before[i].pairedField == after[i].pairedField;

        isWrite = std::find (written.begin(), written.end(), static_cast<int> (i)) != written.end();

        if (isSame && !isWrite)
        {
            isSame = before[i].data.bytes == after[i].data.bytes
                  && before[i].data.isChecksumGood == after[i].data.isChecksumGood;
        }
    }

    for (w = 0; isSame && w < written.size(); w++)
    {
        const SectorWrite *  write = plan.writes[w];

        isSame = after[written[w]].data.bytes == write->bytes
              && (write->checksumMode == ChecksumMode::KeepStored || after[written[w]].data.isChecksumGood);
    }

    return isSame;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SectorFieldWriter::AddError
//
////////////////////////////////////////////////////////////////////////////////

void SectorFieldWriter::AddError (const SectorWrite & write, int slot, SectorWriteFailure reason, vector<SectorWriteError> & inOutErrors)
{
    inOutErrors.push_back ({ write.track, slot, write.sector, reason });
}





////////////////////////////////////////////////////////////////////////////////
//
//  SectorFieldWriter::Describe
//
//  One clause for a refusal, in the form the `disk` command's other refusal
//  reasons take: where, which sector when there is one, and why.
//
////////////////////////////////////////////////////////////////////////////////

std::string SectorFieldWriter::Describe (const SectorWriteError & error)
{
    std::string  where  = (error.track >= 0) ? std::format ("track {}", error.track) : std::format ("track record {}", error.slot);
    std::string  sector = std::format ("sector ${:X}", error.sector);
    std::string  text;



    switch (error.reason)
    {
        case SectorWriteFailure::NoRecord:              text = where + " holds no track, and no track is formatted to make room";                                              break;
        case SectorWriteFailure::TrackIncomplete:       text = where + " does not decode to all 16 sectors, each once with good checksums, so it was left unchanged";        break;
        case SectorWriteFailure::SectorNotFound:        text = where + " has no " + sector;                                                                                  break;
        case SectorWriteFailure::NoDataField:           text = where + " " + sector + " has no data field";                                                                  break;
        case SectorWriteFailure::AddressChecksumFailed: text = where + " " + sector + " has an address field whose checksum fails";                                          break;
        case SectorWriteFailure::NoiseInDataField:      text = where + " " + sector + " has noise inside its data field";                                                    break;
        case SectorWriteFailure::NibblesOutsideTable:   text = where + " " + sector + " has data nibbles outside the translate table";                                       break;
        case SectorWriteFailure::DamagedRecord:         text = where + " is a damaged track record";                                                                         break;
        case SectorWriteFailure::NibbleCountDiffers:    text = where + " " + sector + " would need a data field of another length";                                          break;
        case SectorWriteFailure::VerifyFailed:          text = where + " did not read back as written, so it was left unchanged";                                            break;
    }

    return text;
}
