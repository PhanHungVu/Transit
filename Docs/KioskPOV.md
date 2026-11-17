# Physical kiosk and first-person interaction — 16 November 2026

> Follow-up updates: [KioskScalePickup.md](KioskScalePickup.md) supersedes this report's 144 cm cabinet dimensions. The current cabinet is 192 cm tall. [KioskInteractionCues.md](KioskInteractionCues.md) documents the current reader lights, gold item outlines, immediate reminders and required card/ticket pickup. The earlier implementation and validation below are retained as history.

## Audit before changes

The existing `BP_TransitKiosk` uses `ATransitKioskActor` and 31 separate primitive mesh components. Its body is 45 × 110 × 190 cm. The main screen is centered at 132 cm; payment at 120 cm; Help at 76 cm; transit reader at 47 cm; collection opening at 17 cm. The payment unit sits outside the cabinet. The current 960:700 touchscreen is the editable `/Game/Kiosk/UI/KioskUI`, backed by `UTransitKioskWidget` and `UTransitKioskSession`; transactions and four-language text already work.

`ATransitPlayerController` reuses `IA_Interact` / E in `IMC_Default`, proximity and a visibility trace. It currently switches instantly to an `ACameraActor` 430 cm away at 95 cm height, with 50-degree FOV. It locks both movement and looking, adds the kiosk UI to the viewport, and projects rectangular click regions over physical meshes. There is no world-space Widget Component or free-look interaction. The current dispenser is visual; successful transactions produce on-screen receipts, without a spawned collectible or collection rule.

The project already has uncommitted implementation and asset work. Preserve it and use timestamped file/package copies under `Saved/KioskPOVBackup` / `Saved/KioskMigrationBackup` as recovery points. No source-control reset or binary asset editing is needed.

## Changes planned

1. Refine the existing primitive cabinet and its component positions. Keep its floor-level origin; do not rescale the actor. Mount the payment terminal inside the cabinet width, raise reader/collection, tilt the complete screen assembly 12 degrees, and add a physical sign, Help symbol and small card cradle.
2. Reuse the existing Widget Blueprint on a true world-space `UWidgetComponent`, aligned from the display mesh's actual transform. Do not regenerate its layouts or translations. Retire the runtime projected viewport rectangles.
3. Adapt the existing controller to a short blended first-person camera move, preserving the approaching player's eye height. Lock walking, retain Enhanced Input look, clamp free-look, and use a center ray plus `UWidgetInteractionComponent` for clicks. Route physical hits only to the active kiosk's existing actions. Escape cancels/exits through the established close logic.
4. Add only a small noninteractive aiming/hint HUD. Restore input, cursor, view, visibility, timers and focus on exit. Keep fares, session state, card balance and day-pass rules unchanged. Card and output props are presentation of existing states, without a new financial or collection rule.
5. Build and inspect the physical stage, then test real world-space pointer input, multiple eye heights, all transaction paths, reopening and exit. Compile Shipping and verify packaging again.

Main files affected: `TransitKioskActor`, `TransitPlayerController`, `TransitKioskWidget`, native editor `TransitKioskDesign`, existing tests and documentation, plus the existing kiosk Blueprint and Demo map. No duplicate kiosk, character, controller Blueprint or input mapping is planned.

## Reference interpretation

The supplied prompt is the requested adaptation. The previously supplied System Design PDF remains the touchscreen reference. The historical machines are physical references; fictional ticket services stay unchanged.

