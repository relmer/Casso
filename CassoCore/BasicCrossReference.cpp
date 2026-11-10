#include "Pch.h"

#include "BasicCrossReference.h"



static constexpr size_t  s_kColumnGap = 2;





////////////////////////////////////////////////////////////////////////////////
//
//  BasicCrossReference::AppendEntry
//
////////////////////////////////////////////////////////////////////////////////

void BasicCrossReference::AppendEntry (const std::string & label, const LineSet & lines, bool isMissing, std::vector<Entry> & ioEntries)
{
    Entry  entry;



    entry.label     = label;
    entry.isMissing = isMissing;
    entry.lines.assign (lines.begin(), lines.end());

    ioEntries.push_back (entry);
}





////////////////////////////////////////////////////////////////////////////////
//
//  BasicCrossReference::Build
//
////////////////////////////////////////////////////////////////////////////////

HRESULT BasicCrossReference::Build (const std::vector<Byte> & programBytes, Report & outReport, std::string & outReason)
{
    HRESULT                                             hr = S_OK;
    BasicProgram::Program                               program;
    std::vector<BasicProgram::LineReference>            lineReferences;
    std::vector<BasicProgram::AddressReference>         addressReferences;
    std::vector<std::string>                            names;
    std::set<uint32_t>                                  numbers;
    std::map<std::string, LineSet>                      variables;
    std::map<uint32_t, LineSet>                         targets;
    std::map<std::pair<std::string, int32_t>, LineSet>  addresses;



    outReport = Report();

    hr = BasicProgram::Parse (programBytes, program, outReason);
    CHR (hr);

    outReport.dialect = program.dialect;

    for (const BasicProgram::Line & line : program.lines)
    {
        numbers.insert (line.number);

        BasicProgram::FindVariables         (program.dialect, line.body, names);
        BasicProgram::FindLineReferences    (program.dialect, line.body, lineReferences);
        BasicProgram::FindAddressReferences (program.dialect, line.body, addressReferences);

        for (const std::string & name : names)
        {
            variables[name].insert (line.number);
        }

        for (const BasicProgram::LineReference & reference : lineReferences)
        {
            targets[reference.target].insert (line.number);
        }

        for (const BasicProgram::AddressReference & reference : addressReferences)
        {
            addresses[std::make_pair (std::string (reference.word), reference.address)].insert (line.number);
        }
    }

    for (const std::pair<const std::string, LineSet> & variable : variables)
    {
        AppendEntry (variable.first, variable.second, false, outReport.variables);
    }

    for (const std::pair<const uint32_t, LineSet> & target : targets)
    {
        AppendEntry (std::to_string (target.first), target.second, numbers.count (target.first) == 0, outReport.lineReferences);
    }

    for (const std::pair<const std::pair<std::string, int32_t>, LineSet> & address : addresses)
    {
        AppendEntry (address.first.first + " " + std::to_string (address.first.second), address.second, false, outReport.addresses);
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BasicCrossReference::AppendSection
//
////////////////////////////////////////////////////////////////////////////////

void BasicCrossReference::AppendSection (const char * title, const std::vector<Entry> & entries, std::string & ioText)
{
    size_t  width = 0;
    size_t  index = 0;



    ioText += title;
    ioText += "\n";

    for (const Entry & entry : entries)
    {
        width = std::max (width, entry.label.size());
    }

    for (const Entry & entry : entries)
    {
        ioText += "  ";
        ioText += entry.label;
        ioText.append (width - entry.label.size() + s_kColumnGap, ' ');

        for (index = 0; index < entry.lines.size(); index++)
        {
            ioText += (index > 0) ? ", " : "";
            ioText += std::to_string (entry.lines[index]);
        }

        ioText += entry.isMissing ? "  (no such line)\n" : "\n";
    }

    if (entries.empty())
    {
        ioText += "  none\n";
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  BasicCrossReference::Format
//
////////////////////////////////////////////////////////////////////////////////

std::string BasicCrossReference::Format (const Report & report)
{
    std::string  text;



    text  = (report.dialect == BasicProgram::Dialect::Integer) ? "Integer BASIC\n\n" : "Applesoft BASIC\n\n";

    AppendSection ("Variables", report.variables, text);

    text += "\n";
    AppendSection ("Line references", report.lineReferences, text);

    text += "\n";
    AppendSection ("CALL, PEEK and POKE", report.addresses, text);

    return text;
}
