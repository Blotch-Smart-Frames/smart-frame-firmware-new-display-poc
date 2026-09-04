# EPD ghosting/persistence stress-test POC

Standalone ESP32-S3 / ESP-IDF C project that brings up a new 960x680 e-paper
panel from the manufacturer's sample driver, then displays two visually
distinct images back-to-back (charging -> splash screen) with no delay in
between, to stress-test e-ink ghosting and image persistence.

This is a one-shot test: it runs once on boot and stops. Watch the panel for
faint remnants of the first image bleeding through the second.

## Wiring

| Signal       | ESP32-S3 GPIO |
| ------------ | ------------- |
| MOSI         | 11            |
| SCLK         | 12            |
| CS           | 10            |
| DC           | 13            |
| RESET        | 14            |
| BUSY         | 21            |
| Power enable | 15            |

Pin mapping reused from the existing smart-frame-firmware 13.3" panel wiring.

## Build & flash

```
idf.py set-target esp32s3
idf.py build
idf.py flash monitor
```

Requires ESP-IDF (tested against v5.5.2, matching smart-frame-firmware's
toolchain) with `idf.py` on your PATH (e.g. `. $IDF_PATH/export.sh`).

## Images

`main/images.c` / `main/images.h` are generated (1bpp, MSB-first, row-byte-
aligned, 0xFF = white) from `raw-assets/images/charging.png` and
`raw-assets/images/splash screen.png` using smart-frame-firmware's
`scripts/generate-images.ts`. `charging.png` here is smart-frame-firmware's
350x350 icon composited onto a white 960x680 canvas (centered) first, since
`generate-images.ts` embeds images at their native resolution and the panel
needs a full 960x680 (81600-byte) buffer per frame. To regenerate after
swapping in different test images, from the smart-frame-firmware repo:

```
npx tsx scripts/generate-images.ts \
  --images-dir <path-to-this-repo>/raw-assets/images \
  --output-dir <path-to-this-repo>/main
```

then rename the generated `ImageData.c`/`ImageData.h` to `images.c`/`images.h`
(and fix the `#include` in `images.c`), or update `main.c`/`main/CMakeLists.txt`
to reference the new filenames/array names directly.
