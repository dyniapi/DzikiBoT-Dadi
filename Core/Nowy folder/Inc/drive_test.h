/**
 * @file    drive_test.h
 * @brief   Sekwencje testowe napędu (FWD/NEU/REV) do szybkiej diagnostyki.
 * @date    2025-11-02
 *
 * Uwaga:
 *   Zachowaj spójność z resztą modułów oraz konwencje projektu.
 */

#ifndef DRIVE_TEST_H
#define DRIVE_TEST_H

#include <stdbool.h>
#include <stdint.h>

void DriveTest_Start(void);
void DriveTest_Tick(void);
bool DriveTest_IsRunning(void);

#endif /* DRIVE_TEST_H */
