#pragma once

#include "Pch.h"

#include "Seams/IHostFirewall.h"

class EmulatorShell;





////////////////////////////////////////////////////////////////////////////////
//
//  ShellNetworkCard
//
//  The Uthernet II's infobar: a port the running guest cannot listen on,
//  because the machine's serial connector or another program on the PC has
//  it, or because Windows Firewall would block it. The infobar is up until
//  the port opens or the user closes it, and a different problem raises it
//  again. A port the firewall would block is deferred rather than opened, so
//  no Windows Firewall prompt appears for it. The infobar's Allow admits it.
//
//  The guest listens on the CPU thread, so that thread notes the sentence
//  (NoteListenProblem) and the UI thread applies it on its next frame
//  (Tick), as ShellSerialCard does for the serial card's notice. The infobar
//  shares the chrome's change band, below the serial card's.
//
//  Tick also reads the firewall once a second for the shell's ListenGate,
//  which the card's deferred sockets wait on.
//
////////////////////////////////////////////////////////////////////////////////

class ShellNetworkCard
{
public:
    explicit ShellNetworkCard (EmulatorShell & shell);
    ~ShellNetworkCard ();

    //  The program in the firewall's text and in its helper's rules.
    static constexpr const wchar_t *  kFirewallTitle = L"Casso";

    // CPU thread: the sentence for the guest's failed and deferred listens,
    // empty when there are none, the part of it about the machine's serial
    // connector, and the ports deferred for the firewall.
    void  NoteListenProblem (const wstring & text, const wstring & serialText, const std::vector<FirewallPort> & deferred);

    // Any thread: the sentences last noted, for the Network and Serial card
    // settings pages.
    wstring  GetListenProblem       () const;
    wstring  GetSerialListenProblem () const;

    // UI thread, once per frame: applies a noted sentence.
    void  Tick ();

    DxuiInfoBanner &  GetBanner           () { return m_banner; }
    int               GetBandThicknessPx  (int clientWidthPx) const;
    void              LayoutBanner        (const RECT & bounds);
    bool              OfferMouseToBanner  (DxuiMouseEventKind kind, int x, int y);

    // UI thread: the firewall read again for the shell's ListenGate when it
    // may have changed, at most once a second unless `force`.
    void  ReadFirewall (bool force);

private:
    void  Show      (bool visible);
    void  OnAllow   ();

    static constexpr uint64_t  kFirewallReadMs = 1000;

    EmulatorShell              & m_shell;
    DxuiInfoBanner               m_banner;

    mutable mutex                m_pendingLock;
    wstring                      m_pending;
    wstring                      m_pendingSerial;
    std::vector<FirewallPort>    m_pendingDeferred;
    atomic<uint32_t>             m_pendingGeneration = 0;
    uint32_t                     m_appliedGeneration = 0;

    wstring                    m_shown;   // the sentence the infobar has, closed or not
    std::vector<FirewallPort>  m_deferred;   // the ports the infobar's Allow admits
    HANDLE                     m_firewallHelper      = nullptr;
    uint64_t                   m_nextFirewallReadMs  = 0;
    uint32_t                   m_gateFirewallVersion = UINT32_MAX;   // the monitor's version the gate has
};
