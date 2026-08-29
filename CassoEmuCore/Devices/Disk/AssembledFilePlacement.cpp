#include "Pch.h"

#include "AssembledFilePlacement.h"

#include "Dos33Volume.h"
#include "ProDosVolume.h"
#include "VolumeImage.h"





////////////////////////////////////////////////////////////////////////////////
//
//  AssembledFilePlacement::ToUpper
//
////////////////////////////////////////////////////////////////////////////////

std::string AssembledFilePlacement::ToUpper (const std::string & text)
{
    std::string  upper = text;
    size_t       i     = 0;



    for (i = 0; i < upper.size(); i++)
    {
        if (upper[i] >= 'a' && upper[i] <= 'z')
        {
            upper[i] = (char) (upper[i] - 'a' + 'A');
        }
    }

    return upper;
}





////////////////////////////////////////////////////////////////////////////////
//
//  AssembledFilePlacement::ResolveType
//
//  The two filesystems number their types differently and name them
//  differently, so the forms accepted here are each one's own. A letter that
//  means one thing under DOS 3.3 and nothing under ProDOS must not resolve to
//  whatever ProDOS keeps at that number.
//
//  The default is binary, which is what an assembly produces.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT AssembledFilePlacement::ResolveType (VolumeKind          kind,
                                             const std::string & typeName,
                                             Byte              & outType,
                                             std::string       & outError)
{
    HRESULT      hr         = S_OK;
    bool         isDos      = kind == VolumeKind::Dos33;
    bool         named      = !typeName.empty();
    bool         recognized = true;
    std::string  form       = ToUpper (typeName);



    outType = isDos ? Dos33Volume::kTypeBinary : ProDosVolume::kTypeBinary;

    BAIL_OUT_IF (!named, S_OK);

    if (isDos)
    {
        if      (form == "T" || form == "TXT") { outType = Dos33Volume::kTypeText;        }
        else if (form == "I" || form == "INT") { outType = Dos33Volume::kTypeInteger;     }
        else if (form == "A" || form == "BAS") { outType = Dos33Volume::kTypeApplesoft;   }
        else if (form == "B" || form == "BIN") { outType = Dos33Volume::kTypeBinary;      }
        else if (form == "R" || form == "REL") { outType = Dos33Volume::kTypeRelocatable; }
        else                                   { recognized = false;                      }
    }
    else
    {
        if      (form == "T" || form == "TXT") { outType = ProDosVolume::kTypeText;   }
        else if (form == "B" || form == "BIN") { outType = ProDosVolume::kTypeBinary; }
        else if (form == "A" || form == "BAS") { outType = ProDosVolume::kTypeBasic;  }
        else if (form == "S" || form == "SYS") { outType = ProDosVolume::kTypeSystem; }
        else                                   { recognized = false;                  }
    }

    if (!recognized)
    {
        outError = typeName + " means nothing on this volume: "
                 + (isDos ? "DOS 3.3 takes T, I, A, B or R"
                          : "ProDOS takes TXT, BIN, BAS or SYS");
    }

    CBR (recognized);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  AssembledFilePlacement::ResolveSourceType
//
//  The type the source stated, as the target filesystem numbers it.
//
//  ProDOS types are what a source states, because the directive that states
//  them is a ProDOS-era one and names a ProDOS type. Reaching a DOS 3.3 volume
//  therefore means translating, and three of the four translate cleanly.
//
//  The fourth does not, and is refused rather than approximated. Nothing in
//  DOS 3.3 means what a system file means, so any answer would be a guess filed
//  under a type the source did not ask for.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT AssembledFilePlacement::ResolveSourceType (VolumeKind    kind,
                                                   Byte          proDosType,
                                                   Byte        & outType,
                                                   std::string & outError)
{
    HRESULT  hr    = S_OK;
    bool     isDos = kind == VolumeKind::Dos33;
    bool     known = true;
    bool     maps  = true;



    outType = proDosType;

    if (proDosType == ProDosVolume::kTypeText)
    {
        outType = isDos ? Dos33Volume::kTypeText : ProDosVolume::kTypeText;
    }
    else if (proDosType == ProDosVolume::kTypeBinary)
    {
        outType = isDos ? Dos33Volume::kTypeBinary : ProDosVolume::kTypeBinary;
    }
    else if (proDosType == ProDosVolume::kTypeBasic)
    {
        outType = isDos ? Dos33Volume::kTypeApplesoft : ProDosVolume::kTypeBasic;
    }
    else if (proDosType == ProDosVolume::kTypeSystem)
    {
        outType = ProDosVolume::kTypeSystem;
        maps    = !isDos;
    }
    else
    {
        known = false;
    }

    if (!known)
    {
        outError = std::format ("file type ${:02X} is not one this tool recognizes: "
                                "it takes $04 text, $06 binary, $FC Applesoft or $FF system",
                                proDosType);
    }

    CBR (known);

    if (!maps)
    {
        outError = std::format ("file type ${:02X} is a ProDOS system file and DOS 3.3 has no equivalent: "
                                "DOS 3.3 has no system-program concept at all",
                                proDosType);
    }

    CBR (maps);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  AssembledFilePlacement::BuildPayload
//
//  One output as a file the volume layer can place.
//
//  A SPAN WITH NO ADDRESS IS LEFT WITHOUT ONE rather than given a default. Zero
//  is a legal load address, so a default would be indistinguishable from an
//  answer, and the volume layer already refuses a binary that has none. That
//  refusal is the right one: a defaulted address is exactly the silent
//  disagreement writing objects here exists to remove.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT AssembledFilePlacement::BuildPayload (const SavePoint   & span,
                                              VolumeKind          kind,
                                              const std::string & typeName,
                                              FilePayload       & outPayload,
                                              std::string       & outError)
{
    HRESULT  hr         = S_OK;
    bool     flagNamed  = !typeName.empty();
    bool     fromSource = span.hasFileType && !flagNamed;
    Byte     type       = 0;



    //  The command line beats the source, which is the precedence the tool
    //  already applies to the output's name. It is settled here rather than in
    //  the parser for the same reason: this is the layer that sees both.
    hr = fromSource ? ResolveSourceType (kind, span.fileType, type, outError)
                    : ResolveType (kind, typeName, type, outError);
    CHR (hr);

    outPayload.bytes          = span.bytes;
    outPayload.type           = type;
    outPayload.loadAddress    = span.loadAddress;
    outPayload.hasLoadAddress = span.hasLoadAddress;
    outPayload.encoding       = PayloadEncoding::Verbatim;

Error:
    return hr;
}
