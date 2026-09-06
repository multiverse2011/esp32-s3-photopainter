# Layout B reference

`layout.png` is the approved 800×480 design reference. Values and events are fictional samples. It uses IBM Plex Sans/Mono, Noto Sans JP for Japanese text, and MDI icons. Layout dimensions, colors, states, and spacing are defined in [selected-layout.md](../selected-layout.md).

The browser preview was checked at 800/736/360/320px in light/dark appearance with normal/stale/offline status and calendar overflow. At 800px, the calendar block gap measured approximately 38px and the Rooms-to-Forecast gap approximately 36px. All content fits the paper. Browser antialiasing is a visual approximation; the production renderer must emit native palette pixels and pass hardware acceptance.

## Icon license

Icons are from `@mdi/font` 7.4.47, [MaterialDesign-Webfont](https://github.com/Templarian/MaterialDesign-Webfont), distributed under [Apache-2.0](MDI-LICENSE.txt). Owner icons are `mdi:star-four-points` and `mdi:moon-waning-crescent` at 24px. Forecast icons are 32px.

Production assets must pin the source version and hash, preserve the applicable license, and render offline on HA. No product font binary or production icon mask is included in this specification package.
