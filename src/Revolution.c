// Copyright (c) 2013 Douwe Maan <http://www.douwemaan.com/>
// The above copyright notice shall be included in all copies or substantial portions of the program.

// Envisioned as a watchface by Jean-Noël Mattern
// Based on the display of the Freebox Revolution, which was designed by Philippe Starck.

#include <pebble.h>
#include "common.h"


// Settings
#define USE_AMERICAN_DATE_FORMAT      false
#define VIBE_ON_HOUR                  false
#define TIME_SLOT_ANIMATION_DURATION  500

// Magic numbers
#define SCREEN_WIDTH        144
#define SCREEN_HEIGHT       168

#define TIME_IMAGE_WIDTH    70
#define TIME_IMAGE_HEIGHT   70

#define DATE_IMAGE_WIDTH    20
#define DATE_IMAGE_HEIGHT   20

#define SECOND_IMAGE_WIDTH  10
#define SECOND_IMAGE_HEIGHT 10

#define DAY_IMAGE_WIDTH     20
#define DAY_IMAGE_HEIGHT    10

#define MARGIN              1
#define TIME_SLOT_SPACE     2
#define DATE_PART_SPACE     4

#define SMALL_TEXT_WIDTH    140
#define SMALL_TEXT_HEIGHT   55

// Images
#define NUMBER_OF_TIME_IMAGES 10
const int TIME_IMAGE_RESOURCE_IDS[NUMBER_OF_TIME_IMAGES] = {
  RESOURCE_ID_IMAGE_TIME_0, 
  RESOURCE_ID_IMAGE_TIME_1, RESOURCE_ID_IMAGE_TIME_2, RESOURCE_ID_IMAGE_TIME_3, 
  RESOURCE_ID_IMAGE_TIME_4, RESOURCE_ID_IMAGE_TIME_5, RESOURCE_ID_IMAGE_TIME_6, 
  RESOURCE_ID_IMAGE_TIME_7, RESOURCE_ID_IMAGE_TIME_8, RESOURCE_ID_IMAGE_TIME_9
};

#define NUMBER_OF_DATE_IMAGES 10
const int DATE_IMAGE_RESOURCE_IDS[NUMBER_OF_DATE_IMAGES] = {
  RESOURCE_ID_IMAGE_DATE_0, 
  RESOURCE_ID_IMAGE_DATE_1, RESOURCE_ID_IMAGE_DATE_2, RESOURCE_ID_IMAGE_DATE_3, 
  RESOURCE_ID_IMAGE_DATE_4, RESOURCE_ID_IMAGE_DATE_5, RESOURCE_ID_IMAGE_DATE_6, 
  RESOURCE_ID_IMAGE_DATE_7, RESOURCE_ID_IMAGE_DATE_8, RESOURCE_ID_IMAGE_DATE_9
};

/*
#define NUMBER_OF_SECOND_IMAGES 10
const int SECOND_IMAGE_RESOURCE_IDS[NUMBER_OF_SECOND_IMAGES] = {
  RESOURCE_ID_IMAGE_SECOND_0, 
  RESOURCE_ID_IMAGE_SECOND_1, RESOURCE_ID_IMAGE_SECOND_2, RESOURCE_ID_IMAGE_SECOND_3, 
  RESOURCE_ID_IMAGE_SECOND_4, RESOURCE_ID_IMAGE_SECOND_5, RESOURCE_ID_IMAGE_SECOND_6, 
  RESOURCE_ID_IMAGE_SECOND_7, RESOURCE_ID_IMAGE_SECOND_8, RESOURCE_ID_IMAGE_SECOND_9
};

#define NUMBER_OF_DAY_IMAGES 7
const int DAY_IMAGE_RESOURCE_IDS[NUMBER_OF_DAY_IMAGES] = {
  RESOURCE_ID_IMAGE_DAY_0, RESOURCE_ID_IMAGE_DAY_1, RESOURCE_ID_IMAGE_DAY_2, 
  RESOURCE_ID_IMAGE_DAY_3, RESOURCE_ID_IMAGE_DAY_4, RESOURCE_ID_IMAGE_DAY_5, 
  RESOURCE_ID_IMAGE_DAY_6
};
*/

// General
static Window *window;


#define EMPTY_SLOT -1
typedef struct Slot {
  int         number;
  GBitmap     *image;
  BitmapLayer *image_layer;
  int         state;
} Slot;

// Time
typedef struct TimeSlot {
  Slot              slot;
  int               new_state;
  PropertyAnimation *slide_out_animation;
  PropertyAnimation *slide_in_animation;
  bool              updating;
} TimeSlot;

#define NUMBER_OF_TIME_SLOTS 4
static Layer *time_layer;
static TimeSlot time_slots[NUMBER_OF_TIME_SLOTS];
static struct tm *current_time;

// Footer
static Layer *footer_layer;

