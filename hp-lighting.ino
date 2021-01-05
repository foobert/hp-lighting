#include <Arduino.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <ESP8266WiFi.h>
#include <ESP8266WiFiMulti.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClientSecure.h>

#include "config.h"

#define FASTLED_ESP8266_RAW_PIN_ORDER
#include <FastLED.h>
#define NUM_LEDS 8
#define DATA_PIN 4
#define CLOCK_PIN 5

#define DEBUG 1
#define TLS 0

#include "plan.h"

Plan plan;

typedef struct Pixel {
  uint8_t state;
  unsigned int delay;
  fract8 level;
} Pixel;

ESP8266WiFiMulti WiFiMulti;

CRGB leds[NUM_LEDS];
Pixel pixels[NUM_LEDS];
unsigned long previousMillis = 0;
const long interval = 10;

void load_plan() {

  Serial.print("Connecting to network");
  WiFi.mode(WIFI_STA);
  WiFiMulti.addAP(WLAN_SSID, WLAN_PASS);
  while (WiFiMulti.run() != WL_CONNECTED) {
    Serial.print(".");
    delay(100);
  }

  Serial.println();

  if ((WiFiMulti.run() == WL_CONNECTED)) {

    BearSSL::WiFiClientSecure client;
    client.setInsecure();

    HTTPClient http;
    Serial.printf("Loading plan from %s...\n", PLAN_URL);
    http.begin(client, PLAN_URL);
    int httpCode = http.GET();
    Serial.printf("HTTP response code: %d\n", httpCode);

    if (httpCode == HTTP_CODE_OK) {
        String payload = http.getString();
        Serial.println(payload);
        char* buffer = (char*)malloc(4096);
        char* orig_buffer = buffer;
        payload.toCharArray(buffer, 4096);
        parse_plan(&plan, &buffer);

        Serial.printf("Plan has %d pixels\n", plan.len);
        for (int i = 0; i < plan.len; i++) {
            PixelPlan* pixel_plan = plan.pixels[i];
            if (pixel_plan == NULL) {
                Serial.printf("- Pixel %2d has no pixel plan\n", i);
                continue;
            }
            Serial.printf("- Pixel %2d has %d actions\n", i, pixel_plan->len);
            for (int j = 0; j < pixel_plan->len; j++) {
                Action2 action = pixel_plan->actions[j];
                Serial.printf("  - State: %02d, Color: %06X, Level: %3d, CMP Flags: %d\n",
                        action.state, action.color, action.level, action.cmp_flags);
            }
        }
        free(orig_buffer);
    }
    http.end();
  }
}

void update_pixel(uint8_t target_pixel, uint8_t target_state) {
    if (target_pixel >= NUM_LEDS) {
        return;
    }
    PixelPlan* pixel_plan = plan.pixels[target_pixel];
    if (pixel_plan == NULL) {
        return;
    }
    if (pixel_plan->len <= target_state) {
        return;
    }
    Action2 action = pixel_plan->actions[target_state];
    if (action.cmp_flags == CMP_INVALID || action.pixel >= NUM_LEDS) {
        return;
    }

    pixels[action.pixel].state = target_state;
    pixels[action.pixel].delay = action.delay;
    pixels[action.pixel].level = action.level;

    leds[action.pixel] = dim(action.color, action.level);

    Serial.printf("Pixel %d/%02d: Color: %06X, Level: %3d, Delay: %5d\n",
        action.pixel, target_state, action.color, action.level, action.delay);
}

void init_state() {
    for (int i = 0; i < plan.len; i++) {
        update_pixel(i, 0);
    }
}

void setup() {
  Serial.begin(115200);
  Serial.println();
  Serial.println();

  Serial.printf("Waiting before startup");
  for (uint8_t t = 4; t > 0; t--) {
      Serial.printf(".", t);
      Serial.flush();
      delay(100);
  }
  Serial.println();

  FastLED.addLeds<WS2811, DATA_PIN>(leds, NUM_LEDS);
  fill_solid(leds, NUM_LEDS, CRGB(255, 255, 255));
  //leds[3] = CRGB(255, 0, 0);
  //leds[4] = CRGB(232, 145, 14);
  FastLED.show();

  randomSeed(micros());

  load_plan();
  init_state();

  Serial.printf("Initialization complete\n\n");
}

void tick() {
   for (int i = 0; i < NUM_LEDS; i++) {
       tick_pixel(i);
   }
   FastLED.show();
}

void tick_pixel(int pixel_index) {
    if (pixel_index >= plan.len) {
        return;
    }

    PixelPlan* pixel_plan = plan.pixels[pixel_index];
    if (pixel_plan == NULL) {
        return;
    }

    Pixel* pixel = &pixels[pixel_index];
    uint8_t current_state = pixel->state;

    if (current_state >= pixel_plan->len) {
        Serial.printf("Pixel %d/%02d: No matching state\n", pixel_index, current_state);
        return;
    }
    Action2 action = pixel_plan->actions[current_state];

    if ((action.cmp_flags & CMP_NOP) == CMP_NOP) {
    } else if ((action.cmp_flags & CMP_WAIT) == CMP_WAIT) {
        if ((action.delta_delay < 0 && pixel->delay <= action.cmp_delay) ||
                (action.delta_delay > 0 && pixel->delay >= action.cmp_delay) ||
                (pixel->delay == action.cmp_delay)) {
            uint8_t new_state = action.target_state;
            uint8_t new_pixel = action.target_pixel;
            Serial.printf("Pixel %d/%02d: Delay transition pixel %d/%02d\n", pixel_index, current_state, new_pixel, new_state);
            update_pixel(new_pixel, new_state);
        }
    } if ((action.cmp_flags & CMP_LEVEL) == CMP_LEVEL) {
        if ((action.delta_level < 0 && pixel->level <= action.cmp_level) ||
                (action.delta_level > 0 && pixel->level >= action.cmp_level) ||
                (pixel->level == action.cmp_level)) {
            uint8_t new_state = action.target_state;
            uint8_t new_pixel = action.target_pixel;
            Serial.printf("Pixel %d/%02d: Level transition pixel %d/%02d\n", pixel_index, current_state, new_pixel, new_state);
            update_pixel(new_pixel, new_state);
        }
    } else {
        // unknown cmp flag
    }

    if (action.delta_delay < 0) {
        pixel->delay = std::max(0u, pixel->delay + action.delta_delay);
    } else if (action.delta_delay > 0) {
        pixel->delay = std::min(UINT_MAX, pixel->delay + action.delta_delay);
    }

    if (action.delta_level < 0) {
        pixel->level = std::max(0, pixel->level + action.delta_level);
    } else if (action.delta_level > 0) {
        pixel->level = std::min(UCHAR_MAX, pixel->level + action.delta_level);
    }

    //Serial.printf("Tick! Pixel: %d, Delay: %d (Delta %d), Level: %d (Delta %d)\n", pixel_index, pixel->delay,action.delta_delay, pixel->level, action.delta_level);
    leds[pixel_index] = dim(action.color, pixel->level);
}

CRGB dim(CRGB color, fract8 level) {
    CRGB black = CRGB(0, 0, 0);
    return black.lerp8(color, level);
}

void loop() {
    unsigned long currentMillis = millis();

    if (currentMillis - previousMillis >= interval) {
        previousMillis = currentMillis;
        tick();
    }
}
