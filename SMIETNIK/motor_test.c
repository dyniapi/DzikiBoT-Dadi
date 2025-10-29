/**
 * @file    motor_test.c
 * @brief   Nieblokujący test dwóch ESC (TIM1 CH1/CH4) z rampą: 5s FWD -> STOP -> 5s REV -> STOP.
 */
#include "motor_test.h"
#include "motor_bldc.h"   // ESC_Init / ESC_WriteUs / ESC_MapSpeedToUs / ESC_CH1 / ESC_CH4
#include <string.h>

/* ───────────────────── Konfiguracja domyślna ───────────────────── */
#define TEST_SPEED_TARGET_FWD   (+90)   // % docelowe w przód
#define TEST_SPEED_TARGET_REV   (-90)   // % docelowe w tył
#define TEST_PHASE_TIME_MS      (10000U)  // 5 sekund na fazę ruchu
#define TEST_STOP_TIME_MS       (50U)   // krótki postój między fazami (opcjonalnie)

/* ───────────────────── Zmienna stanu ───────────────────── */
typedef enum {
    MT_IDLE = 0,
    MT_FWD,        // rampa do +100 i jedzie do końca 5 s (czas fazy liczony od startu fazy)
    MT_STOP1,      // rampa do 0 (krótki postój)
    MT_REV,        // rampa do -100 i jedzie do końca 5 s
    MT_STOP2,      // rampa do 0 (koniec)
    MT_DONE
} mt_state_t;

static struct {
    uint8_t   running;
    mt_state_t state;

    int16_t   current_spd;   // bieżąca prędkość [-100..100]
    int16_t   target_spd;    // docelowa prędkość fazy
    uint8_t   ramp_rate;     // maks. zmiana prędkości na krok (|Δ| per tick)

    uint16_t  tick_ms;       // okres „kroku” sterowania
    uint32_t  t_last;        // znacznik czasu ostatniego kroku (HAL_GetTick)
    uint32_t  t_phase0;      // znacznik początku fazy (do odmierzania 5 s)
} mt;

/* ───────────────────── Pomocnicze ───────────────────── */
static void mt_set_targets(int16_t spd_target)
{
    mt.target_spd = spd_target;
    mt.t_phase0   = HAL_GetTick();
}

static void mt_apply_output(int16_t spd_percent)
{
    /* Mapuj na µs i wyślij na oba kanały (TIM1 CH1/CH4) */
    uint16_t us = ESC_MapSpeedToUs((int8_t)spd_percent);
    ESC_WriteUs(ESC_CH1, us);
    ESC_WriteUs(ESC_CH4, us);
}

static void mt_ramp_step(void)
{
    /* Zbliż current_spd do target_spd nie szybciej niż ramp_rate na krok */
    if (mt.current_spd < mt.target_spd) {
        mt.current_spd += mt.ramp_rate;
        if (mt.current_spd > mt.target_spd) mt.current_spd = mt.target_spd;
    } else if (mt.current_spd > mt.target_spd) {
        mt.current_spd -= mt.ramp_rate;
        if (mt.current_spd < mt.target_spd) mt.current_spd = mt.target_spd;
    }
    if (mt.current_spd < -100) mt.current_spd = -100;
    if (mt.current_spd >  100) mt.current_spd =  100;

    mt_apply_output(mt.current_spd);
}

/* ───────────────────── API ───────────────────── */
void MotorTest_Start(uint8_t ramp_rate, uint16_t tick_ms)
{
    memset(&mt, 0, sizeof(mt));
    mt.running   = 1;
    mt.state     = MT_FWD;
    mt.tick_ms   = (tick_ms == 0) ? 20 : tick_ms;
    mt.ramp_rate = (ramp_rate == 0) ? 3  : ramp_rate;  // sensowne minimum

    mt.current_spd = 0;              // start od STOP
    mt_set_targets(TEST_SPEED_TARGET_FWD);
    mt.t_last = HAL_GetTick();
    /* Nie ustawiamy neutral tu — zakładamy, że ESC_Init/Neutral już było w main.c */
}

void MotorTest_Tick(void)
{
    if (!mt.running) return;

    uint32_t now = HAL_GetTick();
    if ((now - mt.t_last) < mt.tick_ms) return;   // czekamy do następnego kroku
    mt.t_last = now;

    switch (mt.state)
    {
    case MT_FWD:
        mt_ramp_step();
        /* Faza trwa 5 s od startu fazy (łącznie z rampą) */
        if ((now - mt.t_phase0) >= TEST_PHASE_TIME_MS) {
            mt.state = MT_STOP1;
            mt_set_targets(0);
        }
        break;

    case MT_STOP1:
        mt_ramp_step();
        /* gdy dojechaliśmy do 0 – krótka pauza i następna faza */
        if (mt.current_spd == 0) {
            if ((now - mt.t_phase0) >= TEST_STOP_TIME_MS) {
                mt.state = MT_REV;
                mt_set_targets(TEST_SPEED_TARGET_REV);
            }
        }
        break;

    case MT_REV:
        mt_ramp_step();
        if ((now - mt.t_phase0) >= TEST_PHASE_TIME_MS) {
            mt.state = MT_STOP2;
            mt_set_targets(0);
        }
        break;

    case MT_STOP2:
        mt_ramp_step();
        if (mt.current_spd == 0) {
            mt.state   = MT_DONE;
        }
        break;

    case MT_DONE:
    default:
        /* Test zakończony – zostaw neutral na obu kanałach */
        ESC_SetNeutralAll();
        mt.running = 0;
        break;
    }
}

uint8_t MotorTest_IsRunning(void)
{
    return mt.running;
}
