# AuroraLumen temporal-owner isolation

- Active original: `6e8e1942…` (restored byte-for-byte after the failed
  wrong-variant test).
- TAA-off private: `bddfa7aa…` — `TAA_DEFINE=0`.
- TAA+WSR-off private: `1ef96db5…` — TAA=0 and
  `WORLD_SPACE_REFLECTIONS=-1`.

Observed:

- TAA off: far-terrain ghost **reduced**.
- TAA+WSR off: **unchanged** relative to TAA-off.

So pack TAA contributes, but the remaining residue is not WSR. It is the
post-final FSR3 double-resample boundary. FSR3 history remains disabled.

The private test packs preserve the original archive. The original must be
reselecting before the next product run.
