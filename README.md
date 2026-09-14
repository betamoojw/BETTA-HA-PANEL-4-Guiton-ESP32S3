<!-- SPDX-License-Identifier: LicenseRef-FNCL-1.1 | Copyright (c) 2026 Cpt_Kirk -->
<img src="images/BETTAOS.jpg" alt="BETTA OS Logo" width="10%" />

# Guition 4 Wall Panel (BETTA HA Panel fork, `panels3`)

**Runtime-configurable Home Assistant wall panel for the Guition ESP32-S3-4848S040
(4.8" 480×480 touchscreen).** Build your dashboard directly on the device — no YAML
edits, no firmware rebuilds.

This repository is a **fork of [BETTA HA Panel v0.8.2](https://github.com/CptKirk/BETTA-HA-Panel)**
by **Cpt_Kirk**, extended with a set of custom features (Music Assistant integration,
Polish language, MQTT, static IP, 24 h auto-restart, screensaver with clock/date and
more — see [What's new in this fork](#whats-new-in-this-fork)).

> 🇵🇱 **Polska wersja dokumentacji:** [README.pl.md](README.pl.md)

---

## License & attribution (please read)

- Original project: **BETTA HA Panel v0.8.2** — Copyright (c) 2026 **Cpt_Kirk**.
- License: **[LicenseRef-FNCL-1.1](LICENSE)** (Federation Non-Commercial License v1.1) —
  **non-commercial**. Any commercial use requires a separate written license from the
  copyright holder (see §11 of the license).
- **This fork modifies the original software.** Per §3 of the license, the changes are
  clearly marked and described in
  [What's new in this fork](#whats-new-in-this-fork) and in [release-notes.md](release-notes.md).

---

## What's new in this fork

Custom additions on top of upstream BETTA HA Panel v0.8.2:

| Feature | Description |
|---|---|
| 🎵 **Music Assistant** | Music Assistant media-player integration with a dedicated music page (play/pause, track info, cover art). |
| 🇵🇱 **Polish language** | Full Polish translation of the editor and panel UI, plus Poppins fonts with full Polish diacritics (ą, ć, ę, ł, ń, ó, ś, ź, ż). |
| 📡 **MQTT** | Built-in MQTT client with Home Assistant auto-discovery (device shows up in HA automatically); configurable broker host/port/username/password/discovery prefix. |
| 🌐 **Static IP** | Optional static IP configuration (IP / netmask / gateway / DNS) in the Wi-Fi settings — no DHCP needed. |
| 🔁 **24 h auto-restart** | Automatic restart every N hours (1–168) with a configurable schedule, plus a persistent **error log viewer ("Logs")** and log download. Fixes graphics artifacts that appear after many hours of continuous operation. |
| ⏰ **Screensaver / dimming** | Screensaver with clock and date, brightness control, separate screen-off timeout, 24 h clock / seconds / date toggles, clock/date colors. |
| 🎨 **Theme editor** | Custom theme creation: edit colors on a live preview canvas, save as custom theme, import/export theme JSON. |
| 🧱 **Extended tiles** | Binary-sensor widget, switch-button icon selection, tile appearance options, sensor text color, sensor value rounding. |
| 💡 **LED effects** | RGB LED press/hold effects. |
| 🛠 **Stability fixes** | PSRAM / RGB PCLK lowered to 10 MHz (fixes screen "bushes"/flashing bars after long uptime), `max_uri_handlers` raised 32 → 40. |

---

## Supported hardware

This fork ships ready-built firmware for one variant:

| Variant   | Device                          | Resolution | Flash | Firmware                                                                 |
|-----------|---------------------------------|------------|-------|---------------------------------------------------------------------------|
| `panels3` | Guition **ESP32-S3-4848S040** (4.8") | 480 × 480 (ST7701S RGB, GT911 touch) | 16 MB + 8 MB PSRAM | [firmware/](firmware/) |

The source tree also still supports the upstream `panel4` and `panel10` variants
(see [release-notes.md](release-notes.md)), but this fork's custom features and the
ready-built binaries target `panels3`.

---

## Main features (inherited from upstream)

- **Live Home Assistant link** — WebSocket connection with REST fallback for forecasts and long-poll states.
- **On-device editor** — BETTA Editor in the browser at `http://<panel-ip>`; drag-and-drop widgets, multi-page layouts, room-grouped entity picker.
- **Widget library** — sensor, button, slider, graph, light, heating, weather, up to 5-day forecast, media player, todo list, Roborock, energy dashboard, empty tile.
- **Advanced light control** — brightness, color temperature, RGB — exposed only when Home Assistant reports the capability.
- **Energy dashboard** — automatic grid / solar / battery / gas / water visualization from the HA energy model.
- **Graphs** — line, smoothed line, or bar-chart modes; event-rate sampling up to 4096 points.
- **First-run provisioning** — `BETTA-Setup` Wi-Fi AP, guided Wi-Fi + Home Assistant setup, Quick Setup starter dashboard.
- **OTA updates** — upload an `.ota.bin` or an OTA URL from the web editor.
- **Multilingual** — built-in languages plus custom translation JSON upload/download.
- **Touch-friendly UX** — auto-dimming backlight after idle, pointer-capture drag/resize, stable GT911 touch startup.

---

## Web UI — settings screenshots

The panel is fully configured from the web editor (`http://<panel-ip>`):

| | | |
|---|---|---|
| ![Editor layout](images/screenshots/01-editor-layout.png) | ![Wi-Fi](images/screenshots/02-settings-wifi.png) | ![Home Assistant](images/screenshots/03-settings-ha.png) |
| ![Time](images/screenshots/04-settings-time.png) | ![Display / Screensaver](images/screenshots/05-settings-display.png) | ![MQTT](images/screenshots/06-settings-mqtt.png) |
| ![UI](images/screenshots/07-settings-ui.png) | ![Theme](images/screenshots/08-settings-theme.png) | ![Setup AP](images/screenshots/09-settings-ap.png) |
| ![Firmware Update](images/screenshots/10-settings-firmware.png) | ![System](images/screenshots/11-settings-system.png) | ![Logs](images/screenshots/12-settings-logs.png) |

### On the wall

![Guiton 4 panel mounted and working](images/panel-on-wall.jpg)

*Guiton 4 panel (ESP32-S3-4848S040) running the `panels3` firmware — mounted on the wall and working.*

---

## Getting started

1. **Flash** the factory image — see [firmware/README.md](firmware/README.md) for full
   instructions (single-file or four-file method).
2. **Reboot** the device. It opens a Wi-Fi AP called `BETTA-Setup`.
3. Connect to `BETTA-Setup`, open `http://192.168.4.1`, pick your country, scan for
   your network and save.
4. After reboot the panel joins your LAN. Open its IP in a browser, link Home Assistant
   via a long-lived access token, and build your first page with **Quick Setup**.

> ⚠️ **No personal data is compiled into the firmware.** Wi-Fi credentials, Home Assistant
> URL/token and MQTT credentials are entered only through the web UI and stored on the
> device (NVS / LittleFS). Do not publish your own `sdkconfig` overlays.

Future updates install via OTA from the editor — no cable needed.

---

## Building from source

Prerequisites:

- **ESP-IDF v5.5** (tested with v5.5.5), Python 3.11+
- Internet access on first build (the component manager downloads `lvgl`, `esp_lvgl_port`,
  `esp_lcd_st7701`, `esp_lcd_touch_gt911`, `littlefs`, `esp_websocket_client`, etc.)

```powershell
# Guition ESP32-S3-4848S040 (panels3) — the variant this fork targets:
idf.py -B build-panels3 -DSDKCONFIG_DEFAULTS="sdkconfig.defaults.s3;sdkconfig.defaults.panels3" build
```

> The top-level `CMakeLists.txt` infers the panel variant from `SDKCONFIG_DEFAULTS` and
> materialises `main/idf_component.yml` from `main/idf_component.panels3.yml` on every
> configure — you never edit `main/idf_component.yml` by hand.

Other upstream variants (unchanged from BETTA v0.8.2):

```powershell
idf.py -B build-panel4  -DSDKCONFIG_DEFAULTS="sdkconfig.defaults;sdkconfig.defaults.panel4"  build
idf.py -B build-panel10 -DSDKCONFIG_DEFAULTS="sdkconfig.defaults;sdkconfig.defaults.panel10" build
```

Build artifacts for `panels3` land in `build-panels3/` (`betta-ha-panel-s3.bin` +
bootloader / partition table / OTA data). To repackage the merged factory image, see
[tools/make_factory_bin.ps1](tools/make_factory_bin.ps1).

---

## Project structure

```
.
├── main/                  # Panel application (C, LVGL)
│   ├── ui/                #   LVGL screens, widgets, fonts (Poppins, MDI icons)
│   ├── mqtt/              #   MQTT client
│   └── idf_component.panels3.yml   # variant manifest (deps for the S3 board)
├── components/webui/www/  # Web editor (BETTA Editor) served by the panel
├── firmware/              # Ready-to-flash .bin files (panels3) + flash instructions
├── images/screenshots/    # Web-UI screenshots
├── partitions.s3.csv      # Partition table for the 16 MB S3 flash
├── sdkconfig.defaults.s3  # } Clean, credential-free build configs
├── sdkconfig.defaults.panels3
└── LICENSE                # FNCL v1.1 (Copyright (c) 2026 Cpt_Kirk)
```

---

## License

- Source released under [LicenseRef-FNCL-1.1](LICENSE) — **Copyright (c) 2026 Cpt_Kirk**.
- **Non-commercial only.** Commercial use requires a separate written license from the
  copyright holder (see §4 and §11 of the license).
- This is a **modified version** of BETTA HA Panel v0.8.2; changes are marked and
  described above. See [release-notes.md](release-notes.md) for upstream per-version
  history.

