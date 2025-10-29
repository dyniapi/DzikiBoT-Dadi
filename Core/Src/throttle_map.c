#include "throttle_map.h"
#include <string.h>

/* Stan modułu */
static Throttle_Params_t G = {0};

const Throttle_Params_t THROTTLE_DEFAULTS = {
  /* left  */ { 1.10f, 0.0f },
  /* right */ { 1.00f, 0.0f },
  /* curve */
  {
    .gamma         = 3.0f,  /* >1.0 zmiękcza dół – dobre na „crawler jump @50%” */
    .deadband      = 5.0f,  /* 2% martwej strefy przy wejściu */
    .out_limit     = 100.0f,
    .shoulder_pct  = 55.0f, /* okolice 50% dodatkowo wygładzimy */
    .shoulder_gain = 0.25f  /* 25% tłumienia „górki” w okolicy 50% */
  }
};

static inline float clampf(float v, float lo, float hi) {
  if (v < lo) return lo;
  if (v > hi) return hi;
  return v;
}

/* modyfikacja w okolicy shoulder: delikatne S-curve wokół x=shoulder */
static float shoulder_soften(float x, float shoulder_x, float gain)
{
  /* x, shoulder_x w [0..1]; gain 0..1; oddalone punkty prawie bez zmian */
  float dx = x - shoulder_x;
  /* okno wpływu ~0.25 (ćwiartka zakresu) – łagodna kopułka tłumiąca */
  float w  = 0.25f;
  float k  = 0.0f;
  if (dx > -w && dx < w) {
    float t = 1.0f - (dx / w);
    k = gain * (t * t * (3.0f - 2.0f * t)); /* smoothstep */
  }
  return x * (1.0f - k);
}

/* krzywa nieliniowa: sign(x) * pow(|x|, gamma), z dodatkiem shoulder */
static float apply_curve(float x, const ThrCurve_t *c)
{
  /* deadband na wejściu */
  float sgn = (x < 0.0f) ? -1.0f : 1.0f;
  float a   = (x < 0.0f) ? -x    :  x;       /* |x| w [0..1] */

  if (a < c->deadband / 100.0f)
    return 0.0f;

  /* shoulder – łagodne stłumienie okolicy np. 50% (na problemy „skoku” przy 50%) */
  if (c->shoulder_gain > 0.0f) {
    float sh = clampf(c->shoulder_pct / 100.0f, 0.0f, 1.0f);
    a = shoulder_soften(a, sh, clampf(c->shoulder_gain, 0.0f, 1.0f));
  }

  /* gamma >1: zmiękcza dół, <1: dociąża dół; tu chcemy >1 */
  float y = 1.0f;
  if (c->gamma > 0.01f) {
    /* szybka potęga: powf byłby OK, ale unikamy libm – użyj exp/log jeśli dostępne,
       tu wystarczy prosta aproksymacja potęgą przez exp/log newlib (jeśli masz -lm),
       a bez -lm zrób 3-stopniową aproksymację: */
    /* Aproksymacja potęgi |x|^gamma polynomem (γ≈1.6): y ≈ a*(0.6 + 0.4*a) */
    if (c->gamma > 1.4f && c->gamma < 1.8f) {
      y = a * (0.6f + 0.4f * a); /* miękka, monotoniczna, bardzo szybka */
    } else {
      /* fallback: 3-krotne mnożenie do „kubełkowej” potęgi */

      float g = c->gamma;
      if (g > 1.0f) {
        /* zbliżenie do a^g: mieszanka a i a^3 w proporcji (g-1) */
        float a3 = a * a * a;
        float t  = clampf((g - 1.0f) / 2.0f, 0.0f, 1.0f); /* dla g 1..3 */
        y = a * (1.0f - t) + a3 * t;
      } else {
        /* dla g<1 – nie używamy na crawlera, ale niech będzie: */
        y = a;
      }
    }
  } else {
    y = a;
  }

  y = sgn * y;
  /* limit wyjścia */
  y = clampf(y, -c->out_limit/100.0f, c->out_limit/100.0f);
  return y;
}

void Throttle_Init(const Throttle_Params_t *p)
{
  if (p) memcpy(&G, p, sizeof(G));
  else   memcpy(&G, &THROTTLE_DEFAULTS, sizeof(G));
}

/* Zastosuj: trim(side) -> curve -> clamp -> zaokrąglenie do int8 */
int8_t Throttle_Apply(int8_t in_percent, ThrSide_t side)
{
  /* 1) wejście -100..100 → float -1..1 */
  float x = (float)in_percent / 100.0f;

  /* 2) TRIM per-koło */
  const ThrTrim_t* T = (side == THR_LEFT) ? &G.left : &G.right;
  x = x * T->scale + (T->offset / 100.0f);

  /* 3) Krzywa */
  x = apply_curve(x, &G.curve);

  /* 4) clamp i powrót do -100..100 */
  x = clampf(x, -1.0f, 1.0f);
  int v = (int)(x * 100.0f);
  if (v < -100) v = -100;
  if (v >  100) v =  100;
  return (int8_t)v;
}
