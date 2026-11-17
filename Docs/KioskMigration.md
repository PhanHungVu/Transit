# System Design migration

> Current physical interaction: [KioskPOV.md](KioskPOV.md), 16 November 2026. The original touchscreen layouts/transaction code are preserved, with an integrated accessible cabinet and real world-space first-person interaction replacing viewport projection.

> Hardware update, 13 November 2026: the flat hardware artwork has been replaced with separate solid 3D parts in the original narrow cabinet arrangement. The touchscreen asset and transaction flows are unchanged. See [KioskHardware.md](KioskHardware.md) for the current physical construction and validation.

## Audit and recovery

Baseline: clean Git commit `b618ad2` (`Fixes`), inspected on 2026-11-06. Engine: Unreal 5.8.2, CL 56702186. Source reference: `C:/Users/CHUUNI/Downloads/System Design.pdf`, all 148 pages inspected as rendered contact sheets and extracted text. Pages 7-8 are shared; the following 35-state groups repeat in English, Chinese, Malay and Tamil. They are states, not 148 sequential screens.

The existing `/Game/Kiosk/UI/KioskUI` derives from `UTransitKioskWidget`. Ten pages, timers, receipts, card balance, retained day-pass ownership and assistance work in C++. `/Game/Kiosk/Blueprint/BP_TransitKiosk` derives from `ATransitKioskActor`, retaining MachineBase, ScreenFrame, ScreenDisplay, PaymentReader and HelpButton meshes. Existing first-person Blueprints/controller, Demo map, IA_Interact and E mapping remain the integration points. The native editor commandlet can inspect, construct, compile and serialize real editable UMG assets.

## Comparison

| Existing screen/system | Decision | PDF migration |
| --- | --- | --- |
| Welcome / Language | Restyle | Multilingual welcome, train artwork, four choices; pages 7-8 |
| Main Menu | Restyle | Two illustrated service cards; page 14 |
| Ticket Type | Refactor | Destination Select or Day Pass; page 15 |
| Route / dropdowns | Replace presentation | One interactive map, eight destinations; pages 16, 36-43 |
| Review | Retire from active flow | Details in selection/payment overlays |
| Top-up | Refactor | Card placement first, then six amounts; pages 17-18 |
| Payment / Processing / Success pages | Refactor to overlays | Shared local simulation; pages 19-35 |
| Staff popup | Restyle/refactor | Physical assistance control, localized confirmation and OK |
| Session / transactions | Adapt | Destination fares, 1.5x round-trip price, card-present state, central modal state |
| 3D interaction / input restoration | Keep | Approach, E, one UI, movement lock and exit restoration |
| Physical meshes | Refine | Upper-left screen, right terminal/help, lower reader/collection slot |

## File plan

- `TransitKioskSession.h/.cpp`: authoritative screens/overlays, configurable station/fare data, six top-ups, card state and logical Back/Cancel.
- `TransitKioskWidget.h/.cpp`: existing native parent, shared localized bindings, editable UMG screen/overlay presentation, simulated hardware events and timers.
- New localization data/helper and extracted artwork provenance: wording from the PDF, shared layouts, packaged CJK/Tamil fonts.
- `TransitKioskActor` and `TransitPlayerController`: retain E/proximity and input ownership; adapt hardware interaction and front-view presentation as needed.
- `TransitEditor` automation: back up affected assets, import extracted artwork, migrate the existing widget and refine the existing kiosk through Unreal APIs, then compile/reload/render.
- Existing transaction/gameplay tests: update expectations to the new flow, all eight fares, all six amounts, four languages, cancellation/assistance and input restoration.
- `Content/Kiosk/UI/KioskUI.uasset`, `Content/Kiosk/Blueprint/BP_TransitKiosk.uasset` and new artwork/font/material assets only as required. Preserve unrelated maps/gameplay/assets.

## Implementation assumptions

