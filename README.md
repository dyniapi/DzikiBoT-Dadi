
# DzikiBoT 

> **Platforma:** STM32 Nucleo‑L432KC  
> **Cel:** Robot mini‑sumo — napęd 2× BLDC (ESC), czujniki TF‑Luna (Lidar) i TCS3472 (kolor), OLED SSD1306, panel diagnostyczny UART.  
> **Styl projektu:** `main.c` minimalny, cała obsługa peryferiów i logiki w oddzielnych modułach.

---

## Spis treści
1. [Szybki start](#szybki-start)
2. [Wymagania i środowisko](#wymagania-i-środowisko)
3. [Struktura katalogów](#struktura-katalogów)
4. [Sprzęt i połączenia](#sprzęt-i-połączenia)
5. [Przepływ programu (architektura)](#przepływ-programu-architektura)
6. [Konfiguracja (`config.c/.h`)](#konfiguracja-configch)
7. [Moduły — opis techniczny](#moduły--opis-techniczny)
8. [Test jazdy i debug](#test-jazdy-i-debug)
9. [Kalibracja i strojenie](#kalibracja-i-strojenie)
10. [Znane zachowania i uwagi](#znane-zachowania-i-uwagi)
11. [Migracja na DShot + telemetrię AM32](#migracja-na-dshot--telemetrię-am32)
12. [Rozwiązywanie problemów](#rozwiązywanie-problemów)
13. [Bezpieczeństwo](#bezpieczeństwo)
14. [Licencja / Autorzy](#licencja--autorzy)

---

## Szybki start

1. **Otwórz projekt w STM32CubeIDE** lub zbuduj z linii poleceń (`make`).  
2. **Skonfiguruj sprzęt** (sekcja _Sprzęt i połączenia_).  
3. **Wgraj firmware** na Nucleo‑L432KC.  
4. **Uruchom**: po starcie ESC dostają **3 s neutralu (arm)**, potem działa rampa i test jazdy (`DriveTest`).  
5. **Podłącz UART 115200 8N1** — w terminalu zobaczysz panel diagnostyczny (TF‑Luna, TCS3472, stany napędu).  
6. **Strojenie**: zacznij od `esc_start_pct/esc_max_pct`, potem `ramp_step_pct`, na końcu `left_scale/right_scale`.

---

## Wymagania i środowisko

- **Płytka:** STM32 Nucleo‑L432KC
- **IDE:** STM32CubeIDE (zalecane) lub toolchain `arm-none-eabi-gcc` + `make`
- **Biblioteki:** HAL/LL wygenerowane z CubeMX dla: GPIO, I2C1, I2C3, USART2, TIM1
- **Zasilanie:** zgodne z wymaganiami ESC i czujników (pamiętaj o masie wspólnej GND)

---

## Struktura katalogów

```
Core/
├─ Inc/
│  ├─ config.h         # Opis wszystkich parametrów konfiguracyjnych (czytane przez moduły)
│  ├─ motor_bldc.h     # Interfejs sterownika ESC (RC PWM)
│  ├─ tank_drive.h     # Interfejs logiki napędu (rampa + neutral)
│  └─ throttle_map.h   # Interfejs kształtowania przepustnicy (trim L/R + krzywa)
└─ Src/
   ├─ main.c           # Minimalny "klej" uruchamiający moduły i zadania okresowe
   ├─ motor_bldc.c     # RC PWM: mapowanie -100..100 → 1000..2000 µs z oknem start..max
   ├─ tank_drive.c     # Rampa czasowa, neutral przy zmianie kierunku, wywołuje throttle_map
   └─ throttle_map.c   # Korekty L/R, delikatna krzywa (domyślnie prawie liniowa)
```

(Dodatkowe moduły używane w projekcie, nie ujęte tutaj: `tf_luna_i2c.*`, `tcs3472.*`, `ssd1306.*`, `oled_panel.*`, `debug_uart.*`, `i2c_scan.*`, `drive_test.*`.)

---

## Sprzęt i połączenia

**Napęd (ESC, RC PWM / TIM1):**
- **PRAWY ESC:** TIM1\_CH1 → **PA8**
- **LEWY  ESC:** TIM1\_CH4 → **PA11**
- Zalecany TIM1 (CubeMX): `PSC=79` (1 MHz, 1 µs/tick), `ARR=19999` (50 Hz), PWM Mode 1

**I²C (sensory + OLED):**
- **I2C1 (RIGHT bus):** `PB6=SCL`, `PB7=SDA`  
  - TF‑Luna Right (`0x10`), TCS3472 Right (`0x29`), SSD1306 OLED (`0x3C`)
- **I2C3 (LEFT bus):** `PC0=SCL`, `PC1=SDA`  
  - TF‑Luna Left (`0x10`), TCS3472 Left (`0x29`)

**UART (debug):**
- **USART2, 115200 8N1** — panel diagnostyczny (ANSI „w miejscu”).

> **Ważne:** GND wszystkich urządzeń musi być wspólne. Przestrzegaj ograniczeń prądowych i napięciowych zasilania ESC oraz czujników.

---

## Przepływ programu (architektura)

```
logika/AI/RC  ──► TankDrive_SetLR(fwd/turn) ──► [rampa + neutral przy REV] ──► cur_L/R
                                   │
                                   ▼
                           Throttle_Apply(L/R)   (trim + krzywa)
                                   │
                                   ▼
                            motor_bldc (RC PWM)  (okno esc_start..esc_max → 1000..2000 µs)
```

- `main.c` jest **minimalny**: inicjalizuje peryferia i moduły, a potem co `CFG_Motors()->tick_ms` woła `TankDrive_Update()`.
- `TankDrive` odpowiada za rampę i krótkie „neutral dwell” przy zmianie kierunku.
- `Throttle` robi drobne korekty L/R i (opcjonalnie) łagodną krzywą na dole.
- `motor_bldc` zamienia „%” na µs w standardzie RC PWM z *oknem* `esc_start_pct → esc_max_pct`.

---

## Konfiguracja (`config.c/.h`)

Główne parametry dostępne przez `CFG_Motors()`:

- `tick_ms` — rytm aktualizacji rampy (np. **20 ms**).
- `ramp_step_pct` — maks. zmiana mocy na 1 krok update (np. **4%**).
- `neutral_dwell_ms` — ile ms trzymać neutral przy zmianie kierunku (np. **500 ms**).
- `reverse_threshold_pct` — próg wykrycia prawdziwej zmiany kierunku (np. **5%**).
- `left_scale/right_scale` — korekta balansu L/R (np. `1.00f` i `1.00f`).
- `esc_start_pct` — ile % odpowiada „wyjściu z martwej strefy” ESC (np. **30%**).
- `esc_max_pct` — ile % odpowiada „naszemu 100%” (np. **60%** → ograniczenie góry).

Skrót do strojenia:
1. **ESC okno:** `esc_start_pct` (start), `esc_max_pct` (sufit).
2. **Rampa:** `ramp_step_pct`, `neutral_dwell_ms` (gładkość i „bezpieczne” reversy).
3. **Balans:** `left_scale/right_scale` (prosto jeździ = dobrze).

---

## Moduły — opis techniczny

### `motor_bldc.c/.h` (RC PWM, TIM1)
- Przyjmuje `%` w zakresie `-100..+100` i mapuje na **1500±Δ µs**.
- **Okno** z `config.c`: `esc_start_pct..esc_max_pct` — gwarantuje ruszenie z dołu i ogranicza górę.  
  Przykład: `30..60%` oznacza, że `+10%` → ~33% drogi do 2000 µs (zawsze „ponad deadband”).
- API:
  - `ESC_Init()`, `ESC_ArmNeutral(ms)`
  - `ESC_WritePercentRaw(ch, pct)`
  - `ESC_SetLeftPercent(pct)`, `ESC_SetRightPercent(pct)`
  - aliasy dla `TankDrive_OutputLeft/Right` (można nadpisać w DShot).

### `tank_drive.c/.h` (rampa + neutral)
- **Rampa czasowa**: co `tick_ms`, modyfikuje `cur_l/r` o `ramp_step_pct` w stronę `target_l/r`.
- **Neutral przy zmianie kierunku**: po wykryciu przejścia przez `±reverse_threshold_pct` — trzyma `0` przez `neutral_dwell_ms` (przeliczone na liczbę kroków).
- Po rampie wywołuje `Throttle_Apply(L/R)`, a potem `TankDrive_OutputLeft/Right()`.
- API:
  - `TankDrive_Init(&TANKDRIVE_DEFAULT_CFG)` (wartości i tak nadpisywane z `config.c`)
  - `TankDrive_SetLR(l, r)`, `TankDrive_SetArcade(fwd, turn)`
  - `TankDrive_Update()`
  - `TankDrive_GetCurrent(&l,&r)`

### `throttle_map.c/.h` (trim + krzywa)
- `left_scale/right_scale` z `config.c` — najprostsze równoważenie L/R.
- Krzywa domyślnie **prawie liniowa** (gamma ~ 1.0; martwa strefa zostaje w ESC).
- API:
  - `Throttle_Init(&THROTTLE_DEFAULTS)` (albo `NULL` → pobierze z `config.c`)
  - `Throttle_Apply(in, THR_LEFT/THR_RIGHT)`

---

## Test jazdy i debug

- **`drive_test.c`** (jeśli włączony): automatyczna sekwencja FWD/NEU/REV do szybkiej diagnostyki rampy i ESC.
- **Panel UART** (`debug_uart.*`): ramka „w miejscu” — Lidar/TCS i wybrane parametry napędu.
- **OLED** (`oled_panel.*`): 7‑liniowy panel z podstawowymi danymi (Lidar, TCS).

> W `main.c` zobaczysz wywołania: `DriveTest_Start()` i `DriveTest_Tick()` — proste do wyłączenia, gdy przejdziesz na sterowanie z AI/RC.

---

## Kalibracja i strojenie

1. **ESC rusza za późno / syczy na małych** → zwiększ `esc_start_pct` (np. 25 → 30 → 35).  
2. **Za gwałtowny start / szarpanie** → zwiększ `ramp_step_pct` (np. 3 → 4 → 6) **i**/lub `neutral_dwell_ms` (np. 300 → 500 → 700).
3. **Robot ściąga w jedną stronę** → dostrój `left_scale/right_scale` (np. 1.00/1.00 → 0.97/1.00).
4. **Za szybkie „100%”** → ogranicz `esc_max_pct` (np. 60 → 55).  
5. **REV różni się od FWD** (typowe dla RC PWM) → na razie **akceptujemy**; docelowo przejście na **DShot**.

---

## Znane zachowania i uwagi

- **Asymetria +/‑ (np. −10 „mocniejsze” niż +10)** — to cecha wielu ESC na RC PWM; „reverse” bywa inaczej skalowany. Na to najlepszym rozwiązaniem jest **DShot** + kalibracja na **RPM/telemetrii**.
- **Start od zera** — rampa i `esc_start_pct` są celowo ustawione, by unikać „piku prądowego” i wyjść z deadbandu stabilnie.
- **Rampa vs. ESC** — jeśli ESC (AM32) przejmie „soft‑start”, można zmniejszyć `ramp_step_pct` po naszej stronie.

---

## Migracja na DShot + telemetrię AM32

- Logika (`TankDrive`, `Throttle`) **zostaje bez zmian**.  
- Podmieniasz wyjścia na nowy sterownik (np. `dshot.c` z funkcją `DSHOT_SendPercent()`), a w pliku „adapterze” nadpisujesz:
  ```c
  void TankDrive_OutputLeft (int8_t pct) { DSHOT_SendPercent(DSHOT_CH_LEFT,  pct); }
  void TankDrive_OutputRight(int8_t pct) { DSHOT_SendPercent(DSHOT_CH_RIGHT, pct); }
  ```
- **Zalety:** cyfrowe komendy, brak kalibracji „1500±x”, możliwość zwrotnej telemetrii (U, I, eRPM).
- **Autobalans na RPM:** po komendzie 40/40 odczytaj eRPM lewego/prawego i dynamicznie podbij słabszy kanał (zapewnia „prosto” bez zgadywania).

---


## Bezpieczeństwo

- **Koła w górze** przy pierwszych testach napędu.
- **Nie dotykaj** wirujących elementów i nie pracuj przy odkrytych przewodach zasilania.
- **Wspólna masa (GND)** między MCU, ESC i czujnikami jest obowiązkowa.
- **Zapas prądowy** źródła zasilania dla ESC (szczytowe pobory).

---
<<<<<<< Updated upstream
=======

## Licencja / Autorzy

- Kod modułów był rozwijany wspólnie w sesjach z asystentem (ChatGPT) i użytkownikiem.  
- Brak sformalizowanej licencji — do użytku własnego w projekcie DzikiBoT. Jeżeli planujesz publikację, rozważ dodanie pliku `LICENSE`.

---

### Załącznik A — sekwencja inicjalizacji (fragment `main.c`)

```c
ESC_Init(&htim1);
ESC_ArmNeutral(3000);                 // 3 s neutral (arming)
Throttle_Init(&THROTTLE_DEFAULTS);    // krzywa/trim
TankDrive_Init(&TANKDRIVE_DEFAULT_CFG);
```

### Załącznik B — pętla (fragment `main.c`)

```c
if ((now - tTank) >= CFG_Motors()->tick_ms) {
    TankDrive_Update();
    DriveTest_Tick();   // opcjonalny test krokowy
    tTank = now;
}
```
>>>>>>> Stashed changes
