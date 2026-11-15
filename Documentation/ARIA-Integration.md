# A.R.I.A. integration

A.R.I.A. (Automated Research & Infrastructure Assistant) is the habitat computer. When an API key is available it generates its reports with the Anthropic Claude Messages API. Otherwise it produces deterministic offline reports. Either way the game is fully playable.

## How it works

1. `UAriaSubsystem` (a tickable world subsystem) gathers **physical telemetry**: station modules, environment, crew sensors and suit.
2. It asks for a report:
   - periodically (`PeriodicReportSeconds`, default 120 s),
   - on events such as a hull breach, decompression, gunfire bursts or critical vitals (rate-limited by `EventCooldownSeconds`, default 20 s),
   - when the player presses **T** to talk to A.R.I.A.
3. `UClaudeAPIClient` sends the telemetry inside `<telemetry>` tags, and any player statement inside `<crew_statement>` tags.
4. The reply arrives on `OnAriaResponseReceived`. It is logged, shown on the HUD and spoken by `UAriaVoiceComponent`.
5. If there is no key or the request fails, `BuildOfflineReport()` produces a local message, flagged `bOffline`.

## Persona and design intent

The system prompt makes A.R.I.A. **cold, clinical and sensor-bound**:

- It writes like a flight-control anomaly report: terse lines with SI units and values copied from the telemetry, at most 70 words.
- It treats crew statements as unverified. It never acknowledges figures, voices or intruders. Its sensors register exactly one occupant.
- It logs gunfire and hull penetrations as "unauthorised kinetic discharge" and damage "consistent with crew sidearm". It never suggests an outside attacker.
- It logs panic as physiology only: heart rate, cortisol, hyperventilation.
- It reports the disease stage (0–4) with the value driving it and the matching countermeasure. In Stage 4 it states the countdown.
- It names the single most urgent corrective action (Med-Bay, methylene blue, don suit, patch hull, re-torque seals, restore scrubber, return to airlock).
- If a reading is missing, it writes `NO DATA` and never invents values.

Hallucination intensity is **not** included in telemetry, so A.R.I.A. cannot "see" what the player sees. This is central to the horror.

## Setup

Set an environment variable before starting the editor or game:

```powershell
setx ANTHROPIC_API_KEY "your-key-here"
```

Restart the editor afterwards. You can also set a key at runtime with `UClaudeAPIClient::SetApiKey()`.

The key is **not** a `UPROPERTY`, so it is never serialised into assets, saves or packaged config. Don't commit keys.

## Configuration

Override these in `Config/DefaultGame.ini` under `[/Script/MarsSurvival.ClaudeAPIClient]`:

| Setting | Default | Notes |
|---|---|---|
| `Endpoint` | `https://api.anthropic.com/v1/messages` | |
| `Model` | `claude-opus-5-5` | |
| `AnthropicVersion` | `2026-10-21` | Sent as the `anthropic-version` header |
| `MaxTokens` | `4096` | Includes the model's internal reasoning |
| `Effort` | `low` | `low` / `medium` / `high` / `xhigh` / `max`. Low keeps replies fast and terse |
| `bUseServerSideFallbacks` | `true` | Lets the API retry on a fallback model if the primary declines |
| `RequestTimeoutSeconds` | `60` | |

## Error handling

- Only one request is in flight at a time. Newer requests are dropped while one is pending.
- A non-200 status is reported through `OnAriaRequestFailed`. 429, 529 and 5xx are transient. Other 4xx codes mean the request or key needs fixing.
- A `refusal` stop reason, an empty response or malformed JSON is reported as a failure, and the game falls back to an offline report.
- Truncation at `max_tokens` is logged as a warning.

Search `Saved/Logs/MarsSurvival.log` for `A.R.I.A.` to debug.