- The PDF's static frames do not prove navigation connections. Use card placement before balance/amount selection, then an explicit local hardware tap event for payment.
- Screen content occupies a 480:350 aspect rectangle in the full 708x572 kiosk illustration. Use a 960x700 touchscreen design surface; the hardware is separate from that surface.
- Reuse extracted image objects for train/map/illustrations. Interactive text, route lines, markers and buttons remain widgets, not a flattened screen image.
- Origin is the kiosk's central green interchange. Only A-H are selectable destinations. Round-trip fare is one-way x 1.5, in cents.
- Default card balance becomes $3.40 for a new play session. Completed top-ups persist during that play session.
- Preserve the user's earlier one-day-pass-per-play-session restriction. The new reference does not revoke that explicit request.
- Processing delay remains configurable (default 2 seconds); success waits for explicit completion rather than an undocumented automatic timeout.
- Assistance preserves both the underlying screen and active payment state. It is explicitly a simulation.
- Extra demo controls/disclaimers have no PDF translation; keep them visually separate from the source design and record them as additions.

## Implemented approach

The existing native session and Widget Blueprint have been migrated in place. The touchscreen uses a 960x700 design surface, matching the PDF's 480x350 touchscreen rectangle. A temporary front camera and projected UMG interaction regions align the screen and clickable hardware with the existing kiosk actor; closing restores the prior view target and the player's original visibility. The terminal, yellow assistance button, purple reader and collection slot are actual actor components outside the touchscreen. E/proximity/movement input remain the existing controller's responsibility.

`Scripts/extract_kiosk_design.py` extracts nine separate embedded image objects and renders three isolated hardware illustrations into `Content/Kiosk/DesignSource`. The bell and printed assistance label consist of multiple PDF drawing layers, so those individual illustrations are rendered together; no completed screen is flattened. The manifest records the PDF hash, object IDs, component rectangles and image sizes. Reusable map lines, station markers, text and controls remain native UMG elements. `Localization.json` identifies four prototype-only text additions separately from source wording. Engine Roboto/Droid and the existing Noto Sans Tamil face are referenced by a shared composite font; the new Latin/CJK face payloads are embedded in their assets.

The active flow is Welcome -> Language -> Options. Purchase -> Single Use Ticket -> Destination -> Trip overlay -> Payment -> Processing -> Success, or Day Pass -> Payment. Top-up -> Card Placement -> Amount -> Payment. Assistance temporarily overlays and restores any active transaction/modal. Completing success returns Welcome. Payment requires a click on the separate 3D terminal region; card reading requires the purple reader region. Keyboard equivalents P (payment), C (transit card), F1 (assistance), and Escape (dismiss/exit) support demonstration testing without extra payment controls inside the PDF dialog.

## Stage results

| Stage | Result / main files and assets | Validation |
| --- | --- | --- |
| 0 - Audit/recovery | Baseline `b618ad2`; timestamped source/package copies under `Saved/KioskMigrationBackup`; complete PDF text and visual inspection | Existing engine, classes, assets, graphs and input setup inspected |
| 1 - Foundation | Adapted `TransitKioskSession`; seven screen states and five popup states; one authoritative transaction and modal state | Guarded navigation, cancellation, duplicate payment and card-read prerequisites tested |
| 2 - Welcome/language | Existing `KioskUI`: multilingual Welcome, shared four-choice language screen, extracted train background | Shared frames 7-8 rendered; Chinese and Tamil glyphs reviewed |
| 3 - Services/tickets | Illustrated service and Destination/Day Pass cards in the same Widget Blueprint | Forward/Back paths and retained selections tested |
| 4 - Destination | Eight real UMG station buttons, colored lines, central origin, selected marker/label/fare and Confirm; shared trip overlay | All eight one-way and round-trip fares exercised in every language |
| 5 - Day pass | Direct $10 payment, next local midnight expiry, retained one-per-play rule | Year rollover, pending/paid ownership, repeat rejection and reopening tested |
| 6 - Top-up | Card Placement screen, separate physical reader event, $3.40 initial balance, six amount choices | All six amounts tested in every language; cancellation adds no credit |
| 7 - Payment | Shared dimmed payment/processing/success overlays, physical terminal event, configurable world timer, receipt and explicit completion | Correct totals, one credit per approval, timer cancellation and confirmation tested |
| 8 - Assistance/localization | Physical assistance event and localized OK popup; shared 42-key text table and composite font | Repeated Help, preservation/restoration of payment and processing, all localized layouts tested |
| 9 - Physical kiosk | Existing BP/SCS parts refined; separate reader/collection components and extracted face artwork; temporary camera and hidden/restored player | Real rendered Demo gameplay and projection checked; final capture reviewed |
| 10 - Build/test | Development Editor and Development Game targets, Win64 | Both built successfully; results below |