Reviewed [SGTrains ticketing machines](https://www.sgtrains.com/technology-ticketing.html), [SimplyGo kiosks](https://www.simplygo.com.sg/ticketing-service-kiosks/), and the [BCA Code on Accessibility 2025](https://go.gov.sg/bca-coa2025). BCA clause 9.2.2 gives a general 450–1200 mm operable-control range, and its advisory to 9.2 recommends 800–1000 mm for devices viewed/used by wheelchair users. Use these as design guidance and measure actual model heights; this prototype is not a certified physical installation. The listed 120/145/175/190 cm user heights are body heights, so viewpoint tests must state their separate simulated eye heights.

## Completed implementation

The existing kiosk has a fixed 45 × 112 × 144 cm cabinet, with its original floor-level actor origin and placement. The separate parts remain editable in `BP_TransitKiosk`. A continuous control panel mechanically supports the payment unit within the body width; no side-mounted dangling device remains.

| Part | Actual model placement |
| --- | --- |
| Touchscreen | 48 × 35 cm active surface, centered about 106 cm above floor; face spans approximately 89–123 cm; 12-degree backward tilt |
| Payment | Key rows centered at 102, 105, 108 and 111 cm; terminal/display above them in the right panel |
| Help | Round yellow physical button centered at 93 cm, metal bezel and modeled black telephone handset |
| Transit reader | Angled almost horizontally, centered at 86 cm with the usable pad/holder around 88 cm |
| Card cradle | 10 × 6.5 cm support, small retaining lip and side guides; 8.6 × 5.4 cm simulated card |
| Collection opening | Centered at 85 cm, with a shallow framed recess and tray |
| Sign | Attached header with TICKET KIOSK, centered at 136 cm |

These are measured prototype dimensions, not a claim that every pixel or control meets the BCA advisory band. In particular the upper touchscreen and payment display exceed 100 cm. The previous 190 cm body, 132 cm screen center, 47 cm reader and 17 cm dispenser are retired.

`ScreenAssembly` groups the bezel and display so they tilt and move together. `ATransitKioskActor` creates one world-space Widget Component at runtime, attached directly to the actual display. Its 960 × 700 draw surface derives its world scale and rotation from the mesh, with a small offset in front of the glass. The original Widget Blueprint is reused. The former desktop hardware/exit rectangles and viewport disclaimer are hidden only in this world-space instance; the touchscreen layouts are not regenerated.

The controller now blends into a first-person camera in 0.33 seconds, at the approaching player's actual eye height. It stands 75 cm in front of the cabinet face, approximately 62 cm from the central glass, with an 80-degree FOV. Mouse movement supplies bounded free look (±45 degrees horizontally, sufficient vertical travel for all controls). Walking/jumping/unrelated actions are locked. There is no camera shake or dynamic resizing of the kiosk.

Left click uses an actual camera ray: `UWidgetInteractionComponent` supplies virtual pointer input to the physical screen; traces of the active kiosk's hardware call its existing Pay, ReadCard or Help actions. Other actors are filtered out. A small noninteractive reticle and input hint replace the large viewport UI. The regular mapping contexts and E action are reused. On exit, the timer/input/cursor/view/body state is restored, pressed keys are flushed, pointer capture is released, the HUD is removed, and the temporary camera is destroyed after the return blend.

The card prop stays in its cradle while the existing card-present state is active. Successful ticket purchases show a small visual ticket/card in the raised opening. Home/exit clear these props. No inventory, collectible-pickup rule, NFC, staff communication or real payment service was introduced. Existing fares, localization, receipts, balances and one-day-pass-per-play-session behavior remain authoritative.

The project's new world-screen material duplicates Unreal's standard widget pass-through material and compensates for eye adaptation; only that material/instance binding changes screen lighting. A runtime material refresh prevents the default checker texture. Printed labels and the small outside-screen control hints remain English; touchscreen localization is unchanged.

## Files changed

- `TransitKioskActor.h/.cpp`: world-space screen ownership/alignment, natural-eye-height view calculation and visual card/output state.
- `TransitPlayerController.h/.cpp`: blended camera, bounded look, actual screen/hardware traces, pointer input and full exit cleanup.
- `TransitKioskWidget.h/.cpp`: runtime world-space adapter and small aim HUD. Existing session operations and visible touchscreen design remain unchanged.
- `TransitKioskDesign.cpp`: supported Blueprint component edits, grouped tilt, hardware/card cradle/signage, new solid materials and exposure-stable screen material. Hardware-only updates preserve configured demo values.
- Existing `BP_TransitKiosk`, `Demo` map and new material assets in `Content/Kiosk/Design`.
- `TransitKioskPOVTest.cpp` and the existing smoke-test launcher: actual input/ray regression, screenshots and simulated-height checks. Shipping excludes all test implementation consistently.
- `Scripts/kiosk.ps1`: Gameplay now requires graphics for real Slate hit testing; the legacy RenderGameplay switch remains accepted. Package mode cooks the Demo to a filesystem store on the project drive.

`KioskUI.uasset`, `TransitKioskSession.h/.cpp` and `TransitKioskText.cpp` retain their exact pre-task hashes, recorded in `Saved/KioskPOVPreservedFiles.json`. Unrelated character/game-mode/input assets are untouched. Recovery copies are under `Saved/KioskPOVBackup/20261116_155134` and timestamped package backups.

## Verification

- Development Editor and Windows Shipping compiled successfully with installed UE 5.8.2. The earlier Shipping test-guard fix remains intact.
- The physical stage was rendered and inspected before implementing the camera. Its attached panel, lowered controls, tilted bezel, phone icon, cradle and signage were reviewed in `Saved/KioskPOVHardware.png`.
- The first complete 1280×720 run passed 198 real-ray checks. After the final screen-material instance fix, the 1920×1080 run passed **199 checks, zero failed**, including the material assertion. `Saved/KioskPOVVerification1920.txt` and `Saved/Logs/KioskPOVGameplay1920.log` record that run.
- **1,908 existing transaction/UI regression checks passed, zero failed**, covering all station fares, trip types, six top-ups, four languages, modal/back/cancel paths, timers and day-pass restrictions. Report: `Saved/KioskVerification.txt`; log: `Saved/Logs/KioskPOVUIRegression.log`.
- Tests use real E and mouse input, camera aiming, collision traces, Slate hit testing and pointer press/release. They do not bypass screen interactions by broadcasting OnClicked. They verify purchases, top-up, repeated Help, cancellation during processing, returned movement, reopening, hidden cursor, camera clearance and cleanup.
- Simulated viewpoint profiles: 120 cm child / 110 cm eyes; 145 cm shorter user / 135 cm eyes; 175 cm adult / 165 cm eyes; 190 cm tall adult / 180 cm eyes; seated / 105 cm eyes. Every profile passed actual touchscreen/Help clicks and distinct traces to payment, reader, collection and signage, without camera overlap. The real approaching character's measured eye height was 161.43 cm and was preserved on entry.
- Representative real gameplay captures: `KioskPOV_Standing.png`, `KioskPOV_Payment.png`, `KioskPOV_CardHolder.png`, `KioskPOV_TamilSuccess.png`, and the five `KioskPOV_<profile>.png` images under `Saved`.

Packaging completed successfully. The initial Zen-store cook hit HTTP 507 (insufficient storage on the shared cache drive); merely omitting the Zen flag still inherited the project default. The validated Package script explicitly passes `SkipZenStore`, uses project-drive DDC, cooks the Demo into `Saved/CookedPOVFiles/Windows`, and stages separately in `Saved/StagedPOV`. It changes no global cache/editor setting and deletes no existing archives to make room.

The complete Shipping cook, Pak/IoStore stage/package and archive finished with exit code 0. Log: `Saved/Logs/KioskPOVPackageFinal.log` (UTF-16, produced by PowerShell). The updated executable is **`Saved/Packaged/POVVerified/Windows/Transit.exe`**. Keep the complete Windows folder together when copying/sharing it; older `Build/Windows` and `Saved/Packaged/ShippingVerified` archives remain available.

The actual cooked Shipping game was launched offscreen with its default graphics backend and remained running for a 15-second startup smoke check, after which only that test process was deliberately stopped. `Saved/KioskPOVPackagedStartup.json` records the result. This confirms startup, not a manual packaged playthrough.

## Use and remaining checks

Open the existing Demo map in UE 5.8.2, Play, approach the front and press E. Move the mouse to look/aim, left click the touchscreen or physical controls, and press Escape to exit. C/P/F1 remain optional card/payment/Help shortcuts. No manual UMG creation, Blueprint reparenting, actor replacement or input assignment is needed.

For repeat verification use `Scripts/kiosk.ps1 -Mode Gameplay`; for a Shipping package use `-Mode Package`. If this machine's signed-script policy blocks direct execution, use `powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\Scripts\kiosk.ps1 -Mode Package`; this changes policy only for that process. Package mode uses the Demo map and saves to `Saved/Packaged/POVVerified`, leaving older archive folders available. No global editor/cache setting is changed. `-Mode Hardware` regenerates the scripted physical design, and full Migrate also regenerates the touchscreen; neither is needed to play.

The tests are automated mouse/ray simulations, not a manual touch-hardware or ergonomic study. Human comfort/readability review, gamepad/touch input coverage and a demonstration video remain separate checks. The initial view focuses the screen; looking around reveals the sign and right/lower hardware. Exact artwork brightness depends on world lighting/tone mapping. No accessibility certification, physical hand-reach test or new inventory collection system is claimed.