// Day
typedef struct DayItem {
  GBitmap     *image;
  BitmapLayer *image_layer;
  Layer       *layer;
  bool       loaded;
} DayItem;
static DayItem day_item;

// Date
#define NUMBER_OF_DATE_SLOTS 4
static Layer *date_layer;
static Slot date_slots[NUMBER_OF_DATE_SLOTS];

// Seconds
#define NUMBER_OF_SECOND_SLOTS 2
static Layer *seconds_layer;
static Slot second_slots[NUMBER_OF_SECOND_SLOTS];


// General
void destroy_property_animation(PropertyAnimation **prop_animation);
BitmapLayer *load_digit_image_into_slot(Slot *slot, int digit_value, Layer *parent_layer, GRect frame, const int *digit_resource_ids);
void unload_digit_image_from_slot(Slot *slot);

// Time
void display_time(struct tm *tick_time);
void display_time_value(int value, int row_number);
void update_time_slot(TimeSlot *time_slot, int digit_value);
GRect frame_for_time_slot(TimeSlot *time_slot);
void slide_in_digit_image_into_time_slot(TimeSlot *time_slot, int digit_value);
void time_slot_slide_in_animation_stopped(Animation *slide_in_animation, bool finished, void *context);
void slide_out_digit_image_from_time_slot(TimeSlot *time_slot);
void time_slot_slide_out_animation_stopped(Animation *slide_out_animation, bool finished, void *context);

// Small Time - ShaBP
static TextLayer *small_time_layer;
static char small_time_text[] = "99:99";

// Day
//void display_day(struct tm *tick_time);
void unload_day_item();

// Date
void display_date(struct tm *tick_time);
void display_date_value(int value, int part_number);
void update_date_slot(Slot *date_slot, int digit_value);

// Seconds
//void display_seconds(struct tm *tick_time);
void update_second_slot(Slot *second_slot, int digit_value);

// Handlers
int main(void);
void init();
void handle_minute_tick(struct tm *tick_time, TimeUnits units_changed);
void deinit();

// Week Text status - ShaBP
static TextLayer *week_layer;
static char week_text[] = "week 99";

// Bluetooth - ShaBP
static BitmapLayer *bt_layer;
static GBitmap *icon_bt_connected;
static GBitmap *icon_bt_disconnected;
static bool bt_status = false;
static bool already_vibrated = false;

// Pebble Battery Icon - ShaBP
static BitmapLayer *battery_layer;
static GBitmap *icon_battery_charging;
static GBitmap *icon_battery_100;
static GBitmap *icon_battery_90;
static GBitmap *icon_battery_80;
static GBitmap *icon_battery_70;
static GBitmap *icon_battery_60;
static GBitmap *icon_battery_50;
static GBitmap *icon_battery_40;
static GBitmap *icon_battery_30;
static GBitmap *icon_battery_20;
static GBitmap *icon_battery_10;
static uint8_t battery_level;
static bool battery_plugged = false;
static bool battery_charging = false;
static bool battery_hide = false;

// Status icons & events - ShaBP
static bool status_showing = false;
static TextLayer *event_layer;
static TextLayer *event_layer2; // from ModernCalendar
static char event_text[21]; // from ModernCalendar
static char event_text2[21]; // from ModernCalendar
static GBitmap *icon_status_1; // from ModernCalendar
static GBitmap *icon_status_2; // from ModernCalendar
static GBitmap *icon_status_3; // from ModernCalendar
static Layer *event_status_layer; // from ModernCalendar
static int event_status_display = 0; // from ModernCalendar

// timers - ShaBP
static AppTimer *display_timer;
static AppTimer *vibrate_timer;

// General
void destroy_property_animation(PropertyAnimation **animation) {
  if (*animation == NULL)
    return;

  if (animation_is_scheduled((Animation *)*animation)) {
    animation_unschedule((Animation *)*animation);
  }

  property_animation_destroy(*animation);
  *animation = NULL;
}

BitmapLayer *load_digit_image_into_slot(Slot *slot, int digit_value, Layer *parent_layer, GRect frame, const int *digit_resource_ids) {
  if (digit_value < 0 || digit_value > 9)
    return NULL;

  if (slot->state != EMPTY_SLOT)
    return NULL;

  slot->state = digit_value;

  slot->image = gbitmap_create_with_resource(digit_resource_ids[digit_value]);

  slot->image_layer = bitmap_layer_create(frame);
  bitmap_layer_set_bitmap(slot->image_layer, slot->image);
  layer_add_child(parent_layer, bitmap_layer_get_layer(slot->image_layer));

  return slot->image_layer;
}

void unload_digit_image_from_slot(Slot *slot) {
  if (slot->state == EMPTY_SLOT)
    return;

  layer_remove_from_parent(bitmap_layer_get_layer(slot->image_layer));
  bitmap_layer_destroy(slot->image_layer);

  gbitmap_destroy(slot->image);

  slot->state = EMPTY_SLOT;
}