## Files and assets changed

- `Source/Transit/TransitKioskSession.h/.cpp`: screen/popup state, eight station records, destination fares, 1.5x round-trip pricing, six top-ups, card state, logical navigation and receipt/ownership preservation.
- `Source/Transit/TransitKioskWidget.h/.cpp`: existing widget parent, localized text, button bindings, selected marker/fare styling, modal visibility, hardware actions, payment timer and keyboard shortcuts.
- `Source/Transit/TransitKioskText.h/.cpp`: shared language lookup; generated text data corresponds to `Content/Kiosk/DesignSource/Localization.json`.
- `Source/Transit/TransitPlayerController.h/.cpp`: existing interaction/input behavior retained; adds temporary camera, player visibility preservation and projection of touchscreen/hardware controls.
- `Source/Transit/Tests/TransitKioskTests.cpp` and `TransitKioskSmokeTest.cpp`: updated transaction tests and real-player checks. The gameplay test uses world time, matching the payment timer, and waits for shaders before its screenshot.
- `Source/TransitEditor/TransitKioskCommandlet.cpp`, new `TransitKioskDesign.h/.cpp`, and `TransitEditor.Build.cs`: supported asset inspection, image/font imports, editable UMG construction, component refinement, safe saves, reload/verification and rendering. `-UpdateHardware` leaves the validated touchscreen asset untouched.
- `Content/Kiosk/UI/KioskUI.uasset`: seven editable pages under `WS_Pages`, five editable overlays under `WS_Popups`; source PDF train/artwork and real text/buttons. The former Review and full-page payment states are retired from the active flow.
- `Content/Kiosk/Blueprint/BP_TransitKiosk.uasset`: same actor and original named physical components, with updated dimensions/materials and additional reader/collection/face components.
- `Content/Kiosk/Maps/Demo.umap`: saves the existing kiosk's updated construction/default demo data. All 16 original level actor labels/classes/positions match the pre-migration inspection exactly.
- New `Content/Kiosk/Design` texture/material/quad assets, four `Kiosk*` font assets under `Content/Kiosk/Fonts`, and twelve source PNGs plus provenance/localization JSON under `Content/Kiosk/DesignSource`.
- `Scripts/extract_kiosk_design.py`, `Scripts/kiosk.ps1`, and documentation. Existing character, game mode, prompt, input mapping and original material assets are retained.

## Verified results

- Unreal 5.8.2 **Development Editor / Win64** and **Development Game / Win64** builds succeeded with the installed VS 2022/Windows SDK.
- The final migration/hardware saves and read-only inspection completed with **zero commandlet errors and warnings**. Saved Blueprint compilation succeeded.
- **5,456 UI/transaction/layout checks passed, zero failed** in `Saved/KioskVerification.txt`. This includes the transaction regression suite, actual UMG click delegates, all station/trip/language combinations, all six top-ups in each language, cancellation, repeat day-pass rejection, receipt/balance updates and timer behavior.
- **284 GPU previews** cover all 142 relevant PDF screen/dialog states (pages 7-148), at both **1280x720 and 1920x1080**, under `Saved/KioskDesignPreview/PDF_<page>_<width>.png`. Active translated text was checked against its allocated height; revised fonts/spacing passed. Representative English, Chinese, Malay and Tamil screens were visually reviewed.
- Preview export explicitly converts linear render pixels to sRGB, avoiding misleading dark/oversaturated preview colors.
- **57 rendered gameplay checks passed, zero failed** in `Saved/KioskGameplayVerification.txt`. The Demo level ran with DirectX 11 and a real 1280x720 viewport. Checks cover E/proximity, camera and projected controls, player visibility/input locking and restoration, repeated assistance, retained day-pass ownership, cancelled payment, a $100 top-up and return to Welcome. Inputs/actions are automated; this was not a manual mouse/touch session.
- `Saved/KioskGameplayWelcome.png` is the actual 3D kiosk capture after shaders are ready. The complete terminal, yellow assistance button/label, purple reader and collection slot are visible; the player body does not obstruct the screen.
- The final physical-artwork update preserved the **exact SHA-256 of the touchscreen asset** that passed all 5,456 checks. Final inspection confirms the same 16 placed level actors and the existing `IA_Interact = E` mapping.

