// Designed for Waveshare 1.85" round lcd without touch screen
// https://www.waveshare.com/wiki/ESP32-S3-LCD-1.85
// 
// LVGL version is 8.3.11
// Display_ST77916, esp_lcd_st77916, I2C_Driver files sourced from WaveShare's demo code for the display

#define DCSBIOS_DEFAULT_SERIAL
#define DCSBIOS_DISABLE_SERVO 
#include <DcsBios.h>


#include <Arduino.h>
#include "Display_ST77916.h"
#include <lvgl.h>

// LVGL bitmaps
#include "radarAltBackground.c"
#include "radarAltNeedle.c"
#include "radarAltMinHeight.c"
#include "radarAltOff.c"
#include "RedLedOff.c"
#include "GreenLedOff.c"
#include "RedLedOn.c"
#include "GreenLedOn.c"


#define DISP_WIDTH  360
#define DISP_HEIGHT 360

// LVGL draw buffers
static lv_color_t buf1[DISP_WIDTH * 40];
static lv_color_t buf2[DISP_WIDTH * 40];

// ===== Globals =====
lv_obj_t *img_radarAltBackground;
lv_obj_t *img_radarAltNeedle;
lv_obj_t *img_radarAltMinHeight;
lv_obj_t *img_radarAltOff;
lv_obj_t *img_RedLed;
lv_obj_t *img_GreenLed;

// Center and radius
const int16_t center_x = DISP_WIDTH / 2;
const int16_t center_y = DISP_HEIGHT / 2;

void my_disp_flush(lv_disp_drv_t *disp, const lv_area_t *area, lv_color_t *color_p) {
    LCD_addWindow(area->x1, area->y1, area->x2, area->y2, (uint16_t*)color_p);
    lv_disp_flush_ready(disp);
}


void onRadaltMinHeightPtrChange(unsigned int newValue) 
{
  lv_img_set_angle(img_radarAltMinHeight, map(newValue, 1800, 65530, 0, 3200));
}
DcsBios::IntegerBuffer radaltMinHeightPtrBuffer(0x7518, 0xffff, 0, onRadaltMinHeightPtrChange);



static void onRadaltOffFlagChange(unsigned int newValue) {
  const int16_t H = (int16_t)radarAltOff.header.h;
  const int16_t OFF_EXTRA = 5;

  // Translate Y goes from -(H+5) (off) to 0 (fully visible)
  int32_t ty = - (int32_t)H - OFF_EXTRA
               + ((int32_t)(H + OFF_EXTRA) * (int32_t)newValue) / 65535;

  // Clamp (just in case)
  if (ty > 0) ty = 0;
  if (ty < -((int32_t)H + OFF_EXTRA)) ty = -((int32_t)H + OFF_EXTRA);

  lv_obj_set_style_translate_y(img_radarAltOff, (int16_t)ty, 0);
}
DcsBios::IntegerBuffer radaltOffFlagBuffer(0x751c, 0xffff, 0, onRadaltOffFlagChange);



void onRadaltGreenLampChange(unsigned int newValue)
{
  if (newValue==1)
  {
      lv_img_set_src(img_GreenLed, &GreenLedOn);
  }
  else
  {
      lv_img_set_src(img_GreenLed, &GreenLedOff);
  }
}
DcsBios::IntegerBuffer radaltGreenLampBuffer(0x74a0, 0x0100, 8, onRadaltGreenLampChange);

void onLowAltWarnLtChange(unsigned int newValue)
{
  if (newValue==1)
  {
      lv_img_set_src(img_RedLed, &RedLedOn);
  }
  else
  {
      lv_img_set_src(img_RedLed, &RedLedOff);
  }
}
DcsBios::IntegerBuffer lowAltWarnLtBuffer(0x749c, 0x8000, 15, onLowAltWarnLtChange);

void onRadaltAltPtrChange(unsigned int newValue) 
{
    lv_img_set_angle(img_radarAltNeedle, map(newValue,3450, 65530, 0, 3200));
}
DcsBios::IntegerBuffer radaltAltPtrBuffer(0x751a, 0xffff, 0, onRadaltAltPtrChange);


// ===== Startup demo =====
// Runs a callback from lo to hi and back so each element visits its full range.
static void demoSweep(void (*cb)(unsigned int), unsigned int lo, unsigned int hi) {
    const int STEPS = 40;
    const uint32_t STEP_MS = 35;   // a bit over LV_DISP_DEF_REFR_PERIOD so every step renders

    for (int i = 0; i <= STEPS; i++) {
        cb(lo + (uint32_t)(hi - lo) * i / STEPS);
        lv_timer_handler();
        delay(STEP_MS);
    }
    for (int i = STEPS; i >= 0; i--) {
        cb(lo + (uint32_t)(hi - lo) * i / STEPS);
        lv_timer_handler();
        delay(STEP_MS);
    }
}

static void demoHold(uint32_t ms) {
    uint32_t start = millis();
    while (millis() - start < ms) {
        lv_timer_handler();
        delay(5);
    }
}

