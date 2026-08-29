#pragma once

#include "Pch.h"

#include "AssemblerTypes.h"
#include "VolumeTypes.h"

//  Forward-declared rather than included, for the reason DiskCommandRunner's
//  header records: pulling the volume image in drags the disk image through
//  this header and into the console project, which does not share the core
//  library's precompiled conveniences.
enum class VolumeKind;





////////////////////////////////////////////////////////////////////////////////
//
//  AssembledFilePlacement
//
//  One of an assembly's outputs, as a file a volume can place.
//
//  THE LOAD ADDRESS COMES FROM THE SOURCE AND NOWHERE ELSE. The assembler knows
//  the origin, so it supplies it; there is no option that could disagree with
//  it, and removing that disagreement is the point of writing objects here
//  rather than placing them afterwards.
//
//  The conversion is always verbatim. An assembler emits bytes, and a character
//  conversion applied to an object corrupts it -- which is the one place this
//  path deliberately offers less than placing a host file does.
//
////////////////////////////////////////////////////////////////////////////////

class AssembledFilePlacement
{
public:

    //  Which type byte an output takes: the one the caller named, or the
    //  filesystem's own binary type when nobody named one.
    //
    //  AN UNRECOGNIZED NAME IS REFUSED rather than defaulted, for the reason
    //  placing a host file refuses one: filing an object under a type the
    //  caller did not ask for surfaces much later as a program that will not
    //  load, and by then nothing points back here.
    static HRESULT  ResolveType   (VolumeKind          kind,
                                   const std::string & typeName,
                                   Byte              & outType,
                                   std::string       & outError);

    //  The type a SOURCE asked for, which it states as a ProDOS type byte.
    //
    //  A TYPE WITH NO COUNTERPART IS REFUSED BY NAME rather than approximated.
    //  A ProDOS system file has no DOS 3.3 equivalent at all: the ProDOS kernel
    //  boots by finding a system-typed entry in the volume directory, and
    //  DOS 3.3 has no system-program concept to stand in for it. Mapping it to
    //  the nearest thing would file a program under a type that cannot start
    //  it, and nothing would say so until somebody tried to boot the disk.
    static HRESULT  ResolveSourceType (VolumeKind    kind,
                                       Byte          proDosType,
                                       Byte        & outType,
                                       std::string & outError);

    //  One output as a payload, with its type resolved and its address taken
    //  from the span.
    static HRESULT  BuildPayload  (const SavePoint   & span,
                                   VolumeKind          kind,
                                   const std::string & typeName,
                                   FilePayload       & outPayload,
                                   std::string       & outError);

private:

    //  The name as the tables below match it: upper case, and nothing else
    //  changed. Written out rather than reached for from a library, because the
    //  ones that exist are locale-sensitive and a type name is not.
    static std::string  ToUpper (const std::string & text);
};
