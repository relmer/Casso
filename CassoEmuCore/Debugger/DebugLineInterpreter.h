#pragma once

#include "Debugger/DebugCommand.h"
#include "Debugger/MonitorState.h"
#include "Debugger/Reply.h"

class DebugSession;
class OpcodeTable;





////////////////////////////////////////////////////////////////////////////////
//
//  DebugLineInterpreter
//
//  A debug session's command lines: each is parsed in the dialect of its
//  mode and its commands executed through the session, a Monitor line's
//  several commands merged into one reply. While the line assembler is
//  open, a line is an instruction to assemble instead.
//
////////////////////////////////////////////////////////////////////////////////

class DebugLineInterpreter
{
public:
    explicit DebugLineInterpreter (DebugSession & session);
    ~DebugLineInterpreter();

    DebugLineInterpreter             (const DebugLineInterpreter &) = delete;
    DebugLineInterpreter & operator= (const DebugLineInterpreter &) = delete;

    Reply                        ExecuteLine        (const std::string & line, CommandMode mode);
    Reply                        ExecutePaneLine    (const std::string & line, CommandMode mode);

    //  The mode of the line being run, empty outside a line.
    std::optional<CommandMode>   GetLineMode        () const { return m_lineMode; }

    void                         BeginAssembly      (Word address);
    void                         EndAssembly        ();
    bool                         IsAssembling       () const { return m_assemblyAddress.has_value(); }
    std::optional<Word>          GetAssemblyAddress () const { return m_assemblyAddress; }
    void                         SetAssemblyAddress (std::optional<Word> address) { m_assemblyAddress = address; }

    MonitorState               & GetMonitorState    ()       { return m_monitorState; }

private:
    Reply                        ExecuteAppleWinLine   (const std::string & text);
    Reply                        ExecuteWinDbgLine     (const std::string & text);
    Reply                        ExecuteMonitorLine    (const std::string & text, CommandMode mode);
    Reply                        ExecuteGSSquaredLine  (const std::string & text, CommandMode mode);
    void                         KeepRegisterEditArmed (const DebugCommand & command, const Reply & reply);
    bool                         TryResolveIdOrAddress (DebugCommand & command, Reply & reply);
    void                         ExecuteAssemblyLine   (const std::string & line, Reply & reply);
    void                         MergeReply            (Reply & merged, Reply & one, CommandMode mode) const;

    static std::string           Trim                  (const std::string & text);

    DebugSession                  & m_session;
    std::optional<CommandMode>      m_lineMode;
    std::optional<Word>             m_assemblyAddress;
    std::unique_ptr<OpcodeTable>    m_assemblyOpcodes;
    MonitorState                    m_monitorState;
};
