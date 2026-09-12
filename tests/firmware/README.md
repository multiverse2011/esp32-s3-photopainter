# Firmware host tests

These tests compile the production C reliability policy with the pinned ESP-IDF
image and run it as a native executable. The first suite covers HTTP status
classification, bounded `Retry-After` values, and the explicit cooldown bit
that prevents a manifest schedule from overriding authentication or rate-limit
responses. It also covers one-shot cold-boot recovery, conservative
minimum-refresh time handling, and the durable conditions required before
clearing a recovery marker.

The WiFi lifecycle suite compiles the production manager with synchronous
ESP-IDF test doubles. It checks 20 consecutive init/connect/sync/deinit cycles,
cleanup and recovery after failures at ten initialization stages, WiFi start
failure, intentional versus unexpected disconnects, and an immediate SNTP reply.
It checks that handlers, netifs and event groups are released after every cycle.
These doubles do not simulate FreeRTOS task races or actual WiFi behavior.

From the repository root:

```sh
docker run --rm -v "$PWD:/project" -w /project espressif/idf:v5.5.1 \
  bash -lc 'cmake -S tests/firmware -B /tmp/photopainter-firmware-host && \
            cmake --build /tmp/photopainter-firmware-host && \
            ctest --test-dir /tmp/photopainter-firmware-host --output-on-failure'
```

On Windows, run the same CMake build and CTest commands from a Visual Studio
Developer terminal with CMake available (use a local build directory such as
`build/firmware-host`). Both suites support MSVC as well as the container compiler.

## Hardware regression check

With `CONFIG_DISABLE_DEEP_SLEEP=y`, capture UART output across at least two
scheduled wake cycles without resetting the board or reconnecting a monitor
that toggles reset. The second cycle must connect successfully, keep increasing
uptime, and log `cold_boot_recovery=0`. Unchanged frames should log a refresh
skip. New frames should refresh only when the minimum interval allows it.
Check the physical panel as well as the log.

For a faster local experiment, temporarily cap only the final runtime wait at
30 seconds. Do not change the panel minimum interval or commit the cap. Afterwards
remove the cap, force recompilation of `ha_frame_runtime.c`, flash the normal
binary, and check that `next wake in ... seconds` agrees with the HA schedule.
Restoring a backup with an older file timestamp can otherwise leave a stale
experimental object in an incremental build.

See [the September 2026 investigation](../../docs/refresh-investigation-2026-09-12.md)
for the observed failure and device validation.
