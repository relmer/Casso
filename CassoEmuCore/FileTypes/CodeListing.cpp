#include "Pch.h"

#include "FileTypes/CodeListing.h"
#include "CassoExplorer/Model/PreviewDecoder.h"
#include "Machines/Apple2/Common/VolumeImage.h"





////////////////////////////////////////////////////////////////////////////////
//
//  CodeListing::Decode
//
//  An empty file has no code to list.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT CodeListing::Decode (std::span<const Byte> bytes, const FileFacts & facts, const DecodeOptions & options, DecodedFile & outFile, std::wstring & outError)
{
    HRESULT  hr          = S_OK;
    bool     relocatable = facts.HasProDosType (kTypeRelocatable);
    Word     origin      = PreviewDecoder::GetCodeOrigin (facts.type, facts.volume, facts.hasLoadAddress, facts.loadAddress, facts.hasAuxType, facts.auxType);



    CBRFEx (!bytes.empty(), HRESULT_FROM_WIN32 (ERROR_INVALID_DATA), outError = L"The file is empty.");

    outFile.kind = DecodedFile::Kind::Lines;

    PreviewDecoder::RenderDisassembly (bytes, origin, facts.volume, relocatable, outFile.lines,
                                       (options.disassembly == DisassemblyCpu::None) ? DisassemblyCpu::Mos6502Undocumented : options.disassembly,
                                       options.longRegisters, options.followFlags);

Error:
    return hr;
}
