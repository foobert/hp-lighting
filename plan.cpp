#define FASTLED_ESP8266_RAW_PIN_ORDER
#include <FastLED.h>

#include "plan.h"

long read_long(char** buffer) {
    //Serial.printf("About to read_long from\n%s\n", *buffer);
    long res = strtol(*buffer, buffer, 10);
    while (**buffer == ',') {
        (*buffer)++;
    }
    return res;
}

void skip_line(char** buffer) {
    while (**buffer != '\n') {
        (*buffer)++;
    }
    (*buffer)++;
}

CRGB read_rgb(char** buffer) {
    //Serial.printf("About to read_rgb from\n%s\n", *buffer);
    int rgb = (int)strtol(*buffer, buffer, 16);
    while (**buffer == ',') {
        (*buffer)++;
    }
    CRGB color = CRGB((rgb >> 16) & 0xFF, (rgb >> 8) & 0xFF, (rgb >> 0) & 0xFF);
    return color;
}

Action2* parse_line(char** buffer) {
    if (buffer == NULL || **buffer == '\0') {
        return NULL;
    }

    if (**buffer == '#') {
        skip_line(buffer);
        return NULL;
    }

    Action2* action = (Action2*)malloc(sizeof(Action2));

    action->pixel = read_long(buffer);
    action->state = read_long(buffer);

    action->color = read_rgb(buffer);
    action->level = read_long(buffer);
    action->delay = read_long(buffer);
    action->delta_level = read_long(buffer);
    action->delta_delay = read_long(buffer);

    action->cmp_flags = read_long(buffer);
    action->cmp_delay = read_long(buffer);
    action->cmp_level = read_long(buffer);

    action->target_pixel = read_long(buffer);
    action->target_state = read_long(buffer);

    skip_line(buffer);

    return action;
}

PixelPlan* get_pixel_plan(Plan* plan, uint8_t pixel) {
    if (pixel >= plan->len) {
        // resize plan
        //printf("need to resize pixel plans, current %p %d\n", plan.pixels, plan.len);
        plan->pixels = (PixelPlan**)realloc(plan->pixels, sizeof(PixelPlan*) * (pixel + 1));
        //printf("new pixels: %p\n", plan.pixels);
        for (int i = plan->len; i < pixel + 1; i++) {
            //printf("init pixelplan %d to NULL\n", i);
            plan->pixels[i] = NULL;
        }
        plan->len = pixel + 1;
    }
    if (plan->pixels[pixel] == NULL) {
        PixelPlan* pixel_plan = (PixelPlan*)malloc(sizeof(PixelPlan));
        pixel_plan->len = 0;
        pixel_plan->actions = NULL;
        plan->pixels[pixel] = pixel_plan;
    }
    //printf("returning pixel plan %p for pixel %d\n", plan.pixels[pixel], pixel);
    return plan->pixels[pixel];
}

void parse_plan(Plan* plan, char** buffer) {
    Serial.println("Parsing plan...");
    while (**buffer != '\0') {
        Action2* action = parse_line(buffer);
        //printf("parsed action %p\n", action);
        if (action != NULL) {
            //printf("action is for pixel %d\n", action->pixel);
            PixelPlan* pixel_plan = get_pixel_plan(plan, action->pixel);
            unsigned int index = action->state;
            if (index >= pixel_plan->len) {
                // resize plan
                pixel_plan->actions = (Action2*)realloc(pixel_plan->actions, sizeof(Action2) * (index + 1));
                for (int i = pixel_plan->len; i < index; i++) {
                    pixel_plan->actions[i].cmp_flags = CMP_INVALID;
                }
                pixel_plan->len = index + 1;
            }
            // put the action into an existing slot
            memcpy(pixel_plan->actions + index, action, sizeof(Action2));
            free(action);
            //Serial.println("Added action for pixel %d\n", action->pixel);
        }
    }
}

void free_plan(Plan* plan) {
    if (plan == NULL) {
        return;
    }
    for (int i = 0; i < plan->len; i++) {
        if (plan->pixels[i] != NULL) {
            free(plan->pixels[i]);
            plan->pixels[i] = NULL;
        }
    }
    plan->len = 0;
}