// Time
void display_time(struct tm *tick_time) {
  int hour = tick_time->tm_hour;

  if (!clock_is_24h_style()) {
    hour = hour % 12;
    if (hour == 0) {
      hour = 12;
    }
  }

  display_time_value(hour,              0);
  display_time_value(tick_time->tm_min, 1);
}

void display_time_value(int value, int row_number) {
  value = value % 100; // Maximum of two digits per row.

  for (int column_number = 1; column_number >= 0; column_number--) {
    int time_slot_number = (row_number * 2) + column_number;

    TimeSlot *time_slot = &time_slots[time_slot_number];

    update_time_slot(time_slot, value % 10);

    value = value / 10;
  }
}

void update_time_slot(TimeSlot *time_slot, int digit_value) {
  if (time_slot->slot.state == digit_value)
    return;

  if (time_slot->updating) {
    // Otherwise we'll crash when the animation is replaced by a new animation before we're finished.
    return;
  }

  if (time_slot->slot.state == EMPTY_SLOT) {
    GRect frame = frame_for_time_slot(time_slot);
    load_digit_image_into_slot(&time_slot->slot, digit_value, time_layer, frame, TIME_IMAGE_RESOURCE_IDS);
  }
  else {
    time_slot->updating = true;
    time_slot->new_state = digit_value;
    slide_out_digit_image_from_time_slot(time_slot);
  }
}

GRect frame_for_time_slot(TimeSlot *time_slot) {
  int x = MARGIN + (time_slot->slot.number % 2) * (TIME_IMAGE_WIDTH + TIME_SLOT_SPACE);
  int y = MARGIN + (time_slot->slot.number / 2) * (TIME_IMAGE_HEIGHT + TIME_SLOT_SPACE);

  return GRect(x, y, TIME_IMAGE_WIDTH, TIME_IMAGE_HEIGHT);
}

void slide_in_digit_image_into_time_slot(TimeSlot *time_slot, int digit_value) {
  destroy_property_animation(&time_slot->slide_in_animation);

  GRect to_frame = frame_for_time_slot(time_slot);

  int from_x = to_frame.origin.x;
  int from_y = to_frame.origin.y;
  switch (time_slot->slot.number) {
    case 0:
      from_x -= TIME_IMAGE_WIDTH + MARGIN;
      break;
    case 1:
      from_y -= TIME_IMAGE_HEIGHT + MARGIN;
      break;
    case 2:
      from_y += TIME_IMAGE_HEIGHT + MARGIN;
      break;
    case 3:
      from_x += TIME_IMAGE_WIDTH + MARGIN;
      break;
  }
  GRect from_frame = GRect(from_x, from_y, TIME_IMAGE_WIDTH, TIME_IMAGE_HEIGHT);

  BitmapLayer *image_layer = load_digit_image_into_slot(&time_slot->slot, digit_value, time_layer, from_frame, TIME_IMAGE_RESOURCE_IDS);

  time_slot->slide_in_animation = property_animation_create_layer_frame(bitmap_layer_get_layer(image_layer), &from_frame, &to_frame);

  Animation *animation = (Animation *)time_slot->slide_in_animation;
  animation_set_duration( animation,  TIME_SLOT_ANIMATION_DURATION);
  animation_set_curve(    animation,  AnimationCurveLinear);
  animation_set_handlers( animation,  (AnimationHandlers){
    .stopped = (AnimationStoppedHandler)time_slot_slide_in_animation_stopped
  }, (void *)time_slot);

  animation_schedule(animation);
}

void time_slot_slide_in_animation_stopped(Animation *slide_in_animation, bool finished, void *context) {
  TimeSlot *time_slot = (TimeSlot *)context;

  destroy_property_animation(&time_slot->slide_in_animation);

  time_slot->updating = false;
}

void slide_out_digit_image_from_time_slot(TimeSlot *time_slot) {
  destroy_property_animation(&time_slot->slide_out_animation);

  GRect from_frame = frame_for_time_slot(time_slot);

  int to_x = from_frame.origin.x;
  int to_y = from_frame.origin.y;
  switch (time_slot->slot.number) {
    case 0:
      to_y -= TIME_IMAGE_HEIGHT + MARGIN;
      break;
    case 1:
      to_x += TIME_IMAGE_WIDTH + MARGIN;
      break;
    case 2:
      to_x -= TIME_IMAGE_WIDTH + MARGIN;
      break;
    case 3:
      to_y += TIME_IMAGE_HEIGHT + MARGIN;
      break;
  }
  GRect to_frame = GRect(to_x, to_y, TIME_IMAGE_WIDTH, TIME_IMAGE_HEIGHT);

  BitmapLayer *image_layer = time_slot->slot.image_layer;

  time_slot->slide_out_animation = property_animation_create_layer_frame(bitmap_layer_get_layer(image_layer), &from_frame, &to_frame);

  Animation *animation = (Animation *)time_slot->slide_out_animation;
  animation_set_duration( animation,  TIME_SLOT_ANIMATION_DURATION);
  animation_set_curve(    animation,  AnimationCurveLinear);
  animation_set_handlers(animation, (AnimationHandlers){
    .stopped = (AnimationStoppedHandler)time_slot_slide_out_animation_stopped
  }, (void *)time_slot);

  animation_schedule(animation);
}

