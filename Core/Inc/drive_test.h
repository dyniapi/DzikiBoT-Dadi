/**
 * @file    drive_test.h
 * @brief   Nieblokujący test napędu: FWD 2s → NEU 0.6s → REV 1s → NEU 0.3s
 * @date    2025-10-28
 *
 * Użycie:
 *   1) #include "drive_test.h" w main.c
 *   2) Po ESC_Init/ESC_ArmNeutral/Tank_Init wywołaj DriveTest_Start();
 *   3) W pętli (razem z Tank_Update) wywołuj DriveTest_Tick();
 */

#ifndef DRIVE_TEST_H
#define DRIVE_TEST_H

#include <stdbool.h>
#include <stdint.h>

void DriveTest_Start(void);
void DriveTest_Tick(void);
bool DriveTest_IsRunning(void);

#endif /* DRIVE_TEST_H */
