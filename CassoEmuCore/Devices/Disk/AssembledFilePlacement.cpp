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
    HRESULT  hr   = S_OK;
    Byte     type = 0;



    hr = ResolveType (kind, typeName, type, outError);
    CHR (hr);

    outPayload.bytes          = span.bytes;
    outPayload.type           = type;
    outPayload.loadAddress    = span.loadAddress;
    outPayload.hasLoadAddress = span.hasLoadAddress;
    outPayload.encoding       = PayloadEncoding::Verbatim;

Error:
    return hr;
}
