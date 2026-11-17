#pragma once

#include "Debugger/DataBlockTable.h"
#include "Debugger/DebugFile.h"
#include "Debugger/DebugSessionView.h"
#include "Debugger/LineTable.h"
#include "Debugger/ProgramCheck.h"
#include "Debugger/SymbolTable.h"

class IDebugTarget;
struct DebugCommand;
struct Reply;





////////////////////////////////////////////////////////////////////////////////
//
//  DebugProgramFiles
//
//  The files a debug session loaded for the program in memory: the debug
//  file with its line table and data blocks, the checks of whether memory
//  still has the program each file was made for, and the record of each
//  symbol file, added symbol and binary loaded, for a debug project to load
//  again.
//
////////////////////////////////////////////////////////////////////////////////

class DebugProgramFiles
{
public:
    //  Each symbol or debug file loaded, with the table and the offset; a
    //  later load of one path into one table replaces the earlier, and
    //  clearing a table removes its loads.
    struct SymbolLoad
    {
        SymbolTableId  table  = SymbolTableId::User;
        std::wstring   path;
        int            offset = 0;
    };

    //  Each symbol the user added, with `SYM<table> name = addr` or Add
    //  symbol here...; adding a name again replaces it, and removing it or
    //  clearing its table removes it.
    struct AddedSymbol
    {
        SymbolTableId  table   = SymbolTableId::User;
        std::string    name;
        Word           address = 0;
    };

    DebugProgramFiles (IDebugTarget & target, SymbolTable & symbols) : m_target (target), m_symbols (symbols) {}

    void                  SetDebugFile     (DebugFile file, const std::wstring & path, const std::string & key);
    void                  ClearDebugFile   ();
    bool                  HasDebugFile     () const { return !m_debugFilePath.empty(); }
    const DebugFile     & GetDebugFile     () const { return m_debugFile; }
    const std::wstring  & GetDebugFilePath () const { return m_debugFilePath; }
    const std::string   & GetDebugFileKey  () const { return m_debugFileKey; }
    const LineTable     & GetLineTable     () const { return m_lineTable; }

    void                          SetDebugFileTable (SymbolTableId table) { m_debugFileTable = table; m_filesRevision++; }
    std::optional<SymbolTableId>  GetDebugFileTable () const { return m_debugFileTable; }

    void                  SetDebugFileData     (std::vector<DataBlockEntry> blocks);
    bool                  TryFindDebugFileData (Word address, DataBlockEntry & block) const;
    void                  RemoveDebugFileData  (Word first, Word last);

    void                  SetDebugFileCheck (std::vector<ProgramCheck::Entry> entries);
    void                  SetLabelCheck     (SymbolTableId table);
    void                  CheckProgram      ();
    bool                  IsDebugFileStale  () const { return m_programChecks != nullptr && m_programChecks->isDebugFileStale; }

    void                                      SetDebugFileLines       (std::vector<ProgramCheck::Entry> entries) { m_debugFileLines = std::move (entries); }
    const std::vector<ProgramCheck::Entry> &  GetDebugFileLines       () const { return m_debugFileLines; }
    std::set<std::pair<int, int>>             GetUnmatchedSourceLines () const;
    std::vector<std::string>                  GetStaleFileNames       () const;
    uint64_t                                  GetFilesRevision        () const { return m_filesRevision; }

    bool                  TryGetSourceLine  (Word address, std::string & file, int & line) const;
    bool                  HasSourceLines    () const { return !m_lineTable.IsEmpty() && !IsDebugFileStale(); }
    bool                  HasCurrentFile    () const { return HasDebugFile() && !IsDebugFileStale(); }
    void                  GetRoutineEntries (std::set<Word> & outEntries) const;

    //  Whether steps go by source line, and the line table a step takes
    //  when they do: null with no source lines to step by.
    void                  SetStepBySource      (bool bySource) { m_stepBySource = bySource; }
    bool                  IsStepBySource       () const        { return m_stepBySource; }
    const LineTable     * GetSteppingLineTable () const        { return (m_stepBySource && HasSourceLines()) ? &m_lineTable : nullptr; }

    //  SRC and its forms, with the PC the source line is looked up at.
    void                  ExecuteSource     (const DebugCommand & command, Word pc, Reply & reply);

    void                              NoteSymbolLoad     (SymbolTableId table, const std::wstring & path, int offset);
    void                              ForgetSymbolLoads  (SymbolTableId table);
    const std::vector<SymbolLoad>   & GetSymbolLoads     () const { return m_symbolLoads; }
    void                              NoteAddedSymbol    (SymbolTableId table, const std::string & name, Word address);
    void                              ForgetAddedSymbol  (SymbolTableId table, const std::string & name);
    void                              ForgetAddedSymbols (SymbolTableId table);
    const std::vector<AddedSymbol>  & GetAddedSymbols    () const { return m_addedSymbols; }

    void                  NoteBinaryLoad    (const std::string & hash, Word address, const std::string & name, const std::wstring & path);
    void                  ForgetBinaryLoad  ();
    const std::string   & GetBinaryKey      () const { return m_binaryKey; }
    const std::string   & GetBinaryName     () const { return m_binaryName; }
    const std::wstring  & GetBinaryPath     () const { return m_binaryPath; }

    //  The checks and the debug file for the panes' session, copied only
    //  when the files' revision changed.
    void                  TakeView          (DebugSessionView & out) const;
    void                  InstallView       (const DebugSessionView & view);

private:
    IDebugTarget                        & m_target;
    SymbolTable                         & m_symbols;
    DebugFile                             m_debugFile;
    std::wstring                          m_debugFilePath;
    std::string                           m_debugFileKey;
    std::optional<SymbolTableId>          m_debugFileTable;
    DataBlockTable                        m_debugFileData;
    LineTable                             m_lineTable;
    uint64_t                              m_filesRevision = 0;
    std::shared_ptr<const ProgramChecks>  m_programChecks;
    std::vector<ProgramCheck::Entry>      m_debugFileLines;
    bool                                  m_stepBySource  = false;
    std::vector<SymbolLoad>               m_symbolLoads;
    std::vector<AddedSymbol>              m_addedSymbols;
    std::string                           m_binaryKey;
    std::string                           m_binaryName;
    std::wstring                          m_binaryPath;
};
