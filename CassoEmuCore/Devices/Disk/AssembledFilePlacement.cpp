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
//  THE ACCEPTED SET IS MERLIN'S, taken from its manual: "Valid file types are
//  0,6,$F0~$F7, and $FF (no type, BIN, CMD, user defined, SYS)". It accepts no
//  others and answers ILLEGAL FILE TYPE, so a byte outside the set is refused
//  here too rather than filed under something the source did not ask for.
//
//  FOUR OF THOSE WERE REFUSED FOR A RELEASE. Only $06 and $FF were recognized
//  out of Merlin's five, so a genuine Merlin source stating $00 or any of the
//  command types was answered "not one this tool recognizes" -- a port failing
//  on a construct the assembler being copied assembles, which is the whole
//  failure this feature exists to prevent.
//
//  $04 and $FC are OURS and go beyond Merlin, which lists neither. They cost
//  nothing: a text or Applesoft output is a thing a caller can want, both
//  filesystems have the type, and accepting them refuses no source Merlin
//  accepts. The extension runs in the safe direction, unlike the refusals above.
//
//  REACHING A DOS 3.3 VOLUME MEANS TRANSLATING, and only three of the set
//  translate. DOS 3.3 has five types and none of them means a system program, a
//  command file, or no type at all, so those are refused by name rather than
//  approximated. Any answer would be a guess filed under a type the source did
//  not state.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT AssembledFilePlacement::ResolveSourceType (VolumeKind    kind,
                                                   Byte          proDosType,
                                                   Byte        & outType,
                                                   std::string & outError)
{
    HRESULT       hr       = S_OK;
    bool          isDos    = kind == VolumeKind::Dos33;
    bool          isCmd    = (proDosType >= kProDosCommandFirst) && (proDosType <= kProDosCommandLast);
    bool          known    = true;
    bool          maps     = true;
    const char *  whatItIs = "";
    const char *  whyNot   = "";



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
        maps     = !isDos;
        whatItIs = "a ProDOS system file";
        whyNot   = "DOS 3.3 has no system-program concept at all";
    }
    else if (proDosType == kProDosNoType)
    {
        maps     = !isDos;
        whatItIs = "the ProDOS typeless file";
        whyNot   = "every DOS 3.3 file carries one of its five types";
    }
    else if (isCmd)
    {
        maps     = !isDos;
        whatItIs = "a ProDOS command or user-defined file";
        whyNot   = "DOS 3.3 has no type outside T, I, A, B and R";
    }
    else
    {
        known = false;
    }

    if (!known)
    {
        outError = std::format ("file type ${:02X} is not one this tool recognizes: it takes "
                                "$00, $06, $F0 through $F7 and $FF as Merlin does, "
                                "and $04 text and $FC Applesoft besides",
                                proDosType);
    }

    CBR (known);

    if (!maps)
    {
        outError = std::format ("file type ${:02X} is {} and DOS 3.3 has no equivalent: {}",
                                proDosType, whatItIs, whyNot);
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
