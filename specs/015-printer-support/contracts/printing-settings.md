# Contract: Printing Settings, Machine Config, Persistence Sidecar

## GlobalUserPrefs additions (global — host print services, FR-011)

```json
{
  "printDestination": "pngFile",        // "pngFile" | "windowsPrinter"
  "printPngFolder": "C:/Users/.../Pictures/Casso Prints",
  "printOutputDpi": 576,                 // 288 | 576 (FR-028)
  "printDotStyle": "ink",               // "ink" | "plain" (FR-027)
  "printerAudioMuted": false,
  "printerAudioVolume": 0.6
}
```

Surfaced on Settings → Printing (new `PrintingPage`, FR-011). Unknown fields
preserved on round-trip per existing prefs behavior.

## Machine config slot entry (per machine, FR-001)

```json
{ "slot": 1, "device": "parallel-printer", "capability": "optional" }
```

- Present in embedded `Machines/Apple2e*.json` defaults.
- `MachineConfigUpgrade`: adds the entry to existing configs when slot 1 has
  no entry; never overwrites an occupied slot; never resurrects a slot the
  user disabled (`enabled: false` honored as today).
- No `rom` field: firmware bytes are embedded (contract printer-card-io.md).

## Pending-strip persistence (per machine, FR-026)

Location: `<per-machine user state>/PendingPrint/`

- `strip.png` — native-grid raster, indexed-color PNG (lossless; ink
  bitfield values 0-15 as palette indices).
- `strip.json`:

```json
{
  "formatVersion": 1,
  "rowsUsed": 4212,
  "paperRow": 4212,
  "pageBoundaryRows": [1584, 3168],
  "capReached": false
}
```

Rules: load at machine open (missing/corrupt/newer version → empty paper,
silent); write on clean exit, eject, discard; delete directory contents on
eject/discard success. Renderer settings are deliberately NOT persisted here
— the native grid is the source of truth and re-renders under current
settings (research R-010).

## Recognition signature table (embedded, FR-022/023/025)

Constant table entries:

```cpp
{ id, displayName, filenameSubstrings[], metaTitles[], catalogNames[] }
```

Initial population: Print Shop family (Print Shop, Companion, New Print
Shop), PrintMaster/+, Print Magic, Bannermania, Newsroom, Certificate Maker,
Children's Writing & Publishing Center, SuperPrint!, Dazzle Draw, AppleWorks,
Apple Writer, Bank Street Writer, FrEdWriter. Exact strings tuned during
implementation against real image names/catalogs.
