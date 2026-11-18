#include "Pch.h"

#include "Core/TextEncoding.h"
#include "Devices/BlockDevice/BlockDeviceCatalog.h"
#include "Ui/Settings/SettingsPanelState.h"
#include "Ui/Settings/SlotCards.h"




static constexpr size_t  s_kScsiDefaultId = 6;





////////////////////////////////////////////////////////////////////////////////
//
//  GetChoices
//
////////////////////////////////////////////////////////////////////////////////

std::vector<SlotCardChoice> SlotCards::GetChoices (const std::string & machineId)
{
    std::vector<SlotCardChoice>  out;
    SlotCardChoice               none;



    none.label = L"No card";
    out.push_back (none);

    for (const BlockCardSpec & card : BlockDeviceCatalog::GetCards())
    {
        SlotCardChoice  choice;



        choice.device  = card.device;
        choice.label   = TextEncoding::Utf8ToWide (card.label);
        choice.allowed = BlockDeviceCatalog::CanHost (card, machineId);

        if (!choice.allowed)
        {
            choice.tip = TextEncoding::Utf8ToWide (std::format ("The {} works in the {}.", card.label, BlockDeviceCatalog::GetHostList (card)));
        }

        out.push_back (std::move (choice));
    }

    return out;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FindSlot
//
////////////////////////////////////////////////////////////////////////////////

int SlotCards::FindSlot (const std::vector<HardwareEntry> & entries, int slot)
{
    for (size_t i = 0; i < entries.size(); i++)
    {
        if (entries[i].kind == HardwareEntryKind::Slot && entries[i].slot == slot && !entries[i].type.empty())
        {
            return (int) i;
        }
    }

    return -1;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SetCard
//
//  A replaced or new card has its default drives and no images.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT SlotCards::SetCard (std::vector<HardwareEntry> & entries, const std::string & machineId,
                            int slot, const std::string & device, std::wstring & outError)
{
    HRESULT                 hr      = S_OK;
    const BlockCardSpec   * card    = BlockDeviceCatalog::FindCard (device);
    int                     index   = FindSlot (entries, slot);
    bool                    canHost = false;
    HardwareEntry           entry;



    if (device.empty())
    {
        if (index >= 0)
        {
            entries[(size_t) index].type.clear();
            entries[(size_t) index].displayName.clear();
            entries[(size_t) index].ports.clear();
            entries[(size_t) index].portImages.clear();
            entries[(size_t) index].portRealTiming.clear();
        }

        BAIL_OUT_IF (true, S_OK);
    }

    CBREx (card != nullptr, HRESULT_FROM_WIN32 (ERROR_NOT_FOUND));

    canHost = BlockDeviceCatalog::CanHost (*card, machineId);

    if (!canHost)
    {
        outError = TextEncoding::Utf8ToWide (std::format ("The {} works in the {}.", card->label, BlockDeviceCatalog::GetHostList (*card)));
        CBREx (false, HRESULT_FROM_WIN32 (ERROR_NOT_SUPPORTED));
    }

    entry.kind      = HardwareEntryKind::Slot;
    entry.jsonIndex = (index >= 0) ? entries[(size_t) index].jsonIndex : -1;
    entry.slot      = slot;
    entry.enabled   = true;

    entry.type        = device;
    entry.displayName = MakeDisplayName (slot, card->label);
    entry.ports.assign ((size_t) card->portCount, std::string());

    if (std::string (card->device) == kpszAppleScsiCardDevice)
    {
        entry.ports[s_kScsiDefaultId] = kpszHd20scDevice;
    }
    else if (!card->drives.empty())
    {
        entry.ports[0] = card->drives[0];
    }

    entry.portImages.assign     (entry.ports.size(), std::string());
    entry.portRealTiming.assign (entry.ports.size(), false);

    if (index >= 0)
    {
        entries[(size_t) index] = std::move (entry);
    }
    else
    {
        entries.push_back (std::move (entry));
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SetPortImage
//
////////////////////////////////////////////////////////////////////////////////

HRESULT SlotCards::SetPortImage (std::vector<HardwareEntry> & entries, size_t index, size_t port, const std::string & path)
{
    HRESULT          hr      = S_OK;
    bool             inRange = index < entries.size() && port < entries[index].ports.size();
    bool             isDrive = false;
    HardwareEntry  * entry   = nullptr;



    CBR (inRange);

    entry   = &entries[index];
    isDrive = BlockDeviceCatalog::FindDrive (entry->ports[port]) != nullptr;
    CBR (isDrive);

    entry->portImages.resize     (entry->ports.size());
    entry->portRealTiming.resize (entry->ports.size(), false);
    entry->portImages[port] = path;

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  AddChainDrive
//
////////////////////////////////////////////////////////////////////////////////

HRESULT SlotCards::AddChainDrive (std::vector<HardwareEntry> & entries, size_t index, std::wstring & outError)
{
    HRESULT                 hr      = S_OK;
    const BlockCardSpec   * card    = nullptr;
    HardwareEntry         * entry   = nullptr;
    size_t                  used    = 0;
    bool                    inRange = index < entries.size();
    bool                    isChain = false;



    CBR (inRange);

    entry   = &entries[index];
    card    = BlockDeviceCatalog::FindCard (entry->type);
    isChain = card != nullptr && card->isChain && !card->drives.empty();
    CBR (isChain);

    used = (size_t) std::count_if (entry->ports.begin(), entry->ports.end(), [] (const std::string & port) { return !port.empty(); });

    if (used >= (size_t) card->portCount)
    {
        outError = TextEncoding::Utf8ToWide (std::format ("The {} takes at most {} drives.", card->label, card->portCount));
        CBREx (false, HRESULT_FROM_WIN32 (ERROR_TOO_MANY_NAMES));
    }

    std::erase (entry->ports, std::string());
    entry->ports.push_back (card->drives[0]);
    entry->portImages.resize     (entry->ports.size());
    entry->portRealTiming.resize (entry->ports.size(), false);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  RemoveChainDrive
//
////////////////////////////////////////////////////////////////////////////////

HRESULT SlotCards::RemoveChainDrive (std::vector<HardwareEntry> & entries, size_t index, size_t port)
{
    HRESULT          hr      = S_OK;
    bool             inRange = index < entries.size() && port < entries[index].ports.size();
    HardwareEntry  * entry   = nullptr;



    CBR (inRange);

    entry = &entries[index];
    entry->portImages.resize     (entry->ports.size());
    entry->portRealTiming.resize (entry->ports.size(), false);

    entry->ports.erase          (entry->ports.begin()          + (ptrdiff_t) port);
    entry->portImages.erase     (entry->portImages.begin()     + (ptrdiff_t) port);
    entry->portRealTiming.erase (entry->portRealTiming.begin() + (ptrdiff_t) port);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  MakeDisplayName
//
////////////////////////////////////////////////////////////////////////////////

std::string SlotCards::MakeDisplayName (int slot, const std::string & label)
{
    return std::string ("Slot ") + std::to_string (slot) + ": " + label;
}
