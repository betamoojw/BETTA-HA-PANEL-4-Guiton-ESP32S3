<!-- SPDX-License-Identifier: LicenseRef-FNCL-1.1 | Copyright (c) 2026 Cpt_Kirk -->
<img src="images/BETTAOS.jpg" alt="BETTA OS Logo" width="10%" />

# Panel ścienny Guition 4 (fork BETTA HA Panel, wariant `panels3`)

**Konfigurowalny w locie panel ścienny Home Assistant dla Guition ESP32-S3-4848S040
(ekran dotykowy 4,8", 480×480).** Swój pulpit budujesz bezpośrednio na urządzeniu —
bez edycji YAML i bez przebudowywania firmware.

To repozytorium to **fork [BETTA HA Panel v0.8.2](https://github.com/CptKirk/BETTA-HA-Panel)**
autorstwa **Cpt_Kirk**, rozbudowany o duży zestaw własnych funkcji: znacznie bogatszy
mechanizm wyglądu kafelków i stron, siedem wbudowanych motywów z automatycznym
przełączaniem dzień/noc, wygaszacz z zegarem typu flip, sześć nowych typów kafelków,
głęboką integrację z **Alarmo** z Home Assistant, profesjonalną diagnostykę oraz
kopię zapasową i odtwarzanie, obsługę karty microSD, mniejszy i szybszy firmware
oraz pełną polską lokalizację.

> 🇬🇧 **English documentation:** [README.md](README.md)

---

## Spis treści

- [Licencja i autorstwo (przeczytaj)](#licencja-i-autorstwo-przeczytaj)
- [Najważniejsze nowości](#najważniejsze-nowości)
- [Pełna, szczegółowa lista usprawnień](#pełna-szczegółowa-lista-usprawnień)
  - [A. Dotyk, reakcja panelu, płynność](#a-dotyk-reakcja-panelu-płynność)
  - [B. Wygląd kafelka (`tile_*`)](#b-wygląd-kafelka-tile_)
  - [C. Wygląd strony (`page_*`)](#c-wygląd-strony-page_)
  - [D. Motywy](#d-motyw)
  - [E. Wygaszacz ekranu i zegar](#e-wygaszacz-ekranu-i-zegar)
  - [F. Nowe kafelki i integracja z Alarmo](#f-nowe-kafelki-i-integracja-z-alarmo)
  - [G. Sieć, stabilność, diagnostyka](#g-sieć-stabilność-diagnostyka)
  - [H. Edytor WWW](#h-edytor-www)
  - [I. Rozmiar i szybkość firmware](#i-rozmiar-i-szybkość-firmware)
  - [J. Karta microSD](#j-karta-microsd)
  - [K. Wyrównanie wariantów, kopie, dokumentacja](#k-wyrównanie-wariantów-kopie-dokumentacja)
  - [Świadomie poza zakresem](#świadomie-poza-zakresem)
- [Zrzuty ekranu — panel](#zrzuty-ekranu--panel)
- [Zrzuty ekranu — edytor WWW](#zrzuty-ekranu--edytor-www)
- [Obsługiwany sprzęt](#obsługiwany-sprzęt)
- [Funkcje przejęte z upstreamu](#funkcje-przejęte-z-upstreamu)
- [Pierwsze uruchomienie](#pierwsze-uruchomienie)
- [Budowanie ze źródeł](#budowanie-ze-źródeł)
- [Budżet pamięci flash i wydajność](#budżet-pamięci-flash-i-wydajność)
- [Sekcje edytora WWW](#sekcje-edytora-www)
- [Biblioteka kafelków](#biblioteka-kafelków)
- [API HTTP](#api-http)
- [Klucze poleceń MQTT](#klucze-poleceń-mqtt)
- [Uwagi i pułapki](#uwagi-i-pułapki)
- [Struktura projektu](#struktura-projektu)
- [Prywatność — brak danych osobowych w repozytorium](#prywatność--brak-danych-osobowych-w-repozytorium)
- [Licencja](#licencja)

---

## Licencja i autorstwo (przeczytaj)

- Projekt oryginalny: **BETTA HA Panel v0.8.2** — Copyright (c) 2026 **Cpt_Kirk**.
- Licencja: **[LicenseRef-FNCL-1.1](LICENSE)** (Federation Non-Commercial License v1.1) —
  **wyłącznie niekomercyjna**. Każde użycie komercyjne wymaga osobnej, pisemnej licencji
  od właściciela praw autorskich (patrz §11 licencji).
- **Ten fork modyfikuje oryginalne oprogramowanie.** Zgodnie z §3 licencji zmiany są
  wyraźnie oznaczone i opisane w sekcji
  [Pełna, szczegółowa lista usprawnień](#pełna-szczegółowa-lista-usprawnień)
  oraz w [release-notes.md](release-notes.md).
- To projekt prywatny i **nie** jest oficjalnym wydaniem BETTA HA Panel; numer wersji
  upstreamu jest celowo zachowany.

---

## Najważniejsze nowości

| Funkcja | Opis |
|---|---|
| 🎨 **Silnik wyglądu kafelka** | Tło, gradient, obramowanie, promień, kształt narożników, krycie, cień, rozmiar czcionki oraz pięć niezależnych kolorów tekstu (tytuł / opis encji / wartość / ikona / cały kafelek). Siedem gotowych presetów, cztery akcje „kopiuj wygląd” i reset. |
| 🖼 **Wygląd strony** | Kolor tła, gradient, użycie tapety, przyciemnienie tapety, motyw tylko dla tej strony i osiem presetów strony. |
| 🌗 **7 motywów + dzień/noc** | `dark_v2`, `classic_v1`, `light`, `ocean`, `contrast`, `oled`, `retro`, dodatkowo motywy własne, motyw dla pojedynczej strony oraz automatyczne przełączanie dzień/noc w zadanym oknie godzinowym. |
| ⏰ **Wygaszacz z zegarem flip** | Zegar klasyczny lub flip (cyfry na przewijanych kartach), 12 h / 24 h, opcjonalna plakietka AM/PM, data, sekundy, własne kolory, przyciemnienie, tryb nocny i własna tapeta. |
| 🧱 **6 nowych kafelków** | `cover_tile`, `scene_tile`, `person_tile`, `timer_tile`, `alarm_tile` oraz zwykły zegar (`clock_alarm`) — obok biblioteki upstreamu. |
| 🚨 **Integracja z Alarmo** | Pięć trybów uzbrojenia, usługi Alarmo, klawiatura PIN, automatyczny PIN z atrybutów encji, lista otwartych i pominiętych czujek, pominięcie opóźnienia, potwierdzenie uzbrojenia z `force`, odliczanie opóźnienia, maski gotowości trybów i powód odmowy uzbrojenia wprost ze zdarzeń Alarmo. |
| 📊 **Diagnostyka** | Statystyki pamięci dla każdej puli, liczniki łącza Wi-Fi i HA, przyczyny rozłączeń, brakujące encje, watchdogi UI i boot guard oraz dziennik systemowy czytany w przeglądarce. |
| 💾 **Kopia zapasowa / odtwarzanie** | Cała konfiguracja w jednym pliku JSON — układ, publiczne ustawienia i motywy własne. **Sekrety nigdy nie są eksportowane.** |
| 🗂 **Karta microSD** | Montowanie, formatowanie, przeglądanie i usuwanie plików, tapeta wygaszacza na karcie, okresowy eksport logów, ochrona przed wyjściem ze ścieżki. |
| ⚡ **Mniejszy i szybszy** | Build z `-O2`, interfejs WWW kompresowany gzip: zasoby WWW −501 kB, firmware −483 kB (≈23 % wolnej partycji aplikacji), szybsze odpowiedzi API i szybsza przebudowa układu. |
| 🇵🇱 **Język polski** | Pełna polska lokalizacja edytora i panelu, czcionki Poppins z polskimi znakami, teksty PL/EN dla każdej nowej funkcji. |
| 📡 **MQTT** | Automatyczne wykrywanie w HA oraz długa lista nowych kluczy poleceń (strony, wygaszacz, zegar, paski, jasność, animacje). |
| 🌐 **Statyczne IP** | Opcjonalne IP / maska / brama / DNS. |
| 🔁 **Auto-restart i logi** | Zaplanowany restart co N godzin (1–168) i trwały dziennik systemowy — rozwiązanie problemu artefaktów grafiki po wielu godzinach pracy. |
| 👆 **Komfort dotyku** | Dotyk całego toru suwaka, reakcja kafelka na naciśnięcie, animowane zmiany wartości i animowane przejścia stron. |

---

## Pełna, szczegółowa lista usprawnień

Wszystko poniżej powstało na bazie upstreamu v0.8.2 w dniach **15–17 września 2026**,
na działającym panelu `panels3`. Lista zawiera **225 pojedynczych zmian**, pogrupowanych
tematycznie. Ścieżki plików są względne wobec katalogu firmware.

### A. Dotyk, reakcja panelu, płynność

1. **Nowy moduł `main/ui/ui_slider_touch.c/.h`** — szeroki pas dotyku suwaków: `ui_slider_touch_enable_padded(slider, pad_along, pad_cross)`.
2. **Kliknięcie w dowolnym miejscu toru ustawia wartość** — wcześniej działało wyłącznie złapanie gałki (pas ~44 px).
3. **`w_slider.c`** — suwak ogólny przeszedł na nowy pas dotyku (był jednym z najbardziej „opornych”).
4. **`w_light_tile.c`** — suwak jasności i koloru światła na nowym dotyku.
5. **`w_media_player.c`** — suwak głośności odtwarzacza na nowym dotyku.
6. **`ui_screen_settings.c/.h`** — suwak jasności ekranu na nowym dotyku.
7. **Nowy moduł `main/ui/ui_press_feedback.c/.h`** — wizualna reakcja kafelka na naciśnięcie (zmniejszenie i przyciemnienie), żeby panel nie sprawiał wrażenia opóźnionego.
8. **Ustawienia efektu naciśnięcia** — `tile_press_fx`, `tile_press_fx_dim`, `tile_press_fx_scale` (czytane też z MQTT i `/api/settings`).
9. **Nowy moduł `main/ui/ui_value_anim.c/.h`** — `ui_value_anim_set_text()` zamiast `lv_label_set_text()`; wartości „przeliczają się” zamiast przeskakiwać.
10. **Ustawienia animacji wartości** — `value_anim`, `value_anim_ms` (domyślnie `count`, 320 ms, maks. 1500 ms).
11. **Nowy moduł `main/ui/ui_page_transition.c/.h`** — animowane wejście strony; domyślnie `fade`, 220 ms, maks. 1200 ms (animuje się tylko strona wchodząca).
12. **Ustawienia przejść stron** — `page_transition`, `page_transition_ms`, przycisk „Zastosuj” w edytorze WWW i wpis `page_trans` w logu.
13. **`ui_runtime.c` — watchdog UI z „kopniakiem”** — długie przebudowy układu zgłaszają postęp, więc „wolne, ale zdrowe” nie kończy się restartem.
14. **Naprawa niewidocznego tekstu w kafelku alarmu** — `theme_default_style_card()` nie ustawia `text_color`, a etykiety bez jawnego koloru dziedziczyły `#212121`; dodano 5 wywołań `lv_obj_set_style_text_color()` (tytuł, strefa, stan, linia informacyjna, lista czujek).
15. **Zasada na przyszłość:** każda nowa etykieta kafelka **musi** mieć jawny kolor i flagę `LV_OBJ_FLAG_USER_1/2/3`, żeby działały nadpisania z edytora WWW.
16. **Zastosowanie stylu kafelka w starszych widgetach** — `w_button.c`, `w_binary_sensor.c`, `w_empty_tile.c`, `w_heating_tile.c`, `w_roborock.c`, `w_todo.c` (wcześniej część kafelków całkowicie ignorowała ustawienia wyglądu).
17. **Korekty inicjalizacji panelu** — `main/drivers/display_init_panels3.c` (480×480).

### B. Wygląd kafelka (`tile_*`)

18. **`tile_bg_color`** — kolor tła kafelka.
19. **`tile_bg_grad_color`** — drugi kolor gradientu.
20. **`tile_bg_grad_dir`** — kierunek gradientu (`none` / `hor` / `ver`).
21. **`tile_border_color`** — kolor obramowania.
22. **`tile_border_width`** — grubość obramowania w px.
23. **`tile_radius`** — promień narożników w px.
24. **`tile_opacity`** — krycie tła w %.
25. **`tile_shadow`** — cień (0/1).
26. **`tile_font_scale`** — rozmiar czcionki: `auto` / `s` / `m` / `l` / `xl`.
27. **`tile_text_color`** — kolor całego tekstu kafelka (nadrzędny).
28. **`tile_title_color`** — osobny kolor **tytułu** kafelka.
29. **`tile_label_color`** — osobny kolor **opisu encji**.
30. **`tile_value_color`** — osobny kolor **wartości / statusu** (temperatura, wilgotność, moc W itd.).
31. **`tile_icon_color`** — kolor ikony.
32. **Mechanizm flag `LV_OBJ_FLAG_USER_1/2/3` + `ui_widget_factory_apply_tile_style()`** — dzięki temu rozłączne kolory tytułu/opisu/wartości działają w **każdym** typie kafelka, nie tylko w kilku.
33. **Dziedziczenie z motywu** — puste pole oznacza „kolor z motywu”, `-1` oznacza „brak” (nic nie jest „na sztywno”).
34. **Kształt kafelka** — presety `custom`, `square` (0 px), `soft` (10 px), `rounded` (16 px), `pill` (40 px), `circle` (maksymalny promień; kwadratowy kafelek staje się kołem).
35. **Siedem presetów wyglądu** — `auto`, `graphite`, `emerald`, `amber`, `violet`, `sky`, `glass`.
36. **„Kopiuj wygląd z”** — `layout.tile_look.copy_source` wskazuje kafelek źródłowy.
37. **„Kopiuj wygląd”** — `layout.tile_look.copy_apply` kopiuje na wskazany kafelek.
38. **„Zastosuj do wszystkich kafelków”** — `layout.tile_look.copy_apply_page` kopiuje na całą stronę.
39. **Kopiowany jest wyłącznie wygląd** — tło, obramowanie, kolory i rozmiar czcionki; encja, tytuł i wymiary pozostają nietknięte.
40. **„Przywróć domyślny wygląd”** — `layout.tile_look.reset` czyści wszystkie pola `tile_*` (powrót do motywu).
41. **`main/ui/ui_tile_style.h/.c`** — jeden spójny model wyglądu kafelka używany przez wszystkie widgety.
42. **Kolory wartości w kafelkach danych** — `w_sensor.c`, `w_weather_tile.c`, `w_graph.c`.
43. **18 typów kafelków w rejestrze** — `sensor`, `button`, `slider`, `graph`, `empty_tile`, `light_tile`, `heating_tile`, `weather_tile`/`weather_3day`, `todo_list`, `media_player`, `roborock_tile`, `binary_sensor`, `alarm_tile`, `clock_alarm`, `cover_tile`, `scene_tile`, `person_tile`, `timer_tile` (`ui_widget_factory.c`).

### C. Wygląd strony (`page_*`)

44. **`page_bg_color`** — kolor tła strony.
45. **`page_bg_grad_color`** — drugi kolor gradientu tła strony.
46. **`page_bg_grad_dir`** — kierunek gradientu (`none` / `hor` / `ver`).
47. **`page_wallpaper`** — użycie tapety jako tła strony.
48. **`page_dim`** — przyciemnienie tapety w zakresie 0–90 %.
49. **`page_theme`** — identyfikator motywu **tylko dla tej strony** (puste = motyw globalny / dzienno-nocny).
50. **Osiem presetów strony** — `auto`, `midnight`, `deep_sea`, `forest`, `sunset`, `plum`, `wallpaper`, `wallpaper_dim`.
51. **Reset wyglądu strony** — `layout.page_look.reset` → `ui_page_style_reset`.
52. **`main/ui/ui_page_style.h/.c`** — model wyglądu strony.
53. **Walidacja stylu strony w `layout_validate.c`** — m.in. długość i typ `page_theme`.
54. **Tapeta nigdy nie jest niszczona** — motyw lub tło strony jedynie ją nakłada albo przyciemnia.

### D. Motywy

55. **Motyw `oled` (OLED Black)** — tło `#000000`, nowy motyw wbudowany.
56. **Motyw `retro` (Retro Amber)** — bursztynowy „CRT”, drugi nowy motyw wbudowany.
57. **Razem siedem motywów wbudowanych** — `dark_v2` (domyślny), `classic_v1`, `light`, `ocean`, `contrast`, `oled`, `retro` (potwierdzone przez `GET /api/themes`).
58. **Motyw dla pojedynczej strony** — pole `page_theme` w layoucie strony oraz podgląd w edytorze WWW („Motywy → Strona”).
59. **Nowy moduł `main/ui/ui_theme_router.c/.h`** — kolejność rozstrzygania: `page_theme` strony → motyw dzienny/nocny wg godziny → motyw globalny.
60. **Automatyczny motyw dzień/noc wg godziny** — nowe ustawienia `theme_auto_enabled`, `theme_day_id`, `theme_night_id`.
61. **Własne okno nocne** — brane z `display_night_start_min` / `display_night_end_min` (te same godziny co harmonogram jasności), ale działające **także gdy harmonogram jasności nocnej jest wyłączony**.
62. **Bez zsynchronizowanego zegara (SNTP) używany jest motyw dzienny** — brak migania w złym motywie po starcie.
63. **Ręczny wybór motywu nadpisuje bieżącą połowę doby** — do zmiany granicy okna lub restartu.
64. **Zastosowanie na żywo, bez restartu** — `PUT /api/settings` odpowiada `reboot:false`.
65. **Motyw zmienia się bez przebudowy layoutu** — kolory bez nadpisań `tile_*`/`page_*` idą z motywu.
66. **Motywy własne** — `theme_store` oraz edytor motywów.
67. **API motywów dla C** — `theme_palette_active()`, `theme_palette_active_id()`, `theme_palette_active_name()`, `theme_palette_find_builtin()`, `theme_palette_builtin_list()`, `theme_palette_set_active()`, `theme_palette_activate_by_id()`, `theme_palette_to_json()`, `theme_palette_from_json()`.
68. **Czyszczenie nieudanego identyfikatora motywu** — `api_theme.c` nie zostawia panelu z pustą paletą.
69. **Pusta lub niepoprawna nazwa motywu nie wywala interfejsu** — walidacja przed aktywacją.
70. **Weryfikacja na sprzęcie** — strona z `page_theme=retro` przełączyła paletę po aktywacji, a powrót na inną stronę przywrócił motyw globalny.

### E. Wygaszacz ekranu i zegar

71. **Zegar wyrównany do dołu ekranu** (`LV_ALIGN_BOTTOM_MID`) z odstępem `APP_DISPLAY_SAVER_CLOCK_BOTTOM_GAP` 26 px — godzina nie jest już na środku.
72. **Data 8 px pod zegarem** — jak w panelach typu Tuya.
73. **Styl zegara `classic`** — dotychczasowy płaski zegar.
74. **Styl zegara `flip`** — cyfry zbudowane z osobnych kart-kafelków (czarne płytki z cyfrą), które przy zmianie minuty przewijają się w pionie.
75. **Geometria kart flip w `app_config.h`** — rozmiar, odstęp, promień i grubość kart (`APP_DISPLAY_SAVER_FLIP_*`).
76. **Czas animacji przewijania `APP_DISPLAY_SAVER_FLIP_ANIM_MS` = 150 ms** (testowe 3000 ms cofnięte do wartości produkcyjnej).
77. **Implementacja flip** — `flip_build()`, `flip_tick()`, przebudowa w `destroy_overlay()` / `ensure_overlay()` w `ui_screen_saver.c`.
78. **Format godziny 12 h / 24 h** — nowe ustawienie `clock_24h` (domyślnie 24 h), wybór w WWW jako „Format godziny” oraz w `/api/settings`.
79. **Brak wiodącego zera w trybie 12 h** — np. „7:05” zamiast „07:05”.
80. **Plakietka AM/PM** — obok cyfr, na czarnej płytce 50×46 px, czcionka `APP_FONT_DISPLAY_28`, odstęp `APP_DISPLAY_SAVER_AMPM_GAP`.
81. **Plakietka działa w OBU stylach zegara** — zarówno klasycznym, jak i flip.
82. **Naprawdę pogrubiony napis AM/PM** — 5 nałożonych kopii tego samego `lv_label` (`AMPM_COPY_COUNT` 5, przesunięcia 0,0 / +1,0 / −1,0 / 0,+1 / 0,−1 px), co daje kreskę ~3 px.
83. **Powód triku z kopiami** — `lv_draw_sw_letter.c` honoruje `outline_stroke_*` **wyłącznie** dla fontów wektorowych (`LV_FONT_GLYPH_FORMAT_VECTOR`, wymaga `LV_USE_FREETYPE` + `LV_USE_VECTOR_GRAPHIC` + `LV_USE_THORVG`), a wszystkie fonty panelu to bitmapy A1..A8 — obrys był całkowicie ignorowany.
84. **Przesunięcie rzędu kart o 31 px w trybie 12 h** — styl flip ma o jedną kartę mniej, więc rząd przesuwa się o (50+2+10)/2, żeby pozostał wyśrodkowany.
85. **W trybie 24 h nie ma plakietki** i zegar wraca do pełnego wyśrodkowania.
86. **Zmierzona geometria klasyk + 12 h** — cyfry 124..295 px, płytka 308..357 px.
87. **Zmierzona geometria flip + 12 h** — karty 41..378 px, płytka 389..438 px.
88. **Weryfikacja pikselowa wszystkich czterech kombinacji** — styl × format, na klatkach `/api/screenshot.bmp`.
89. **Przełączniki wygaszacza** — zegar 24 h, pokazuj sekundy, pokazuj datę.
90. **Kolory wygaszacza** — zegar `0xFFFFFF`, data `0xC8C8C8`.
91. **Jasność wygaszacza 20 %, wejście po 120 s**, próg wyłączenia ekranu 300 s (konfiguracja na żywym panelu).
92. **Auto-wyłączanie ekranu domyślnie WYŁĄCZONE** — `APP_DISPLAY_SCREEN_OFF_ENABLED 0` w `main/app_config.h`; przełącznik w WWW nadal pozwala je włączyć ręcznie.
93. **Wartość domyślna z jednego `#define`** — `runtime_settings.c` czyta `APP_DISPLAY_SCREEN_OFF_ENABLED`, więc zmiana w jednym miejscu obowiązuje całe firmware.
94. **Tryb nocny wygaszacza** — 22:00–06:00, jasność 0, okno wybudzenia 20 s.
95. **Tapeta wygaszacza** — upload, usunięcie i przeładowanie (`ui_screen_saver_reload_wallpaper`, `/api/display/wallpaper`).
96. **`ui_screen_saver_wake()` / `ui_screen_saver_handle_screen_clean()`** — API używane przez zdarzenia HA/MQTT.
97. **Kontrola pamięci po zmianach zegara** — dwa odczyty `/api/diagnostics`, po ~1 min i po 8,8 min (529 501 ms): `heap_free` 78 779 B, `heap_free_min` 39 828 B, `lvgl.live_blocks` 1324 — **identyczne**, `fail_count` 0, `alloc_count − free_count = 1324` → brak wycieku.
98. **Łącza po długiej sesji** — Wi-Fi 0 rozłączeń, HA 0, MQTT połączone, CPU 46 °C, `rollback_pending false`, `image_state new`.

### F. Nowe kafelki i integracja z Alarmo

99. **`cover_tile`** — sterowanie roletami i żaluzjami.
100. **Klik = otwórz/zamknij, drugie kliknięcie = STOP** — dokładnie tak, jak było potrzebne.
101. **Rząd Open / Stop / Close** — dostępny od rozmiaru kafelka 170×150 px.
102. **Przeciągany pasek pozycji w %** — `cover.set_cover_position`, czyli otwieranie/zamykanie „na ile procent”.
103. **`scene_tile`** — wywoływanie scen Home Assistant.
104. **`person_tile`** — obecność osób (stany, kto jest w domu).
105. **`timer_tile`** — minutnik.
106. **`alarm_tile`** — kafelek alarmu (stan + uzbrajanie).
107. **Wybór backendu `alarm_backend`** — `auto` (wykrywa Alarmo po atrybutach `open_sensors` / `bypassed_sensors` / `arm_mode` / `ready_to_arm`), `alarmo`, `builtin`.
108. **Pięć trybów uzbrojenia** — `away`, `home`, `night`, `vacation`, `custom_bypass`, lista konfigurowalna dla każdego kafelka (`alarm_modes`).
109. **Usługi `builtin`** — `alarm_control_panel.alarm_arm_*` / `alarm_disarm`.
110. **Usługi Alarmo** — `alarmo.arm {entity_id, mode, force, skip_delay, code}` oraz `alarmo.disarm {entity_id, code}`.
111. **Kolory, ikony i teksty PL/EN dla wszystkich stanów** — `armed_home`, `armed_away`, `armed_night`, `armed_vacation`, `armed_custom_bypass`, `pending`, `triggered`.
112. **Kod PIN `alarm_code` + `alarm_ask_code`** — klawiatura na panelu, kod maskowany, limit `APP_MAX_ALARM_CODE_LEN`.
113. **Auto-PIN bez dodatkowej konfiguracji** — panel czyta atrybuty `code_format` (niepuste = encja chce kodu) i `code_arm_required` (domyślnie tak, gdy atrybutu nie ma); rozbrojenie pyta zawsze, uzbrojenie tylko wtedy, gdy HA tego wymaga.
114. **Checkbox w WWW przemianowany** na „Zawsze pytaj o PIN (auto: gdy wymaga HA)”.
115. **Odczyt `changed_by`** — „uzbrojone przez …” na kafelku.
116. **Odporność na podwójne kliknięcia** — `W_ALARM_BUSY_TIMEOUT_MS` 8000; po nieudanej akcji panel wraca do stanu z HA, a nie zostaje w „czekam”.
117. **Nowy moduł `main/ha/ha_alarm_events.c/.h`** — jedno ostatnie zdarzenie Alarmo w statycznym slocie (sekcja krytyczna `portMUX`, payload 512 B przez `cJSON_PrintPreallocated`, **bez alokacji na stercie**, numer `seq`).
118. **Subskrypcja 3 zdarzeń Alarmo** — `alarmo_failed_to_arm`, `alarmo_command_success`, `alarmo_ready_to_arm_modes_updated`; wysyłana raz na każde połączone i uwierzytelnione WS (`ws_connect_count` jako tożsamość sesji → samonaprawialne bez restartu UI).
119. **Wpis startowy `Subscribed to 3 Alarmo events`** — potwierdzony na porcie szeregowym.
120. **Lista otwartych czujek** — atrybut `open_sensors` parsowany do nazw („Drzwi: Taras, Okno: Kuchnia”), nazwa z `friendly_name`, potem prettify `entity_id`, plus licznik `open_count`.
121. **Licznik pominiętych czujek** — `bypassed_sensors` → „Pominięte: N” na linii informacyjnej.
122. **Uzbrojenie z pominięciem czujek** — `alarmo.arm` z `force: true`; nakładka potwierdzenia z listą czujek i przyciskami „Uzbrój mimo to” / „Anuluj” (gdy `alarm_force_arm`).
123. **`skip_delay`** — przełącznik pomijający opóźnienie wyjścia.
124. **Tryb urlopowy** — `armed_vacation` + przycisk „Uzbrój na urlop”.
125. **Odliczanie opóźnienia** — atrybut `delay` → dopisek „N s” na kafelku, odświeżany co 500 ms i kotwiczony przy każdej zmianie wartości z HA.
126. **Podpis strefy** — pole `alarm_zone_label` w prawym górnym rogu nagłówka (pokazywane przy szerokości kafelka ≥ 200 px), ucinane wielokropkiem.
127. **Typ czujki z `device_class`** — `door` / `window` / `motion` / `smoke` / `water` / `tamper`, tłumaczone PL/EN.
128. **Naprawa angielskiego „(open)” w polskim tekście** — `open_sensors` zawiera **stan** czujki, a nie jej typ; typ bierze się teraz z `device_class`.
129. **Powód odmowy uzbrojenia na kafelku** — zdarzenie `alarmo_failed_to_arm` → czerwona linia informacyjna: `open_sensors` z listą czujek, `invalid_code` („nieprawidłowy kod”), `not_allowed` („teraz niedozwolone”); stan wraca do poprzedniego po 10 s (`W_ALARM_EVENT_TEXT_MS`).
130. **Potwierdzenie komendy** — `alarmo_command_success` natychmiast kończy stan „czekam” (bez czekania na 8 s timeout) i kasuje maski gotowości.
131. **Gotowość trybów** — `alarmo_ready_to_arm_modes_updated` → maski bitowe dla każdego trybu; tryb niegotowy jest przygaszony (`LV_OPA_60` + wyszarzony tekst), ale **nadal klikalny** (uzbrojenie z `force` ma sens); maski kasowane po przyjętej komendzie i przy `unavailable`.
132. **Układ dolnego stosu kafelka alarmu** — przyciski → linia informacyjna → linia czujek; tekst stanu dostaje miejsce pierwszy, potem kolejno znikają linia informacyjna i czujki; czujki zawijane maksymalnie do dwóch linii.
133. **Naprawa edytora WWW dla alarmu** — 9 pól inspektora (`fAlarmCode`, `fAlarmAskCode`, `fAlarmBackend`, `fAlarmZoneLabel`, `fAlarmShowSensors`, `fAlarmShowBypassed`, `fAlarmForceArm`, `fAlarmSkipDelay` + checkboxy trybów) **nie było podłączonych do `bindInspectorAutoApply()`** — zmiany po cichu się cofały; dodano 9 wywołań i zweryfikowano cały przepływ zapis → `GET /api/layout`.
134. **Kafelki zmapowane na żywo** — 3 encje `alarm_control_panel` oraz **107 encji `binary_sensor`** widocznych w discovery.
135. **`w_clock_alarm.c` → zwykły zegar** — usunięte dzwonienie, snooze, ton, akcje i nakładki; zostały godzina i data (`show_seconds`, `show_date`).
136. **`binary_sensor`** — kafelki czujników binarnych z kolorami ON/OFF są częścią zestawu kafelków tego forka.

### G. Sieć, stabilność, diagnostyka

137. **Statystyki pamięci dla każdej puli** — `regions{internal, internal_dma, internal_32bit, psram, default}` w `/api/diagnostics`, każde z `total`, `free`, `largest_block`, `free_min`, `allocated`, `alloc_blocks`, `free_blocks`, `fragmentation_pct` → widać, **która** pula się kończy, a nie tylko sumę.
138. **Rozszerzona linia serca co 30 s** — obok `heap_free`/`psram_free` także `int_free`, `int_largest`, `dma_free`, `dma_largest`, `iram_free` (historia w `/api/logs` pokazuje tendencje).
139. **Statystyki łącza Wi-Fi** — udane połączenia, rozłączenia, zaplanowane reconnecty, twarde odzyskiwania sterownika, powód ostatniego rozłączenia, czas trwania sesji (`wifi_mgr_link_stats_t`).
140. **Statystyki łącza HA** — `connect_count`, `disconnect_count`, `recover_count`, `error_streak`, `short_session_strikes`, `last_connected_uptime_ms`, `last_session_ms`, `session_healthy` → wykrywanie „półotwartych” połączeń.
141. **Bramka ciężkich zapytań HA** — `ha_client_heavy_gate_is_busy()`: energia i duże listy nie blokują pętli UI.
142. **`ha_client_is_initial_sync_done()`** — interfejs wie, kiedy dane są kompletne (koniec migotania pustych kafelków po starcie).
143. **Diagnostyka brakujących encji** — `missing_entities` w `/api/diagnostics` oraz ostrzeżenie w edytorze WWW „te encje nie istnieją w HA”.
144. **API odzyskiwania** — `wifi_mgr_force_reconnect()`, `wifi_mgr_force_transport_recover()`.
145. **Zapisywanie przyczyny rozłączenia** — „dlaczego się rozłączył” to teraz odpowiedź z diagnostyki, a nie zgadywanie.
146. **Boot guard** — licznik bootów, powód resetu (`boot_guard_reset_reason_str`), stan obrazu OTA (`boot_guard_ota_state_str`), `rollback_pending`, `confirmed`.
147. **Potwierdzenie obrazu dopiero gdy zdrowy** — zadanie potwierdzenia startuje, gdy Wi-Fi jest podłączone albo minął `APP_BOOT_CONFIRM_TIMEOUT_MS`, więc nieudany start wraca do poprzedniej wersji zamiast „cegły”.
148. **Watchdog UI `APP_UI_WATCHDOG_TIMEOUT_MS` = 60 s** — kontrolowany restart z zapisem przyczyny oraz `ui_runtime_is_running()`, `ui_runtime_get_heartbeat()`, `ui_runtime_kick_heartbeat()`.
149. **Dziennik systemowy** — `main/diag/system_log.c` (bufor pierścieniowy) + `GET /api/logs`, `DELETE /api/logs` — czytanie logów bez kabla USB.
150. **OTA przez URL i przez upload** — `GET /api/ota/status`, `POST /api/ota/url`, `POST /api/ota/upload`, rollback potwierdzany przez boot guard.
151. **Auto-restart (firmware)** — `system.auto_restart_enabled` + `system.auto_restart_hours` w `runtime_settings` i `/api/settings` (z ograniczaniem zakresu) oraz zadanie okresowe w `app_main` wołające `esp_restart()` po N godzinach pracy.
152. **Auto-restart (WWW)** — przełącznik i pole liczby godzin w `index.html`/`app.js` z walidacją, EN + PL.
153. **Kopia zapasowa / odtwarzanie** — `GET /api/backup` (JSON: `backup_version`, `layout`, `settings`, `themes`), `POST /api/backup/restore` z licznikami odtworzonych elementów; **sekrety nigdy nie są eksportowane** (token HA, hasło MQTT, dane Wi-Fi).
154. **Statyczne IP** — pola IP / maska / brama / DNS w `runtime_settings`, `GET`/`PUT /api/settings` oraz formularz w edytorze WWW.
155. **Sekcja MQTT w edytorze WWW** — włączony, TLS, host, port, użytkownik, hasło, prefiks discovery; pełna obsługa w `app.js` (nawigacja, referencje, render, zapis/zastosowanie) + i18n EN/PL.
156. **Nowe klucze poleceń MQTT** — `wake`, `page`, `screensaver_enabled`, `screen_off_enabled`, `clock_24h`, `saver_show_seconds`, `saver_show_date`, `topbar_show_clock|date|gear|status`, `topbar_icon_text`, `topbar_custom_colors`, `brightness`, `saver_brightness`, `screensaver_timeout_sec`, `screen_off_timeout_sec`, `page_transition`, `page_transition_ms`, `tile_press_fx`, `tile_press_fx_dim`, `tile_press_fx_scale`, `value_anim`, `value_anim_ms`.
157. **Naprawa brakującej obsługi `nav_*` w `api_settings.c`** — `GET`/`PUT` mają własny builder i parser, więc 9 kluczy dolnego paska trzeba było dopisać osobno (inaczej zmiany nie dochodziły).
158. **Kolory dolnego paska nawigacji** — 9 pól `nav_*`, zastosowanie na żywo bez restartu, zweryfikowane pikselowo.
159. **Kolory górnego paska** — tło `0x0D1723`, zegar `0xEAF2FA`, data/gear `0xA1B1C1`, HA/Wi-Fi `0xC7D1DB`, minimalna czcionka zegara 22 px.
160. **Przeniesienie stosów zadań do PSRAM** (`app_task.h`) oraz **własny alokator LVGL z PSRAM** (`ui/ui_lvgl_mem.c`) — `heap_free` 23 595 → 96 571 B, `largest` 10 752 → 47 104 B, `lvgl.fail_count` 0.

### H. Edytor WWW

161. **Dwie zakładki: Layout i Settings** (+ kreator pierwszego uruchomienia).
162. **Sekcja „Wygląd kafelka (ten kafelek)”** w inspektorze — preset, tło, gradient, obramowanie, promień, kształt, krycie, czcionka, cień, kolor tekstu.
163. **Osobne pickery kolorów**: tytuł / opis encji / wartość / ikona.
164. **Przycisk resetu wyglądu** i **kopiowanie wyglądu** (trzy warianty) w inspektorze.
165. **Sekcja „Wygląd strony (ta strona)”** — preset, tło, gradient, tapeta, przyciemnienie, reset.
166. **Wybór encji `entity_picker`** — wyszukiwanie po nazwie, `entity_id` i pomieszczeniu, postęp ładowania, obsługa braku połączenia z HA i informacja o obcięciu listy limitem firmware.
167. **Style przycisku** — przełącznik, przełącznik zasilania, status zasilania, ikona wtyczki, ikona lampy, podświetlenie, tekst statusu.
168. **Zestaw kolumn czujnika binarnego** — kolor i tekst ON oraz OFF, kolor wartości czujnika.
169. **Pole wklejania layoutu JSON** — szybkie wstawienie całego układu.
170. **Ostrzeżenie o encjach nieobecnych w HA** i **podpowiedź o kopii zapasowej** („kopia zawiera układ, publiczne ustawienia i motywy własne”).
171. **Sekcja „Motywy → Strona”** — lista `page_theme` w inspektorze strony.
172. **Przełącznik „Motywy zależne od pory dnia”** + listy motywu dziennego i nocnego, teksty w 5 językach.
173. **Kontrolki „Format godziny”** (`settingsClockFormat`) i **„Styl zegara”** (`settingsClockStyle`).
174. **Kompresja gzip generowana w buildzie** — `components/webui/tools/gzip_asset.py` (poziom 9, `mtime=0` → build powtarzalny) i `target_add_binary_data(... BINARY)`.
175. **Serwowanie z `Content-Encoding: gzip` i `Vary: Accept-Encoding`**.
176. **Komunikat tekstowy dla klientów bez gzip** — żeby przeglądarka nie zapisała sobie `.gz` jako JavaScript.
177. **`Cache-Control: no-store` zostaje** — pliki nie mają numeru wersji w adresie, więc po OTA przeglądarka musi pobrać nowe.
178. **Weryfikacja gzip** — `SHA256` plików pobranych z panelu zgodny z `components/webui/www/` (rozpakowują się bit w bit identycznie).
179. **Klucze i18n wszystkich nowych funkcji** — `settings.display.clock_format`, `clock_format_h24`, `clock_format_h12`, `clock_format_hint`, `clock_style`, `clock_style_classic`, `clock_style_flip` (EN + PL) oraz pozostałe teksty nowych sekcji.
180. **Wniosek: ustawienie musi istnieć w dwóch miejscach** — `runtime_settings.c` (NVS) oraz `api_settings.c` (`GET`/`PUT`); dodatkowo `ui_screen_saver_apply_settings()` wołane z `api_settings.c`, `api_backup.c` (odtwarzanie) i `panel_mqtt.c`.

### I. Rozmiar i szybkość firmware

181. **Interfejs WWW skompresowany w buildzie** — 623 152 B → **121 750 B** (−501 402 B).
182. **`-O2` zamiast `-Og`** — `CONFIG_COMPILER_OPTIMIZATION_PERF=y` w `sdkconfig.panels3` i `sdkconfig.defaults.panels3`, asercje zachowane.
183. **Naprawa `-Werror=stringop-overread`** — `safe_copy_cstr()` w `ha_client.c` bez `strnlen()` (przy `-O2` funkcja była inline'owana i GCC zgłaszał ostrzeżenie).
184. **Naprawa `-Werror=format-truncation`** — komunikaty walidacji w `layout_validate.c` cytują identyfikator przez `%.*s` z limitem `LAYOUT_MSG_VALUE_MAX`.
185. **Ostrzeżenia naprawione w kodzie, nie wyłączone globalnie** — `-Werror` nadal działa.
186. **Obraz firmware** — 3 718 688 B → **3 235 984 B** (−482 704 B).
187. **Wolne miejsce na partycji aplikacji** — 475 616 B (11,3 %) → **958 320 B (22,9 %)**.
188. **`GET /api/diagnostics`** — 53,9 ms → **34,9 ms**.
189. **`GET /api/entities`** — 43,3 ms → **30,6 ms**.
190. **`GET /api/layout`** — 69,1 ms → **31,2 ms**.
191. **Przebudowa layoutu w UI** — 628–660 ms → **401 ms**.
192. **Transfer `/app.js` w przeglądarce** — 623 kB → **103 kB**.
193. **Zdjęcie wygaszacza nie zajmuje miejsca w firmware** — leży w LittleFS i jest wczytywane do PSRAM (480×480×2 = 460 800 B), więc wymiana zdjęcia nie zmienia rozmiaru obrazu.
194. **Zachowany obraz referencyjny z `-Og`** — możliwość porównania i cofnięcia zmiany.
195. **Weryfikacja po wgraniu** — edytor otwiera się i pokazuje strony oraz kafelki, w `/api/logs` brak wpisów watchdoga i paniki, `lvgl.fail_count` 0.

### J. Karta microSD

196. **Montowanie SDMMC 1-bit dopiero po `display_init()`** — panel dzieli IO47/IO48 z magistralą SPI karty.
197. **`GET|PUT /api/sd` + `/api/sd/format`, `/api/sd/files`, `/api/sd/file` (+`DELETE`)** — włącz/wyłącz (zapamiętywane w NVS), montowanie, odmontowanie, formatowanie, lista katalogów, usuwanie plików.
198. **Ochrona ścieżek `sd_card_build_path()`** — odrzuca ścieżki bezwzględne i `..` (brak wyjścia z katalogu).
199. **Tapeta wygaszacza wyłącznie na karcie** (gdy jest włożona) — `APP_SD_WALLPAPER_PATH=/sd/photos/wallpaper.bin` + `APP_SD_WALLPAPER_TMP_PATH`; bez karty wraca do LittleFS.
200. **Jedna kopia wymuszana w trzech miejscach** — zdarzenia karty `MOUNTED`/`RELEASING`/`UNMOUNTED`, upload `POST /api/display/wallpaper`, usunięcie `DELETE /api/display/wallpaper`.
201. **`ui_screen_saver_wallpaper_sync()`** — kopiuje ramkę z PSRAM na kartę po włożeniu i **z powrotem do LittleFS przed odmontowaniem** (`RELEASING`), więc wyjęcie karty nie gubi obrazu.
202. **`wallpaper_store` w `GET /api/sd`** (`sd`/`flash`/`none`) + zdanie o tym w sekcji microSD w edytorze WWW.
203. **Usunięcie `photos/wallpaper.bin` z listy plików czyści też kopię w RAM.**
204. **Eksport logów na kartę** — `POST /api/sd/logs/export`, `GET /api/sd/files?dir=logs`; można wyjąć kartę i odczytać logi bez kabla.
205. **Naprawa rotacji logów (`sd_logs_prune()`)** — katalog był listowany do tablicy o rozmiarze `APP_SD_LOG_MAX_FILES`, a lista była przycinana do 8 wpisów, więc warunek „za dużo plików” **nigdy się nie uruchamiał**; teraz skan rezerwuje jedno miejsce zapasowe (`MAX_FILES + 1`) i ma limit `2 * (MAX_FILES + 1)` przebiegów.
206. **Budżet logów 512 kB → 4 MB** (`APP_SD_LOG_MAX_BYTES`) — jeden eksport to ~380 kB, więc poprzednia wartość zostawiała na karcie tylko jeden plik; teraz zostaje do 8 najnowszych eksportów.
207. **Eksport okresowy co 6 h** (`APP_SD_LOG_EXPORT_PERIOD_SEC`), pierwszy po 30 s od startu.
208. **Weryfikacja na sprzęcie** — `wallpaper_store:sd`, `photos/wallpaper.bin` = 460 800 B na karcie, `SHA256` obrazu bez zmian, `used_bytes` karty +460 800 B.
209. **Stały układ katalogów na karcie** — foldery tworzone automatycznie przy montowaniu.

### K. Wyrównanie wariantów, kopie, dokumentacja

210. **Pełna kopia źródeł przed tą rundą pracy** — kompletny zapis projektu (źródła, komponenty, komponenty zarządzane, narzędzia, obrazy, tabele partycji, `sdkconfig*`, gotowe binaria), dzięki czemu każdą zmianę można cofnąć.
211. **Fork siostrzanego wariantu był daleko za kuchnią** — brakowało 7 plików (`app_task.h`, `ha/ha_alarm_events.{c,h}`, `ui/ui_lvgl_mem.{c,h}`, `ui/ui_theme_router.{c,h}`) i różniło się 29 plików (najwięcej `w_alarm_tile.c` 1001 linii, `ui_screen_saver.c` 528, `sd_card.c` 246, `theme_palette.c` 117).
212. **Kopia bezpieczeństwa przed wyrównaniem** drugiego wariantu.
213. **Przeniesienie źródeł, komponentów i konfiguracji builda** oraz usunięcie starego, wariantowego `sdkconfig` (generuje się z `defaults`) i ponowne wpisanie właściwego dla wariantu progu wyłączenia ekranu.
214. **Weryfikacja równości wariantów** — `main` różni się **tylko** `app_config.h` (celowo), `components` identyczne, `managed_components` 1:1.
215. **Pułapka: `robocopy /E` nie kasuje plików** — w drugim wariancie została starsza, pełna kopia LVGL, więc po skopiowaniu było **740 plików nadmiarowych**.
216. **Skutek: błąd `implicit declaration of function 'lv_image_header_cache_init'`** z martwego `managed_components/lvgl__lvgl/src/draw/lv_image_decoder.c` (nowy LVGL z kuchni nie ma tego pliku; `env_support/cmake/esp.cmake` globuje `src/*.c` rekurencyjnie, więc stary plik się kompilował).
217. **Naprawa** — usunięcie całego `managed_components` wariantu i skopiowanie drzewa od zera (6967 = 6967 plików) oraz usunięcie starego katalogu build (CMake cache wskazywał stary zestaw źródeł).
218. **Build drugiego wariantu od zera — sukces** — `betta-ha-panel-s3.bin` 3 247 040 B, 23 % wolnej partycji, zapisane MD5 i SHA256.
219. **Sprawdzenie konfiguracji wariantu** — `CONFIG_COMPILER_OPTIMIZATION_PERF=y`, `CONFIG_SPIRAM_TRY_ALLOCATE_WIFI_LWIP=y`, `CONFIG_LV_USE_CUSTOM_MALLOC=y`, `CONFIG_FREERTOS_HZ=1000`, PCLK 9 MHz, bounce 16 linii — identycznie jak w kuchni.
220. **Wniosek na przyszłość** — przenosząc fork, kasuj katalog docelowy albo użyj `robocopy /MIR`; nie używaj `/E`.
221. **Katalog funkcji do przenoszenia** — 11 sekcji: wydajność i build, wygląd LVGL, wygaszacz i zegar, kafelki, HA/Alarmo, sieć, microSD, edytor WWW, lista kontrolna przenoszenia, pułapki i co zostało.
222. **Historia sesji** — etapy pracy, fale zmian, weryfikacja na sprzęcie i artefakty w jednym dokumencie chronologicznym.
223. **Archiwum sesji** — checkpointy, baza zadań, metadane workspace i kopia dokumentów w jednym miejscu.
224. **Aktualizacja statusu zadań** — 150 zrobionych, 0 oczekujących, 2 zablokowane (razem 152).
225. **Potwierdzenie gotowości drugiego wariantu do wgrania** — ta sama komenda build z własnym `SDKCONFIG_DEFAULTS`, gotowe do wgrania przez USB.

### Świadomie poza zakresem

- **Gesty przesuwania stron (swipe)** — odrzucone (ryzyko obciążenia CPU/DMA); strony przełączane są z dolnego paska, przez MQTT lub API.
- **Powiadomienia z Home Assistant na panelu** — poza zakresem (dodatkowe obciążenie bez realnej korzyści).
- **Music Assistant i warstwa MQTT** — działają; celowo nie ruszane.
- **Sztuczne podniesienie wersji** (np. „BETTA 9.0”) — nie wykonane; ten fork nie jest prezentowany jako oficjalne wydanie upstreamu.
- **Nadal na liście życzeń:** ikony wstępnie renderowane (jako obrazy), w pełni konfigurowalny górny pasek oraz trzymanie wszystkich obrazów wyłącznie na karcie microSD.

---

## Zrzuty ekranu — panel

| Strona główna | Druga strona | Music Assistant |
|---|---|---|
| ![Strona główna](images/screenshots/panel-01-kuchnia.png) | ![Druga strona](images/screenshots/panel-02-kuchnia-gn.png) | ![Strona Music Assistant](images/screenshots/panel-03-music.png) |
| **Kafelek alarmu (Alarmo)** | **Wygaszacz — zegar flip, 24 h** | **Wygaszacz — 12 h z plakietką AM/PM** |
| ![Kafelek alarmu](images/screenshots/panel-04-alarm.png) | ![Wygaszacz flip 24h](images/screenshots/panel-05-screensaver-clock.png) | ![Wygaszacz 12h AM/PM](images/screenshots/panel-07-screensaver-ampm.png) |

### Nowe typy kafelków

![Kafelki rolet, scen, osób i minutnika](images/screenshots/panel-06-new-tiles.png)

*Kafelki rolet / scen / osób / minutnika na stronie demonstracyjnej zrobionej na potrzeby
tej dokumentacji. Zegar wygaszacza, kafelek alarmu, silnik wyglądu kafelków i router
motywów widać na zrzutach powyżej.*

### Na ścianie

![Panel Guition 4 zamontowany i działający](images/panel-on-wall.jpg)

*Panel Guition 4 (ESP32-S3-4848S040) z firmware `panels3` — zamontowany na ścianie i działający.*

---

## Zrzuty ekranu — edytor WWW

Edytor serwuje sam panel (`http://<ip-panela>`), więc zawsze odpowiada działającemu
firmware. Każdy zrzut poniżej został wykonany z zamaskowanymi danymi Wi-Fi, adresami IP,
nazwami hostów i tokenami.

| | |
|---|---|
| **Zakładka Layout** — strony, kafelki, inspektor z silnikiem wyglądu kafelka<br>![Edytor — layout](images/screenshots/editor-01-layout.png) | **Menu „+ Add”** — nowe typy kafelków (rolet, scena, osoba, minutnik, alarm, zegar)<br>![Typy kafelków](images/screenshots/editor-02-widget-types.png) |
| **Ustawienia → Czas** — format 12 h / 24 h, styl zegara (classic / flip)<br>![Ustawienia czasu](images/screenshots/editor-03-time.png) | **Ustawienia → Ekran i wygaszacz** — przejścia stron, reakcja na naciśnięcie, animacja wartości, wygaszacz, tapeta<br>![Ustawienia ekranu](images/screenshots/editor-04-display.png) |
| **Ustawienia → Strony** — wygląd strony, tapeta, motyw dla strony, presety<br>![Ustawienia stron](images/screenshots/editor-05-pages.png) | **Ustawienia → Interfejs** — kolory górnego paska i dolnego paska nawigacji<br>![Ustawienia interfejsu](images/screenshots/editor-06-ui.png) |
| **Ustawienia → Motyw** — automatyczne motywy dzień/noc, motywy własne<br>![Ustawienia motywu](images/screenshots/editor-07-theme.png) | **Ustawienia → Karta microSD** — montowanie, formatowanie, pliki, tapeta, eksport logów<br>![Ustawienia microSD](images/screenshots/editor-08-sd-card.png) |
| **Ustawienia → Kopia zapasowa** — wszystko w jednym pliku JSON, bez sekretów<br>![Ustawienia kopii](images/screenshots/editor-09-backup.png) | **Ustawienia → Diagnostyka** — pule pamięci, liczniki łączy, brakujące encje<br>![Diagnostyka](images/screenshots/editor-10-diagnostics.png) |
| **Ustawienia → Logi** — dziennik systemowy, pobieranie, czyszczenie<br>![Logi](images/screenshots/editor-11-logs.png) | **Ustawienia → System** — auto-restart, boot guard<br>![Ustawienia systemu](images/screenshots/editor-12-system.png) |
| **Ustawienia → Aktualizacja firmware** — OTA z URL lub z pliku, z rollbackiem<br>![Aktualizacja firmware](images/screenshots/editor-13-firmware.png) | |

---

## Obsługiwany sprzęt

| Wariant | Urządzenie | Rozdzielczość | Flash | Firmware |
|---------|------------|---------------|-------|----------|
| `panels3` | Guition **ESP32-S3-4848S040** (4,8") | 480 × 480 (ST7701S RGB, dotyk GT911) | 16 MB + 8 MB PSRAM | [firmware/](firmware/) |

Dźwięk na tej płytce można podłączyć przez wzmacniacz **NS4168 (I²S)**:

| Sygnał | GPIO |
|---|---|
| BCLK — zegar bitowy | GPIO 1 |
| LRCLK / WS — wybór kanału | GPIO 2 |
| DIN / SDATA — dane audio | GPIO 40 |

Drzewo źródeł nadal obsługuje warianty upstreamu `panel4` i `panel10`
(patrz [release-notes.md](release-notes.md)), ale funkcje tego forka oraz gotowe binaria
dotyczą wariantu `panels3`.

---

## Funkcje przejęte z upstreamu

- **Stałe połączenie z Home Assistant** — WebSocket z awaryjnym REST dla prognoz i stanów długo odpytywanych.
- **Edytor na urządzeniu** — BETTA Editor w przeglądarce pod `http://<ip-panela>`; przeciąganie widgetów, układy wielostronicowe, wybór encji pogrupowany po pomieszczeniach.
- **Biblioteka widgetów** — czujnik, przycisk, suwak, wykres, światło, ogrzewanie, pogoda, prognoza do 5 dni, odtwarzacz, lista zadań, Roborock, panel energii, pusty kafelek.
- **Zaawansowane sterowanie światłem** — jasność, temperatura barwowa, RGB — pokazywane tylko wtedy, gdy Home Assistant zgłasza taką możliwość.
- **Panel energii** — automatyczna wizualizacja sieci / fotowoltaiki / baterii / gazu / wody z modelu energii HA.
- **Wykresy** — liniowy, wygładzony lub słupkowy; próbkowanie zdarzeń do 4096 punktów.
- **Pierwsze uruchomienie** — AP `BETTA-Setup`, prowadzona konfiguracja Wi-Fi i Home Assistant, szybki startowy pulpit (Quick Setup).
- **Aktualizacje OTA** — plik `.ota.bin` albo adres URL z edytora WWW.
- **Wielojęzyczność** — wbudowane języki oraz wgrywanie i pobieranie własnych plików JSON z tłumaczeniami.
- **Interfejs przyjazny dotykowi** — automatyczne przyciemnianie podświetlenia po bezczynności, przeciąganie i zmiana rozmiaru z przechwyceniem wskaźnika, stabilny start dotyku GT911.

---

## Pierwsze uruchomienie

1. **Wgraj** obraz fabryczny — pełna instrukcja (metoda jednego pliku lub czterech plików)
   w [firmware/README.md](firmware/README.md).
2. **Zrestartuj** urządzenie. Uruchomi się AP o nazwie `BETTA-Setup`.
3. Połącz się z `BETTA-Setup`, otwórz `http://192.168.4.1`, wybierz kraj, znajdź swoją
   sieć i zapisz.
4. Po restarcie panel dołącza do Twojej sieci. Otwórz jego adres IP w przeglądarce,
   podłącz Home Assistant długoterminowym tokenem i zbuduj pierwszą stronę przez
   **Quick Setup**.

> ⚠️ **Do firmware nie są wkompilowane żadne prywatne dane.** Dane Wi-Fi, adres i token
> Home Assistant oraz dane MQTT wpisuje się wyłącznie przez interfejs WWW i są trzymane
> na urządzeniu (NVS / LittleFS). Nie publikuj własnych nakładek `sdkconfig`.

Kolejne aktualizacje instaluje się przez OTA z edytora — bez kabla.

---

## Budowanie ze źródeł

Wymagania:

- **ESP-IDF v5.5** (testowane na v5.5.5), Python 3.11+
- Dostęp do internetu przy pierwszym buildzie (menedżer komponentów pobiera `lvgl`,
  `esp_lvgl_port`, `esp_lcd_st7701`, `esp_lcd_touch_gt911`, `littlefs`,
  `esp_websocket_client` itd.)

```powershell
# Guition ESP32-S3-4848S040 (panels3) — wariant, na który celuje ten fork:
idf.py -B build-panels3 -DSDKCONFIG_DEFAULTS="sdkconfig.defaults.s3;sdkconfig.defaults.panels3" build
```

> Główny `CMakeLists.txt` rozpoznaje wariant panelu z `SDKCONFIG_DEFAULTS` i przy każdym
> configure odtwarza `main/idf_component.yml` z `main/idf_component.panels3.yml` —
> nigdy nie edytuj `main/idf_component.yml` ręcznie.

Pozostałe warianty upstreamu (bez zmian wobec BETTA v0.8.2):

```powershell
idf.py -B build-panel4  -DSDKCONFIG_DEFAULTS="sdkconfig.defaults;sdkconfig.defaults.panel4"  build
idf.py -B build-panel10 -DSDKCONFIG_DEFAULTS="sdkconfig.defaults;sdkconfig.defaults.panel10" build
```

Artefakty dla `panels3` trafiają do `build-panels3/` (`betta-ha-panel-s3.bin` + bootloader,
tabela partycji, dane OTA). Aby złożyć scalony obraz fabryczny, patrz
[tools/make_factory_bin.ps1](tools/make_factory_bin.ps1).

Interfejs WWW jest kompresowany przez sam build
([components/webui/tools/gzip_asset.py](components/webui/tools/gzip_asset.py), poziom 9,
`mtime=0` dla powtarzalnego wyniku) i osadzany jako blob binarny — bez żadnego kroku ręcznego.

---

## Budżet pamięci flash i wydajność

| Element | Przed | Po |
|---|---|---|
| Zasoby interfejsu WWW (nieskompresowane) | 623 152 B | 121 750 B (gzip, −501 402 B) |
| Obraz aplikacji | 3 718 688 B | 3 235 984 B (−482 704 B) |
| Wolne miejsce na partycji aplikacji | 475 616 B (11,3 %) | 958 320 B (22,9 %) |
| Optymalizacja kompilatora | `-Og` | `-O2` (`CONFIG_COMPILER_OPTIMIZATION_PERF=y`) |
| `GET /api/diagnostics` | 53,9 ms | 34,9 ms |
| `GET /api/entities` | 43,3 ms | 30,6 ms |
| `GET /api/layout` | 69,1 ms | 31,2 ms |
| Przebudowa układu w interfejsie | 628–660 ms | 401 ms |
| `/app.js` w sieci | 623 kB | 103 kB |

Pamięć po przeniesieniu stosów zadań i alokatora LVGL do PSRAM:

| Wskaźnik | Przed | Po |
|---|---|---|
| `heap_free` (wewnętrzny) | 23 595 B | 96 571 B |
| Największy wolny blok | 10 752 B | 47 104 B |
| `lvgl.fail_count` | 0 | 0 |

---

## Sekcje edytora WWW

Edytor ma zakładkę **Layout** oraz zakładkę **Settings** z 15 sekcjami:

Wi-Fi · Home Assistant · Czas (format i styl zegara) · Ekran i wygaszacz · Karta microSD ·
Strony (wygląd strony, tapeta, motyw strony) · MQTT · Interfejs (górny i dolny pasek) ·
Motyw (dzień/noc, motywy własne) · Konfiguracja AP · Aktualizacja firmware ·
System (auto-restart) · Kopia zapasowa · Diagnostyka · Logi.

Każda nowa kontrolka jest dostępna w **języku polskim i angielskim**.

---

## Biblioteka kafelków

| Typ | Przeznaczenie |
|---|---|
| `sensor` | wartość liczbowa / tekstowa czujnika |
| `button` | przełącznik, przycisk, status zasilania, style ikon |
| `slider` | suwak ogólny (światło, odtwarzacz, roleta itd.) |
| `graph` | wykres liniowy / wygładzony / słupkowy |
| `light_tile` | światło z jasnością, temperaturą barwową i RGB |
| `heating_tile` | ogrzewanie / klimat |
| `weather_tile`, `weather_3day` | pogoda bieżąca i prognoza |
| `media_player` | odtwarzacz z okładką i głośnością |
| `binary_sensor` | drzwi / okno / ruch / dym / woda |
| `todo_list` | lista zadań |
| `roborock_tile` | odkurzacz |
| `empty_tile` | odstęp / dekoracja |
| `alarm_tile` | alarm, świadomy Alarmo |
| `clock_alarm` | zwykły zegar (godzina + data) |
| `cover_tile` | rolety i żaluzje: otwórz, stop, zamknij, pozycja w % |
| `scene_tile` | sceny Home Assistant |
| `person_tile` | obecność osób |
| `timer_tile` | minutnik |

---

## API HTTP

Poza endpointami upstreamu ten fork dodaje:

| Metoda | URI | Przeznaczenie |
|---|---|---|
| `GET`/`PUT` | `/api/settings` | wszystkie ustawienia (wyślij `"reboot": false`, aby zastosować na żywo) |
| `GET`/`PUT` | `/api/layout` | odczyt i zapis całego układu |
| `GET` | `/api/version` | informacje o wersji |
| `GET` | `/api/screenshot.bmp` | klatka 480×480 aktualnego ekranu |
| `POST` | `/api/display/activity` | wybudzenie ekranu / zerowanie licznika bezczynności |
| `POST`/`DELETE` | `/api/display/wallpaper` | wgranie lub usunięcie tapety wygaszacza |
| `GET` | `/api/pages`, `POST /api/pages/activate` | lista stron / aktywacja strony |
| `GET` | `/api/themes`, `/api/themes/active`, `/api/themes/get`, `/api/themes/custom` | motywy wbudowane, aktywny i własne |
| `GET` | `/api/backup`, `POST /api/backup/restore` | eksport i import całej konfiguracji (bez sekretów) |
| `GET` | `/api/diagnostics` | pule pamięci, statystyki łączy, boot guard, brakujące encje |
| `GET`/`DELETE` | `/api/logs` | dziennik systemowy (bufor pierścieniowy) |
| `GET` | `/api/ota/status`, `POST /api/ota/url`, `POST /api/ota/upload` | OTA z URL lub z pliku, z rollbackiem |
| `GET`/`PUT` | `/api/sd`, `POST /api/sd/format`, `GET /api/sd/files`, `GET|DELETE /api/sd/file`, `POST /api/sd/logs/export` | obsługa microSD i eksport logów |
| `GET` | `/api/state`, `/api/entities`, `/api/ha/energy`, `/api/ha/diagnostics`, `/api/ha/light_entities` | dane z Home Assistant |
| `GET` | `/api/wifi/scan` | skanowanie sieci Wi-Fi |
| `GET`/`POST` | `/api/i18n/languages`, `/api/i18n/effective`, `/api/i18n/custom` | tłumaczenia |

---

## Klucze poleceń MQTT

Klient MQTT zachowuje automatyczne wykrywanie w Home Assistant i dodaje klucze poleceń:

`wake`, `page`, `screensaver_enabled`, `screen_off_enabled`, `clock_24h`,
`saver_show_seconds`, `saver_show_date`, `topbar_show_clock`, `topbar_show_date`,
`topbar_show_gear`, `topbar_show_status`, `topbar_icon_text`, `topbar_custom_colors`,
`brightness`, `saver_brightness`, `screensaver_timeout_sec`, `screen_off_timeout_sec`,
`page_transition`, `page_transition_ms`, `tile_press_fx`, `tile_press_fx_dim`,
`tile_press_fx_scale`, `value_anim`, `value_anim_ms`.

---

## Uwagi i pułapki

- **`PUT /api/settings` domyślnie restartuje panel.** Argument `reboot` w handlerze ma
  wartość domyślną `true`; wyślij `"reboot": false`, aby zmiany ekranu, stron, animacji,
  pasków, motywu i MQTT zastosowały się od razu. Warto o tym wiedzieć, gdy sterujesz
  panelem ze skryptu.
- **`/api/screenshot.bmp`** zwraca klatkę tylko wtedy, gdy ekran jest wybudzony; nie ma
  endpointu wymuszającego wygaszacz, więc klatkę z wygaszaczem robi się po upływie
  jego czasu bezczynności.
- **Adresy plików nie mają numeru wersji** i są serwowane z `Cache-Control: no-store`,
  więc przeładowanie w przeglądarce zawsze pobiera nowy interfejs po OTA.
- **gzip**: panel serwuje wstępnie skompresowane zasoby; przeglądarka musi wysyłać
  `Accept-Encoding: gzip` (wszystkie nowoczesne to robią).
- **Przejścia stron** nie używają równoległego rysowania dwóch stron — animuje się tylko
  strona wchodząca, co utrzymuje obciążenie CPU na stałym poziomie.
- Panel dzieli IO47/IO48 między magistralę ekranu i kartę microSD, dlatego karta montowana
  jest dopiero **po** `display_init()`.

---

## Struktura projektu

```
.
├── main/                  # Aplikacja panelu (C, LVGL)
│   ├── api/               #   API HTTP: ustawienia, layout, kopia, diagnostyka, sd, ota, motywy
│   ├── diag/              #   dziennik systemowy, boot guard
│   ├── drivers/           #   inicjalizacja ekranu (panels3) i dotyku
│   ├── ha/                #   klient Home Assistant, zdarzenia Alarmo
│   ├── layout/            #   model układu i walidacja
│   ├── mqtt/              #   klient MQTT + discovery w HA
│   ├── net/               #   menedżer Wi-Fi (statystyki, reconnect, statyczne IP)
│   ├── sd/                #   karta microSD + eksport logów
│   ├── settings/          #   ustawienia runtime (NVS), magazyn i18n
│   └── ui/                #   ekrany LVGL, widgety, motywy, animacje, czcionki
│       ├── theme/         #     palety motywów, router motywów
│       └── widgets/       #     implementacje kafelków (w_*.c)
├── components/webui/      # Edytor WWW (BETTA Editor) serwowany przez panel
│   ├── www/               #   index.html, app.js, styles.css
│   └── tools/             #   gzip_asset.py (kompresja w buildzie)
├── firmware/              # Gotowe pliki .bin (panels3) + instrukcja wgrania
├── images/screenshots/    # Zrzuty ekranu panelu i edytora WWW
├── partitions.s3.csv      # Tabela partycji dla 16 MB flash S3
├── sdkconfig.defaults.s3  # } Czyste konfiguracje builda, bez danych
├── sdkconfig.defaults.panels3
└── LICENSE                # FNCL v1.1 (Copyright (c) 2026 Cpt_Kirk)
```

---

## Prywatność — brak danych osobowych w repozytorium

- Nazwa i hasło Wi-Fi, adres i token Home Assistant oraz adres i dane brokera MQTT są
  **wyłącznie konfiguracją**; nic nie jest wkompilowane w firmware i nic nie jest
  przechowywane w tym repozytorium.
- Wszystkie pola Wi-Fi/MQTT/HA w edytorze WWW mają ogólne podpowiedzi
  (`homeassistant.local`, `192.168.1.50`, …).
- Kopie tworzone przez `GET /api/backup` **nigdy nie zawierają sekretów**.
- Opublikowane tu zrzuty ekranu powstały z zamaskowanymi danymi, adresami IP, nazwami
  hostów i tokenami.

---

## Licencja

- Kod źródłowy na licencji [LicenseRef-FNCL-1.1](LICENSE) — **Copyright (c) 2026 Cpt_Kirk**.
- **Wyłącznie niekomercyjnie.** Użycie komercyjne wymaga osobnej pisemnej licencji od
  właściciela praw autorskich (patrz §4 i §11 licencji).
- To **zmodyfikowana wersja** BETTA HA Panel v0.8.2; zmiany są oznaczone i opisane powyżej.
  Historia wersji upstreamu znajduje się w [release-notes.md](release-notes.md).
