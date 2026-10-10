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
#include "radarAltHub.c"
#include "radarAltWindowShade.c"
#include "radarAltGlass.c"
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
lv_obj_t *img_radarAltHub;
lv_obj_t *img_radarAltWindowShade;
lv_obj_t *img_radarAltGlass;
lv_obj_t *img_RedLed;
lv_obj_t *img_GreenLed;

// Full-screen glass reflection and bezel shading drawn above everything. It adds a full-screen
// alpha blend to every redraw; set to 0 to compare CPU load.
#define USE_GLASS_OVERLAY 1

// Center and radius
const int16_t center_x = DISP_WIDTH / 2;
const int16_t center_y = DISP_HEIGHT / 2;

static lv_disp_drv_t disp_drv;

// Queues the SPI/DMA transfer; lv_disp_flush_ready is called when it completes
void my_disp_flush(lv_disp_drv_t *disp, const lv_area_t *area, lv_color_t *color_p) {
    LCD_addWindow(area->x1, area->y1, area->x2, area->y2, (uint16_t*)color_p);
}

static void flush_done() {
    lv_disp_flush_ready(&disp_drv);
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
static void demoHold(uint32_t ms) {
    uint32_t start = millis();
    while (millis() - start < ms) {
        lv_timer_handler();
        delay(5);
    }
}

// Runs a callback from lo to hi and back so each element visits its full range.
// Motion is time-based with smoothstep easing, so it stays smooth at any frame rate.
static void demoLeg(void (*cb)(unsigned int), unsigned int from, unsigned int to, uint32_t ms) {
    const uint32_t start = millis();
    uint32_t elapsed;
    while ((elapsed = millis() - start) < ms) {
        float t = (float)elapsed / ms;
        float s = t * t * (3.0f - 2.0f * t);
        cb(from + (int32_t)((int32_t)to - (int32_t)from) * s);
        lv_timer_handler();
        delay(5);
    }
    cb(to);                         // land exactly on the end value
    lv_timer_handler();
}

static void demoSweep(void (*cb)(unsigned int), unsigned int lo, unsigned int hi) {
    demoLeg(cb, lo, hi, 1800);
    demoLeg(cb, hi, lo, 1800);
    demoHold(250);
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

    lv_disp_drv_init(&disp_drv);
    disp_drv.hor_res = DISP_WIDTH;
    disp_drv.ver_res = DISP_HEIGHT;
    disp_drv.flush_cb = my_disp_flush;
    disp_drv.draw_buf = &draw_buf;
    lv_disp_drv_register(&disp_drv);
    LCD_SetFlushDoneCallback(flush_done);

    // ===== Black background =====
    lv_obj_set_style_bg_color(lv_scr_act(), lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(lv_scr_act(), LV_OPA_COVER, 0);


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

    // Shadow along the top edge of the OFF window (the window is a hole in the background bitmap)
    img_radarAltWindowShade = lv_img_create(lv_scr_act());
    lv_img_set_src(img_radarAltWindowShade, &radarAltWindowShade);
    lv_obj_set_pos(img_radarAltWindowShade, center_x - 44, center_y + 51);

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

    // Pivot at bottom center. LVGL pivots are pixel indices, and the art is symmetric about
    // index (w-1)/2 = 14.5, so use 14: the rotation center is then pixel 165+14 = 179, the same
    // as the needle's.
    lv_point_t pivot = { radarAltMinHeight.header.w / 2 - 1, radarAltMinHeight.header.h };
    lv_img_set_pivot(img_radarAltMinHeight, pivot.x, pivot.y);

    // ===== Radar Altimeter Needle =====
    // The needle bitmap is the shaft only (28x138). The hub is a separate fixed image on top, so
    // a small pivot error is hidden under it. Pivot is the hub center as a pixel index.
    const int16_t ROT_X = center_x - 1;   // rotation center pixel (179, 180), shared with the bug
    const int16_t ROT_Y = center_y;
    const int16_t NEEDLE_PIVOT_X = 13;
    const int16_t NEEDLE_PIVOT_Y = 137;

    img_radarAltNeedle = lv_img_create(lv_scr_act());
    lv_img_set_src(img_radarAltNeedle, &radarAltNeedle);
    lv_img_set_pivot(img_radarAltNeedle, NEEDLE_PIVOT_X, NEEDLE_PIVOT_Y);
    lv_obj_set_pos(img_radarAltNeedle, ROT_X - NEEDLE_PIVOT_X, ROT_Y - NEEDLE_PIVOT_Y);

    // ===== Fixed hub cap (73x73, centered on the rotation center) =====
    img_radarAltHub = lv_img_create(lv_scr_act());
    lv_img_set_src(img_radarAltHub, &radarAltHub);
    lv_obj_set_pos(img_radarAltHub, ROT_X - radarAltHub.header.w / 2, ROT_Y - radarAltHub.header.h / 2);

#if USE_GLASS_OVERLAY
    // ===== Glass reflection and bezel shading =====
    img_radarAltGlass = lv_img_create(lv_scr_act());
    lv_img_set_src(img_radarAltGlass, &radarAltGlass);
    lv_obj_set_pos(img_radarAltGlass, 0, 0);
#endif

    // Start needle at 0°
    lv_img_set_angle(img_radarAltNeedle, 0);

    startupDemo();

}


void loop() {
    DcsBios::loop();        // Process DCS-BIOS
    lv_timer_handler();     // Refresh LVGL

    delay(5);  // short delay to keep things smooth
}
