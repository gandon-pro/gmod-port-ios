# 8BitDo Micro on iPhone

The Micro's **K** mode is a Bluetooth keyboard on iOS and is the manufacturer's supported path. The port also includes SDL mappings and a digital-button layout for **S/D** if iOS exposes either mode to the GameController framework. 8BitDo lists iOS compatibility for K mode only, so pairing S/D does not guarantee the app can see its input. All modes hide the on-screen action buttons when detected, like a DualSense. The touch look/move zones remain active under the hidden buttons: the Micro has no analogue sticks, so drag on the right half of the screen to look around.

## K mode: pair and map once

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

The game loads `gmp_micro.cfg` from `gmp_bindings.cfg` at startup. The app installs both files into `Documents/garrysmod/cfg` on each launch. To change this mapping permanently, edit the bundled `resources/gmp_micro.cfg` and rebuild; the installed copy is refreshed on each launch. Keep the key assignments on the Micro and in the game identical.

## S and D modes: conditional controller path

Switch to **S** or **D**, hold Pair for one second, and pair again in iPhone Bluetooth settings. These modes use `gmp_micro_gamepad.cfg`; Ultimate Software remapping applies only to K mode. If iOS exposes the pad as a GameController, SDL recognizes its known Micro reports and loads the following mapping. The app logs `Detected supported joystick` and `Touch: hardware controller or keyboard connected` in the game console. If the iPhone pairs it but no joystick is detected, iOS has not exposed S/D to the app; use K mode.

| Micro control | S/D game action |
| --- | --- |
| D-pad | Move |
| A / B / X / Y | Jump / crouch / reload / use |
| L / R | Previous / next weapon |
| L2 / R2 | Secondary / primary attack |
| Minus / Plus | Spawn menu / pause menu |
| Home | Context menu, if reported as a separate button |

The Star button is not assigned in S/D: the device uses it for Turbo in D mode, and the available SDL reports do not expose it as a distinct game button. In every mode, finger drag on the right half turns the camera. The action buttons should disappear on connection; tapping a hidden button area reveals them again. The touch look/move zones work while buttons are hidden. S/D recognition and button order still need a test with the physical Micro on this iPhone.

Manufacturer references: [Micro compatibility and modes](https://www.8bitdo.com/micro/), [pairing manual](https://download.8bitdo.com/Manual/Controller/Micro/Micro_Manual.pdf), [D-pad and Star FAQ](https://support.8bitdo.com/faq/micro.html).
