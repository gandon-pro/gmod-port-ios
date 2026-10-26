# 8BitDo Micro on iPhone

The Micro is a Bluetooth **keyboard** on iOS. Its S/D modes are not the supported iOS gamepad path. The port keeps the touchscreen controls visible when K-mode keys are pressed; DualSense and other SDL gamepads keep their separate joystick bindings. The Micro has no analogue sticks, so use the right half of the screen to look around.

## Pair and map once

1. Turn the rear switch to **K**, power on with Home, hold Pair for one second, then connect **8BitDo Micro gamepad** in iPhone Bluetooth settings. Switching from S/D to K may require pairing again.
2. In 8BitDo Ultimate Software on the phone, assign **single keyboard keys** to the buttons below. Save the profile to the Micro. The stock K-mode key assignments can vary, so the game does not guess them.
3. If the D-pad does not send four separate directions, hold **Minus + D-pad Up** for five seconds to select D-pad control, then set its four keys to the arrow keys.

| Micro control | Key to assign | GMod action |
| --- | --- | --- |
| D-pad Up / Down / Left / Right | Up / Down / Left / Right Arrow | Move forward / backward / left / right |
| A | K | Jump |
| B | L | Crouch |
| X | R | Reload |
| Y | E | Use |
| L | U | Previous weapon |
| R | O | Next weapon |
| L2 | J | Secondary attack / Physgun freeze |
| R2 | H | Primary attack |
| Minus | Q | Spawn menu while held |
| Plus | P | Pause menu |
| Home | C | Context menu while held, if the firmware permits remapping Home |
| Star | T | Next Toolgun mode; Star needs an explicit K-mode assignment |

The game loads `gmp_micro.cfg` from `gmp_bindings.cfg` at startup. The app installs both files into `Documents/garrysmod/cfg` on each launch. You can edit that local config to change keys, but keep the key assignments on the Micro and in the game identical.

In game, verify that the D-pad moves while finger drag on the right half still turns the camera. Hold R2 to fire and L2 for secondary attack. The touch buttons should stay visible. A DualSense should still hide the action buttons and use its analogue sticks. This mapping has not yet been tested on the physical Micro.

Manufacturer references: [Micro compatibility and modes](https://www.8bitdo.com/micro/), [pairing manual](https://download.8bitdo.com/Manual/Controller/Micro/Micro_Manual.pdf), [D-pad and Star FAQ](https://support.8bitdo.com/faq/micro.html).
