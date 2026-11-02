# Transit kiosk implementation

## Project inspection

- Project association: Unreal Engine 5.8; installed version **5.8.2**, CL 56702186, at `E:\Unreal\UE_5.8`.
- Startup/game map: `/Game/Kiosk/Maps/Demo`.
- Game mode: `/Game/FirstPerson/Blueprints/BP_FirstPersonGameMode`, using the existing first-person character/controller subclasses.
- Actual widgets: `/Game/Kiosk/UI/KioskUI` and `/Game/Kiosk/UI/InteractPrompt`. There are no assets with the requested `WBP_` prefix.
- `KioskUI` originally contained six editable pages under `WS_Pages`, with the expected Welcome, Language, Main Menu, Ticket Type, Route and Review layouts. The graph contained unconnected template events; no button navigation had been implemented. Route dropdowns and transaction summary text fields already existed.
- `/Game/Kiosk/Blueprint/BP_TransitKiosk` originally inherited from `Actor`, with `DefaultSceneRoot`, `MachineBase`, `ScreenFrame`, `ScreenDisplay`, `PaymentReader` and `HelpButton`. Its graph also contained unconnected template events. The C++ `TransitKioskActor` was an unused stub.
- `/Game/Kiosk/Input/IA_Interact` was already mapped to **E** in `/Game/Input/IMC_Default`. Existing movement, mouse look and touch mappings are reused.
- Python automation could load the assets but did not expose their widget trees or graphs. A native editor commandlet was therefore used to edit real Widget Blueprints through Unreal's supported C++ widget-tree, Blueprint compiler and package serialization APIs.

The original read-only native inspection is in `Saved/KioskInspection.txt`. Original versions of the two edited assets are in `Saved/KioskBackup/Content/Kiosk/`. Keep that backup if you move the project; `Saved` is not tracked by Git.

## Implementation

The original six pages and their child controls remain in the same Widget Blueprint. Pages 6–9 and the modal are ordinary editable UMG widgets:

| Index | Page | Behavior |
| --- | --- | --- |
| 0 | Welcome | Start or exit |
| 1 | Language | Select English, Chinese, Malay or Tamil |
| 2 | Main Menu | Purchase or top up |
| 3 | Ticket Type | One-way, round-trip or day pass |
| 4 | Route | Select distinct origin/destination; preview fare |
| 5 | Review | Ticket/top-up details, price, validity and Back |
| 6 | Top-Up Selection | Three configurable amounts and current demo balance |
| 7 | Payment | Explicit simulated card tap; Back to Review |
| 8 | Processing | Timed feedback; repeated taps cannot charge twice |
| 9 | Success | Receipt, route/validity or credited card balance; return to Welcome |
| Modal | Staff Assistance | Simulated request and arrival; retains transaction selections |

The complete original canvas is inside a 1280×720 design surface and Scale Box, retaining its aspect ratio at 1920×1080. Added widgets follow the existing light blue/dark blue palette, white choices, green navigation and large text. Help remains the existing button; Exit is available throughout. Escape dismisses Help, or exits the kiosk when Help is closed.

### C++ responsibilities

- `TransitKioskActor`: configurable demo settings and proximity/visibility checks. It creates no replacement meshes or materials. The existing Blueprint now inherits this class.
- `TransitKioskSession`: guarded transaction state, integer-cent fares, route validation, local-midnight day-pass expiry and receipt generation. No external payment SDK or service is used.
- `TransitKioskWidget`: native parent of `KioskUI`; binds existing and added buttons, populates station choices, updates summaries and manages timers/modal behavior.
- `TransitPlayerController`: reuses the existing Enhanced Input action. Shows the existing prompt near the kiosk, opens one widget, captures UI input and restores game input/cursor on close. Demo card credit persists for the local controller's play session. Hard asset references keep the UI and action discoverable by the cooker.
- `TransitEditor`: editor-only module with the `TransitKiosk` commandlet for inspecting, extending, verifying and rendering the assets. It is excluded from the standalone game target.
- `Tests/TransitKioskTests`: transaction regression tests available as `Transit.Kiosk.Transactions`.
- `Tests/TransitKioskSmokeTest`: opt-in standalone gameplay check (`-KioskSmokeTest`); inactive during normal play and excluded from Shipping.

No map, physical mesh, material, first-person Blueprint, prompt asset or input mapping is intentionally changed. Only `KioskUI.uasset` and `BP_TransitKiosk.uasset` need content changes.

## Configuration

Select the existing kiosk in the level, then expand **Kiosk → Demo Config** in Details (or change defaults in `BP_TransitKiosk`). Defaults:

- Stations: Central, Museum, Harbour, Airport, in line order.
- One-way: SGD 2.50 for the first stop, plus SGD 0.50 per additional stop.
- Round-trip: twice the one-way fare. Central–Airport is SGD 3.50 one-way / SGD 7.00 round-trip.
- Day pass: SGD 10.00, unlimited demonstration travel until the next local midnight. The confirmation includes the exact expiry date/time. Only one day pass can be purchased per local player's play session; returning to Welcome and closing/reopening the kiosk preserve ownership. Starting a new Play session clears it.
- Top-up choices: SGD 5.00 / 10.00 / 20.00, starting demo balance SGD 7.50.
- Payment processing delay: 2 seconds; interaction distance: 250 cm with a visibility check.

