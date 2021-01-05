#ifndef __PLAN_H
#define __PLAN_H

#define CMP_INVALID 0x00
#define CMP_WAIT 0x01
#define CMP_COLOR 0x02
#define CMP_LEVEL 0x04
#define CMP_NOP 0x08

typedef struct _Action2 {
    uint8_t pixel;
    uint8_t state;

    CRGB color;
    fract8 level;
    unsigned int delay;
    int8_t delta_level;
    int8_t delta_delay;

    uint8_t cmp_flags;
    unsigned int cmp_delay;
    uint8_t cmp_level;

    uint8_t target_pixel;
    uint8_t target_state;
} Action2;

typedef struct _PixelPlan {
    uint8_t len;
    Action2* actions;
} PixelPlan;

typedef struct _Plan {
    uint8_t len;
    PixelPlan** pixels;
} Plan;

void parse_plan(Plan* plan, char** buffer);
void free_plan(Plan* plan);

#endif
