/**
 * @file    drive_test.c
 * @brief   Sekwencje testowe napędu (FWD/NEU/REV) do szybkiej diagnostyki.
 * @date    2025-11-02
 *
 * Uwaga:
 *   Zachowaj spójność z resztą modułów oraz konwencje projektu.
 *
 * Funkcje w pliku (skrót):
 *   - apply_target(int8_t l, int8_t r)
 *   - DriveTest_Start(void)
 *   - DriveTest_Tick(void)
 *   - DriveTest_IsRunning(void)
 */

#include "drive_test.h"
#include "tank_drive.h"
#include "stm32l4xx_hal.h"  /* dla HAL_GetTick */

typedef struct {
    int8_t  l_pct;     /* -100..+100 */
    int8_t  r_pct;     /* -100..+100 */
    uint32_t dur_ms;
} dt_step_t;

/* Scenariusz zgodnie z Twoim testem ręcznym */
static const dt_step_t DT_SCRIPT[] = {
    { +50, 50, 3000 },  /* FWD 2 s */
    {   0,   0,  600 },  /* NEU 0.6 s (przed REV) */
    { -50, -50, 3000 },  /* REV 1 s */
    {   0,   0,  300 },  /* NEU 0.3 s (koniec) */
};
static const size_t DT_LEN = sizeof(DT_SCRIPT) / sizeof(DT_SCRIPT[0]);

static size_t   s_idx = 0;
static uint32_t s_t0  = 0;
static bool     s_running = false;

static inline void apply_target(int8_t l, int8_t r)
{
    /* Bezpośrednio – to eliminuje wszelkie „automatyki” warstwy testu */
    Tank_SetTarget(l, r);
}

void DriveTest_Start(void)
{
    s_idx = 0;
    s_running = true;
    s_t0 = HAL_GetTick();
    apply_target(DT_SCRIPT[s_idx].l_pct, DT_SCRIPT[s_idx].r_pct);
}

void DriveTest_Tick(void)
{
    if (!s_running) return;

    uint32_t now = HAL_GetTick();
    if ((now - s_t0) >= DT_SCRIPT[s_idx].dur_ms) {
        s_idx++;
        s_t0 = now;

        if (s_idx < DT_LEN) {
            apply_target(DT_SCRIPT[s_idx].l_pct, DT_SCRIPT[s_idx].r_pct);
        } else {
            Tank_SetTarget(0, 0);
            s_running = false;
        }
    }
}

bool DriveTest_IsRunning(void)
{
    return s_running;
}
