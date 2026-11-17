# Customer feedback update — 17 November 2026

This update extends [KioskInteractionCues.md](KioskInteractionCues.md): day passes can be purchased repeatedly, tickets support a passenger count, Help has physical/green feedback, and pickup highlights belong to the machine.

## Day passes and customer reset

The session, player controller and UI no longer store or check day-pass ownership. A completed purchase does not disable the Day Pass option for later customers, including after leaving/reopening the kiosk. The fare and midnight expiry remain unchanged. Finishing/resetting a purchase returns the next customer's ticket count to one. The existing demo transit-card balance behavior remains unchanged.

## Number of tickets

One-way, round-trip and day-pass selections now open an editable **Number of tickets** popup before Card Payment. Its large minus/plus controls change the number of people, show the per-ticket price and update the total immediately. One ticket is printed for each person. Top-ups retain their existing amount/payment flow.

The default range is **1–10 tickets**. `DemoConfig.MaxTicketsPerPurchase` on the existing kiosk can configure the limit, with supported bounds 1–20. Minus/plus are disabled at their limits and the C++ session rejects out-of-range quantities. Payment cannot start until the quantity is confirmed.

For example, station G's $6.00 round trip costs $18.00 for three people. Two $10.00 day passes cost $20.00. Rounding is applied to the unit fare before multiplication. The payment description, immutable paid total and receipt include the ticket count. Cancelling Card Payment returns to the same quantity for editing; cancelling quantity returns to the previous ticket choice.

Multiple tickets appear as a small physical stack in the existing dispenser. The complete stack is collected in one click. Its reminder reports how many tickets are ready, and navigation remains locked until collection. Collecting the stack hides all ticket layers and does not charge again.

The new popup was appended to the existing Widget Blueprint's switcher, preserving the indices/layouts of the five prior overlays and seven pages. Quantity text and pickup instructions use the existing four-language font/localization system.

## Help button

The physical face is plain yellow. The old phone-logo mesh components were removed from the kiosk Blueprint and reconstructed map instance. The adjacent HELP label and existing assistance-popup artwork remain.

A valid physical Help press (or F1) lights the outer bezel green and moves the yellow face **0.65 cm inward**, then returns it to rest. The animation takes approximately 0.48 seconds. Green acknowledgment lasts at least 0.8 seconds and remains on while the assistance popup is open; it clears after dismissal. A timer runs only while feedback is settling. Help remains locked during mandatory pickup, in accordance with the preceding request.

The first rendered test caught an outward press caused by the cylinder's reversed local axis. Motion was corrected to the cabinet's inward direction; final verification is recorded below.

## Machine highlights

Gold pickup outlines now mark the **card-reader/holder assembly** and **dispenser opening**, rather than outlining the card or paper itself. Cyan continues to mark a reader when input is needed. Clicking the highlighted holder/dispenser collects its ready item; clicking the actual item also works. This keeps the visible cue aligned with the interactive area and makes collection less dependent on a thin card/ticket edge.

Payment illumination, immediate card/ticket reminders, input/camera restoration and the existing physical scale are retained.

## Files and recovery

Changes are in `TransitKioskSession`, `TransitKioskWidget`, `TransitKioskActor`, `TransitPlayerController`, their existing tests, and the editor's focused `UpdateInteraction` operation. Saved assets are the existing `KioskUI`, `BP_TransitKiosk`, `Demo` map, and new `M_HelpRingOff` material. The focused editor operation preserves the other physical components, page layouts and configurable fares. `TransitKioskText.cpp`, `Localization.json` and the extraction/generation script contain matching new translations.

Pre-change copies are under `Saved/KioskFeedbackBackup/20261117_132930`; the editor asset updater also keeps timestamped package copies. Character/input asset hashes are recorded in `Saved/KioskFeedbackPreservedFiles.json` and remain unchanged. No duplicate character, kiosk, mapping context or viewport widget was added.

## Verification

Development Editor compilation and the native asset update succeeded. The transaction/UI suite passed **3,143 checks, zero failures**, covering all stations, languages, ticket products, quantity changes/cancellation, repeated day-pass eligibility, payment timers, top-ups and pickup gates. Report: `Saved/KioskFeedbackUIVerification.txt`; log: `Saved/Logs/KioskFeedbackUI.log`.

The new quantity popup was rendered and inspected in English, Chinese, Malay and Tamil at 1920 × 1080, with full-HD quantity captures preserved in `Saved/KioskFeedbackPreview1080`. Physical images also confirm removal of the Help logo and outlines on the machine assemblies.

The final corrected-motion run at **1280 × 720** passed **265 gameplay checks, zero failures** (`Saved/KioskFeedbackGameplayFinalVerification.txt`, `Saved/Logs/KioskFeedbackGameplayFinal.log`). These use real camera rays and pointer presses/releases. They verify plus/minus and fare updates, confirmation before payment, three physical tickets, two day passes followed by another customer's purchase in the same play session, Help depression/return/green feedback, no physical logo, collection by machine assembly or item, top-ups, cancellation without late credit, forced close/reopen preservation, five simulated eye heights and restored input on exit.

Representative final images are `Saved/KioskFeedback_Quantity_English.png` (plus Chinese/Malay/Tamil versions), `KioskFeedback_HelpRest.png`, `KioskFeedback_HelpPressed.png`, `KioskInteraction_CardOutline.png` and `KioskInteraction_TicketOutline.png`. All four quantity layouts were inspected at both tested sizes. The earlier full-HD gameplay run found the animation-direction issue; the final zero-failure report supersedes it.

Windows Shipping compilation, cooking of **606 packages**, Pak/IoStore staging and archiving succeeded with **exit code 0** (`Saved/Logs/KioskFeedbackPackage.log`, PowerShell UTF-16). Cooking reported zero errors and two nonblocking warnings about existing cook-cache directories. The new material and edited widgets/Blueprint/map were included; no manual cache deletion or global settings change was required.

The updated launcher is **`Saved/Packaged/POVVerified/Windows/Transit.exe`**. Keep the complete `Windows` folder together when sharing it. The new Shipping game stayed running for a 15-second hidden/offscreen startup check, then only that test process was stopped (`Saved/KioskFeedbackPackagedStartup.json`). The full interaction run above was automated in the installed engine; packaged startup is not a manual packaged playthrough. Older `Build/Windows` and `Saved/Packaged/ShippingVerified` archives remain separate.

## Use

Open `/Game/Kiosk/Maps/Demo`, Play and press E near the kiosk. Choose a ticket type, adjust the passenger count, Continue, then use the cyan payment reader. Collect the stack from the gold-highlighted dispenser and return to Welcome for the next customer. Day Pass remains available. Press the plain yellow Help button to see the inward motion and green ring.

No manual widget creation, Blueprint reparenting, actor replacement or input assignment is needed. The complete manual packaged playthrough and comfort/readability review remain separate from automated engine tests.