void time_slot_slide_out_animation_stopped(Animation *slide_out_animation, bool finished, void *context) {
  TimeSlot *time_slot = (TimeSlot *)context;

  destroy_property_animation(&time_slot->slide_out_animation);

  if (time_slot->new_state == EMPTY_SLOT) {
    time_slot->updating = false;
  }
  else {
    unload_digit_image_from_slot(&time_slot->slot);

    slide_in_digit_image_into_time_slot(time_slot, time_slot->new_state);

    time_slot->new_state = EMPTY_SLOT;
  }
}

// Day
/*
void display_day(struct tm *tick_time) {
  unload_day_item();

  day_item.image = gbitmap_create_with_resource(DAY_IMAGE_RESOURCE_IDS[tick_time->tm_wday]);

  day_item.image_layer = bitmap_layer_create(day_item.image->bounds);
  bitmap_layer_set_bitmap(day_item.image_layer, day_item.image);
  layer_add_child(day_item.layer, bitmap_layer_get_layer(day_item.image_layer));

  day_item.loaded = true;
}
*/

void unload_day_item() {
  if (!day_item.loaded) 
    return;

  layer_remove_from_parent(bitmap_layer_get_layer(day_item.image_layer));
  bitmap_layer_destroy(day_item.image_layer);

  gbitmap_destroy(day_item.image);
}

// Date
void display_date(struct tm *tick_time) {
  int day   = tick_time->tm_mday;
  int month = tick_time->tm_mon + 1;

#if USE_AMERICAN_DATE_FORMAT
  display_date_value(month, 0);
  display_date_value(day,   1);
#else
  display_date_value(day,   0);
  display_date_value(month, 1);
#endif
}

void display_date_value(int value, int part_number) {
  value = value % 100; // Maximum of two digits per row.

  for (int column_number = 1; column_number >= 0; column_number--) {
    int date_slot_number = (part_number * 2) + column_number;

    Slot *date_slot = &date_slots[date_slot_number];

    update_date_slot(date_slot, value % 10);

    value = value / 10;
  }
}

void update_date_slot(Slot *date_slot, int digit_value) {
  if (date_slot->state == digit_value)
    return;

  int x = date_slot->number * (DATE_IMAGE_WIDTH + MARGIN);
  if (date_slot->number >= 2) {
    x += 3; // 3 extra pixels of space between the day and month
  }
  GRect frame =  GRect(x, 0, DATE_IMAGE_WIDTH, DATE_IMAGE_HEIGHT);

  unload_digit_image_from_slot(date_slot);
  load_digit_image_into_slot(date_slot, digit_value, date_layer, frame, DATE_IMAGE_RESOURCE_IDS);
}

// Seconds
/*
void display_seconds(struct tm *tick_time) {
  int seconds = tick_time->tm_sec;

  seconds = seconds % 100; // Maximum of two digits per row.

  for (int second_slot_number = 1; second_slot_number >= 0; second_slot_number--) {
    Slot *second_slot = &second_slots[second_slot_number];

    update_second_slot(second_slot, seconds % 10);
    
    seconds = seconds / 10;
  }
}

void update_second_slot(Slot *second_slot, int digit_value) {
  if (second_slot->state == digit_value)
    return;

  GRect frame = GRect(
    second_slot->number * (SECOND_IMAGE_WIDTH + MARGIN), 
    0, 
    SECOND_IMAGE_WIDTH, 
    SECOND_IMAGE_HEIGHT
  );

  unload_digit_image_from_slot(second_slot);
  load_digit_image_into_slot(second_slot, digit_value, seconds_layer, frame, SECOND_IMAGE_RESOURCE_IDS);
}
*/

// Status (upon Shake) - ShaBP
// Draw/Update Time text
void draw_small_time() {
  strftime(small_time_text, sizeof(small_time_text), "%H:%M", current_time);
  text_layer_set_text(small_time_layer, small_time_text);
  if (status_showing) layer_set_hidden(text_layer_get_layer(small_time_layer), false);
  else layer_set_hidden(text_layer_get_layer(small_time_layer), true);
}

