# Gameplay

This page describes the game's systems as they are implemented in `Source/MarsSurvival/`. Numbers are default tuning values and may change.

## Start of a mission

1. **Splash:** PARADOX logo, then the PARADOX PRODUCTION TEAM poster.
2. **Main menu:** a view through the MTV Odysseus viewport onto a procedural Mars globe. Options are BEGIN NEW MISSION, CONTINUE, SETTINGS & CONTROLS, and QUIT TO DESKTOP (which asks for confirmation).
3. **Archival briefing.** Skip it with the `-NoBriefing` flag.
4. You start **suited in SEC-01 Airlock** and watch the supply shuttle leave through the viewport.

## Suit training (Chapter I, step 1)

| # | Card | Tasks |
|---|---|---|
| 1 | Pressure & Suit | Walk through the inner hatch into the ring · stow the MCP suit at the EVA Prep rack |
| 2 | Radiation | Hold the Geiger dosimeter (4) |
| 3 | Med-Bay | Scan yourself on the scanner bed · print methylene blue at the bioprinter (hold E) · inject it from the med-kit (hold H) |
| 4 | Cold Seals | Torque a bulkhead flange (3, hold LMB) · patch a hull puncture if you find one (2) |

## Story

`UMarsNarrativeDirector` runs a linear list of objectives across three chapters:

**Chapter I: Stabilisation (Sol 1–2)**
1. Complete SV-4 suit training.
2. Open the supply lockers in SEC-06 Storage & Comm.
3. Run a diagnostic at the Comm-Relay terminal.
4. Rest in the SEC-07 Crew Quarters bunk.
5. Decode the black-box signal at the Comm-Relay terminal.
6. Pre-flight and board the Recon Shuttle.

**Chapter II: The Survey (Sol 3–4)**
1. Survey Beacon ALPHA: crashed relay satellite R-3. Recover its black box.
2. Survey Beacon BETA: the deep ice-core drill rig.
3. Investigate the abandoned ARES-III outpost.
4. Return to Ares-IV and rest. Storm season is coming.

Each site has a storage locker with supplies and a data log. Recovered logs can be reread from the pause menu.

**Chapter III: The Uplink (Sol 5+)**
1. Install the quantum transistors at the Comm-Relay terminal. There are three: Storage, relay R-3 and the drill rig.
2. Repair the transceiver with spare parts.
3. Align the dish: hold E at the alignment terminal until it reaches 100 %.
4. Transmit to Earth. This ends the game in **victory**.

Narrative escalation raises symptom intensity and storm frequency as the chapters progress.

## Body: `UPlayerBiometricsComponent`

Tracked values: cumulative radiation dose and current dose rate, inspired CO2, methaemoglobin (MetHb) %, perchlorate body burden, functional O2 saturation, extremophile bio-load, hydration, nutrition, stamina, panic and hallucination intensity.

- **Radiation** builds faster on EVA and much faster during dust storms and solar particle events. Radiation hazard zones on the surface add extra dose. Light flashes (Cherenkov-style phosphenes) become more frequent as the dose rate rises.
- **Perchlorate** from regolith dust turns haemoglobin into MetHb, and the kidneys slowly clear it. Methylene blue speeds up MetHb reduction for a limited time.
- **Functional O2 saturation** falls with low cabin pressure and with high MetHb.
- **Hypercapnia**: symptoms appear above about 5,000 ppm CO2 and become severe from 40,000 ppm.

### Disease stages

| Stage | Trigger (any of) | Countermeasure |
|---|---|---|
| 1 Prodromal | MetHb 3–10 % · rad 50–150 mSv · bio-load 0.15 | KI pills + O2 |
| 2 Acute MetHb / hypercapnia | MetHb 10–25 % · rad 150–400 · bio-load 0.40 | Methylene blue, CO2 scrubbing |
| 3 Neuro-toxic encephalopathy | MetHb 25–45 % · rad 400–750 · bio-load 0.70 | Neuro-Antitoxin + Amifostine (via xeno-analyzer assay of a bio-agent culture) |
| 4 Systemic collapse | MetHb > 45 % · rad > 750 · bio-load > 0.85 | Emergency stabilisation: hold E on the scanner bed. A lethal countdown runs |

**Lethal conditions:** MetHb ≥ 70 %, SaO2 ≤ 50 %, CO2 ≥ 70,000 ppm, dose ≥ 1,000 mSv, or ambient pressure ≤ 6.3 kPa (Armstrong limit).

## Hallucinations

