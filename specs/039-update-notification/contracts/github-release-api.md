# Contract: GitHub release API (consumed)

Request: `GET https://api.github.com/repos/relmer/Casso/releases/latest`,
headers `User-Agent: Casso/<version>` and `Accept: application/vnd.github+json`.

Fields read (all others ignored):

```json
{
  "tag_name": "v1.31.0",
  "html_url": "https://github.com/relmer/Casso/releases/tag/v1.31.0",
  "prerelease": false,
  "published_at": "2026-10-20T18:04:11Z",
  "assets": [
    { "name": "Casso-1.31.0-x64.zip", "size": 1234567,
      "browser_download_url": "https://github.com/.../Casso-1.31.0-x64.zip",
      "digest": "sha256:<64 hex>" }
  ]
}
```

Failure mapping: no network or timeout → Network; 403/429 → RateLimited;
other non-200, missing `tag_name`, unparsable tag, or `prerelease: true` →
BadData.

Notes: `GET https://raw.githubusercontent.com/relmer/Casso/<tag>/CHANGELOG.md`
and `/README.md`.

Asset names (produced by ci.yml): `Casso-<ver>-x64.zip`,
`Casso-<ver>-ARM64.zip`, `Casso-<ver>.msixbundle`.