// Draw/Update/Hide Battery icon. Loads appropriate icon per battery state. Sets appropriate hide/unhide status for state. Activates hide status. -- DONE
void draw_battery_icon() {
  if (battery_plugged && !status_showing) {
    if (battery_charging) bitmap_layer_set_bitmap(battery_layer, icon_battery_charging);
    else bitmap_layer_set_bitmap(battery_layer, icon_battery_100);
  }
  else {
    if (battery_level == 100) bitmap_layer_set_bitmap(battery_layer, icon_battery_100);
    else if (battery_level == 90) bitmap_layer_set_bitmap(battery_layer, icon_battery_90);
    else if (battery_level == 80) bitmap_layer_set_bitmap(battery_layer, icon_battery_80);
    else if (battery_level == 70) bitmap_layer_set_bitmap(battery_layer, icon_battery_70);
    else if (battery_level == 60) bitmap_layer_set_bitmap(battery_layer, icon_battery_60);
    else if (battery_level == 50) bitmap_layer_set_bitmap(battery_layer, icon_battery_50);
    else if (battery_level == 40) bitmap_layer_set_bitmap(battery_layer, icon_battery_40);
    else if (battery_level == 30) bitmap_layer_set_bitmap(battery_layer, icon_battery_30);
    else if (battery_level == 20) bitmap_layer_set_bitmap(battery_layer, icon_battery_20);
    else if (battery_level == 10) bitmap_layer_set_bitmap(battery_layer, icon_battery_10);
  }
  if (battery_plugged) battery_hide = false;
  else if (battery_level <= 10) battery_hide = false;
  else if (status_showing) battery_hide = false;
  else battery_hide = true;
  layer_set_hidden(bitmap_layer_get_layer(battery_layer), battery_hide);
}
// Draw/Update/Hide bluetooth status icon. Loads appropriate icon. Sets appropriate hide/unhide status per status. ("draw_bt_icon" is separate from "bt_connection_handler" as to not vibrate if not connected on init)
void draw_bt_icon() {
  if (bt_status) bitmap_layer_set_bitmap(bt_layer, icon_bt_connected);
  else bitmap_layer_set_bitmap(bt_layer, icon_bt_disconnected);
  if (status_showing) layer_set_hidden(bitmap_layer_get_layer(bt_layer), false);
  else layer_set_hidden(bitmap_layer_get_layer(bt_layer), true);
}
// Draw/Update/Hide week text, as part of status info
void draw_week_text() {
  strftime(week_text, sizeof(week_text), "week %V", current_time);
  text_layer_set_text(week_layer, week_text);  
  if (status_showing) layer_set_hidden(text_layer_get_layer(week_layer), false);
  else layer_set_hidden(text_layer_get_layer(week_layer), true);
}

// Draw/Update/Hide event status icons. Sets appropriate hide/unhide status per status.
void draw_event_status() {
	if (status_showing) layer_set_hidden(event_status_layer, false);
	else layer_set_hidden(event_status_layer, true);
}

// Draw/Update/Hide Event text - from ModernCalendar
void draw_event_text(){
  if (status_showing) {
    layer_set_hidden(text_layer_get_layer(event_layer), false);
    layer_set_hidden(text_layer_get_layer(event_layer2), false);
  }
  else {
    layer_set_hidden(text_layer_get_layer(event_layer), true);
    layer_set_hidden(text_layer_get_layer(event_layer2), true);
  }  
}

void display_event_text(char *text, char *relative) {
  strncpy(event_text2, relative, sizeof(event_text2));
  text_layer_set_text(event_layer2, event_text2);
  strncpy(event_text, text, sizeof(event_text));
  text_layer_set_text(event_layer, event_text);
}

// Hides status icons. Call draw of default battery/bluetooth icons (which will show or hide icon based on set logic). Hides date/window if selected
void hide_status() {
  status_showing = false;
  draw_battery_icon();
  draw_bt_icon();
  draw_week_text();
//  layer_set_hidden(footer_layer, true);
  draw_small_time();
  draw_event_status();
  draw_event_text();
  layer_set_hidden(time_layer, false);
//layer_set_hidden(bitmap_layer_get_layer(date_window_layer), hide_date);
}

// Shows status icons. Call draw of default battery/bluetooth icons (which will show or hide icon based on set logic). Shows date/window.
void show_status() {
  status_showing = true;
// Hide main (big) time layer
  layer_set_hidden(time_layer, true);
// Redraws battery icon
  draw_battery_icon();
// Shows current bluetooth status icon
  draw_bt_icon();
// Shows week of year text
  draw_week_text();
// Show small time text
  draw_small_time();
// Shows event status icons
  draw_event_status();
// Shows event text
  draw_event_text();
//  layer_set_hidden(footer_layer, false);
// 5 Sec timer then call "hide_status". Cancels previous timer if another show_status is called within the 5000ms
  app_timer_cancel(display_timer);
  display_timer = app_timer_register(5000, hide_status, NULL);
}


// Handlers

// Shake/Tap Handler. On shake/tap... call "show_status" - ShaBP
void tap_handler(AccelAxisType axis, int32_t direction) {
  show_status();
}

