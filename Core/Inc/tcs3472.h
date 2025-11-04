/**
 * ============================================================================
 *  MODULE: tcs3472.h — Driver TCS3472 (I²C) dla DzikiBoT
 * -----------------------------------------------------------------------------
 *  CO:
 *    - Init Right/Left (oddzielne I²C), odczyt stabilizowanych RAW C/R/G/B.
 *
 *  JAK DZIAŁA (wewnątrz drivera):
 *    - EMA na kanałach C/R/G/B (wygładza szumy).
 *    - Auto-gain (1×/4×/16×/60×) z histerezą na kanale Clear.
 *    - Przy zmianie gainu: reskalowanie EMA (anty-skoki).
 *
 *  TUNING BEZ ZMIANY STRUKTUR (opcjonalnie w config.c):
 *    float CFG_TCS_EMA_Alpha(void); // alfa EMA (domyślnie 0.30)
 *    float CFG_TCS_AG_LoPct(void);  // próg dolny (domyślnie 0.60)
 *    float CFG_TCS_AG_HiPct(void);  // próg górny (domyślnie 0.70)
 *    void  TCS3472_OnGainChange(const char* side, TCS_Gain_t oldg, TCS_Gain_t newg); // weak hook
 * ============================================================================
 */

#ifndef TCS3472_H_
#define TCS3472_H_

#include "main.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint16_t clear;  // kanał Clear
    uint16_t red;    // kanał Red
    uint16_t green;  // kanał Green
    uint16_t blue;   // kanał Blue
} TCS3472_Data_t;

/* Right = I2C1, Left = I2C3 */
void           TCS3472_Right_Init(I2C_HandleTypeDef *hi2c1);
void           TCS3472_Left_Init (I2C_HandleTypeDef *hi2c3);
TCS3472_Data_t TCS3472_Right_Read(void);
TCS3472_Data_t TCS3472_Left_Read (void);

/* Konfiguracja rejestrów (PON/AEN/ATIME/GAIN) – publiczna jak w baseline */
void           TCS3472_Config(I2C_HandleTypeDef *hi2c);

#ifdef __cplusplus
}
#endif
#endif /* TCS3472_H_ */
