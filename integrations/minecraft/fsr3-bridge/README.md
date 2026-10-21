# Historical in-tree FSR3 bridge

Active multi-version development moved to
[MobileGlues-FSR3-Bridge](https://github.com/BaiTian6641/MobileGlues-FSR3-Bridge)
on 2026-10-21. The standalone checkout is
`[workspace]/MobileGlues-FSR3-Bridge` in this workspace.

That repository has separate adapters and GitHub Actions artifacts for Minecraft
26.2 (Fabric/NeoForge), 1.21.1 (Fabric/NeoForge), 1.20.1 (Fabric/Forge), and 1.18.2
(Fabric/Forge). Its README records the exact Iris/Oculus dependencies and the
distinction between build/host verification and actual game qualification.

This directory is retained as the historical integration snapshot referenced by
earlier device reports. Its old JAR and device history do not qualify the changed
standalone JARs. Use the standalone build instructions for new development; do
not mix its generated adapters with this snapshot's classes.
