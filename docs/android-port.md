# Android port notes

[CHOSEN, not recovered] Work towards a 64-bit, non-Windows client.

- `Core/X87Emu.h` and the `#else` arms beside the client's `__asm` blocks replace inline x87.
  Intermediates are double, not 80-bit extended.
- `Core/PortableClient.h` compiles out retail-layout `static_assert`s in the portable client.
  Code that uses retail byte offsets must still be audited, starting with `Menu/HudVehicleFrontiers.cpp`,
  `Menu/HudScoreboard.cpp`, `Menu/HudSquad.cpp`, `Menu/Bf2MenuMapNode.cpp` and `Menu/ISwiffHost.h`.
- `PortShim/` carries only Win32 type names. Window, input and plugin loading need an SDL layer.
