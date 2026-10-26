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
| Heart / Home | Leave unassigned | Power/system button; do not use for camera |
| Star | T | Hold with D-pad to turn camera; tap to select next Toolgun mode |

The game loads `gmp_micro.cfg` from `gmp_bindings.cfg` at startup. The app installs both files into `Documents/garrysmod/cfg` on each launch. To change this mapping permanently, edit the bundled `resources/gmp_micro.cfg` and rebuild; the installed copy is refreshed on each launch. Keep the key assignments on the Micro and in the game identical.

## S and D modes: conditional controller path

Switch to **S** or **D**, hold Pair for one second, and pair again in iPhone Bluetooth settings. These modes use `gmp_micro_gamepad.cfg`; Ultimate Software remapping applies only to K mode. If iOS exposes the pad as a GameController, SDL recognizes its known Micro reports and loads the following mapping. The app logs `Detected supported joystick` and `Touch: hardware controller or keyboard connected` in the game console. If the iPhone pairs it but no joystick is detected, iOS has not exposed S/D to the app; use K mode.

| Micro control | S/D game action |
| --- | --- |
| D-pad | Move |
| A / B / X / Y | Jump / crouch / reload / use |
| L / R | Hold L with D-pad to turn camera; tap L for previous weapon / R for next weapon |
| L2 / R2 | Secondary / primary attack |
| Minus / Plus | Spawn menu / pause menu |
| Heart / Home | Unbound in game; iOS may open Apple Arcade |
| Star | No separate game input in the known S/D reports |

With the camera modifier released, the D-pad moves. In K mode, assign **Star = T** in Ultimate Software and hold Star with the D-pad to turn the camera. In S/D, hold **L + D-pad**; the Star button is used internally by the controller and does not appear as a separate button in the known SDL reports. The Heart button can launch Apple Arcade before the game receives it, so no in-game binding can suppress that system action. D-pad left/right turns, up/down looks up/down. D-pad movement stops while the modifier is held; touch movement remains available. Camera speed can be changed in the console with `gmp_micro_look_yaw` (default 180 degrees/second) and `gmp_micro_look_pitch` (default 140 degrees/second).

In every mode, finger drag on the right half also turns the camera. The action buttons should disappear on connection; tapping a hidden button area reveals them again. The touch look/move zones work while buttons are hidden. S/D recognition and button order still need a test with the physical Micro on this iPhone.

Manufacturer references: [Micro compatibility and modes](https://www.8bitdo.com/micro/), [pairing manual](https://download.8bitdo.com/Manual/Controller/Micro/Micro_Manual.pdf), [D-pad and Star FAQ](https://support.8bitdo.com/faq/micro.html).