Top-ups affect only the demo card. Payments and staff requests are simulations. Credit is not saved across stopping/restarting Play. Returning to Welcome resets transaction/language selections while retaining completed top-ups and day-pass ownership. Closing during Processing cancels its pending timer and adds no credit. Closing Staff Assistance (Close or Escape) cancels any pending staff timer and resets the request, allowing another request immediately when Help is reopened.

Language selection records and displays the chosen language. **The demonstration interface remains English; full interface translations are not supplied.** Existing language labels and the Tamil font are preserved.

## Automated commands

Close an open Editor before rebuilding or changing assets. From the project directory:

```powershell
.\Scripts\kiosk.ps1 -Mode Build
.\Scripts\kiosk.ps1 -Mode Inspect
.\Scripts\kiosk.ps1 -Mode Apply
.\Scripts\kiosk.ps1 -Mode Verify
.\Scripts\kiosk.ps1 -Mode Gameplay
```

`Apply` is for regenerating/integrating the extension; it preserves pages 0–5 and does not append duplicates when there are already ten pages. It keeps the first asset backups. The completed project does not require running it again. Build uses the installed Visual Studio/Windows SDK. Rendering needs access to Unreal's shader cache directories and can take longer on its first run.

Verification writes `Saved/KioskVerification.txt`, images under `Saved/KioskPreview`, and logs under `Saved/Logs`. Gameplay checking writes `Saved/KioskGameplayVerification.txt`. The commandlet invokes real UMG button delegates; the gameplay check additionally drives the existing E input action and verifies character movement after closing. Gameplay checks default to Null RHI (headless); add `-RenderGameplay` for an offscreen DirectX 11 run, which can require lengthy first-run scene shader compilation.

## Editor steps

1. Open `Transit.uproject` in Unreal Engine 5.8.2. The `Demo` map should open automatically; otherwise open `/Game/Kiosk/Maps/Demo`.
2. Click **Play**, walk to the existing kiosk (approximately X=-880, Y=-310), and press **E** when prompted. Use the on-screen controls; **Exit** or **Escape** returns to the character.
3. To edit layouts, open `/Game/Kiosk/UI/KioskUI`, expand `WS_Pages`, and select the desired page. The new pages and `StaffOverlay` can be edited in Designer. No manual widget construction, input mapping or actor replacement is needed.

## Validated results

- Unreal Engine 5.8.2 **Development Editor / Win64** and **Development Game / Win64** builds succeeded.
- The saved Widget Blueprint and kiosk Blueprint reloaded and compiled successfully. Repeated `Apply` runs retained exactly ten pages, without duplicate controls or input mappings.
- **263 verification checks passed, zero failed**, including all ticket/top-up forward and Back paths, four language choices, repeat day-pass rejection, repeated staff requests, processing completion, cancelled timer behavior, saved page order and existing kiosk component names. This includes the transaction regression suite (fare values, invalid routes/amounts, repeated payments, retained ownership, reset behavior and midnight/year rollover), plus arranged header bounds on all ten pages at both display resolutions.
- All ten pages plus the staff modal were rendered using Unreal's DirectX 11 UMG renderer at **1280×720 and 1920×1080**. Layout review corrected long button labels, ticket option headings and confirmation date formatting.
- Expanded Origin and Destination menus were also opened and their actual list contents rendered, verifying readable station labels on white/light-blue rows. Service headings use bounded wrapping so **Top Up Transit Card** fits inside its existing card.
- Closed station dropdowns use a distinct blue fill, a dark 2-unit outline, larger down arrows and hover feedback, so the controls remain visibly clickable against the white route page.
- All ten existing page headers now match Welcome: full kiosk width, flush with the top edge, equal height, centered white title, and the same dark blue. Page/header slot alignment and padding were normalized in the saved Widget Blueprint, preserving its existing controls. The GPU render checks compare each header's actual position and size against Welcome at both resolutions.
- **37 standalone gameplay checks passed, zero failed**, in the existing Demo map with Null RHI. They verified distance rejection, E opening through the existing Enhanced Input mapping, movement/look suppression, day-pass payment and staff timers, retained day-pass ownership across Exit/reopening, rejection of a second day pass, Exit restoring input/cursor, actual character movement after Exit, and cancellation of an in-progress top-up without credit.
- No new map-check errors or Blueprint compile warnings were reported by the final asset extension.

## Remaining limitations

- Language choices are functional selections; the interface text is still English. Full Chinese/Malay/Tamil translations remain future work.
- The prototype uses demonstration stations, fare rules, receipts and one local demo card; no real ticket, card, staff or payment integration is present.
- Gameplay verification was automated and headless. Interactive Play-in-Editor mouse/touch use and a rendered 3D gameplay run were not manually exercised. The separate UMG render checks used the real GPU renderer.
- A cooked/package distribution and Shipping build were not produced or tested. The Development Game target was compiled; UI/input asset references are explicit for cooking.
- Scene shader compilation in an uncached graphical launch was lengthy; those graphical test launches were stopped. The original project rendering configuration and materials were retained.

No additional manual UI creation, Blueprint reparenting, actor placement or input wiring remains. Follow the three Editor steps above to try the prototype.

The follow-up readability fixes were saved into the existing `KioskUI` through the editor commandlet; its pre-fix backup is under `Saved/KioskFixBackup`. The physical kiosk asset and map were not edited by this follow-up.
