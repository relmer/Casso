#include "Pch.h"

#include "Debugger/DebugFileData.h"

#include "Core/TextEncoding.h"
#include "Debugger/Handlers/DataDirectiveHandlers.h"
#include "Ui/Debugger/SourceSyntax.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DebugFileData::GetBlocks
//
////////////////////////////////////////////////////////////////////////////////

std::vector<DataBlockEntry> DebugFileData::GetBlocks (const DebugFile & file, const std::map<int, std::vector<std::string>> & sources)
{
    static constexpr const char *        kShapes[] = { "DB", "DW", "DA", "ASC", "DF" };
    std::vector<DataBlockEntry>          blocks;
    std::map<int, const DebugSpan *>     spans;
    std::map<int, const DebugSegment *>  segments;



    for (const DebugSpan & span : file.spans)
    {
        spans[span.id] = &span;
    }

    for (const DebugSegment & segment : file.segments)
    {
        segments[segment.id] = &segment;
    }

    for (const DebugLine & line : file.lines)
    {
        auto           source = sources.find (line.file);
        DataBlockKind  kind   = DataBlockKind::Bytes;
        int            items  = 1;
        int            width  = 1;

        if (line.type != DebugLineType::Asm || line.spans.empty() || source == sources.end() ||
            line.line < 1 || (size_t) line.line > source->second.size() ||
            !TryGetKind (source->second[(size_t) line.line - 1], kind))
        {
            continue;
        }

        DataDirectiveHandlers::TryGetShape (kShapes[(int) kind], kind, items, width);

        for (int id : line.spans)
        {
            auto            span    = spans.find (id);
            auto            segment = (span != spans.end()) ? segments.find (span->second->segment) : segments.end();
            uint32_t        first   = 0;
            DataBlockEntry  block;

            if (segment == segments.end() || span->second->size == 0)
            {
                continue;
            }

            first = segment->second->start + span->second->start;

            if (first + span->second->size - 1 > 0xFFFF)
            {
                continue;
            }

            block.first   = (Word) first;
            block.last    = (Word) (first + span->second->size - 1);
            block.kind    = kind;
            block.perLine = items;
            blocks.push_back (block);
        }
    }

    return blocks;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebugFileData::TryGetKind
//
//  The directive is matched in lower case without its period, so the
//  as65, ca65 and Merlin forms of one directive share an entry.
//
////////////////////////////////////////////////////////////////////////////////

bool DebugFileData::TryGetKind (const std::string & line, DataBlockKind & kind)
{
    static constexpr std::pair<std::string_view, DataBlockKind>  kKinds[] =
    {
        { "asc",  DataBlockKind::Text  }, { "ascii", DataBlockKind::Text  }, { "asciiz", DataBlockKind::Text    }, { "asciz", DataBlockKind::Text  },
        { "str",  DataBlockKind::Text  }, { "text",  DataBlockKind::Text  }, { "dci",    DataBlockKind::Text    }, { "inv",   DataBlockKind::Text  },
        { "fls",  DataBlockKind::Text  }, { "rev",   DataBlockKind::Text  }, { "word",   DataBlockKind::Words   }, { "dw",    DataBlockKind::Words },
        { "dbyt", DataBlockKind::Words }, { "ddb",   DataBlockKind::Words }, { "addr",   DataBlockKind::Address }, { "da",    DataBlockKind::Address },
    };
    std::wstring  wide      = TextEncoding::NarrowToWide (line);
    bool          hasString = false;
    std::string   directive;



    if (!SourceSyntax::IsDirectiveLine (wide, SourceSyntax::Assembler::Any))
    {
        return false;
    }

    directive = GetDirective (wide, hasString);
    kind      = hasString ? DataBlockKind::Text : DataBlockKind::Bytes;

    for (const auto & [name, each] : kKinds)
    {
        kind = (directive == name) ? each : kind;
    }

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebugFileData::GetDirective
//
//  The line's first directive in lower case without a leading period, and
//  whether a string follows it before any comment.
//
////////////////////////////////////////////////////////////////////////////////

std::string DebugFileData::GetDirective (const std::wstring & line, bool & hasString)
{
    std::string  directive;



    hasString = false;

    for (const SourceSyntax::Run & run : SourceSyntax::GetSourceRuns (line))
    {
        if (run.token == SourceSyntax::Token::Comment)
        {
            break;
        }

        if (run.token == SourceSyntax::Token::Directive && directive.empty())
        {
            directive = TextEncoding::WideToNarrow (line.substr ((size_t) run.start, (size_t) run.length));
            directive = directive.starts_with ('.') ? directive.substr (1) : directive;
            std::ranges::transform (directive, directive.begin(), [] (char ch) { return (char) std::tolower ((unsigned char) ch); });
        }
        else if (run.token == SourceSyntax::Token::String && !directive.empty())
        {
            hasString = true;
        }
    }

    return directive;
}
