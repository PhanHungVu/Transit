# Transit Kiosk

Transit is a first-person public transit kiosk prototype built with Unreal Engine C++ and UMG. Walk up to a 3D machine and use its world-space touchscreen, payment terminal, card reader, ticket dispenser, and Help button. All payments, tickets, and staff requests are simulated.

## Build and play

You’ll need Windows, Unreal Engine 5.8, Visual Studio with the **Game development with C++** workload and a Windows SDK, and Git LFS. The project was tested with Unreal Engine 5.8.2.

From the repository root in PowerShell, fetch the Unreal assets and build the Development Editor target. Set `$engineRoot` to your Unreal Engine installation directory.

```powershell
git lfs install --local
git lfs pull

$engineRoot = 'C:\Program Files\Epic Games\UE_5.8'
.\Scripts\kiosk.ps1 -Mode Build -EngineRoot $engineRoot
```

Open `Transit.uproject` and click **Play** in the **Demo** map (`/Game/Kiosk/Maps/Demo`). You start facing the kiosk. Walk toward it and press **E** when the interaction prompt appears.

## Controls

| Input | Where | Action |
| --- | --- | --- |
| **W A S D** | Walking | Move |
| **Mouse** | Anywhere | Look around and aim the reticle at the kiosk controls |
| **E** | Near the kiosk | Start using the machine |
| **C** | Outside the kiosk | Switch between normal height and the child preset at **115 cm eye level** |
| **Left-click** | At the kiosk | Select touchscreen options and operate physical controls |
| **C**, **P**, **F1** | At the kiosk | Place the transit card, tap to pay, or request help when available |
| **Esc** | At the kiosk | Exit after collecting any ticket or card you’re prompted to pick up |

The height shortcut appears in the top-right corner while walking. The panel hides when you start a kiosk session, and your chosen height is fixed until you exit.

## Demo walkthrough

Tap the Welcome screen and choose **English**, **Chinese**, **Malay**, or **Tamil**.

- **Buy tickets.** Choose a destination from stations A–H for a one-way or round-trip ticket, or choose a day pass valid until midnight. Select the ticket quantity and confirm the fare.
- **Top up a transit card.** Click the card reader to place the demo card on its holder, then choose a top-up of SGD 5, 10, 20, 30, 50, or 100.
- **Request assistance.** Click the yellow Help button on the machine to open the simulated assistance prompt.

When the payment terminal lights up, aim at it and click to simulate a card tap. After processing, follow the pickup reminder and click your ticket or card to collect it. Click the terminal’s red **X** before paying to return to your selection. Picking up the card before paying cancels the top-up and returns the kiosk to Welcome after five seconds.

## Package for Windows

After building the Editor target, run the packaging script with the same `$engineRoot`.

```powershell
.\Scripts\kiosk.ps1 -Mode Package -EngineRoot $engineRoot
```

Launch `Saved/Packaged/POVVerified/Windows/Transit.exe` when packaging finishes. Keep the entire `Windows` folder together when moving or sharing the game.

## Run the automated checks

```powershell
.\Scripts\kiosk.ps1 -Mode Verify -EngineRoot $engineRoot
.\Scripts\kiosk.ps1 -Mode Gameplay -EngineRoot $engineRoot
```

- **Verify** checks the saved widgets, navigation, and transactions. It writes `Saved/KioskVerification.txt`.
- **Gameplay** runs a rendered playthrough that exercises camera heights, world-space clicks, hardware controls, cancellation, and pickup. It writes `Saved/KioskGameplayVerification.txt`.

## Project layout

| Area | Where to look |
| --- | --- |
| Demo map, physical kiosk Blueprint, and widget assets | [Content/Kiosk](Content/Kiosk) |
| Transaction state and configurable fares, stations, and amounts | [TransitKioskSession.h](Source/Transit/TransitKioskSession.h) |
| First-person interaction, camera, and keyboard shortcuts | [TransitPlayerController.cpp](Source/Transit/TransitPlayerController.cpp) |
| Physical controls and item pickup | [TransitKioskActor.cpp](Source/Transit/TransitKioskActor.cpp) |
| Touchscreen behavior and on-screen prompts | [TransitKioskWidget.cpp](Source/Transit/TransitKioskWidget.cpp) |
| Build, test, and packaging commands | [Scripts/kiosk.ps1](Scripts/kiosk.ps1) |
| Design and implementation notes | [Docs](Docs) |
