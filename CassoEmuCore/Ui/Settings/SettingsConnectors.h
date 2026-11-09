#pragma once

#include "Pch.h"

#include "Config/NetworkCardSettings.h"
#include "Core/MachineConfig.h"
#include "Machines/Apple2/Common/SscSwitches.h"

class ISettingsApplySink;
class SettingsPanelState;
struct HardwareEntry;
struct SettingsMachinePort;





////////////////////////////////////////////////////////////////////////////////
//
//  SettingsConnectors
//
//  The staged machine's connectors: what a //c serial port connects to,
//  where the ImageWriter II is attached, the Super Serial Card's switches
//  and attachment, and the Uthernet II's address, LAN access and port
//  forwards. Each change is staged on the panel state's current snapshot,
//  and a change staged here for a setting on another page sets a notice.
//
////////////////////////////////////////////////////////////////////////////////

class SettingsConnectors
{
public:
    explicit SettingsConnectors (SettingsPanelState & owner);

    // What a //c serial port, 1 or 2, connects to: its back-panel port's
    // device (see SerialPortDevice). A change is applied to the running
    // machine, with no reset.
    bool         HasSerialPorts      () const;
    std::string  GetSerialPortDevice (int port) const;
    void         SetSerialPortDevice (int port, const std::string & device);

    // Where the ImageWriter II is attached: nothing, or one of the staged
    // machine's connectors that can take it, which are each turned-on
    // parallel card and Super Serial Card, then a //c's ports 1 and 2. It is
    // on one connector at a time, so staging it on one, here or as a serial
    // port's or the card's device, empties the connector that had it. A
    // change is applied to the running machine, with no reset.
    //
    // A Super Serial Card the printer is staged on is set to printer mode,
    // and its previous mode is saved. When the printer moves off the card,
    // the saved mode is restored if the card is in printer mode then. A mode
    // change away from printer mode on a card with the printer disconnects
    // the printer.
    std::vector<PrinterConnection>  GetPrinterChoices    () const;
    PrinterConnection               GetPrinterConnection () const;
    void                            SetPrinterConnection (const PrinterConnection & connection);

    // The text of the last change staged on one page for a setting on
    // another, such as the card's mode, for the sheet to report; empty when
    // there is none. Taking it clears it.
    std::wstring  TakeNotice ();

    // The staged Super Serial Card: its index in GetHardware(), or -1 when
    // the machine lists none. HasEnabledSerialCard drives the Serial card
    // page's presence, as HasDiskIIController drives the Storage page's.
    int     FindSerialCard        () const;
    bool    HasEnabledSerialCard  () const;

    // Stage the card's switches, or what is attached to its connector
    // ("" for nothing, or one of the serial attachment devices) with the
    // host file the file attachments use.
    HRESULT SetSerialCardSwitches   (const std::string & sw1, const std::string & sw2);
    HRESULT SetSerialCardAttachment (const std::string & device, const std::string & path);

    const HardwareEntry *  GetNetworkCard        () const;
    bool                   HasEnabledNetworkCard () const;
    HRESULT                SetNetworkCardMac     (const std::string & text, std::string & outError);
    void                   SetNetworkLanAccess   (bool lanAccess);
    HRESULT                AddPortForward        (const PortForward & forward, std::string & outError);
    HRESULT                RemovePortForward     (size_t index);

    bool                AreSerialPortsChanged () const;


private:
    friend class SettingsPanelState;

    // The rule MachineConfig::FindPrinterConnection applies, to staged state.
    static PrinterConnection  FindPrinterConnection (const std::vector<HardwareEntry> & hardware, const std::vector<SettingsMachinePort> & machinePorts);

    void  StageSerialPortDevice (int port, const std::string & device);
    void  ApplyConnectors       (ISettingsApplySink & sink) const;

    // The staged card's switches, or the card's defaults where a bank is
    // unset or does not parse, and the switches staged back as text.
    static SscSwitches  ReadSwitches  (const HardwareEntry & entry);
    static void         WriteSwitches (HardwareEntry & entry, const SscSwitches & switches);

    static const wchar_t *  GetModeName (SscMode mode);

    HardwareEntry *  FindSlotEntry          (int slot, const char * type);
    void             SetCardPrinterMode     (int slot);
    void             RestoreCardMode        (int slot);
    void             StageCardPrinterChange (const PrinterConnection & from, const PrinterConnection & to);

    int   FindNetworkCard () const;

    static MacAddress  MakeRandomMac ();

    SettingsPanelState  & m_owner;
    std::wstring          m_notice;
};