Radiation, acute dose rate, hypoxia, hypercapnia and panic combine into a single hallucination intensity value. This drives:

- **Post-process:** UV warp, chromatic aberration, tunnel vision, grey-out, flashes.
- **Phantoms:** at low intensity, ambiguous sounds (hull knocking, footsteps, peripheral movement). At high intensity, shadow figures, whispers and radio voices.
- **Real structural groans** when seals contract in the cold, so sound alone can't tell you what is real.

**Ballistic awakening:** shooting a phantom makes it collapse into ash in under 1.5 s and briefly restores clarity. The bullet carries on and punctures the bulkhead behind it.

## Habitat: `AMarsBaseModule`

Each module simulates the following:

| System | Behaviour |
|---|---|
| Thermal | Thermostat heater against conductive loss. Nights fall to about −80 °C, occasionally −100 °C |
| Seals | Below their critical temperature, seals lose integrity and leak. Seal heaters reduce this |
| Pressure | Air escapes through punctures, breaches and seals. Life support repressurises the module. Martian ambient pressure is 0.6 kPa |
| Hull | Every bullet adds a hole. Below 35 % integrity the module **breaches catastrophically** |
| CO2 | Occupants add CO2 and the scrubber removes it. An emergency LiOH purge is available |

**Repairs:** patch punctures (sealant gun), weld the hull, service or re-torque seals (torque wrench).

**Power routing terminal:** cycles the RTG grid between Even Split, Airlock/EVA/Quarters +, Med-Bay +, Bio-Dome + and Comm-Relay +. Sectors without priority lose lighting.

## Stations

| Station | Purpose |
|---|---|
| Airlock panel | Cycle the airlock (requires the suit) |
| Suit rack | Put on or stow the MCP suit |
| O2 refill | Refill O2 |
| Dust scrubber | Decontaminate regolith dust |
| CO2 scrubber | Emergency LiOH purge (hold E) |
| Module status panel | Module readouts. Toggles seal heaters, amine scrubbers and the grow-light breaker |
| Water dispenser / food locker | Hydration and nutrition |
| Chlorella harvester | Harvest Chlorella biomass for food |
| Xeno-analyzer / microscope / containment box | Bio-agent culture work and assay |
| Diagnostic scanner bed | Full diagnostic scan, emergency stabilisation (hold E) |
| Bioprinter | Synthesise medication from feedstock |
| RTG core / power routing | Electrical grid |
| Comm-Relay terminal | Diagnostics, transistors, transceiver repair, transmission |
| Dish alignment terminal | Calibrate the orbital dish (hold E) |
| Storage lockers | Story supplies and data logs |
| Bunk / rest terminal | Sleep, or wait until 06:00, 12:00, 18:00 or 22:00 |
| Mission Log terminal | Manual save |

## Suit, shuttle and surface

- **MCP suit:** 100 % O2 at 30 kPa, LiOH CO2 scrubbing and a battery-powered heater. The battery drains twice as fast below −60 °C. If it dies outside, you get hypothermia. It recharges indoors.
- **RS-2 recon shuttle:** a flyable VTOL with its own battery and O2 reserve. It tops up your suit while you are seated.
- **Surface beacons** can be deployed with B. Story sites have their own beacons, shown on the compass.
- **Environment:** a day/night cycle, dust storms that build, peak and clear, and solar particle events.

## Items

Glock-18 and 9×19 mm rounds · weld/sealant gun and sealant cartridges · torque wrench · radiation dosimeter · O2 canister · methylene blue cartridge · potassium iodide pills · Neuro-Antitoxin · Amifostine · water pouch · food ration · Chlorella paste · regolith and rock samples · bio-agent culture · quantum transistor · spare parts · surface beacon.

New item types are shown in an inspect view the first time you pick them up. This can be turned off under Settings → Gameplay.

## Saving

| | |
|---|---|
| Automatic | Shortly after the opening shuttle departs, then every 240 s while inside a pressurised module and not in Stage 4 |
| Manual | MISSION LOG terminals |
| Saved state | Biometrics, suit, inventory, all modules and stations, story step, recovered logs, collected pickups |
| Loading | CONTINUE (main menu), LOAD LAST SAVE (pause menu), or the end-of-mission menu |

## Settings

Tabs: **Controls** (rebind any key), **Camera** (sensitivity, invert Y, smoothing, head-bob, weapon sway, FOV), **Audio** (master, SFX, ambience, A.R.I.A. voice, music, subtitles), **Display** (window mode, resolution, V-Sync, frame limit, quality), and **Gameplay** (inspect new items on pickup).
