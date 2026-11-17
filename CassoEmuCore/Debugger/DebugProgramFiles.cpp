#include "Pch.h"

#include "Core/TextEncoding.h"
#include "Debugger/DebugCommand.h"
#include "Debugger/DebugProgramFiles.h"
#include "Debugger/IDebugTarget.h"
#include "Debugger/Reply.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DebugProgramFiles::SetDebugFile
//
////////////////////////////////////////////////////////////////////////////////

void DebugProgramFiles::SetDebugFile (DebugFile file, const std::wstring & path, const std::string & key)
{
    m_debugFile     = std::move (file);
    m_debugFilePath = path;
    m_debugFileKey  = key;
    m_debugFileTable.reset();
    m_debugFileData.Clear();
    m_lineTable.Build (m_debugFile);
    m_filesRevision++;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebugProgramFiles::SetDebugFileData
//
////////////////////////////////////////////////////////////////////////////////

void DebugProgramFiles::SetDebugFileData (std::vector<DataBlockEntry> blocks)
{
    m_debugFileData.Clear();

    for (const DataBlockEntry & block : blocks)
    {
        m_debugFileData.Insert (block);
    }

    m_filesRevision++;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebugProgramFiles::TryFindDebugFileData
//
//  The debug file's block counts only while the program in memory is the
//  file's.
//
////////////////////////////////////////////////////////////////////////////////

bool DebugProgramFiles::TryFindDebugFileData (Word address, DataBlockEntry & block) const
{
    return HasDebugFile() && !IsDebugFileStale() && m_debugFileData.TryFindAt (address, block);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebugProgramFiles::RemoveDebugFileData
//
////////////////////////////////////////////////////////////////////////////////

void DebugProgramFiles::RemoveDebugFileData (Word first, Word last)
{
    m_debugFileData.Remove (first, last);

    //  The panes' copy of the debug file's data is taken again at a new
    //  revision.
    if (HasDebugFile())
    {
        m_filesRevision++;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebugProgramFiles::TryGetSourceLine
//
////////////////////////////////////////////////////////////////////////////////

bool DebugProgramFiles::TryGetSourceLine (Word address, std::string & file, int & line) const
{
    const std::vector<SourcePosition>  & positions = m_lineTable.GetPositionsAt (address);
    int                                  fileId    = 0;



    if (positions.empty() || IsDebugFileStale())
    {
        return false;
    }

    fileId = positions.back().file;

    for (const DebugSourceFile & each : m_debugFile.files)
    {
        if (each.id == fileId)
        {
            file = each.name;
            line = positions.back().line;
            return true;
        }
    }

    return false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebugProgramFiles::GetRoutineEntries
//
//  Every address a label in the debug file has: a routine entry, for the
//  call stack's walk.
//
////////////////////////////////////////////////////////////////////////////////

void DebugProgramFiles::GetRoutineEntries (std::set<Word> & outEntries) const
{
    for (const DebugSymbol & symbol : m_debugFile.symbols)
    {
        if (symbol.type == "lab")
        {
            outEntries.insert ((Word) symbol.value);
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebugProgramFiles::SetDebugFileCheck
//
//  A debug file checks the tables of no symbol file loaded into its table.
//
////////////////////////////////////////////////////////////////////////////////

void DebugProgramFiles::SetDebugFileCheck (std::vector<ProgramCheck::Entry> entries)
{
    ProgramChecks  checks = (m_programChecks != nullptr) ? *m_programChecks : ProgramChecks();



    checks.debugFile        = std::move (entries);
    checks.isDebugFileStale = false;

    if (m_debugFileTable.has_value())
    {
        checks.tables.erase (*m_debugFileTable);
        m_symbols.SetHidden (*m_debugFileTable, false);
    }

    m_programChecks = std::make_shared<const ProgramChecks> (std::move (checks));
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebugProgramFiles::SetLabelCheck
//
//  The labels in RAM, whose bytes are taken at the first stop in their
//  range.
//
////////////////////////////////////////////////////////////////////////////////

void DebugProgramFiles::SetLabelCheck (SymbolTableId table)
{
    ProgramChecks            checks = (m_programChecks != nullptr) ? *m_programChecks : ProgramChecks();
    ProgramChecks::Labels    labels;
    std::vector<SymbolInfo>  symbols;



    m_symbols.GetAll (table, symbols);

    for (const SymbolInfo & symbol : symbols)
    {
        if (!symbol.isConstant && ProgramCheck::IsRamLabel (symbol.address))
        {
            labels.labels.push_back (symbol.address);
        }
    }

    checks.tables.erase (table);
    m_symbols.SetHidden (table, false);

    if (!labels.labels.empty())
    {
        labels.first         = *std::ranges::min_element (labels.labels);
        labels.last          = *std::ranges::max_element (labels.labels);
        checks.tables[table] = std::move (labels);
    }

    m_programChecks = std::make_shared<const ProgramChecks> (std::move (checks));
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebugProgramFiles::GetUnmatchedSourceLines
//
////////////////////////////////////////////////////////////////////////////////

std::set<std::pair<int, int>> DebugProgramFiles::GetUnmatchedSourceLines() const
{
    ProgramCheck::PeekFn  peek = [this] (Word address, Byte & value) { return m_target.TryPeek (address, value); };



    return ProgramCheck::GetUnmatchedLines (m_debugFileLines, peek, m_target.GetInstructionSet());
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebugProgramFiles::CheckProgram
//
//  A table whose labels have no bytes yet takes them at a stop in the
//  labels' range, since a symbol file loaded before its program would
//  otherwise be matched to whatever was in memory then.
//
////////////////////////////////////////////////////////////////////////////////

void DebugProgramFiles::CheckProgram()
{
    ProgramChecks         checks;
    ProgramCheck::PeekFn  peek  = [this] (Word address, Byte & value) { return m_target.TryPeek (address, value); };
    const Microcode     * set   = m_target.GetInstructionSet();
    Word                  pc    = m_target.GetRegisters().pc;



    if (m_programChecks == nullptr)
    {
        return;
    }

    checks                  = *m_programChecks;
    checks.isDebugFileStale = HasDebugFile() && !ProgramCheck::IsMatch (checks.debugFile, peek, set);

    for (auto it = checks.tables.begin(); it != checks.tables.end(); )
    {
        auto & [table, labels] = *it;

        //  A table cleared since its file loaded has nothing to check.
        if (m_symbols.GetOrigins (table).empty())
        {
            it = checks.tables.erase (it);
            continue;
        }

        if (labels.entries.empty() && labels.IsInRange (pc))
        {
            labels.entries = ProgramCheck::GetLabelEntries (labels.labels, peek);
        }

        labels.isStale = !labels.entries.empty() && !ProgramCheck::IsMatch (labels.entries, peek, set);
        m_symbols.SetHidden (table, labels.isStale);
        ++it;
    }

    if (m_debugFileTable.has_value())
    {
        m_symbols.SetHidden (*m_debugFileTable, checks.isDebugFileStale);
    }

    if (checks != *m_programChecks)
    {
        m_programChecks = std::make_shared<const ProgramChecks> (std::move (checks));
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebugProgramFiles::GetStaleFileNames
//
////////////////////////////////////////////////////////////////////////////////

std::vector<std::string> DebugProgramFiles::GetStaleFileNames() const
{
    std::vector<std::string>  names;



    if (m_programChecks == nullptr)
    {
        return names;
    }

    if (m_programChecks->isDebugFileStale)
    {
        names.push_back (TextEncoding::WideToUtf8 (fs::path (m_debugFilePath).filename().wstring()));
    }

    for (const auto & [table, labels] : m_programChecks->tables)
    {
        if (labels.isStale)
        {
            names.insert (names.end(), m_symbols.GetOrigins (table).begin(), m_symbols.GetOrigins (table).end());
        }
    }

    return names;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebugProgramFiles::ClearDebugFile
//
////////////////////////////////////////////////////////////////////////////////

void DebugProgramFiles::ClearDebugFile()
{
    if (m_debugFileTable.has_value())
    {
        m_symbols.SetHidden (*m_debugFileTable, false);
    }

    if (m_programChecks != nullptr && (!m_programChecks->debugFile.empty() || m_programChecks->isDebugFileStale))
    {
        ProgramChecks  checks = *m_programChecks;

        checks.debugFile.clear();
        checks.isDebugFileStale = false;
        m_programChecks         = std::make_shared<const ProgramChecks> (std::move (checks));
    }

    m_debugFile = DebugFile();
    m_debugFilePath.clear();
    m_debugFileKey.clear();
    m_debugFileTable.reset();
    m_debugFileData.Clear();
    m_lineTable.Clear();
    m_filesRevision++;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebugProgramFiles::ExecuteSource
//
//  SRC gives the source line at PC and how steps go; SRC ON and SRC OFF
//  choose between source lines and instructions. SRC CLEAR unloads the
//  debug file and the symbols it put in its table.
//
////////////////////////////////////////////////////////////////////////////////

void DebugProgramFiles::ExecuteSource (const DebugCommand & command, Word pc, Reply & reply)
{
    MessageData  message;
    std::string  file;
    int          line      = 0;
    bool         hasSource = HasSourceLines();
    std::string  name;



    if (command.verb == DebugVerb::ShowSource && command.text == "CLEAR")
    {
        name = TextEncoding::WideToNarrow (std::filesystem::path (m_debugFilePath).filename().wstring());

        if (!HasDebugFile())
        {
            message.lines.push_back ("No debug file is loaded.");
        }
        else
        {
            if (m_debugFileTable.has_value())
            {
                m_symbols.Clear (*m_debugFileTable);
            }

            ClearDebugFile();
            message.lines.push_back (std::format ("Unloaded the debug file {}.", name));
        }

        reply.data = message;
        return;
    }

    if (command.verb == DebugVerb::SetSourceStepping)
    {
        m_stepBySource = command.count != 0;
    }

    if (!HasDebugFile())
    {
        message.lines.push_back ("No debug file is loaded. Use SYM LOAD to load one.");
    }
    else if (!hasSource)
    {
        message.lines.push_back ("The debug file has no source lines.");
    }
    else if (TryGetSourceLine (pc, file, line))
    {
        message.lines.push_back (std::format ("${:04X} is {} line {}.", pc, file, line));
    }
    else
    {
        message.lines.push_back (std::format ("${:04X} has no source line.", pc));
    }

    message.lines.push_back ((m_stepBySource && hasSource)       ? "Source mode."
                             : (m_stepBySource && HasDebugFile()) ? "Source mode. The debug file has no source lines, so steps go by instruction."
                             : m_stepBySource                     ? "Source mode. Until a debug file is loaded, steps go by instruction."
                             :                                      "Disassembly mode.");
    reply.data = message;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebugProgramFiles::NoteSymbolLoad
//
//  A load replaces its table's symbols, so it replaces the table's earlier
//  load too. Otherwise every file ever loaded into the table was replayed
//  in turn, and one passed over as loaded was replaced by an earlier one.
//
////////////////////////////////////////////////////////////////////////////////

void DebugProgramFiles::NoteSymbolLoad (SymbolTableId table, const std::wstring & path, int offset)
{
    std::erase_if (m_symbolLoads, [&] (const SymbolLoad & load) { return load.table == table; });
    m_symbolLoads.push_back ({ table, path, offset });
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebugProgramFiles::ForgetSymbolLoads
//
////////////////////////////////////////////////////////////////////////////////

void DebugProgramFiles::ForgetSymbolLoads (SymbolTableId table)
{
    std::erase_if (m_symbolLoads, [table] (const SymbolLoad & load) { return load.table == table; });
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebugProgramFiles::NoteAddedSymbol
//
//  A name matches without regard to case, as the symbol tables match it.
//
////////////////////////////////////////////////////////////////////////////////

void DebugProgramFiles::NoteAddedSymbol (SymbolTableId table, const std::string & name, Word address)
{
    ForgetAddedSymbol (table, name);
    m_addedSymbols.push_back ({ table, name, address });
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebugProgramFiles::ForgetAddedSymbol
//
////////////////////////////////////////////////////////////////////////////////

void DebugProgramFiles::ForgetAddedSymbol (SymbolTableId table, const std::string & name)
{
    std::string  upper = SymbolTable::ToUpper (name);



    std::erase_if (m_addedSymbols, [&] (const AddedSymbol & added) { return added.table == table && SymbolTable::ToUpper (added.name) == upper; });
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebugProgramFiles::ForgetAddedSymbols
//
////////////////////////////////////////////////////////////////////////////////

void DebugProgramFiles::ForgetAddedSymbols (SymbolTableId table)
{
    std::erase_if (m_addedSymbols, [table] (const AddedSymbol & added) { return added.table == table; });
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebugProgramFiles::NoteBinaryLoad
//
////////////////////////////////////////////////////////////////////////////////

void DebugProgramFiles::NoteBinaryLoad (const std::string & hash, Word address, const std::string & name, const std::wstring & path)
{
    m_binaryKey  = std::format ("{}@{:04X}", hash, address);
    m_binaryName = name;
    m_binaryPath = path;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebugProgramFiles::ForgetBinaryLoad
//
////////////////////////////////////////////////////////////////////////////////

void DebugProgramFiles::ForgetBinaryLoad()
{
    m_binaryKey.clear();
    m_binaryName.clear();
    m_binaryPath.clear();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebugProgramFiles::TakeView
//
//  The debug file already in out is kept while its revision matches, so
//  taking a view copies it once per change rather than once per frame.
//
////////////////////////////////////////////////////////////////////////////////

void DebugProgramFiles::TakeView (DebugSessionView & out) const
{
    out.isStepBySource = m_stepBySource;
    out.programChecks  = m_programChecks;

    if (out.files == nullptr || out.filesRevision != m_filesRevision)
    {
        out.files         = std::make_shared<const DebugSessionFiles> (DebugSessionFiles { m_debugFile, m_debugFilePath, m_debugFileKey,
                                                                                           m_debugFileTable, m_lineTable, m_debugFileData });
        out.filesRevision = m_filesRevision;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebugProgramFiles::InstallView
//
//  The debug file is copied in only when its revision differs from the one
//  already here.
//
////////////////////////////////////////////////////////////////////////////////

void DebugProgramFiles::InstallView (const DebugSessionView & view)
{
    m_stepBySource  = view.isStepBySource;
    m_programChecks = view.programChecks;

    if (view.files != nullptr && view.filesRevision != m_filesRevision)
    {
        m_debugFile      = view.files->debugFile;
        m_debugFilePath  = view.files->path;
        m_debugFileKey   = view.files->key;
        m_debugFileTable = view.files->table;
        m_lineTable      = view.files->lineTable;
        m_debugFileData  = view.files->data;
        m_filesRevision  = view.filesRevision;
    }
}
