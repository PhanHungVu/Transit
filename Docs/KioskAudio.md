# Kiosk sound effects — 19 November 2026

The seven supplied SoundWave assets in `Content/Sound` are connected to the existing C++ kiosk. No sound was replaced, duplicated or edited. The existing UI, kiosk Blueprint, Demo map, character and input assets retain their pre-update hashes.

## Playback mapping

| Supplied asset | Interaction |
| --- | --- |
| `Start` | Opening the kiosk with E; once per successful opening, including reopening |
| `Click` | Each enabled touchscreen button action; physical Help/payment/reader actions; equivalent C/P/F1/Escape shortcuts |
| `Accept_Sound` | Confirming a destination or ticket quantity, successfully reading the transit card, acknowledging the Help popup, and payment approval |
| `Reject_Sound` | Cancelling the trip/quantity/payment popup; invalid or blocked actions; attempting to leave with a card/ticket awaiting collection |
| `Help` | A successful Help request, alongside the existing button animation and green ring |
| `PaymentShowUp` | The Card Payment/bill popup becoming visible, including returning to it from Help |
| `PickUp` | Successful physical collection of the transit card or dispensed ticket stack; once per pickup |

Selection/navigation buttons and quantity plus/minus use Click. Affirmative confirmation adds Accept; explicit cancellation adds Reject. Tapping the payment terminal does not prematurely play Accept: approval plays it when processing actually succeeds. A valid card placement gets Click plus the recognition/accept cue. Top-ups and every ticket product use the same mappings.

The start cue plays on E/opening, rather than as ambient sound when the level loads. Tapping Welcome's Continue button then uses Click. Returning to Welcome uses the ordinary button feedback; it does not replay the opening sound automatically.

`PaymentShowUp` follows the visible popup transition. It is not emitted by repeated redraws, language refreshes or tick updates. Pickup audio follows a successful collection state change, so failed/double collections cannot replay it. Mandatory pickup locks remain intact: Help/payment/card actions are rejected during that phase, and Help does not falsely announce a new request.

There is currently **no simulated bank-declined/payment-failed transaction state**. Reject is wired to cancellation and rejected interactions; this update adds sound to the existing flows rather than introducing a new payment outcome.

## Interactions without dedicated SFX

| Event | Current audio coverage / missing cue |
| --- | --- |
| Ticket printing, feed motor and ticket ejection | Approval has Accept; no printer/motor/ejection sound |
| Placing/inserting/tapping a transit card | Click and Accept acknowledge the reader action; no distinct card contact/insertion sound |
| Payment processing/waiting | Initial terminal click and final Accept exist; no processing loop or progress cue |
| Automatic pickup-reminder appearance | No separate reminder chime or spoken prompt; payment approval still has Accept and a blocked attempt has Reject |
| Approach prompt appearing/disappearing | Silent; no proximity/attention cue |
| Camera blend, looking around, status lights and highlight changes | Silent; no separate movement/indicator sounds |
| Machine idle | No ambient machine hum |
| Leaving/shutting down the kiosk | Click feedback exists; no separate exit/sign-off sound |
| Physical Help press and spring return | Click/Help exist; no additional mechanical release sound |

Disabled UMG buttons do not produce an OnClicked event, so they remain silent. Blank panels, the floor and other non-interactive surfaces are also silent. These are intentionally not treated as button clicks.

## Implementation and configuration

`ATransitKioskActor` owns the seven configurable sound references and one playback method. Native hard references bind the supplied assets and keep them in cooked builds. `Kiosk > Audio` on the existing actor exposes the sounds, the enable switch and the master volume (default 0.75). Click is played at half that gain so semantic cues remain clear. These are one-shot 2D UI sounds, remaining audible as the player turns to the physical reader/holder/dispenser.

`UTransitKioskWidget::HandleAction` supplies a single click and the appropriate semantic feedback through the existing session methods. Its payment timer supplies approval audio. The actor supplies pickup audio only after collection succeeds. The controller supplies opening audio and routes Escape through the same exit action; physical buttons and shortcuts share the same behavior.

No new Widget Blueprint, input mapping or gameplay class was introduced. No manual Editor wiring is needed. Recovery source copies are under `Saved/KioskAudioBackup/20261119_205244`; asset hashes are in `Saved/KioskAudioPreservedFiles.json`.

## Validation

- Development Editor compilation passed with the installed UE 5.8.2.
- The native inspector confirmed all **seven** assets load from the saved kiosk Blueprint defaults, are stereo SoundWaves, have positive durations and do not loop (`Saved/KioskAudioAssets.txt`).
- The real audio-mixer test passed **22 checks** and recorded each clip. PCM analysis confirmed **all seven recordings contain non-silent 48 kHz stereo audio**, with no clipped peaks (`Saved/KioskAudioPCMVerification.json`, `Saved/KioskAudioMixerVerification.txt`, `Saved/Logs/KioskAudioMixer.log`). Recording paths are listed in the JSON and mixer report.
- The real-input gameplay run passed **343 checks, zero failures**, including exactly one Click request per touchscreen button action, all seven cue types being triggered by actual interaction, payment-popup de-duplication, and all existing ticket/top-up/pickup/Help/input-restoration paths (`Saved/KioskAudioGameplayVerification.txt`, `Saved/Logs/KioskAudioGameplay.log`). This full route run used `-nosound` to avoid replaying the entire transaction suite aloud; actual mixer output was verified separately above.
- The native transaction/UI regression suite passed **3,143 checks, zero failures** (`Saved/KioskAudioUIVerification.txt`, `Saved/Logs/KioskAudioUI.log`).

Windows Shipping compilation, cooking of **613 packages**, Pak/IoStore staging and archiving succeeded with **exit code 0** (`Saved/Logs/KioskAudioPackage.log`, PowerShell UTF-16). All seven sound assets and their cooked audio payloads were included. Cooking reported zero errors and the same two nonblocking warnings about existing cook-cache directories. No manual cache deletion or global configuration change was needed.

The updated launcher is **`Saved/Packaged/POVVerified/Windows/Transit.exe`**; keep the entire `Windows` folder together when copying it. The new Shipping game stayed running for a 15-second hidden/offscreen startup check with audio enabled, after which only that test process was stopped (`Saved/KioskAudioPackagedStartup.json`). Older archive folders remain available. Source-asset hash verification confirmed all 12 tracked UI/kiosk/map/character/input/sound files remain unchanged.

Mixer recordings verify playback; a subjective listening/mix review on the intended speakers and a complete manual packaged playthrough remain separate checks.
