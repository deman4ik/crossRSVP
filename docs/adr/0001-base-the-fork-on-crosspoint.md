---
status: accepted
---

# Base the fork on CrossPoint develop

CrossRSVP forks `crosspoint-reader/crosspoint-reader`'s `develop` branch at commit `7bcc3a692231445f870ea524bea0e14cd0a05300` rather than using `uxjulia/CrossInk`. CrossPoint already provides the required Russian text support and reader boundaries while keeping the long-lived fork closer to the active upstream; CrossInk offers useful reader features but adds a larger downstream delta and does not accept pull requests. The fork will remain independently releasable and keep its RSVP changes modular so that upstream updates can be integrated with limited conflict.

## Considered Options

- `crosspoint-reader/crosspoint-reader:develop`: selected for upstream proximity, active hardware support, and lower maintenance cost.
- `uxjulia/CrossInk:v1.5.0`: rejected as the base because its additional reader features are not required for the EPUB-only RSVP MVP and increase the rebase surface.

## Consequences

The initial hardware acceptance target is Xteink X3. RSVP concepts may be adapted from `jmeboch/CrossInk-RSVP`, but its unconditional replacement of Paged Mode and byte-oriented text handling will not be copied.
