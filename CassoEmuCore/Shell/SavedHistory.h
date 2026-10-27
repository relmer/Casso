#pragma once

#include "Pch.h"

#include "Core/IMachineState.h"
#include "Debugger/Reverse/KeyframeStore.h"

class StateReader;
class StateWriter;





////////////////////////////////////////////////////////////////////////////////
//
//  SavedBayMedium
//
//  The medium a bay had when history was saved, by the id its keyframes
//  refer to it by.
//
////////////////////////////////////////////////////////////////////////////////

struct SavedBayMedium
{
    int       slot    = 0;
    int       drive   = 0;
    uint64_t  mediaId = 0;
};





////////////////////////////////////////////////////////////////////////////////
//
//  SavedHistoryContents
//
//  A machine's history as a state file has it: the fingerprint of the build
//  that saved it (MachineHost::ComputeStateFingerprint), the keyframes and
//  input journal entries of every segment, the media in the bays, and where
//  the
//  machine was when it was saved, which is the active path's live end
//  unless it was behind live.
//
////////////////////////////////////////////////////////////////////////////////

struct SavedHistoryContents
{
    uint64_t                      fingerprint    = 0;
    SavedKeyframes                keyframes;
    std::vector<SavedBayMedium>   media;
    uint64_t                      resumePosition = 0;
    uint64_t                      resumeCycle    = 0;
};





////////////////////////////////////////////////////////////////////////////////
//
//  SavedHistory
//
//  The history section a machine state file has after its payload:
//
//    tag             4 bytes   'HIST'
//    version         2 bytes
//    reserved        2 bytes   zero
//    body size       8 bytes
//    body hash       8 bytes   StateHash over the body
//    body            StateWriter sections, stored unpacked, since the
//                    keyframes in them are packed already: HFMT the
//                    fingerprint, HSET the keyframe settings, HSEG the
//                    segments, HKEY their keyframes, HARN the keyframes'
//                    stored bytes, HJRN their journal entries, HMED the
//                    bays' media ids, HPOS the position to resume at
//
//  The active path is the first segment and the branches left behind
//  follow, oldest first. A section that fails any check is reported as
//  ERROR_INVALID_DATA, and its history is not loaded.
//
////////////////////////////////////////////////////////////////////////////////

class SavedHistory
{
public:
    static constexpr uint32_t  kTag        = IMachineState::MakeTag ('H', 'I', 'S', 'T');
    static constexpr uint16_t  kVersion    = 1;
    static constexpr size_t    kHeaderSize = 24;

    static HRESULT  Build (const SavedHistoryContents & contents, std::vector<Byte> & outSection);
    static HRESULT  Parse (const Byte * data, size_t size, SavedHistoryContents & outContents);

private:
    static constexpr uint16_t  kSectionVersion = 1;
    static constexpr uint32_t  kFormatTag      = IMachineState::MakeTag ('H', 'F', 'M', 'T');
    static constexpr uint32_t  kSettingsTag    = IMachineState::MakeTag ('H', 'S', 'E', 'T');
    static constexpr uint32_t  kSegmentsTag    = IMachineState::MakeTag ('H', 'S', 'E', 'G');
    static constexpr uint32_t  kKeyframesTag   = IMachineState::MakeTag ('H', 'K', 'E', 'Y');
    static constexpr uint32_t  kArenaTag       = IMachineState::MakeTag ('H', 'A', 'R', 'N');
    static constexpr uint32_t  kJournalTag     = IMachineState::MakeTag ('H', 'J', 'R', 'N');
    static constexpr uint32_t  kMediaTag       = IMachineState::MakeTag ('H', 'M', 'E', 'D');
    static constexpr uint32_t  kResumeTag      = IMachineState::MakeTag ('H', 'P', 'O', 'S');

    static HRESULT   WriteBody          (const SavedHistoryContents & contents, StateWriter & body);
    static HRESULT   ReadBody           (const Byte * data, size_t size, SavedHistoryContents & outContents);
    static HRESULT   ReadSegments       (StateReader & reader, size_t size, SavedKeyframes & outKeyframes);
    static HRESULT   ReadKeyframes      (StateReader & reader, SavedKeyframes & ioKeyframes);
    static HRESULT   ReadJournal        (StateReader & reader, size_t size, SavedKeyframes & ioKeyframes);
    static HRESULT   ReadMedia          (StateReader & reader, std::vector<SavedBayMedium> & outMedia);
    static void      WriteInfo          (const KeyframeInfo & info, StateWriter & writer);
    static void      ReadInfo           (StateReader & reader, KeyframeInfo & outInfo);
    static void      WriteRecord        (const InputRecord & record, StateWriter & writer);
    static HRESULT   ReadRecord         (StateReader & reader, size_t size, InputRecord & outRecord);
    static uint64_t  ReadLittleEndian   (const Byte * data, size_t byteCount);
    static void      AppendLittleEndian (uint64_t value, size_t byteCount, std::vector<Byte> & outBytes);
};