// Battery state handler. Updates battery level, plugged, charging states. Calls "draw_battery_icon".
void battery_state_handler(BatteryChargeState c) {
  battery_level = c.charge_percent;
  battery_plugged = c.is_plugged;
  battery_charging = c.is_charging;
  draw_battery_icon();
}

void short_pulse(){
  vibes_short_pulse();
}

// If bluetooth is still not connected after 5 sec delay, vibrate. (double vibe was too easily confused with a signle short vibe.  Two short vibes was easier to distinguish from a notification)
void bt_vibrate(){
  if (!bt_status && !already_vibrated) {
    already_vibrated = true;
    app_timer_register(0, short_pulse, NULL);
    app_timer_register(350, short_pulse, NULL);
  }
}

// Bluetooth connection status handler.  Updates bluetooth status. Calls "draw_bt_icon". If bluetooth not connected, wait 5 seconds then call "bt_vibrate". Cancel any vibrate timer if status change in 5 seconds (minimizes repeat vibration alerts)
void bt_connection_handler(bool bt) {
  bt_status = bt;
  draw_bt_icon();
  app_timer_cancel(vibrate_timer);
  if (!bt_status) vibrate_timer = app_timer_register(5000, bt_vibrate, NULL);
  else if (bt_status && already_vibrated) already_vibrated = false;
}

// Status icon callback handler - from ModernCalendar
void event_status_layer_update_callback(Layer *layer, GContext *ctx) { 
  graphics_context_set_compositing_mode(ctx, GCompOpAssign);
  if (event_status_display == STATUS_REQUEST) {
     graphics_draw_bitmap_in_rect(ctx, icon_status_1, GRect(0, 0, 38, 9));
  } else if (event_status_display == STATUS_REPLY) {
     graphics_draw_bitmap_in_rect(ctx, icon_status_2, GRect(0, 0, 38, 9));
  } else if (event_status_display == STATUS_ALERT_SET) {
     graphics_draw_bitmap_in_rect(ctx, icon_status_3, GRect(0, 0, 38, 9));
  }
}

// Setting event status - from ModernCalendar
void set_event_status(int new_event_status_display) {
	event_status_display = new_event_status_display;
	layer_mark_dirty(event_status_layer);
}

int main(void) {
  init();
  app_event_loop();
  deinit();
}