Useful logs: `Saved/Logs/KioskMigrationBuild.log`, `KioskMigrationGameBuild.log`, `KioskMigrationApply.log`, `KioskMigrationHardware.log`, `KioskMigrationVerify.log`, `KioskMigrationGameplayRendered.log`, and `KioskMigrationFinalInspection.log`.

## Using and editing the migrated prototype

1. Open `Transit.uproject` in UE 5.8.2 and open `/Game/Kiosk/Maps/Demo` if necessary.
2. Press Play, approach the existing machine, and press **E** at the existing prompt.
3. Use the touchscreen. For top-up, click the **purple physical reader** before choosing an amount. At Card Payment, click the **physical terminal** on the right. These are local simulations. Keyboard equivalents are **C** (transit card), **P** (payment) and **F1** (assistance).
4. The **yellow physical button** opens assistance; OK restores the prior screen/modal. Return to Welcome completes a successful transaction. Exit closes the kiosk; Escape dismisses a cancellable popup or exits.
5. To edit visuals, open the existing `/Game/Kiosk/UI/KioskUI`. The screen and popup switchers contain normal editable UMG controls. Hardware is edited in the existing `BP_TransitKiosk`; projected hit regions align at runtime. No manual widget construction, Blueprint reparenting, actor placement or input wiring is required.

`Scripts/kiosk.ps1 -Mode Build`, `-Mode Inspect`, `-Mode Verify` and `-Mode Gameplay -RenderGameplay` rebuild or verify the prototype. `-Mode Migrate` (and the legacy `Apply` alias) **regenerate the design layout and kiosk configuration** through Unreal, with timestamped backups; they are not needed to play, and would replace later manual layout edits with the scripted design. Close Editor before builds or migration. The extraction script is development tooling only; the saved prototype does not require Python or access to the source PDF at runtime.

## Remaining differences and limits

- This is a reconstruction of static frames, not an imported Figma prototype. Card-placement ordering, simulated physical taps and explicit completion are documented assumptions; no complex transition animation was added.
- The 3D dimensions, lighting, font weights, some button/arrow/shadow details and modal proportions are approximate. Success intentionally has extra space for the retained receipt, balance/validity and completion control. Pixel-perfect reproduction is not claimed. Option illustrations retain the PDF's original limited raster resolution.
- Exit, completion, owned-pass messaging and the demo notice are prototype additions; their four-language wording is marked separately from PDF-provided wording. Physical printed labels retain the PDF's English artwork.
- Payment/card reading/staff assistance remain entirely simulated. Balance and ownership persist for the local player's play session, not across restarting Play.
- The initial migration used Development builds and rendered uncooked gameplay. On 13 November, a Shipping-only test-code compiler error was fixed and the complete Windows Shipping packaging process passed. The verified executable is under `Saved/Packaged/ShippingVerified/Windows`; the older `Build/Windows` archive was preserved. See [KioskPackaging.md](KioskPackaging.md).
- Manual mouse/touch use, a packaged playthrough and a demonstration video still need their own final submission check. Optional sound feedback from the PDF's initial note was not added.
