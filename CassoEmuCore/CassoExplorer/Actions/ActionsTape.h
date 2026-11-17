#pragma once

#include "Pch.h"

#include "CassoExplorer/Actions/ActionsTypes.h"
#include "Devices/Tape/TapeRecordingWriter.h"
#include "Machines/Apple2/Common/VolumeImage.h"

class CassoExplorerActions;





////////////////////////////////////////////////////////////////////////////////
//
//  ActionsTape
//
//  A tape recording of the selected files of a disk image: each Applesoft
//  program, Integer BASIC program and binary, in the order selected, as
//  the Apple II's cassette output saves them. The recording is WAV, MP3 or
//  Ogg Vorbis by the target's extension, and a sidecar, NAME.tape.txt,
//  is written beside it with each file's start time and the command that
//  loads it. Other files are left out, with a note.
//
////////////////////////////////////////////////////////////////////////////////

class ActionsTape
{
public:
    explicit ActionsTape (CassoExplorerActions & actions) : m_actions (actions) {}

    ActionsTypes::Outcome  MakeRecording (const std::wstring & target);

    //  How a file of this type on this volume is saved to tape, or false
    //  for a type a tape doesn't save.
    static bool  TryGetTapeKind (VolumeKind volume, Byte type, TapeFileKind & outKind);

    //  Whether any of the entries can be saved to tape.
    static bool  CanMakeRecording (VolumeKind volume, const std::vector<FileEntry> & entries);

    //  WAV, MP3 or Ogg Vorbis by the path's extension, WAV for any other.
    static TapeRecordingFormat  GetFormat (const std::wstring & path);

    //  The sidecar's path: the recording's, with ".tape.txt" in place of
    //  its extension.
    static std::wstring  GetSidecarPath (const std::wstring & path);

private:
    CassoExplorerActions & m_actions;
};