void init() {
  window = window_create();
  window_stack_push(window, true /* Animated */);
  window_set_background_color(window, GColorBlack);

  Layer *root_layer = window_get_root_layer(window);

  // Time
  for (int i = 0; i < NUMBER_OF_TIME_SLOTS; i++) {
    TimeSlot *time_slot = &time_slots[i];
    time_slot->slot.number  = i;
    time_slot->slot.state   = EMPTY_SLOT;
    time_slot->new_state    = EMPTY_SLOT;
    time_slot->updating     = false;
  }

  time_layer = layer_create(GRect(0, 0, SCREEN_WIDTH, SCREEN_WIDTH));
  layer_set_clips(time_layer, true);
  layer_add_child(root_layer, time_layer);

  // Footer
  int footer_height = SCREEN_HEIGHT - SCREEN_WIDTH;

  footer_layer = layer_create(GRect(0, SCREEN_WIDTH, SCREEN_WIDTH, footer_height));
  layer_add_child(root_layer, footer_layer);

  // Day
  day_item.loaded = false;

  GRect day_layer_frame = GRect(
    MARGIN, 
    footer_height - DAY_IMAGE_HEIGHT - MARGIN, 
    DAY_IMAGE_WIDTH, 
    DAY_IMAGE_HEIGHT
  );
  day_item.layer = layer_create(day_layer_frame);
  layer_add_child(footer_layer, day_item.layer);

  // Date
  for (int i = 0; i < NUMBER_OF_DATE_SLOTS; i++) {
    Slot *date_slot = &date_slots[i];
    date_slot->number = i;
    date_slot->state  = EMPTY_SLOT;
  }

  GRect date_layer_frame = GRectZero;
  date_layer_frame.size.w   = DATE_IMAGE_WIDTH + MARGIN + DATE_IMAGE_WIDTH + DATE_PART_SPACE + DATE_IMAGE_WIDTH + MARGIN + DATE_IMAGE_WIDTH;
  date_layer_frame.size.h   = DATE_IMAGE_HEIGHT;
  date_layer_frame.origin.x = (SCREEN_WIDTH - date_layer_frame.size.w) / 2;
  date_layer_frame.origin.y = footer_height - DATE_IMAGE_HEIGHT - MARGIN;

  date_layer = layer_create(date_layer_frame);
  layer_add_child(footer_layer, date_layer);

  // Seconds
  for (int i = 0; i < NUMBER_OF_SECOND_SLOTS; i++) {
    Slot *second_slot = &second_slots[i];
    second_slot->number = i;
    second_slot->state  = EMPTY_SLOT;
  }

  GRect seconds_layer_frame = GRect(
    SCREEN_WIDTH - SECOND_IMAGE_WIDTH - MARGIN - SECOND_IMAGE_WIDTH - MARGIN, 
    footer_height - SECOND_IMAGE_HEIGHT - MARGIN, 
    SECOND_IMAGE_WIDTH + MARGIN + SECOND_IMAGE_WIDTH, 
    SECOND_IMAGE_HEIGHT
  );
  seconds_layer = layer_create(seconds_layer_frame);
  layer_add_child(footer_layer, seconds_layer);

  // Small time text init (for status upon shake) - ShaBP
  small_time_layer = text_layer_create(GRect(2, 24, SMALL_TEXT_WIDTH, SMALL_TEXT_HEIGHT));
  text_layer_set_text_color(small_time_layer, GColorWhite);
  text_layer_set_text_alignment(small_time_layer, GTextAlignmentCenter);
  text_layer_set_background_color(small_time_layer, GColorClear);
  text_layer_set_font(small_time_layer, fonts_get_system_font(FONT_KEY_ROBOTO_BOLD_SUBSET_49));
  layer_add_child(root_layer, text_layer_get_layer(small_time_layer));
  layer_set_hidden(text_layer_get_layer(small_time_layer), true);
  
	// Bluetooth icon init, then call "draw_bt_icon" (doesn't call bt_connection_handler to avoid vibrate at init if bluetooth not connected)
	icon_bt_connected = gbitmap_create_with_resource(RESOURCE_ID_BLUETOOTH_CONNECTED);
	icon_bt_disconnected = gbitmap_create_with_resource(RESOURCE_ID_BLUETOOTH_DISCONNECTED);
	bt_layer = bitmap_layer_create(GRect(2,2,28,24));
	layer_add_child(root_layer, bitmap_layer_get_layer(bt_layer));
	bt_status = bluetooth_connection_service_peek();
	draw_bt_icon();
	
	// Pebble battery icon init, then call "battery_state_handler" with current battery state
	icon_battery_charging = gbitmap_create_with_resource(RESOURCE_ID_ICON_BATTERY_CHARGING);
	icon_battery_100 = gbitmap_create_with_resource(RESOURCE_ID_ICON_BATTERY_100);
	icon_battery_90 = gbitmap_create_with_resource(RESOURCE_ID_ICON_BATTERY_90);
	icon_battery_80 = gbitmap_create_with_resource(RESOURCE_ID_ICON_BATTERY_80);
	icon_battery_70 = gbitmap_create_with_resource(RESOURCE_ID_ICON_BATTERY_70);
	icon_battery_60 = gbitmap_create_with_resource(RESOURCE_ID_ICON_BATTERY_60);
	icon_battery_50 = gbitmap_create_with_resource(RESOURCE_ID_ICON_BATTERY_50);
	icon_battery_40 = gbitmap_create_with_resource(RESOURCE_ID_ICON_BATTERY_40);
	icon_battery_30 = gbitmap_create_with_resource(RESOURCE_ID_ICON_BATTERY_30);
	icon_battery_20 = gbitmap_create_with_resource(RESOURCE_ID_ICON_BATTERY_20);
	icon_battery_10 = gbitmap_create_with_resource(RESOURCE_ID_ICON_BATTERY_10);
	battery_layer = bitmap_layer_create(GRect(SCREEN_WIDTH-41,2,41,24));
	bitmap_layer_set_background_color(battery_layer, GColorClear);
	layer_add_child(root_layer, bitmap_layer_get_layer(battery_layer));
	battery_state_handler(battery_state_service_peek());
	
  //week status layer (ShaBP)
  week_layer = text_layer_create(GRect(SCREEN_WIDTH/2-40, SMALL_TEXT_HEIGHT+22, 80, 30));
  text_layer_set_text_color(week_layer, GColorWhite);
  text_layer_set_text_alignment(week_layer, GTextAlignmentCenter);
  text_layer_set_background_color(week_layer, GColorClear);
  text_layer_set_font(week_layer, fonts_get_system_font(FONT_KEY_ROBOTO_CONDENSED_21));
  layer_add_child(root_layer, text_layer_get_layer(week_layer));
  layer_set_hidden(text_layer_get_layer(week_layer), false);
  // draw_week_text();

  // Status icons and event layers init - from ModernCalendar / ShaBP
  icon_status_1 = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_STATUS_1);
  icon_status_2 = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_STATUS_2);
  icon_status_3 = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_STATUS_3);
 
  event_layer = text_layer_create(GRect(1, SMALL_TEXT_HEIGHT+44, SCREEN_WIDTH, 21));
  text_layer_set_text_color(event_layer, GColorWhite);
  text_layer_set_text_alignment(event_layer, GTextAlignmentCenter);
  text_layer_set_background_color(event_layer, GColorClear);
  text_layer_set_font(event_layer, fonts_get_system_font(FONT_KEY_GOTHIC_18));
  layer_add_child(root_layer, text_layer_get_layer(event_layer));
	layer_set_hidden(text_layer_get_layer(event_layer), true);

  event_layer2 = text_layer_create(GRect(1, SMALL_TEXT_HEIGHT+62, SCREEN_WIDTH, 21));
  text_layer_set_text_color(event_layer2, GColorWhite);
  text_layer_set_text_alignment(event_layer2, GTextAlignmentCenter);
  text_layer_set_background_color(event_layer2, GColorClear);
  text_layer_set_font(event_layer2, fonts_get_system_font(FONT_KEY_GOTHIC_18));
  layer_add_child(root_layer, text_layer_get_layer(event_layer2));
	layer_set_hidden(text_layer_get_layer(event_layer2), true);
  //draw_event_text();
  
  // Event status layer creation - from ModernCalendar
  GRect sframe;
  sframe.origin.x = 54;
  sframe.origin.y = 1;
  sframe.size.w = 38;
  sframe.size.h = 9;

  event_status_layer = layer_create(sframe);
  layer_set_update_proc(event_status_layer, event_status_layer_update_callback);
  layer_add_child(root_layer, event_status_layer);
	layer_set_hidden(event_status_layer, true);

  // Message inbox and Calendar init - from ModernCalendar
  app_message_register_inbox_received(received_message);
  app_message_open(124, 256);

  calendar_init();

  // Display
  time_t now = time(NULL);
  struct tm *tick_time = localtime(&now);
  display_time(tick_time);
