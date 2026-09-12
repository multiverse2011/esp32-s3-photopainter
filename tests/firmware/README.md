# Firmware host tests

These tests compile the production C reliability policy with the pinned ESP-IDF
image and run it as a native executable. The first suite covers HTTP status
classification, bounded `Retry-After` values, and the explicit cooldown bit
that prevents a manifest schedule from overriding authentication or rate-limit
responses.

From the repository root:

```sh
docker run --rm -v "$PWD:/project" -w /project espressif/idf:v5.5.1 \
  bash -lc 'cmake -S tests/firmware -B /tmp/photopainter-firmware-host && \
            cmake --build /tmp/photopainter-firmware-host && \
            ctest --test-dir /tmp/photopainter-firmware-host --output-on-failure'
```
