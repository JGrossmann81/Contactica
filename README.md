# Contactica

A small, deliberately simple CRM for hunting new music-retail accounts: instrument shops, DJ shops and music schools, by region.
It sits beside a full CRM (HubSpot, NetSuite) and does one job: **find a shop, work it, win it.** Once an account buys, it moves to an
archive, out of your working list, but it still counts on the territory map. Windows desktop app; your data stays in one file on your PC.

## What it does
- **Accounts**: add by hand (quick-add box, Ctrl+N) or import CSV (Ctrl+I) with a column-mapping window and duplicate detection.
- **Pipeline**: Prospect, Contacted, Interested, Quoted. "They bought" archives the account as Won (with first-order value); Lost is archived too. Both can be reopened.
- **Follow-ups**: next-contact dates, overdue highlighting, a "Today" list, activity log (call / email / visit / note).
- **Brands**: tick which of your brands you are pitching to each shop (edit the list in Settings).
- **Three views**: pin map (pan, zoom, clustering), territory map (one tile per state/province: active, wins, win rate, follow-ups due; click a tile to filter), pipeline board (drag cards between stages, drop on Won).
- **Export**: won accounts (or any list) as CSV with HubSpot-style or NetSuite-style headers, ready for their import tools.
- **Safety**: autosave after every change, a `.bak` copy of the previous save, Undo/Redo (Ctrl+Z / Ctrl+Y).

## Data
Stored in `%APPDATA%\Contactica\contactica.json`. Put a GeoJSON file named `map.geojson` in that folder to replace the built-in rough US outline on the pin map.
Accounts without coordinates are drawn near the middle of their state (hollow pin); add latitude/longitude to place them exactly.

## Build
`cmake -S . -B build && cmake --build build --config Release`. Linux builds the tests only (`contactica_coretest`, `contactica_uitest`).
Windows CI (`.github/workflows/build.yml`) produces an installer and a zip.

## Honest limits
Written and tested headlessly; look and feel have not been checked by hand. The HubSpot/NetSuite export headers follow their standard import field names but have not been tried against live imports. The built-in map outline is rough.
