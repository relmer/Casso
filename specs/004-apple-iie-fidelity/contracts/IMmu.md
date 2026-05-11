# Contract: IMmu (AppleIIeMmu)

Owns //e MMU routing state and rebinds the `MemoryBus` page table on every
banking-changed event. Single source of truth for RAMRD, RAMWRT, ALTZP,
80STORE, INTCXROM, SLOTC3ROM, INTC8ROM.

## Header (selected)

```cpp
class AppleIIeMmu : public IBankingObserver {
public:
    HRESULT  Initialize (MemoryBus * bus, RamDevice * mainRam,
                         RamDevice * auxRam, RomDevice * internalRom,
                         LanguageCard * lc, AppleSoftSwitchBank * ssBank);

    void     SetRamRd       (bool v);
    void     SetRamWrt      (bool v);
    void     SetAltZp       (bool v);
    void     Set80Store     (bool v);
    void     SetIntCxRom    (bool v);
    void     SetSlotC3Rom   (bool v);
    void     ResetIntC8Rom  ();

    bool     GetRamRd       () const;
    bool     GetRamWrt      () const;
    bool     GetAltZp       () const;
    bool     Get80Store     () const;
    bool     GetIntCxRom    () const;
    bool     GetSlotC3Rom   () const;

    void     OnSoftSwitchChanged () override;   // PAGE2 / HIRES / TEXT / MIXED
    void     OnSoftReset    ();
    void     OnPowerCycle   ();
};
```

## Semantics

- Every setter calls `RebindPageTable()` for affected regions only.
- `OnSoftSwitchChanged` is invoked by the soft-switch bank when display flags
  change so 80STORE-routed regions can be re-resolved.
- INTC8ROM auto-disables on read of $CFFF (per audit §8); modeled by
  `ResetIntC8Rom` invoked from the ROM mapper read path.

## $C011-$C01F status read sourcing

| Address | Bit 7 source |
|---|---|
| $C013 RDRAMRD | `GetRamRd()` |
| $C014 RDRAMWRT | `GetRamWrt()` |
| $C015 RDINTCXROM | `GetIntCxRom()` |
| $C016 RDALTZP | `GetAltZp()` |
| $C017 RDSLOTC3ROM | `GetSlotC3Rom()` |
| $C018 RD80STORE | `Get80Store()` |

## Test obligations

`MmuTests`:
- RAMRD routes $0200-$BFFF reads to aux.
- RAMWRT routes $0200-$BFFF writes to aux.
- ALTZP routes $0000-$01FF.
- 80STORE+PAGE2 routes $0400-$07FF text writes (independent of RAMRD/RAMWRT).
- 80STORE+HIRES+PAGE2 routes $2000-$3FFF (independent of RAMRD/RAMWRT).
- INTCXROM masks slot ROMs at $C100-$CFFF.
- SLOTC3ROM=0 maps //e firmware at $C300; =1 maps slot 3 card ROM.
- INTC8ROM disables on read of $CFFF.
- Soft reset resets per //e MMU rules (RAM contents preserved).
- Power cycle resets to power-on defaults + re-seeds RAM.

## Replaces

`AuxRamCard` is **deleted**. The buggy $C003-$C006 mapping is removed.
