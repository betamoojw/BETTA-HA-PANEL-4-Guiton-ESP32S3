<!-- SPDX-License-Identifier: LicenseRef-FNCL-1.1 | Copyright (c) 2026 Cpt_Kirk -->
<img src="images/BETTAOS.jpg" alt="BETTA OS Logo" width="10%" />

# Guition 4 — panel ścienny (fork BETTA HA Panel, wariant `panels3`)

**Konfigurowalny w locie panel ścienny Home Assistant dla urządzenia Guition
ESP32-S3-4848S040 (dotykowy ekran 4,8" 480×480).** Zbuduj swój pulpit bezpośrednio na
urządzeniu — bez edycji YAML i bez ponownego budowania firmware.

To repozytorium to **fork projektu [BETTA HA Panel v0.8.2](https://github.com/CptKirk/BETTA-HA-Panel)**
autorstwa **Cpt_Kirk**, rozszerzony o zestaw własnych funkcji (integracja Music Assistant,
język polski, MQTT, statyczny adres IP, automatyczny restart co 24 h, wygaszacz z zegarem
i datą oraz wiele więcej — patrz [Co nowego w tym forku](#co-nowego-w-tym-forku)).

> 🇬🇧 **English documentation:** [README.md](README.md)

---

## Licencja i autorstwo (przeczytaj koniecznie)

- Projekt oryginalny: **BETTA HA Panel v0.8.2** — Copyright (c) 2026 **Cpt_Kirk**.
- Licencja: **[LicenseRef-FNCL-1.1](LICENSE)** (Federation Non-Commercial License v1.1) —
  **do użytku niekomercyjnego**. Każde użycie komercyjne wymaga osobnej pisemnej licencji
  od właściciela praw autorskich (patrz §11 licencji).
- **Ten fork modyfikuje oryginalne oprogramowanie.** Zgodnie z §3 licencji zmiany są
  wyraźnie oznaczone i opisane w sekcji [Co nowego w tym forku](#co-nowego-w-tym-forku)
  oraz w [release-notes.md](release-notes.md).

---

## Co nowego w tym forku

Własne dodatki względem oryginalnego BETTA HA Panel v0.8.2:

| Funkcja | Opis |
|---|---|
| 🎵 **Music Assistant** | Integracja odtwarzacza Music Assistant z dedykowaną stroną muzyki (play/pauza, informacje o utworze, okładka). |
| 🇵🇱 **Język polski** | Pełne polskie tłumaczenie edytora i interfejsu panela oraz fonty Poppins z pełnymi polskimi znakami (ą, ć, ę, ł, ń, ó, ś, ź, ż). |
| 📡 **MQTT** | Wbudowany klient MQTT z auto-odkrywaniem w Home Assistant (urządzenie pojawia się w HA automatycznie); konfigurowalny host/port/użytkownik/hasło/prefiks discovery. |
| 🌐 **Statyczny adres IP** | Opcjonalna konfiguracja statycznego IP (IP / maska / brama / DNS) w ustawieniach Wi-Fi — bez DHCP. |
| 🔁 **Auto-restart 24 h** | Automatyczny restart co N godzin (1–168) z ustawianą godziną, a także trwała **przeglądarka logów błędów ("Logi")** i pobieranie logu. Rozwiązuje artefakty graficzne pojawiające się po wielu godzinach ciągłej pracy. |
| ⏰ **Wygaszacz / przyciemnianie** | Wygaszacz z zegarem i datą, regulacja jasności, osobny czas wyłączenia ekranu, przełączniki zegara 24 h / sekund / daty, kolory zegara i daty. |
| 🎨 **Edytor motywów** | Tworzenie własnych motywów: edycja kolorów na podglądzie na żywo, zapis jako własny motyw, import/eksport motywu w JSON. |
| 🧱 **Rozszerzone kafelki** | Widżet czujnika binarnego, wybór ikony przycisku-przełącznika, opcje wyglądu kafelków, kolor tekstu czujnika, zaokrąglanie wartości czujników. |
| 💡 **Efekty LED** | Efekty LED RGB przy wciśnięciu i przytrzymaniu. |
| 🛠 **Poprawki stabilności** | PSRAM / obniżenie PCLK RGB do 10 MHz (usuwa „krzaki"/migające paski po długiej pracy), podniesienie `max_uri_handlers` z 32 do 40. |

---

## Wspierany sprzęt

Ten fork zawiera gotowe firmware dla jednego wariantu:

| Wariant   | Urządzenie                          | Rozdzielczość | Flash | Firmware                                                                 |
|-----------|-------------------------------------|---------------|-------|---------------------------------------------------------------------------|
| `panels3` | Guition **ESP32-S3-4848S040** (4,8") | 480 × 480 (ST7701S RGB, dotyk GT911) | 16 MB + 8 MB PSRAM | [firmware/](firmware/) |

Źródła nadal obsługują warianty `panel4` i `panel10` z oryginału (patrz
[release-notes.md](release-notes.md)), ale własne funkcje tego forka i gotowe pliki
binarne dotyczą wariantu `panels3`.

---

## Główne funkcje (odziedziczone z oryginału)

- **Połączenie z Home Assistant na żywo** — WebSocket z awaryjnym REST dla prognoz i stanów.
- **Edytor na urządzeniu** — BETTA Editor w przeglądarce pod `http://<ip-panela>`; przeciąganie widżetów, układy wielostronicowe, wybór encji pogrupowanych po pomieszczeniach.
- **Biblioteka widżetów** — czujnik, przycisk, suwak, wykres, światło, ogrzewanie, pogoda, prognoza do 5 dni, odtwarzacz, lista zadań, Roborock, pulpit energii, pusty kafelek.
- **Zaawansowane sterowanie światłem** — jasność, temperatura barwowa, RGB — pokazywane tylko wtedy, gdy HA zgłasza taką możliwość.
- **Pulpit energii** — automatyczna wizualizacja sieć / fotowoltaika / bateria / gaz / woda na bazie modelu energii HA.
- **Wykresy** — linia, linia wygładzona lub słupki; próbkowanie do 4096 punktów.
- **Pierwsze uruchomienie** — AP `BETTA-Setup`, kreator Wi-Fi + Home Assistant, szybka konfiguracja pulpitu startowego.
- **Aktualizacje OTA** — wgranie `.ota.bin` lub adresu URL OTA z edytora WWW.
- **Wielojęzyczność** — wbudowane języki plus import/eksport własnych tłumaczeń JSON.
- **UX przyjazny dotykowi** — automatyczne przyciemnianie podświetlenia po bezczynności, stabilny start dotyku GT911.

---

## Interfejs WWW — zrzuty ekranu ustawień

Panel konfiguruje się w całości z edytora WWW (`http://<ip-panela>`):

| | | |
|---|---|---|
| ![Układ edytora](images/screenshots/01-editor-layout.png) | ![Wi-Fi](images/screenshots/02-settings-wifi.png) | ![Home Assistant](images/screenshots/03-settings-ha.png) |
| ![Czas](images/screenshots/04-settings-time.png) | ![Wyświetlacz / Wygaszacz](images/screenshots/05-settings-display.png) | ![MQTT](images/screenshots/06-settings-mqtt.png) |
| ![UI](images/screenshots/07-settings-ui.png) | ![Motyw](images/screenshots/08-settings-theme.png) | ![AP konfiguracyjny](images/screenshots/09-settings-ap.png) |
| ![Aktualizacja firmware](images/screenshots/10-settings-firmware.png) | ![System](images/screenshots/11-settings-system.png) | ![Logi](images/screenshots/12-settings-logs.png) |

> 📸 Zdjęcie panela zamontowanego na ścianie zostanie dodane przez autora.

---

## Pierwsze kroki

1. **Wgraj** obraz factory — pełna instrukcja w [firmware/README.md](firmware/README.md)
   (metoda jednoplikowa lub czteroplikowa).
2. **Zrestartuj** urządzenie. Panel uruchomi AP Wi-Fi o nazwie `BETTA-Setup`.
3. Połącz się z `BETTA-Setup`, otwórz `http://192.168.4.1`, wybierz kraj, zeskanuj
   sieć i zapisz.
4. Po restarcie panel dołączy do Twojej sieci. Otwórz jego adres IP w przeglądarce,
   połącz Home Assistant przez długoterminowy token i zbuduj pierwszą stronę funkcją
   **Quick Setup**.

> ⚠️ **Do firmware nie są wkompilowane żadne dane osobowe.** Dane Wi-Fi, adres/token
> Home Assistant i dane MQTT wprowadza się wyłącznie przez interfejs WWW i są one
> przechowywane na urządzeniu (NVS / LittleFS). Nie publikuj własnych plików
> `sdkconfig`.

Późniejsze aktualizacje instaluje się przez OTA z edytora — bez kabla.

---

## Budowanie ze źródeł

Wymagania:

- **ESP-IDF v5.5** (testowane z v5.5.5), Python 3.11+
- Dostęp do internetu przy pierwszym buildzie (menedżer komponentów pobiera `lvgl`,
  `esp_lvgl_port`, `esp_lcd_st7701`, `esp_lcd_touch_gt911`, `littlefs`,
  `esp_websocket_client` itd.)

```powershell
# Guition ESP32-S3-4848S040 (panels3) — wariant docelowy tego forka:
idf.py -B build-panels3 -DSDKCONFIG_DEFAULTS="sdkconfig.defaults.s3;sdkconfig.defaults.panels3" build
```

> Główny `CMakeLists.txt` wykrywa wariant panela na podstawie `SDKCONFIG_DEFAULTS`
> i generuje `main/idf_component.yml` z `main/idf_component.panels3.yml` przy każdej
> konfiguracji — nie edytuje się `main/idf_component.yml` ręcznie.

Pozostałe warianty z oryginału (bez zmian względem BETTA v0.8.2):

```powershell
idf.py -B build-panel4  -DSDKCONFIG_DEFAULTS="sdkconfig.defaults;sdkconfig.defaults.panel4"  build
idf.py -B build-panel10 -DSDKCONFIG_DEFAULTS="sdkconfig.defaults;sdkconfig.defaults.panel10" build
```

Wyniki budowania dla `panels3` trafiają do `build-panels3/` (`betta-ha-panel-s3.bin` +
bootloader / tablica partycji / dane OTA). Aby ponownie zbudować scalony obraz factory,
zobacz [tools/make_factory_bin.ps1](tools/make_factory_bin.ps1).

---

## Struktura projektu

```
.
├── main/                  # Aplikacja panela (C, LVGL)
│   ├── ui/                #   Ekrany LVGL, widżety, fonty (Poppins, ikony MDI)
│   ├── mqtt/              #   Klient MQTT
│   └── idf_component.panels3.yml   # manifest wariantu (zależności dla płyty S3)
├── components/webui/www/  # Edytor WWW (BETTA Editor) serwowany przez panel
├── firmware/              # Gotowe pliki .bin (panels3) + instrukcja wgrywania
├── images/screenshots/    # Zrzuty ekranu interfejsu WWW
├── partitions.s3.csv      # Tablica partycji dla 16 MB flash S3
├── sdkconfig.defaults.s3  # } Czyste konfiguracje budowania bez danych osobowych
├── sdkconfig.defaults.panels3
└── LICENSE                # FNCL v1.1 (Copyright (c) 2026 Cpt_Kirk)
```

---

## Licencja

- Źródła udostępnione na licencji [LicenseRef-FNCL-1.1](LICENSE) — **Copyright (c) 2026 Cpt_Kirk**.
- **Wyłącznie do użytku niekomercyjnego.** Użycie komercyjne wymaga osobnej pisemnej
  licencji od właściciela praw autorskich (patrz §4 i §11 licencji).
- To jest **zmodyfikowana wersja** BETTA HA Panel v0.8.2; zmiany są oznaczone i opisane
  powyżej. Historia wersji oryginału: [release-notes.md](release-notes.md).
