# Tango v0.9.2 release sync

The Steve branch merges the exact upstream `release/game-0.9.2` commit
`66c188cfea5a2404634f8e4f584478294fd11a5f`. The previous release base was
v0.9.1 (`bf42a2418d4396bdd8bb2d61a2cff6cf85c2b743`). Both histories are
preserved in the merge.

The release includes the title/intro scene and sprite transforms, menu and
pause input fixes, HUD corrections, and the 48 kHz audio/sound-mod updates.
Steve's host geometry, UI overlay, terrain filtering, health and actor hooks
are retained. No recovered ROM source or shared decompilation header is changed
relative to the upstream release. Runtime assets and the official Minecraft
1.18 server remain local and untracked.

`release-0.9.2-verification.json` records the executable version and hash, build
guards, native regression cases, block-entity checks, bomber results, audio
smoke checks, clean-runtime check and complete workspace byte count. Earlier
verification files retain the original v0.9.1 test history.
