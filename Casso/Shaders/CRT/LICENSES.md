# Casso CRT Shaders — Third-Party Licenses

This directory contains HLSL ports of three upstream GLSL shaders from the
[libretro `glsl-shaders` collection](https://github.com/libretro/glsl-shaders),
pinned at collection SHA `42fa8a98ab19bdaffb53280746a30819eb21f807` (captured
2026-05-20). All ports are individually marked with an `// ATTRIBUTION:`
header in their corresponding `.hlsl` file.

| Casso file                | Upstream file                                  | Author(s)               | License      |
|---------------------------|------------------------------------------------|-------------------------|--------------|
| `scanlines.hlsl`          | `crt/shaders/crt-pi.glsl`                      | Davide Berra            | MIT          |
| `bloom_h.hlsl`            | `bloom/shaders/bloom.glsl`                     | Hyllian / hunterk       | CC0 / PD     |
| `bloom_v.hlsl`            | `bloom/shaders/bloom.glsl`                     | Hyllian / hunterk       | CC0 / PD     |
| `bloom_composite.hlsl`    | `bloom/shaders/bloom.glsl`                     | Hyllian / hunterk       | CC0 / PD     |
| `color_bleed.hlsl`        | `ntsc/shaders/ntsc-adaptive/ntsc-pass1.glsl`   | Themaister / hunterk    | MIT          |
| `brightness.hlsl`         | (original)                                     | Casso                   | n/a          |
| `copy.hlsl`               | (original)                                     | Casso                   | n/a          |

All third-party shaders are MIT or public-domain (CC0). `scripts/CheckShaderLicenses.ps1`
runs in pre-build to enforce that no GPL / copyleft strings leak into this
directory outside of designated `// ATTRIBUTION:` comment blocks.
