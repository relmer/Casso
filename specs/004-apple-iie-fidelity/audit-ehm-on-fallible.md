# T128a — EHM-on-fallible-internals audit

**Spec**: 004-apple-iie-fidelity / Phase 16 / T128a
**Constitution**: §I 1.4.0 (expanded EHM rule) — every function that
contains fallible operations (`new`, EHM macros, Win32 calls that fail,
or any function documented to fail) must follow the EHM pattern internally
(`HRESULT hr = S_OK;` at top, `Error:` label before cleanup, single exit
via `return hr;`), even if the function itself returns `void` and no
HRESULT escapes.
**Audit date**: Phase 16

## Method

For every `.cpp` file changed in this feature, parse function definitions
and identify every function whose body contains any of:

- `new` allocation
- `CHR` / `CWR` / `CHRA` / `CWRA` / `CHRF` / `CWRF` / `CBR` / `CBRF` /
  `CBRN` / `CHRN` macro
- Known fallible Win32 calls: `CreateFileW`, `RegOpenKey*`,
  `CoCreateInstance`, `CoInitialize*`

For each such function, verify:
- If the function returns `HRESULT`: nothing to check (return type carries
  the contract).
- If the function returns `void` or any non-HRESULT type: confirm the body
  declares `HRESULT hr = S_OK;` and uses an `Error:` label with single-exit
  flat control flow.

Restricted to functions touched by this feature (i.e., function bodies
containing at least one line added by this feature).

## Result

| Category                                                       | Count |
|----------------------------------------------------------------|------:|
| Non-HRESULT functions with fallible ops missing internal EHM   |     0 |

## Verdict

**PASS** — 0 EHM-on-fallible-internal violations. Every non-HRESULT-
returning function added or modified by this feature that contains
fallible operations follows the internal EHM pattern. Examples verified:

- `DiskImageStore::AutoFlushAll()` — void return; uses internal `HRESULT
  hr = S_OK;` / `Error:` / `IGNORE_RETURN_VALUE` for any leaked HRESULT.
- `EmulatorShell::WireLanguageCard()` — void return; uses internal EHM.
- `EmulatorShell::HandleKeyDownCommand()` — void return; the `iieKbd`
  cast block contains no fallible ops, so no EHM is required.

For functions that return `HRESULT`, the contract is carried by the
return type and the audit is satisfied by inspection.
