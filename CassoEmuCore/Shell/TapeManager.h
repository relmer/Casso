#pragma once

#include "Devices/Tape/ITapeAudioDecoder.h"
#include "Devices/Tape/TapeDeck.h"
#include "Shell/CpuCommandDispatcher.h"

class IDiskFileIo;
class IFileSystem;
class UserConfigStore;





////////////////////////////////////////////////////////////////////////////////
//
//  TapeManager
//
//  The shell's side of the cassette recorder. The UI thread reads and decodes
//  a tape file here, so the CPU thread never stalls on file I/O, and asks for
//  everything else by posting a command. The CPU thread runs those commands
//  against the deck through Execute.
//
//  It also remembers the inserted tape per machine and puts it back, rewound,
//  at the next launch.
//
////////////////////////////////////////////////////////////////////////////////

class TapeManager
{
public:
    using PostFn        = std::function<void (WORD, const std::string &)>;
    using MachineNameFn = std::function<std::wstring ()>;

    TapeManager (IDiskFileIo        & fileIo,
                 IFileSystem        & fileSystem,
                 UserConfigStore    & configStore,
                 ITapeAudioDecoder  & compressedDecoder,
                 PostFn               post,
                 MachineNameFn        machineName);

    HRESULT  Insert            (const std::string & path, std::string & error);
    HRESULT  CreateBlank       (const std::string & path, std::string & error);
    void     Eject             ();
    void     Play              ();
    void     Stop              ();
    void     Rewind            ();
    void     SetRecordArmed    (bool isArmed);
    HRESULT  RestoreSavedTape  ();
    HRESULT  RestoreTape       (const std::wstring & savedPath);
    void     OnMachineSwitched ();

    const std::string &  GetInsertedPath () const { return m_insertedPath; }

    void     Execute           (TapeCommand command, TapeDeck & deck, uint64_t nowCycle);

    static constexpr uint32_t  kBlankSampleRate = 44100;

private:
    HRESULT  SaveTapePath    (const std::string & path);

    IDiskFileIo        & m_fileIo;
    IFileSystem        & m_fileSystem;
    UserConfigStore    & m_configStore;
    ITapeAudioDecoder  & m_compressedDecoder;
    PostFn               m_post;
    MachineNameFn        m_machineName;
    std::string          m_insertedPath;

    std::mutex                m_pendingLock;
    std::optional<TapeImage>  m_pending;
};
