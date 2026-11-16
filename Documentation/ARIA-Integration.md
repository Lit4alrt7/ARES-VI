# A.R.I.A. integration

A.R.I.A. (Automated Research & Infrastructure Assistant) is the habitat computer, and it's the only voice the player hears. It is written as a character: cold, clinical, and limited to what its instruments measure.

## How it works

1. `UAriaSubsystem` (a tickable world subsystem) gathers **physical telemetry**: station modules, environment, crew sensors and suit.
2. It produces a report:
   - periodically (`PeriodicReportSeconds`, default 120 s),
   - on events such as a hull breach, decompression, gunfire bursts or critical vitals (rate-limited by `EventCooldownSeconds`, default 20 s),
   - when the player presses **T** to talk to A.R.I.A.
3. By default the report comes from `BuildOfflineReport()`, a deterministic generator that turns telemetry into short status lines.
4. The message is logged, shown on the HUD and spoken by `UAriaVoiceComponent`, a procedural formant voice.

## Character rules

A.R.I.A. writes like a flight-control anomaly report:

- Terse lines with SI units and values copied from telemetry.
- It treats the player's statements as unverified. It never acknowledges figures, voices or intruders. Its sensors register exactly one occupant.
- It logs gunfire and hull penetrations as "unauthorised kinetic discharge" and damage "consistent with crew sidearm". It never suggests an outside attacker.
- It logs panic as physiology only: heart rate, cortisol, hyperventilation.
- It reports the disease stage (0–4) with the value driving it and the matching countermeasure, and names the single most urgent corrective action.
- If a reading is missing, it writes `NO DATA`.

Hallucination intensity is **never** included in telemetry, so A.R.I.A. cannot "see" what the player sees. This is central to the horror.

## Optional online mode (developer feature)

The `AI/` folder also contains an HTTP client, `UClaudeAPIClient`, that can send the same telemetry to the Anthropic Messages API and use the reply as A.R.I.A.'s report, with the character rules above as the system prompt.

- **It is off unless a developer sets an API key** in the `ANTHROPIC_API_KEY` environment variable. Release builds sent to players make no network calls unless the player has set that variable themselves.
- The key is never a `UPROPERTY`, so it is never saved into assets, saves or config. Don't commit keys.
- Settings live in `Config/DefaultGame.ini` under `[/Script/MarsSurvival.ClaudeAPIClient]`: `Endpoint`, `Model`, `AnthropicVersion`, `MaxTokens`, `Effort`, `bUseServerSideFallbacks`, `RequestTimeoutSeconds`.
- Only one request is in flight at a time. Any failure (no key, HTTP error, refusal, empty or malformed reply) falls back to the offline report.

Search `Saved/Logs/MarsSurvival.log` for `A.R.I.A.` to debug.
