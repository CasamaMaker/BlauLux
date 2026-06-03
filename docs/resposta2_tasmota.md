Thanks for pointing that out — you are absolutely right. The naive approach of writing to NVS on every received packet is a real vulnerability: at 1,000 packets/s an attacker can exhaust the ESP32 flash in roughly 100 seconds.

---

**What Matter (Tasmota) actually does**

Looking at `be_matter_counter.cpp`, Matter keeps replay protection entirely in RAM using a 32-bit sliding window bitmap — no flash writes at all:

```
max_counter = 150
window_bitmap = 0b...11010111   ← which of the last 32 nonces have been seen
Range covered: [119 .. 150]
```

A packet is accepted if its nonce is above `max_counter` (shifts the window) or falls within the window with its bit unset. No NVS involved — a flood attack cannot wear out the flash.

The trade-off: after a reboot, both `max_counter` and `window_bitmap` are gone. Matter leaves the post-reboot counter problem to the upper protocol layers (session management). For a sessionless IoT device like BlauLux, this gap remains unaddressed if Matter's counter is used as-is.

---

**My proposal: deferred write + witness + acceptance window**

To cover the post-reboot gap without the UX cost of an ahead-increment scheme, I combined two mechanisms:

**1. Deferred NVS write, capped at 1 write/hour**

The receiver stores the real `max_nonce` (not `max_nonce + N`) to NVS.

Write conditions: `(max_nonce != flash_nonce) AND (millis() - last_write >= 3_600_000)`.
This caps flash writes at **24/day**, giving a flash lifetime of ~11 years under normal use. Combined with rate limiting (max 10 packets/s + exponential backoff: 10 s → 60 s → 10 min), a flood attack cannot drive more than one NVS write per hour.

**2. Post-reboot: witness packet + acceptance window**

After a reboot, `X` (the last saved nonce) is loaded from NVS. Rather than accepting any nonce `> X` blindly (which would allow replay of packets sent since the last hourly save), the receiver uses a two-step handshake:

```
Acceptance window:  X < nonce < X + 100

Step 1 — Witness:
  First packet with nonce in (X, X+100):
    → AES-GCM verified ✅
    → stored as witness_nonce in RAM
    → NOT executed (command dropped)

Step 2 — Confirmation:
  Next packet where: B > witness_nonce  AND  X < B < X + 100:
    → accepted and executed ✅
    → boot_phase = false → normal monotonic operation resumes
```

From the user's perspective: press the button once (absorbed as witness), press again — device responds. Two presses to recover from a reboot.

**Why the window upper bound matters**

Without the upper bound (`X + 100`), any captured packet with `nonce > X` would be accepted after reboot. The window `(X, X+100)` limits the replay surface to at most the last ~100 nonces sent before the reboot (roughly the last hour of button presses). Packets captured earlier — or far ahead of the saved counter — fall outside the window and are rejected.

For an attacker to exploit this residual window they would need: (a) two previously captured valid AES-GCM packets, (b) both with nonces inside `(X, X+100)`, and (c) a reboot event to trigger the boot phase. For a domestic lamp this is a negligible threat.

---

**Comparison: Matter (Tasmota) vs my proposal**

| | Matter (Tasmota) — sliding window RAM | My proposal — deferred write + witness |
|---|---|---|
| Flash writes under flood | ✅ none (RAM only) | Max 1/hour (regardless of pkt rate) |
| Flash lifetime (normal use) | N/A | ~11+ years |
| Replay protection (normal) | ✅ 32-bit sliding window | ✅ strict monotonic |
| Replay protection post-reboot | ❌ not addressed | ⚠️ window of ~100 nonces |
| Recovery after reboot | ❌ not addressed | ✅ 2 button presses |

---

This is still a design proposal — I have not yet started the implementation. Any feedback on the witness + window approach, or vulnerabilities I may have overlooked, would be very welcome.
