# Frequent panel refresh investigation — 2026-09-12

## Failure and cause

The reported symptom was physical refreshes around 15:00 and 15:03 JST.
The retained UART log (firmware `e02cb56`) ends at 15:00:42, so it does not
establish the reset reason for the specific 15:03 refresh. It does contain two
instances of the same reproducible failure:

```text
I (1132845) wifi_manager: Initializing WiFi manager
ESP_ERROR_CHECK failed: esp_err_t 0x103 (ESP_ERR_INVALID_STATE)
func: wifi_manager_init
expression: esp_event_loop_create_default()
Rebooting...
...
I (1799594) wifi_manager: Initializing WiFi manager
ESP_ERROR_CHECK failed: esp_err_t 0x103 (ESP_ERR_INVALID_STATE)
func: wifi_manager_init
expression: esp_event_loop_create_default()
Rebooting...
```

The device uses `CONFIG_DISABLE_DEEP_SLEEP=y`. After a cycle, the application
tears WiFi down, delays, and initializes WiFi again in the same process. The
application default event loop survives teardown, but initialization treated its
already-exists status as fatal. A software reset then invokes cold-boot recovery,
which intentionally forces a panel refresh even for unchanged content and bypasses
the normal five-minute minimum. Thus an unintended reset can produce a refresh
within five minutes. Changing the HA update interval does not fix this lifecycle
error.

Teardown also retained custom event handlers and default WiFi driver handlers,
and intentional disconnects triggered the retry handler.

## Change

- Reuse the application's default event loop; return other explicit initialization
  errors after releasing partially acquired resources.
- Retain and unregister the WiFi/IP handler instances before freeing their state.
- Destroy the default WiFi netif with its matching ESP-IDF cleanup API.
- Suppress retries during intentional disconnection, preserving unexpected-loss retries.
- Track whether this manager started SNTP, and clear its completion bit before
  starting it so a fast reply is not discarded.
- Log reset reason/recovery status, skipped/deferred refreshes and the next wait.

HA schedules and the panel's minimum refresh policy remain unchanged.

## Verification

ESP-IDF v5.5.1 built the firmware successfully. The two CMake/CTest native suites
passed with MSVC 19.44: existing display/HTTP policy tests and the new WiFi lifecycle
suite. The latter exercises repeated cycles, ten injected initialization failures,
start failure, disconnect handling and immediate SNTP completion. These tests do
not model concurrent FreeRTOS callbacks.

On the ESP32-S3 PhotoPainter, a local experimental build capped only the wait to
30 seconds. From 15:10:56 through 15:14:16 JST, all five WiFi connections succeeded.
There was one initial panel refresh; the next four cycles logged unchanged-frame
skips, `cold_boot_recovery=0`, and monotonically increasing uptime. No aborts or
teardown reconnect attempts occurred. The ELECOM webcam showed the refreshed
image returning to a stable display.

A first attempt to restore the normal build retained the experimental object
because the restored source had an older modification time. Its 30-second log
exposed the mistake. The runtime was explicitly recompiled and reflashed; the
experimental cap is absent from the committed code and final deployed binary.

The final firmware contains production code from commit `929dbff`. Only the
application partition was written; NVS credentials and cached frames were retained.
The final binary SHA-256 is:

```text
4254782de2cc4229f4d2376ef5aabdfbb2328bbf9c20307ee66f2f2506751868
```

At 15:18:04 it booted, refreshed once from 15:18:10 to 15:18:29, and logged
`next wake in 691 seconds`, targeting the normal 15:30 schedule. Camera checks
at 15:21:46 and 15:23:51 (more than three and five minutes after refresh completion)
showed a stable display; UART recorded no intervening cycles, refreshes or resets.
The next normal cycle succeeded without a reset:

| Time (JST) | Observation |
| --- | --- |
| 15:30:00.894 | Next cycle, uptime 717688 ms, `cold_boot_recovery=0` |
| 15:30:04.239 | WiFi connected successfully |
| 15:30:11.415 | Panel refresh started |
| 15:30:30.825 | Panel refresh completed |
| 15:30:31.161 | WiFi teardown completed; `next wake in 1770 seconds` (16:00) |

Five-second camera sampling captured the panel changing during the refresh and
returning to the updated dashboard afterwards. No abort, software reboot, or
intentional-disconnect retry appeared in the normal-run log. The device was left
running this normal firmware and schedule. The observation covers one scheduled
reconnection after the initial flash, in addition to the accelerated cycles.

Raw UART logs and webcam images remain in the ignored local directory
`build/refresh-investigation/`; the report includes only relevant diagnostics.
The observations establish the WiFi lifecycle defect and its fix, but cannot
retrospectively prove the cause of the unrecorded 15:03 event. Battery/deep-sleep
operation and long-duration endurance are outside this validation.
