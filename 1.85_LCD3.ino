#define DCSBIOS_DEFAULT_SERIAL
#define DCSBIOS_DISABLE_SERVO 
#include <DcsBios.h>


#include <Arduino.h>
#include "Display_ST77916.h"
#include <lvgl.h>

// LVGL bitmaps
#include "radarAltBackground.c"
#include "radarAltNeedle.c";
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
  lv_img_set_angle(img_radarAltMinHeight, map(newValue, 0, 65530, 0, 3200));
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





//
//
//   BIT TEST for ESP32 Startup (NOT DCS TEST)
//
//
// ======== ADD: BIT globals & helpers (near your other globals) ========
static lv_timer_t* bit_led_timer = nullptr;
static bool bit_led_on = false;

// Flip both LEDs (switching image sources you already have)
static inline void set_leds(bool on) {
  lv_img_set_src(img_GreenLed, on ? &GreenLedOn : &GreenLedOff);
  lv_img_set_src(img_RedLed,   on ? &RedLedOn   : &RedLedOff);
}

// LED flasher (250 ms)
static void bit_led_toggle_cb(lv_timer_t* t) {
  (void)t;
  bit_led_on = !bit_led_on;
  set_leds(bit_led_on);
}

// Utility to start/stop flashing
static void start_led_flash(uint32_t period_ms = 250) {
  if (bit_led_timer) lv_timer_del(bit_led_timer);
  bit_led_on = false;
  set_leds(false);
  bit_led_timer = lv_timer_create(bit_led_toggle_cb, period_ms, NULL);
}
static void stop_led_flash() {
  if (bit_led_timer) { lv_timer_del(bit_led_timer); bit_led_timer = nullptr; }
  set_leds(false);
}

// ======== ADD: LVGL anim exec callbacks ========
static void exec_set_img_angle(void* obj, int32_t v) {
  // v expected in LVGL angle units (0..~3600, where 3600 = 360°*10)
  lv_img_set_angle((lv_obj_t*)obj, v);
}

// Use your off-flag math by calling the same function with 0..65535
static void exec_offflag_translate(void* obj, int32_t v) {
  (void)obj;
  onRadaltOffFlagChange((unsigned int)v);
}

// ======== ADD: Chained step starters ========
static void start_bit_minheight_sweep_forward();   // fwd decl
static void start_bit_minheight_sweep_backward();
static void start_bit_needle_sweep_forward();
static void start_bit_needle_sweep_backward();
static void start_bit_offflag_down();
static void start_bit_offflag_up();

// Build a one-shot anim
static void make_anim(lv_obj_t* obj, lv_anim_exec_xcb_t exec, int32_t start, int32_t end,
                      uint32_t time_ms, uint32_t delay_ms,
                      lv_anim_ready_cb_t on_ready, void* user = NULL)
{
  lv_anim_t a;
  lv_anim_init(&a);
  lv_anim_set_var(&a, obj ? obj : (void*)user); // obj may be NULL when using custom exec that ignores it
  lv_anim_set_exec_cb(&a, exec);
  lv_anim_set_values(&a, start, end);
  lv_anim_set_time(&a, time_ms);
  lv_anim_set_delay(&a, delay_ms);
  lv_anim_set_path_cb(&a, lv_anim_path_linear);
  if (on_ready) lv_anim_set_ready_cb(&a, on_ready);
  lv_anim_start(&a);
}

// ======== ADD: Sequence pieces (ready callbacks) ========
static void bit_led_done_cb(lv_anim_t* a) {
  (void)a;
  stop_led_flash();
  start_bit_minheight_sweep_forward();
}

static void minheight_fwd_done(lv_anim_t* a) {
  (void)a;
  start_bit_minheight_sweep_backward();
}
static void minheight_back_done(lv_anim_t* a) {
  (void)a;
  start_bit_needle_sweep_forward();
}
static void needle_fwd_done(lv_anim_t* a) {
  (void)a;
  start_bit_needle_sweep_backward();
}
static void needle_back_done(lv_anim_t* a) {
  (void)a;
  start_bit_offflag_down();
}
static void offflag_down_done(lv_anim_t* a) {
  (void)a;
  start_bit_offflag_up();
}
static void offflag_up_done(lv_anim_t* a) {
  (void)a;
  // BIT complete — leave LEDs off and flag hidden as initial
  set_leds(false);
}

