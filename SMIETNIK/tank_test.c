/**
 ******************************************************************************
 * @file    tank_test.c
 * @brief   Test jazdy na tank_drive (nieblokujący) z „keepalive” komend.
 ******************************************************************************
 */
#include "tank_test.h"
#include "tank_drive.h"
#include <string.h>

typedef enum {
    TT_IDLE = 0,
    TT_FWD,       /* przód przez fwd_time */
    TT_LEFT,      /* skręt w lewo przez left_time */
    TT_RIGHT,     /* skręt w prawo przez right_time */
    TT_STOP1,     /* krótki stop przed obrotem 180° */
    TT_SPIN180,   /* obrót o ~180° (czasowo) */
    TT_STOP2,     /* końcowy STOP */
    TT_DONE
} tt_state_t;

static struct {
    uint8_t   running;
    tt_state_t st;

    int8_t    fwd_speed;         /* -100..100 */
    uint8_t   turn_speed;        /* 0..100 */

    uint32_t  t0;                /* start aktualnej fazy (ms) */
    uint32_t  fwd_ms;
    uint32_t  left_ms;
    uint32_t  right_ms;
    uint32_t  spin180_ms;

} tt;

/* Pomocnicze: ustaw nową fazę i zapamiętaj czas startu */
static inline void tt_phase(tt_state_t s)
{
    tt.st = s;
    tt.t0 = HAL_GetTick();
}

void TankTest_Start(int8_t fwd_speed,
                    uint8_t turn_speed,
                    uint32_t fwd_time_ms,
                    uint32_t left_time_ms,
                    uint32_t right_time_ms,
                    uint32_t spin180_time_ms)
{
    memset(&tt, 0, sizeof(tt));
    tt.running     = 1;
    tt.fwd_speed   = (fwd_speed < -100) ? -100 : (fwd_speed > 100 ? 100 : fwd_speed);
    tt.turn_speed  = (turn_speed > 100) ? 100 : turn_speed;

    tt.fwd_ms      = (fwd_time_ms   == 0) ? 3000 : fwd_time_ms;
    tt.left_ms     = (left_time_ms  == 0) ? 2000 : left_time_ms;
    tt.right_ms    = (right_time_ms == 0) ? 2000 : right_time_ms;
    tt.spin180_ms  = (spin180_time_ms == 0) ? 1500 : spin180_time_ms;

    /* Start: jedź do przodu */
    tt_phase(TT_FWD);
}

void TankTest_Tick(void)
{
    if (!tt.running) return;

    const uint32_t now = HAL_GetTick();

    switch (tt.st)
    {
    case TT_FWD:
        /* KEEPALIVE: odświeżaj komendę, aby nie zadziałał deadman */
        Tank_SetSpeed(tt.fwd_speed, tt.fwd_speed);
        if ((now - tt.t0) >= tt.fwd_ms) {
            tt_phase(TT_LEFT);
        }
        break;

    case TT_LEFT: {
        int8_t s = (int8_t)tt.turn_speed;
        Tank_SetSpeed(-s, +s);  /* obrót w miejscu w lewo */
        if ((now - tt.t0) >= tt.left_ms) {
            tt_phase(TT_RIGHT);
        }
        } break;

    case TT_RIGHT: {
        int8_t s = (int8_t)tt.turn_speed;
        Tank_SetSpeed(+s, -s);  /* obrót w miejscu w prawo */
        if ((now - tt.t0) >= tt.right_ms) {
            tt_phase(TT_STOP1);
        }
        } break;

    case TT_STOP1:
        Tank_SetSpeed(0, 0);
        if ((now - tt.t0) >= 200U) {
            tt_phase(TT_SPIN180);
        }
        break;

    case TT_SPIN180: {
        int8_t s = (int8_t)tt.turn_speed;
        Tank_SetSpeed(-s, +s);  /* ~180° – kierunek w lewo; czasowo */
        if ((now - tt.t0) >= tt.spin180_ms) {
            tt_phase(TT_STOP2);
        }
        } break;

    case TT_STOP2:
        Tank_SetSpeed(0, 0);
        if ((now - tt.t0) >= 300U) {
            tt.st = TT_DONE;
        }
        break;

    case TT_DONE:
    default:
        tt.running = 0;
        Tank_SetSpeed(0, 0);
        break;
    }
}

uint8_t TankTest_IsRunning(void)
{
    return tt.running;
}

void TankTest_Abort(void)
{
    tt.running = 0;
    tt.st = TT_DONE;
    Tank_SetSpeed(0, 0);
}
