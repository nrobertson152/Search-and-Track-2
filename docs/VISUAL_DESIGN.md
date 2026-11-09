# Command-center presentation

Search and Track II uses a custom, engine-free X11 instrument panel renderer.

The UI evokes late Cold War NORAD and PVO equipment without reproducing a specific real-world control panel. Its dark olive chassis, physical screws, steel bezels, inset screens, amber warning lamps and green phosphor displays establish a consistent hardware language. The schematic theater map and radar PPI use different presentation models of the same simulation state.

## Interaction

- Seven stations on the left are buttons, not decoration. Their old keyboard shortcuts still work.
- Contact selection, scramble, classification, weapons authorization, and SAM launch remain functional.
- The PPI uses its actual configured radar station, range and sweep rather than a video overlay.
- Indicators derive their state from the simulation (paused, active, antenna active, sector integrity and so on).
- The title screen offers NORAD/PVO and loads an existing saved watch.

## Constraints

The renderer remains pure C17 and Xlib, with no proprietary image assets and no OpenGL requirement. CRT effects are approximate and drawn using opaque primitives, not post-processing bloom or shader-based lens distortion. The map is intentionally schematic. Future improvements could add raster glow, more detailed geography and stronger typographic hierarchy.

Screenshot composition tested with a 1440x900 game window under Xvfb. The minimum window size is 1120x860.