static void startupDemo() {
    // LEDs on, then off
    onRadaltGreenLampChange(1);
    onLowAltWarnLtChange(1);
    demoHold(800);
    onRadaltGreenLampChange(0);
    onLowAltWarnLtChange(0);
    demoHold(400);

    // Needle: min -> max -> min
    demoSweep(onRadaltAltPtrChange, 3450, 65530);
    // Min height bug: min -> max -> min
    demoSweep(onRadaltMinHeightPtrChange, 1800, 65530);
    // OFF flag: hidden -> fully visible -> hidden
    demoSweep(onRadaltOffFlagChange, 0, 65535);
}

void setup() {
    DcsBios::setup();

    ST77916_Init();
    Backlight_Init();
    Set_Backlight(25);

    lv_init();

    static lv_disp_draw_buf_t draw_buf;
    lv_disp_draw_buf_init(&draw_buf, buf1, buf2, DISP_WIDTH * 40);

    static lv_disp_drv_t disp_drv;
    lv_disp_drv_init(&disp_drv);
    disp_drv.hor_res = DISP_WIDTH;
    disp_drv.ver_res = DISP_HEIGHT;
    disp_drv.flush_cb = my_disp_flush;
    disp_drv.draw_buf = &draw_buf;
    lv_disp_drv_register(&disp_drv);

    // ===== Black background =====
    lv_obj_t *bg_rect = lv_obj_create(lv_scr_act());
    lv_obj_set_size(bg_rect, DISP_WIDTH, DISP_HEIGHT);
    lv_obj_set_style_bg_color(bg_rect, lv_color_hex(0x000000), 0);
    lv_obj_set_style_border_width(bg_rect, 0, 0);
    lv_obj_clear_flag(bg_rect, LV_OBJ_FLAG_SCROLLABLE);


    // OFF flag (final resting position is centered with +69px)
    img_radarAltOff = lv_img_create(lv_scr_act());
    lv_img_set_src(img_radarAltOff, &radarAltOff);
    lv_obj_align(img_radarAltOff, LV_ALIGN_CENTER, 0, 69);

    // Start fully hidden behind the gauge face
    const int16_t OFF_EXTRA = 5;
    lv_obj_set_style_translate_y(img_radarAltOff,-(int16_t)radarAltOff.header.h - OFF_EXTRA, 0);


    // ===== Radar background =====
    img_radarAltBackground = lv_img_create(lv_scr_act());
    lv_img_set_src(img_radarAltBackground, &radarAltBackground);
    lv_obj_align(img_radarAltBackground, LV_ALIGN_CENTER, 0, 0);

    // ===== LEDs =====
    img_RedLed = lv_img_create(lv_scr_act());
    lv_img_set_src(img_RedLed, &RedLedOff);
    lv_obj_align(img_RedLed, LV_ALIGN_CENTER, -65, 0);

    img_GreenLed = lv_img_create(lv_scr_act());
    lv_img_set_src(img_GreenLed, &GreenLedOff);
    lv_obj_align(img_GreenLed, LV_ALIGN_CENTER, 65, 0);

    // ===== Altitude Min Height Indicator =====
    img_radarAltMinHeight = lv_img_create(lv_scr_act());
    lv_img_set_src(img_radarAltMinHeight, &radarAltMinHeight);

    // Position pointer at top center (example)
    int16_t x = center_x - radarAltMinHeight.header.w / 2;
    // Bottom-center pivot must land on the display center, so y = center_y - image height
    int16_t y = center_y - radarAltMinHeight.header.h;
    lv_obj_set_pos(img_radarAltMinHeight, x, y);

    // Set pivot to bottom center
    lv_point_t pivot = { radarAltMinHeight.header.w / 2, radarAltMinHeight.header.h };
    lv_img_set_pivot(img_radarAltMinHeight, pivot.x, pivot.y);

    // ===== Radar Altimeter Needle =====
    img_radarAltNeedle = lv_img_create(lv_scr_act());
    lv_img_set_src(img_radarAltNeedle, &radarAltNeedle);
    // Hub center inside the needle bitmap (measured from the image alpha: x=35.5, y=123)
    const int16_t NEEDLE_PIVOT_X = 36;
    const int16_t NEEDLE_PIVOT_Y = 123;
    lv_img_set_pivot(img_radarAltNeedle, NEEDLE_PIVOT_X, NEEDLE_PIVOT_Y);

    // Place the image so the pivot lands exactly on the display center
    lv_obj_set_pos(img_radarAltNeedle, center_x - NEEDLE_PIVOT_X, center_y - NEEDLE_PIVOT_Y);

    // Start needle at 0°
    lv_img_set_angle(img_radarAltNeedle, 0);

    startupDemo();

}


void loop() {
    DcsBios::loop();        // Process DCS-BIOS
    lv_timer_handler();     // Refresh LVGL

    delay(5);  // short delay to keep things smooth
}
