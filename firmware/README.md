# Firmware — Guition 4 (ESP32-S3-4848S040) · panel `panels3`

Gotowe pliki firmware dla panela **Guition ESP32-S3-4848S040** (wariant `panels3`).
Wszystkie pliki zostały zbudowane **bez żadnych danych osobowych** (bez SSID, hasła
Wi-Fi ani tokena Home Assistant) — Wi-Fi i Home Assistant konfiguruje się po
pierwszym uruchomieniu przez interfejs WWW panela (`http://<ip-panela>` lub AP
`BETTA-Setup` / `http://192.168.4.1`).

Ready-to-flash firmware for the **Guition ESP32-S3-4848S040** panel (`panels3`
variant). Every file was built **without any personal data** (no Wi-Fi SSID /
password, no Home Assistant token) — Wi-Fi and Home Assistant are configured
after first boot via the panel's web UI.

---

## Pliki / Files

| Plik / File | Typ | Opis / Description |
|---|---|---|
| `guition-4-esp32s3.factory.bin` | Factory (merged) | Cały firmware w jednym pliku — flash pod adres `0x0`. / Full image in one file — flash at `0x0`. |
| `bootloader.bin` | Bootloader | Adres `0x0` |
| `partition-table.bin` | Tabela partycji | Adres `0x8000` |
| `ota_data_initial.bin` | Dane OTA (inicjalne) | Adres `0xf000` |
| `betta-ha-panel-s3.bin` | Aplikacja | Adres `0x20000` |

Parametry pamięci flash: **chip `esp32s3`**, **16 MB**, **DIO**, **80 MHz**.

### Wersja i sumy kontrolne / Version and checksums

| Plik / File | Rozmiar / Size | `SHA256` |
|---|---|---|
| `guition-4-esp32s3.factory.bin` | 3 382 400 B | `1EA3B5CC65104C6AF71D65ECF8C5F592480D459BBFA048B31BCDBAEE5178050C` |
| `betta-ha-panel-s3.bin` | 3 251 328 B | `014EADB219FFD366B974163C72B94BD69899A45F21C9629497A797410D8B8CC8` |
| `bootloader.bin` | 20 912 B | `400CB58DCFEF3F9D9758DE2197093E80A6336B09C810DA712B4725ACEA23C875` |
| `partition-table.bin` | 3 072 B | `8F444929E66F5C9FD262B2CCBCE847429E706D83F852EB47E8DE8D36B5127126` |
| `ota_data_initial.bin` | 8 192 B | `7D2C7AC4888BFD75CD5F56E8D61F69595121183AFC81556C876732FD3782C62F` |

Wersja firmware: **v0.8.2** (wariant `panels3`, optymalizacja `-O2` / `CONFIG_COMPILER_OPTIMIZATION_PERF=y`, WebUI serwowane jako gzip).
Firmware version: **v0.8.2** (`panels3` variant, `-O2` / `CONFIG_COMPILER_OPTIMIZATION_PERF=y`, WebUI served gzipped).

---

## Sposób 1 — jeden plik (najłatwiejszy) / One file (easiest)

Flash the merged factory image at address `0x0`:

```powershell
esptool.py --chip esp32s3 --port COM3 --baud 460800 write_flash 0x0 guition-4-esp32s3.factory.bin
```

> Port `COM3` zamień na swój port. / Replace `COM3` with your port.
> Możesz też użyć przeglądarkowego [esptool-js](https://espressif.github.io/esptool-js/) —
> wybierz `esp32s3`, plik `guition-4-esp32s3.factory.bin`, adres `0x0`.

## Sposób 2 — cztery pliki / Four files

Flash each file at its offset:

```powershell
esptool.py --chip esp32s3 --port COM3 --baud 460800 write_flash `
  0x0      bootloader.bin `
  0x8000   partition-table.bin `
  0xf000   ota_data_initial.bin `
  0x20000  betta-ha-panel-s3.bin
```

---

## Po wgraniu / After flashing

1. Panel uruchamia AP `BETTA-Setup` — połącz się i otwórz `http://192.168.4.1`. /
   The panel starts a `BETTA-Setup` access point — connect and open `http://192.168.4.1`.
2. Skonfiguruj Wi-Fi i Home Assistant (URL WebSocket + długoterminowy token). /
   Configure Wi-Fi and Home Assistant (WebSocket URL + long-lived access token).
3. Po restarcie panel dołącza do Twojej sieci — otwórz jego adres IP w przeglądarce. /
   After reboot the panel joins your network — open its IP address in a browser.

Aktualizacje OTA instaluje się później z edytora WWW (sekcja **Firmware Update**). /
Later updates are installed OTA from the web editor (**Firmware Update** section).
