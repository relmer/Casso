# Contract: IFixtureProvider

Restricted file access for tests. The ONLY sanctioned path through which
test code may read binary fixture data.

## Header

```cpp
class IFixtureProvider {
public:
    virtual         ~IFixtureProvider () = default;
    virtual HRESULT  OpenFixture (const std::string  & relativePath,
                                  std::vector<uint8_t> & outBytes) = 0;
};
```

## Semantics

- `relativePath` is relative to `UnitTest/Fixtures/`.
- Implementation MUST reject any path containing `..`, drive letters, or
  absolute path roots — return E_INVALIDARG.
- Implementation MUST resolve the fixtures directory once at construction
  and cache it; subsequent calls do not re-discover the path.

## Concrete implementation

Single concrete class `FixtureProvider` lives in `UnitTest/EmuTests/`. The
fixtures directory is discovered via the build system: at build time, the
test runner sets a known macro (e.g. `CASSO_FIXTURES_DIR`) baked into the
test DLL pointing at `$(SolutionDir)UnitTest/Fixtures/`.

## Test obligations

`FixtureProviderTests` (small, dedicated):
- Reads a known fixture (e.g. a tiny `hello.txt` checked in for this purpose).
- Rejects `..` traversal.
- Rejects absolute paths.
- Same fixture read twice yields byte-identical results.
