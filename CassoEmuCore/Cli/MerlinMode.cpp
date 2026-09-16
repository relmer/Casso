#include "Pch.h"

#include "MerlinMode.h"
#include "ArtifactWriter.h"
#include "CommandLineParser.h"
#include "Cpu65C02Table.h"





////////////////////////////////////////////////////////////////////////////////
//
//  MerlinMode::CreateInstructionSetProvider
//
//  The 6502 to start on and the 65C02 to switch to. Merlin's `XC` is what
//  decides whether a source reaches the 65C02, and that decision happens
//  mid-assembly -- so both tables have to be in hand before the first line is
//  read. There is no command-line say in it, which is why `options` goes
//  unread.
//
////////////////////////////////////////////////////////////////////////////////

InstructionSetProvider MerlinMode::CreateInstructionSetProvider (const CommandLineOptions & options, const Cpu & cpu) const
{
    (void) options;

    return InstructionSetProvider (cpu.GetInstructionSet(), GetCpu65C02InstructionSet());
}





////////////////////////////////////////////////////////////////////////////////
//
//  MerlinMode::ResolveOutputName
//
//  What the object is called, once the flag and the source have both had their
//  say.
//
//  The precedence itself is NOT decided here: the assembler was handed the
//  caller's answer and reports the one in effect, so a name coming back is
//  already the winner. What is left is the case neither answered -- no flag and
//  no directive -- where the source's own name is the only thing to derive from.
//
////////////////////////////////////////////////////////////////////////////////

std::string MerlinMode::ResolveOutputName (const CommandLineOptions & options, const AssemblyResult & result) const
{
    std::string  name   = result.outputFileName;
    bool         wasSet = !name.empty();



    if (!wasSet)
    {
        name = CommandLineParser::StripExtension (options.inputFile) + ".bin";
    }

    return name;
}





////////////////////////////////////////////////////////////////////////////////
//
//  MerlinMode::WriteExtraArtifacts
//
//  The debug file `-g` asks for, named after the object it describes.
//
//  A SOURCE PRODUCING SEVERAL OBJECTS GETS SEVERAL DEBUG FILES, on the rule the
//  object and the listing already follow. One file indexed by address cannot
//  describe two outputs that both begin at $0300: the entries collide and one
//  symbol wins silently. Merlin's `SAV` is what cuts a source that way, so this
//  is the dialect where the rule earns its keep rather than a loop that never
//  runs twice.
//
//  The name comes from the OBJECT rather than from the source, because that is
//  the file a reader has in hand when it goes looking for symbols.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT MerlinMode::WriteExtraArtifacts (const CommandLineOptions & options, const AssemblyResult & result) const
{
    HRESULT  hr       = S_OK;
    bool     isSingle = result.savePoints.size() <= 1;



    BAIL_OUT_IF (!options.debugInfo, S_OK);

    BAIL_OUT_IF (isSingle, ArtifactWriter::WriteDebugInfo (
                               result,
                               ArtifactWriter::ResolveArtifactName (ResolveOutputName (options, result), ".dbg")));

    for (size_t i = 0; i < result.savePoints.size(); i++)
    {
        AssemblyResult      one    = ArtifactWriter::ForOutput (result, i);
        const std::string & given  = result.savePoints[i].name;
        std::string         object = given.empty() ? ResolveOutputName (options, result) : given;

        hr = ArtifactWriter::WriteDebugInfo (one, ArtifactWriter::ResolveArtifactName (object, ".dbg"));
        CHR (hr);
    }

Error:
    return hr;
}
