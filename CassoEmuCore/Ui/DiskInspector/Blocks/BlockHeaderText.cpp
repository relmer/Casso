#include "Pch.h"

#include "Devices/Disk/Inspector/Blocks/BlockFieldList.h"
#include "Devices/Disk/Inspector/Blocks/BlockInspectorModel.h"
#include "Ui/DiskInspector/Blocks/BlockHeaderText.h"
#include "Ui/DiskInspector/Blocks/BlockHexSource.h"
#include "Core/NumberFormat.h"





////////////////////////////////////////////////////////////////////////////////
//
//  BlockHeaderText::BuildCapsules
//
//  A damaged role's capsule is drawn red.
//
////////////////////////////////////////////////////////////////////////////////

vector<BlockCapsule> BlockHeaderText::BuildCapsules (const BlockInspectorModel & model, uint32_t block)
{
    SectorRole            role     = model.GetRole (block);
    LPCWSTR               label    = model.GetMapLabel (block);
    bool                  isFree   = role == SectorRole::Free && label == nullptr;
    bool                  isBad    = role == SectorRole::AllocatedUnowned || role == SectorRole::OwnedMarkedFree || role == SectorRole::CrossLinked;
    std::wstring          owners   = model.GetOwners (block);
    size_t                start    = 0;
    size_t                end      = 0;
    vector<BlockCapsule>  capsules;



    capsules.push_back ({ isFree ? L"Free" : L"Used", false });

    if (!isFree)
    {
        capsules.push_back ({ label != nullptr ? std::wstring (label) : isBad ? BlockMap::FormatBlockFinding (role) : std::wstring (BlockMap::GetRoleName (role)), isBad });
    }

    while (start < owners.size())
    {
        end = owners.find (L'\n', start);
        end = end == std::wstring::npos ? owners.size() : end;

        capsules.push_back ({ owners.substr (start, end - start), false });
        start = end + 1;
    }

    return capsules;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockHeaderText::BuildFieldLine
//
////////////////////////////////////////////////////////////////////////////////

std::wstring BlockHeaderText::BuildFieldLine (const BlockInspectorModel & model, uint32_t block)
{
    BlockInspectorModel::Tags  tags   = {};
    size_t                     differ = model.IsComparing() ? model.CountDifferent() : 0;
    vector<std::wstring>       parts;
    wchar_t                    hex[4] = {};
    std::wstring               tagText;
    std::wstring               line;



    if (model.TryGetTags (block, tags))
    {
        for (Byte tag : tags)
        {
            swprintf_s (hex, L" %02X", tag);
            tagText += hex;
        }

        parts.push_back (L"Tag bytes:" + tagText);
    }

    if (model.IsComparing())
    {
        parts.push_back ((model.IsDifferent (block) ? L"Differs from " : L"Same as ") + GetOtherName (model)
                         + (differ == 0 ? std::wstring (L" (no blocks differ)") : differ == 1 ? std::wstring (L" (1 block differs)") : std::format (L" ({} blocks differ)", NumberFormat::FormatInteger (differ))));
    }

    for (const std::wstring & part : parts)
    {
        line += (line.empty() ? L"" : L"    ") + part;
    }

    return line;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockHeaderText::BuildByteTip
//
////////////////////////////////////////////////////////////////////////////////

std::wstring BlockHeaderText::BuildByteTip (uint64_t offset, uint8_t mark, const BlockField * field, const std::wstring & otherName, int & outNoteLine)
{
    std::wstring  text = std::format (L"Byte ${:03X}", offset);
    std::wstring  note;



    if (field != nullptr)
    {
        text += L"\n" + field->label + (field->value.empty() ? L"" : L": " + field->value);
    }

    note = mark == BlockHexSource::kMarkLink    ? (field != nullptr && field->link != BlockField::kNoLink ? L"Links to block " + NumberFormat::FormatInteger (field->link) + L". Click to open it." : L"Links to another block")
         : mark == BlockHexSource::kMarkChanged ? L"Changed, not yet written"
         : mark == BlockHexSource::kMarkDiffers ? L"Differs from " + otherName
         : mark == BlockHexSource::kMarkField   ? L"In the field under the pointer in the field list"
         :                                        L"";

    outNoteLine = -1;

    if (!note.empty())
    {
        text        += L"\n" + note;
        outNoteLine  = (int) std::count (text.begin(), text.end(), L'\n');
    }

    return text;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockHeaderText::BuildBlockText
//
////////////////////////////////////////////////////////////////////////////////

std::wstring BlockHeaderText::BuildBlockText (std::span<const Byte> bytes, size_t bytesPerRow)
{
    constexpr Byte  kHighBit    = 0x80;
    constexpr Byte  kFirstGlyph = 0x20;
    constexpr Byte  kLastGlyph  = 0x7E;



    std::wstring  text;
    size_t        i    = 0;
    size_t        j    = 0;
    Byte          ch   = 0;



    for (i = 0; i + bytesPerRow <= bytes.size(); i += bytesPerRow)
    {
        text += std::format (L"{:04X} ", i);

        for (j = 0; j < bytesPerRow; j++)
        {
            text += std::format (L" {:02X}", bytes[i + j]);
        }

        text += L"  ";

        for (j = 0; j < bytesPerRow; j++)
        {
            ch    = (Byte) (bytes[i + j] & ~kHighBit);
            text += (ch >= kFirstGlyph && ch <= kLastGlyph) ? (wchar_t) ch : L'.';
        }

        text += L"\r\n";
    }

    return text;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockHeaderText::GetOtherName
//
////////////////////////////////////////////////////////////////////////////////

std::wstring BlockHeaderText::GetOtherName (const BlockInspectorModel & model)
{
    return model.IsComparing() ? fs::path (model.GetOther()->GetPath()).filename().wstring() : std::wstring();
}
