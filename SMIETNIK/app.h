\
#pragma once
/*
 * ============================================================================
 *  MODULE: app (warstwa harmonogramu aplikacji)
 *  Cel: utrzymać minimalny main.c i skupić wielotaktową logikę w jednym miejscu.
 *
 *  Użycie:
 *    App_Init();  // jednorazowo po starcie (ARM ESC, reset liczników, itp.)
 *    App_Tick();  // wywoływane w każdej iteracji pętli while(1)
 *
 *  Okresy zadań:
 *    pobierane z CFG_Scheduler() (sens/oled/uart) i CFG_Motors()->tick_ms (tank).
 * ============================================================================
 */
#ifdef __cplusplus
extern "C" {
#endif

void App_Init(void);   // przygotowanie modułu (ARM ESC itp.)
void App_Tick(void);   // harmonogram zadań (nieblokujący)

#ifdef __cplusplus
}
#endif
