# 0004 - Apple silicon only, built by CI, or on the user's Mac

- **Date:** 2026-11-08
- **Status:** Accepted. Noterator's 0012 and 0045, carried over.

## Decision

arm64, macOS 11 and later. `.github/workflows/build.yml` runs the tests on
Linux and builds, tests, ad-hoc signs and packages a `.dmg` on a macOS
runner; a `v*` tag publishes a Release. `build-mac.command` builds the app on
the user's own Mac and puts it in Applications.

## Consequences

- Unsigned for now: first launch is right-click > Open.
- If GitHub's Mac runners are unavailable (minutes run out), the
  `build-mac.command` route still works.