// ======== ADD: Step implementations ========
// 1) MinAltHeight 0 -> 3200 -> 0 (2s total)
static void start_bit_minheight_sweep_forward() {
  make_anim(img_radarAltMinHeight, exec_set_img_angle, 0, 3200, 1000, 0, minheight_fwd_done);
}
static void start_bit_minheight_sweep_backward() {
  make_anim(img_radarAltMinHeight, exec_set_img_angle, 3200, 0, 1000, 0, minheight_back_done);
}

// 2) Needle 0 -> 3200 -> 0 (2s total)
static void start_bit_needle_sweep_forward() {
  make_anim(img_radarAltNeedle, exec_set_img_angle, 0, 3200, 1000, 0, needle_fwd_done);
}
static void start_bit_needle_sweep_backward() {
  make_anim(img_radarAltNeedle, exec_set_img_angle, 3200, 0, 1000, 0, needle_back_done);
}

// 3) OFF flag hidden -> visible -> hidden (≈2s total) using same transform curve
//    Animate 0..65535 down (revealing) then back up (hiding)
static void start_bit_offflag_down() {
  // obj unused; pass user data via "user" param (NULL ok). Exec uses onRadaltOffFlagChange.
  make_anim(NULL, exec_offflag_translate, 0, 65535, 1000, 0, offflag_down_done, NULL);
}
static void start_bit_offflag_up() {
  make_anim(NULL, exec_offflag_translate, 65535, 0, 1000, 0, offflag_up_done, NULL);
}

// ======== ADD: BIT entrypoint ========
static void startBIT() {
  // 0) Flash both LEDs for 4 seconds at 4Hz (toggle every 125 ms) or 2Hz if you prefer 250ms
  start_led_flash(250);

  // Use a tiny ‘dummy’ animation just to get a 4s callback, then stop flashing and continue.
  // (Cleaner than juggling extra timers.)
  make_anim(NULL, /*exec*/ (lv_anim_exec_xcb_t)NULL, 0, 1, 4000, 0, bit_led_done_cb, NULL);
}




void setup() {
    DcsBios::setup();

    ST77916_Init();
    Backlight_Init();
    Set_Backlight(20);

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

    // Start fully hidden, but 5px higher than before
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

    // ===== Static Pointer =====
    img_radarAltMinHeight = lv_img_create(lv_scr_act());
    lv_img_set_src(img_radarAltMinHeight, &radarAltMinHeight);

    // Position pointer at top center (example)
    int16_t x = center_x - radarAltMinHeight.header.w / 2;
    int16_t y = 0; // distance from top edge
    lv_obj_set_pos(img_radarAltMinHeight, x, y);

    // Set pivot to bottom center
    lv_point_t pivot = { radarAltMinHeight.header.w / 2, radarAltMinHeight.header.h };
    lv_img_set_pivot(img_radarAltMinHeight, pivot.x, pivot.y);

    // ===== Static Pointer =====
    img_radarAltNeedle = lv_img_create(lv_scr_act());
    lv_img_set_src(img_radarAltNeedle, &radarAltNeedle);
    lv_obj_align(img_radarAltNeedle, LV_ALIGN_CENTER, 0, -44);

      // Assume the screen center is (center_x, center_y)
    int16_t center_x = lv_disp_get_hor_res(NULL) / 2;
    int16_t center_y = lv_disp_get_ver_res(NULL) / 2;

    // Needle image width/height
    int16_t w = radarAltNeedle.header.w;
    int16_t h = radarAltNeedle.header.h;

    // Set pivot explicitly to hub center
    lv_img_set_pivot(img_radarAltNeedle, 36, 124);

    lv_img_set_angle(img_radarAltNeedle, 0);     // start at 0°




  // Kick off Built-In Test (BIT) at power-up
  startBIT();
}



// Check serial input for BIT command
void checkSerialCommand() {
  // Buffer for Serial input
    
    String serialCmd = "";
    while (Serial.available()) {
        char c = Serial.read();
        if (c == '\n' || c == '\r') {
            if (serialCmd.length() > 0) {
                serialCmd.trim();   // remove spaces
                if (serialCmd.equalsIgnoreCase("BIT")) {
                    Serial.println("Running Built-In Test...");
                    startBIT();
                    Serial.println("BIT complete.");
                }
                serialCmd = "";
            }
        } else {
            serialCmd += c;  // accumulate chars
        }
    }
}


void loop() {
    checkSerialCommand();   //Check Serial Montitor input for BIT command

    DcsBios::loop();        // Process DCS-BIOS
    lv_timer_handler();     // Refresh LVGL

    delay(5);  // short delay to keep things smooth
}