//  display_day(tick_time);
  display_date(tick_time);
//  display_seconds(tick_time);

  tick_timer_service_subscribe(MINUTE_UNIT, handle_minute_tick);
  accel_tap_service_subscribe(tap_handler);
  bluetooth_connection_service_subscribe(bt_connection_handler);
	battery_state_service_subscribe(battery_state_handler);
}

void handle_minute_tick(struct tm *tick_time, TimeUnits units_changed) {
//  display_seconds(tick_time);

//  if ((units_changed & MINUTE_UNIT) == MINUTE_UNIT) {
  current_time = tick_time; // store current time in static valirable (ShaBP)  
  display_time(tick_time);
//  }

#if VIBE_ON_HOUR
  if ((units_changed & HOUR_UNIT) == HOUR_UNIT) {
    vibes_double_pulse();
  }
#endif

  if ((units_changed & DAY_UNIT) == DAY_UNIT) {
//    display_day(tick_time);
    display_date(tick_time);
  }
}

void deinit() {
  // Time
  for (int i = 0; i < NUMBER_OF_TIME_SLOTS; i++) {
    unload_digit_image_from_slot(&time_slots[i].slot);

    destroy_property_animation(&time_slots[i].slide_in_animation);
    destroy_property_animation(&time_slots[i].slide_out_animation);
  }
  layer_destroy(time_layer);

  // Day
  unload_day_item();
  layer_destroy(day_item.layer);

  // Date
  for (int i = 0; i < NUMBER_OF_DATE_SLOTS; i++) {
    unload_digit_image_from_slot(&date_slots[i]);
  }
  layer_destroy(date_layer);

  // Seconds
  for (int i = 0; i < NUMBER_OF_SECOND_SLOTS; i++) {
    unload_digit_image_from_slot(&second_slots[i]);
  }
  layer_destroy(seconds_layer);

  // ShaBP:
	bluetooth_connection_service_unsubscribe();
	battery_state_service_unsubscribe();
	accel_tap_service_unsubscribe();
	bitmap_layer_destroy(battery_layer);
	gbitmap_destroy(icon_battery_charging);
	gbitmap_destroy(icon_battery_100);
	gbitmap_destroy(icon_battery_90);
	gbitmap_destroy(icon_battery_80);
	gbitmap_destroy(icon_battery_70);
	gbitmap_destroy(icon_battery_60);
	gbitmap_destroy(icon_battery_50);
	gbitmap_destroy(icon_battery_40);
	gbitmap_destroy(icon_battery_30);
	gbitmap_destroy(icon_battery_20);
	gbitmap_destroy(icon_battery_10);
	bitmap_layer_destroy(bt_layer);
	gbitmap_destroy(icon_bt_connected);
	gbitmap_destroy(icon_bt_disconnected);
  text_layer_destroy(week_layer);
	gbitmap_destroy(icon_status_1); // from ModernCalendar
	gbitmap_destroy(icon_status_2); // from ModernCalendar
	gbitmap_destroy(icon_status_3); // from ModernCalendar
	text_layer_destroy(event_layer); // from ModernCalendar
	text_layer_destroy(event_layer2); // from ModernCalendar
	layer_destroy(event_status_layer); // from ModernCalendar

  layer_destroy(footer_layer);

  window_destroy(window);
}