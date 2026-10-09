#include "ui_manager.h"
#include "config.h"
#include "shared_types.h"
#include "sd_manager.h"
#include "wifi_manager.h"
#include "wifi_analyzer.h"
#include "ble_scanner.h"
#include "game_loader.h"
#include "chat_server.h"

#include <SPI.h>
#include <TFT_eSPI.h>
#include <lvgl.h>

// =====================================================================================
//  Low level: TFT_eSPI + LVGL glue
// =====================================================================================

static TFT_eSPI tft = TFT_eSPI();

static lv_disp_draw_buf_t draw_buf;
static lv_color_t buf1[TFT_HOR_RES * LVGL_BUF_LINES];

static void disp_flush_cb(lv_disp_drv_t *disp, const lv_area_t *area, lv_color_t *color_p) {
  uint32_t w = (area->x2 - area->x1 + 1);
  uint32_t h = (area->y2 - area->y1 + 1);
  tft.startWrite();
  tft.setAddrWindow(area->x1, area->y1, w, h);
  tft.pushColors((uint16_t *)&color_p->full, w * h, true);
  tft.endWrite();
  lv_disp_flush_ready(disp);
}

static void touch_read_cb(lv_indev_drv_t *drv, lv_indev_data_t *data) {
  uint16_t x, y;
  bool touched = tft.getTouch(&x, &y);
  if (touched) {
    data->state = LV_INDEV_STATE_PR;
    data->point.x = x;
    data->point.y = y;
  } else {
    data->state = LV_INDEV_STATE_REL;
  }
}

// =====================================================================================
//  Screen/app state
// =====================================================================================

enum AppScreen { APP_HOME, APP_SETTINGS, APP_CHAT, APP_WIFI_ANALYZER, APP_BLE_SCANNER, APP_GAMES, APP_SNAKE };

static AppScreen s_stack[8];
static int s_stackTop = 0; // s_stack[0..s_stackTop] is the active navigation path

// Pushes `target` onto the back-stack. Returns false (and stays put) when the
// stack is already full, so a runaway tap sequence can never write past the
// end of s_stack.
static bool nav_push(AppScreen target) {
  if (s_stackTop + 1 >= (int)(sizeof(s_stack) / sizeof(s_stack[0]))) {
    Serial.println("[UI] Back-stack full, ignoring navigation.");
    return false;
  }
  s_stack[++s_stackTop] = target;
  return true;
}

static lv_obj_t *s_root;
static lv_obj_t *s_statusBar;
static lv_obj_t *s_navBar;
static lv_obj_t *s_content; // everything app-specific lives inside here

static lv_obj_t *s_wifiIcon, *s_sdIcon, *s_clockLabel;
static lv_obj_t *s_keyboard; // one shared keyboard, reused by every textarea

static DeviceConfig s_cfg;

static void show_app(AppScreen app);

// =====================================================================================
//  Status bar
// =====================================================================================

static void build_status_bar() {
  s_statusBar = lv_obj_create(s_root);
  lv_obj_remove_style_all(s_statusBar);
  lv_obj_set_size(s_statusBar, TFT_HOR_RES, 20);
  lv_obj_align(s_statusBar, LV_ALIGN_TOP_MID, 0, 0);
  lv_obj_set_style_bg_color(s_statusBar, lv_color_hex(0x101418), 0);
  lv_obj_set_style_bg_opa(s_statusBar, LV_OPA_COVER, 0);

  s_wifiIcon = lv_label_create(s_statusBar);
  lv_obj_set_style_text_color(s_wifiIcon, lv_color_hex(0x7fd97a), 0);
  lv_label_set_text(s_wifiIcon, LV_SYMBOL_WIFI);
  lv_obj_align(s_wifiIcon, LV_ALIGN_LEFT_MID, 6, 0);

  s_clockLabel = lv_label_create(s_statusBar);
  lv_obj_set_style_text_color(s_clockLabel, lv_color_white(), 0);
  lv_label_set_text(s_clockLabel, "--:--");
  lv_obj_align(s_clockLabel, LV_ALIGN_TOP_MID, 0, 3);

  s_sdIcon = lv_label_create(s_statusBar);
  lv_obj_set_style_text_color(s_sdIcon, lv_color_hex(0x888888), 0);
  lv_label_set_text(s_sdIcon, LV_SYMBOL_SD_CARD);
  lv_obj_align(s_sdIcon, LV_ALIGN_RIGHT_MID, -6, 0);
}

static void status_bar_refresh() {
  // Wi-Fi / AP indicator
  if (chat_server_is_running()) {
    lv_label_set_text(s_wifiIcon, LV_SYMBOL_UPLOAD); // "broadcasting" glyph for AP mode
    lv_obj_set_style_text_color(s_wifiIcon, lv_color_hex(0x59c1ff), 0);
  } else if (wifi_manager_is_connected()) {
    lv_label_set_text(s_wifiIcon, LV_SYMBOL_WIFI);
    int bars = wifi_manager_signal_bars();
    lv_obj_set_style_text_color(s_wifiIcon, bars >= 2 ? lv_color_hex(0x7fd97a) : lv_color_hex(0xd9a057), 0);
  } else {
    lv_label_set_text(s_wifiIcon, LV_SYMBOL_CLOSE);
    lv_obj_set_style_text_color(s_wifiIcon, lv_color_hex(0x666666), 0);
  }

  // SD card indicator
  lv_obj_set_style_text_color(s_sdIcon, sd_is_present() ? lv_color_hex(0x7fd97a) : lv_color_hex(0x444444), 0);

  // Clock (falls back to uptime if NTP hasn't synced yet)
  time_t now = time(nullptr);
  struct tm timeinfo;
  localtime_r(&now, &timeinfo);
  char buf[16];
  if (timeinfo.tm_year > (2020 - 1900)) {
    strftime(buf, sizeof(buf), "%H:%M", &timeinfo);
  } else {
    unsigned long s = millis() / 1000;
    snprintf(buf, sizeof(buf), "up %lu:%02lu", s / 3600, (s / 60) % 60);
  }
  lv_label_set_text(s_clockLabel, buf);
}

// =====================================================================================
//  Nav bar (Back / Home)
// =====================================================================================

static void nav_back_cb(lv_event_t *e) {
  if (s_stackTop > 0) s_stackTop--;
  show_app(s_stack[s_stackTop]);
}

static void nav_home_cb(lv_event_t *e) {
  s_stackTop = 0;
  s_stack[0] = APP_HOME;
  show_app(APP_HOME);
}

static void build_nav_bar() {
  s_navBar = lv_obj_create(s_root);
  lv_obj_remove_style_all(s_navBar);
  lv_obj_set_size(s_navBar, TFT_HOR_RES, 30);
  lv_obj_align(s_navBar, LV_ALIGN_BOTTOM_MID, 0, 0);
  lv_obj_set_style_bg_color(s_navBar, lv_color_hex(0x101418), 0);
  lv_obj_set_style_bg_opa(s_navBar, LV_OPA_COVER, 0);

  lv_obj_t *back = lv_btn_create(s_navBar);
  lv_obj_set_size(back, 70, 24);
  lv_obj_align(back, LV_ALIGN_LEFT_MID, 8, 0);
  lv_obj_add_event_cb(back, nav_back_cb, LV_EVENT_CLICKED, NULL);
  lv_obj_t *backLbl = lv_label_create(back);
  lv_label_set_text(backLbl, LV_SYMBOL_LEFT " Geri");
  lv_obj_center(backLbl);

  lv_obj_t *home = lv_btn_create(s_navBar);
  lv_obj_set_size(home, 90, 24);
  lv_obj_align(home, LV_ALIGN_RIGHT_MID, -8, 0);
  lv_obj_add_event_cb(home, nav_home_cb, LV_EVENT_CLICKED, NULL);
  lv_obj_t *homeLbl = lv_label_create(home);
  lv_label_set_text(homeLbl, LV_SYMBOL_HOME " Ana Sayfa");
  lv_obj_center(homeLbl);
}

// =====================================================================================
//  Shared keyboard helper - any textarea that wants the on-screen QWERTY
//  keyboard just calls attach_keyboard(ta) once after creating it.
// =====================================================================================

static void kb_event_cb(lv_event_t *e) {
  lv_event_code_t code = lv_event_get_code(e);
  if (code == LV_EVENT_READY || code == LV_EVENT_CANCEL) {
    lv_obj_add_flag(s_keyboard, LV_OBJ_FLAG_HIDDEN);
    lv_indev_reset(NULL, NULL); // clear focus so the textarea loses cursor
  }
}

static void ta_focus_cb(lv_event_t *e) {
  lv_obj_t *ta = (lv_obj_t *)lv_event_get_target(e);
  lv_event_code_t code = lv_event_get_code(e);
  if (code == LV_EVENT_FOCUSED) {
    lv_keyboard_set_textarea(s_keyboard, ta);
    lv_obj_clear_flag(s_keyboard, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(s_keyboard);
  } else if (code == LV_EVENT_DEFOCUSED) {
    lv_obj_add_flag(s_keyboard, LV_OBJ_FLAG_HIDDEN);
  }
}

static void attach_keyboard(lv_obj_t *ta) {
  lv_obj_add_event_cb(ta, ta_focus_cb, LV_EVENT_FOCUSED, NULL);
  lv_obj_add_event_cb(ta, ta_focus_cb, LV_EVENT_DEFOCUSED, NULL);
}

// =====================================================================================
//  APP: Home (launcher grid)
// =====================================================================================

static void home_icon_cb(lv_event_t *e) {
  AppScreen target = (AppScreen)(intptr_t)lv_event_get_user_data(e);
  if (!nav_push(target)) return;
  show_app(target);
}

static void add_launcher_icon(lv_obj_t *parent, int col, int row, const char *symbol, const char *label, AppScreen target) {
  lv_obj_t *btn = lv_btn_create(parent);
  lv_obj_set_size(btn, 70, 60);
  lv_obj_set_pos(btn, 10 + col * 78, 6 + row * 66);
  lv_obj_set_style_bg_color(btn, lv_color_hex(0x1c2128), 0);
  lv_obj_set_style_radius(btn, 10, 0);
  lv_obj_add_event_cb(btn, home_icon_cb, LV_EVENT_CLICKED, (void *)(intptr_t)target);

  lv_obj_t *icon = lv_label_create(btn);
  lv_obj_set_style_text_font(icon, &lv_font_montserrat_20, 0);
  lv_label_set_text(icon, symbol);
  lv_obj_align(icon, LV_ALIGN_TOP_MID, 0, 2);

  lv_obj_t *lbl = lv_label_create(btn);
  lv_obj_set_style_text_font(lbl, &lv_font_montserrat_12, 0);
  lv_label_set_text(lbl, label);
  lv_obj_align(lbl, LV_ALIGN_BOTTOM_MID, 0, -2);
}

static void build_home(lv_obj_t *parent) {
  add_launcher_icon(parent, 0, 0, LV_SYMBOL_CALL,      "Sohbet",       APP_CHAT);
  add_launcher_icon(parent, 1, 0, LV_SYMBOL_CHARGE,    "Wifi Analiz",  APP_WIFI_ANALYZER);
  add_launcher_icon(parent, 2, 0, LV_SYMBOL_BLUETOOTH, "BLE Tarayici", APP_BLE_SCANNER);
  add_launcher_icon(parent, 3, 0, LV_SYMBOL_PLAY,      "Oyunlar",      APP_GAMES);
  add_launcher_icon(parent, 0, 1, LV_SYMBOL_SETTINGS,  "Ayarlar",      APP_SETTINGS);
}

// =====================================================================================
//  APP: Settings (Wi-Fi network list + password keyboard)
// =====================================================================================

static lv_obj_t *s_netList;
static lv_obj_t *s_wifiPassTa;
static lv_obj_t *s_wifiConnectPanel;
static lv_obj_t *s_wifiStatusLbl;
static char s_selectedSsid[33];

static void wifi_scan_btn_cb(lv_event_t *e) {
  lv_obj_clean(s_netList);
  lv_list_add_text(s_netList, "Taraniyor...");
  NetCommand cmd{};
  cmd.type = NET_CMD_RUN_WIFI_SCAN;
  xQueueSend(g_netCmdQueue, &cmd, 0);
}

static void wifi_net_clicked_cb(lv_event_t *e) {
  const char *ssid = lv_list_get_btn_text(s_netList, (lv_obj_t *)lv_event_get_target(e));
  strlcpy(s_selectedSsid, ssid, sizeof(s_selectedSsid));
  lv_obj_clear_flag(s_wifiConnectPanel, LV_OBJ_FLAG_HIDDEN);
  lv_label_set_text_fmt((lv_obj_t *)lv_obj_get_child(s_wifiConnectPanel, 0), "Ag: %s", ssid);
  lv_textarea_set_text(s_wifiPassTa, "");
}

static void wifi_connect_btn_cb(lv_event_t *e) {
  NetCommand cmd{};
  cmd.type = NET_CMD_WIFI_CONNECT;
  strlcpy(cmd.strA, s_selectedSsid, sizeof(cmd.strA));
  strlcpy(cmd.strB, lv_textarea_get_text(s_wifiPassTa), sizeof(cmd.strB));
  xQueueSend(g_netCmdQueue, &cmd, 0);
  lv_label_set_text(s_wifiStatusLbl, "Baglaniliyor...");
}

static void build_settings(lv_obj_t *parent) {
  lv_obj_t *scanBtn = lv_btn_create(parent);
  lv_obj_set_size(scanBtn, 110, 26);
  lv_obj_align(scanBtn, LV_ALIGN_TOP_LEFT, 6, 4);
  lv_obj_add_event_cb(scanBtn, wifi_scan_btn_cb, LV_EVENT_CLICKED, NULL);
  lv_obj_t *scanLbl = lv_label_create(scanBtn);
  lv_label_set_text(scanLbl, LV_SYMBOL_REFRESH " Ag Tara");
  lv_obj_center(scanLbl);

  s_wifiStatusLbl = lv_label_create(parent);
  lv_label_set_text(s_wifiStatusLbl, wifi_manager_is_connected() ? "Bagli" : "Bagli degil");
  lv_obj_align(s_wifiStatusLbl, LV_ALIGN_TOP_RIGHT, -6, 10);

  s_netList = lv_list_create(parent);
  lv_obj_set_size(s_netList, 150, 150);
  lv_obj_align(s_netList, LV_ALIGN_TOP_LEFT, 6, 36);
  lv_list_add_text(s_netList, "\"Ag Tara\" ile basla");

  s_wifiConnectPanel = lv_obj_create(parent);
  lv_obj_set_size(s_wifiConnectPanel, 148, 150);
  lv_obj_align(s_wifiConnectPanel, LV_ALIGN_TOP_RIGHT, -6, 36);
  lv_obj_add_flag(s_wifiConnectPanel, LV_OBJ_FLAG_HIDDEN);

  lv_obj_t *ssidLbl = lv_label_create(s_wifiConnectPanel);
  lv_label_set_text(ssidLbl, "Ag: -");
  lv_obj_align(ssidLbl, LV_ALIGN_TOP_LEFT, 0, 0);

  s_wifiPassTa = lv_textarea_create(s_wifiConnectPanel);
  lv_textarea_set_password_mode(s_wifiPassTa, true);
  lv_textarea_set_one_line(s_wifiPassTa, true);
  lv_textarea_set_placeholder_text(s_wifiPassTa, "Sifre");
  lv_obj_set_width(s_wifiPassTa, 138);
  lv_obj_align(s_wifiPassTa, LV_ALIGN_TOP_LEFT, 0, 22);
  attach_keyboard(s_wifiPassTa);

  lv_obj_t *connectBtn = lv_btn_create(s_wifiConnectPanel);
  lv_obj_set_size(connectBtn, 100, 26);
  lv_obj_align(connectBtn, LV_ALIGN_TOP_LEFT, 0, 56);
  lv_obj_add_event_cb(connectBtn, wifi_connect_btn_cb, LV_EVENT_CLICKED, NULL);
  lv_obj_t *connectLbl = lv_label_create(connectBtn);
  lv_label_set_text(connectLbl, "Baglan");
  lv_obj_center(connectLbl);
}

// Called by ui_handler() whenever a new NetworkListResult arrives while Settings is open.
static void settings_apply_scan_result(const NetworkListResult &r) {
  lv_obj_clean(s_netList);
  if (r.count == 0) {
    lv_list_add_text(s_netList, "Ag bulunamadi");
    return;
  }
  for (int i = 0; i < r.count; i++) {
    // Button text must stay the *plain* SSID (nothing appended) because
    // wifi_net_clicked_cb() reads it back via lv_list_get_btn_text() to
    // know which network to connect to.
    lv_obj_t *btn = lv_list_add_btn(s_netList, r.nets[i].secure ? LV_SYMBOL_WIFI : LV_SYMBOL_WARNING, r.nets[i].ssid);
    lv_obj_add_event_cb(btn, wifi_net_clicked_cb, LV_EVENT_CLICKED, NULL);
  }
}

// =====================================================================================
//  APP: Chat
// =====================================================================================

static lv_obj_t *s_chatLog;
static lv_obj_t *s_chatTa;

static void chat_append(const char *sender, const char *text) {
  lv_obj_t *lbl = lv_label_create(s_chatLog);
  lv_label_set_long_mode(lbl, LV_LABEL_LONG_WRAP);
  lv_obj_set_width(lbl, lv_obj_get_width(s_chatLog) - 12);
  lv_label_set_text_fmt(lbl, "#%s#  %s", sender, text);
  lv_obj_scroll_to_view(lbl, LV_ANIM_ON);
}

static void chat_send_cb(lv_event_t *e) {
  const char *text = lv_textarea_get_text(s_chatTa);
  if (strlen(text) == 0) return;

  if (!chat_server_is_running()) {
    // Auto-start the AP the first time the user tries to chat.
    NetCommand startCmd{};
    startCmd.type = NET_CMD_START_CHAT_AP;
    xQueueSend(g_netCmdQueue, &startCmd, 0);
  }

  NetCommand cmd{};
  cmd.type = NET_CMD_SEND_CHAT;
  strlcpy(cmd.strA, text, sizeof(cmd.strA));
  xQueueSend(g_netCmdQueue, &cmd, 0);

  lv_textarea_set_text(s_chatTa, "");
}

static void build_chat(lv_obj_t *parent) {
  s_chatLog = lv_obj_create(parent);
  lv_obj_set_size(s_chatLog, TFT_HOR_RES - 12, 130);
  lv_obj_align(s_chatLog, LV_ALIGN_TOP_MID, 0, 4);
  lv_obj_set_flex_flow(s_chatLog, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_style_pad_row(s_chatLog, 4, 0);

  lv_obj_t *info = lv_label_create(s_chatLog);
  lv_label_set_text(info, "AP: " DEFAULT_AP_SSID " baslatmak icin bir mesaj yazip gonderin.");
  lv_label_set_long_mode(info, LV_LABEL_LONG_WRAP);
  lv_obj_set_width(info, TFT_HOR_RES - 24);

  s_chatTa = lv_textarea_create(parent);
  lv_textarea_set_one_line(s_chatTa, true);
  lv_textarea_set_placeholder_text(s_chatTa, "Mesaj yaz...");
  lv_obj_set_width(s_chatTa, TFT_HOR_RES - 90);
  lv_obj_align(s_chatTa, LV_ALIGN_BOTTOM_LEFT, 6, -2);
  attach_keyboard(s_chatTa);

  lv_obj_t *sendBtn = lv_btn_create(parent);
  lv_obj_set_size(sendBtn, 66, 30);
  lv_obj_align(sendBtn, LV_ALIGN_BOTTOM_RIGHT, -6, -2);
  lv_obj_add_event_cb(sendBtn, chat_send_cb, LV_EVENT_CLICKED, NULL);
  lv_obj_t *sendLbl = lv_label_create(sendBtn);
  lv_label_set_text(sendLbl, LV_SYMBOL_RIGHT " Gonder");
  lv_obj_center(sendLbl);
}

// =====================================================================================
//  APP: Wi-Fi analyzer (passive channel congestion bar chart)
// =====================================================================================

static lv_obj_t *s_analyzerChart;
static lv_chart_series_t *s_analyzerSeries;
static lv_obj_t *s_analyzerTopList;

static void analyzer_scan_btn_cb(lv_event_t *e) {
  NetCommand cmd{};
  cmd.type = NET_CMD_RUN_WIFI_ANALYZER;
  xQueueSend(g_netCmdQueue, &cmd, 0);
  lv_list_add_text(s_analyzerTopList, "Taraniyor...");
}

static void build_wifi_analyzer(lv_obj_t *parent) {
  lv_obj_t *scanBtn = lv_btn_create(parent);
  lv_obj_set_size(scanBtn, 90, 24);
  lv_obj_align(scanBtn, LV_ALIGN_TOP_LEFT, 4, 2);
  lv_obj_add_event_cb(scanBtn, analyzer_scan_btn_cb, LV_EVENT_CLICKED, NULL);
  lv_obj_t *scanLbl = lv_label_create(scanBtn);
  lv_label_set_text(scanLbl, LV_SYMBOL_REFRESH " Tara");
  lv_obj_center(scanLbl);

  s_analyzerChart = lv_chart_create(parent);
  lv_obj_set_size(s_analyzerChart, 190, 110);
  lv_obj_align(s_analyzerChart, LV_ALIGN_TOP_LEFT, 4, 30);
  lv_chart_set_type(s_analyzerChart, LV_CHART_TYPE_BAR);
  lv_chart_set_point_count(s_analyzerChart, 13); // channels 1-13
  lv_chart_set_range(s_analyzerChart, LV_CHART_AXIS_PRIMARY_Y, 0, 8);
  s_analyzerSeries = lv_chart_add_series(s_analyzerChart, lv_color_hex(0x59c1ff), LV_CHART_AXIS_PRIMARY_Y);
  for (int i = 0; i < 13; i++) lv_chart_set_next_value(s_analyzerChart, s_analyzerSeries, 0);

  s_analyzerTopList = lv_list_create(parent);
  lv_obj_set_size(s_analyzerTopList, 108, 140);
  lv_obj_align(s_analyzerTopList, LV_ALIGN_TOP_RIGHT, -4, 30);
  lv_list_add_text(s_analyzerTopList, "En guclu aglar");
}

static void analyzer_apply_result(const WifiScanResult &r) {
  for (int ch = 1; ch <= 13; ch++) {
    lv_chart_set_value_by_id(s_analyzerChart, s_analyzerSeries, ch - 1, r.channelCounts[ch]);
  }
  lv_obj_clean(s_analyzerTopList);
  lv_list_add_text(s_analyzerTopList, "En guclu aglar");
  for (int i = 0; i < WIFI_TOP_AP_COUNT; i++) {
    if (r.topSsid[i][0] == '\0') continue;
    // 32-char SSID + " (chNN)" + NUL must fit, so size for the worst case.
    char line[64];
    snprintf(line, sizeof(line), "%.32s (ch%d)", r.topSsid[i], r.topChannel[i]);
    lv_list_add_btn(s_analyzerTopList, LV_SYMBOL_WIFI, line);
  }
}

// =====================================================================================
//  APP: BLE scanner (passive discovery only)
//  NOTE: The original brief asked for a "BLE spam" mode that floods nearby
//  phones with fake advertisement packets to force crash dialogs / pairing
//  popups. That is a denial-of-service attack against devices you don't own,
//  so this build replaces it with a passive scanner instead (like nRF
//  Connect) - lists nearby BLE devices and their signal strength only.
// =====================================================================================

static lv_obj_t *s_bleList;

static void ble_scan_btn_cb(lv_event_t *e) {
  NetCommand cmd{};
  cmd.type = NET_CMD_RUN_BLE_SCAN;
  xQueueSend(g_netCmdQueue, &cmd, 0);
  lv_obj_clean(s_bleList);
  lv_list_add_text(s_bleList, "Taraniyor (4 sn)...");
}

static void build_ble_scanner(lv_obj_t *parent) {
  lv_obj_t *scanBtn = lv_btn_create(parent);
  lv_obj_set_size(scanBtn, 100, 26);
  lv_obj_align(scanBtn, LV_ALIGN_TOP_MID, 0, 4);
  lv_obj_add_event_cb(scanBtn, ble_scan_btn_cb, LV_EVENT_CLICKED, NULL);
  lv_obj_t *scanLbl = lv_label_create(scanBtn);
  lv_label_set_text(scanLbl, LV_SYMBOL_BLUETOOTH " Tara");
  lv_obj_center(scanLbl);

  s_bleList = lv_list_create(parent);
  lv_obj_set_size(s_bleList, TFT_HOR_RES - 20, 145);
  lv_obj_align(s_bleList, LV_ALIGN_TOP_MID, 0, 36);
  lv_list_add_text(s_bleList, "\"Tara\" ile yakindaki cihazlari listele");
}

static void ble_apply_result(const BleScanResult &r) {
  lv_obj_clean(s_bleList);
  if (r.count == 0) {
    lv_list_add_text(s_bleList, "Cihaz bulunamadi");
    return;
  }
  for (int i = 0; i < r.count; i++) {
    // name is up to 31 chars + " (-120 dBm)" + NUL.
    char line[64];
    snprintf(line, sizeof(line), "%.31s (%d dBm)", r.devices[i].name, r.devices[i].rssi);
    lv_list_add_btn(s_bleList, LV_SYMBOL_BLUETOOTH, line);
  }
}

// =====================================================================================
//  APP: Games (dynamic SD folder scan)
// =====================================================================================

static void snake_game_start(); // fwd decl

static void game_entry_cb(lv_event_t *e) {
  const char *folder = (const char *)lv_event_get_user_data(e);
  if (strcmp(folder, "snake") == 0) {
    if (!nav_push(APP_SNAKE)) return;
    show_app(APP_SNAKE);
  } else {
    // Any other folder found on the card: acknowledged but no built-in
    // engine yet - this is where you'd wire up a per-game renderer.
    lv_obj_t *mbox = lv_msgbox_create(lv_scr_act(), "Yakinda", "Bu oyun icin motor henuz yazilmadi.", NULL, true);
    lv_obj_center(mbox);
  }
}

static void build_games(lv_obj_t *parent) {
  GameList list;
  game_loader_scan(list);

  lv_obj_t *lbl = lv_label_create(parent);
  lv_label_set_text_fmt(lbl, "SD:/games klasorunde %d oyun bulundu", list.count);
  lv_obj_align(lbl, LV_ALIGN_TOP_LEFT, 6, 4);

  lv_obj_t *gameList = lv_list_create(parent);
  lv_obj_set_size(gameList, TFT_HOR_RES - 20, 150);
  lv_obj_align(gameList, LV_ALIGN_TOP_MID, 0, 28);

  for (int i = 0; i < list.count; i++) {
    // static storage so the folder-name pointer survives after this
    // function returns (event user_data must stay valid).
    static char names[MAX_GAMES][24];
    strlcpy(names[i], list.games[i].folderName, sizeof(names[i]));
    int hs = game_loader_get_highscore(list.games[i].folderName);
    char line[48];
    snprintf(line, sizeof(line), "%s (en yuksek: %d)", names[i], hs);
    lv_obj_t *btn = lv_list_add_btn(gameList, LV_SYMBOL_PLAY, line);
    lv_obj_add_event_cb(btn, game_entry_cb, LV_EVENT_CLICKED, names[i]);
  }
}

// =====================================================================================
//  APP: Snake (built-in demo game engine - proves out the dynamic loader)
// =====================================================================================

#define SNAKE_GRID_W 18
#define SNAKE_GRID_H 12
#define SNAKE_CELL_PX 11
#define SNAKE_MAX_LEN (SNAKE_GRID_W * SNAKE_GRID_H)

struct Pt { int8_t x, y; };
static Pt s_snakeBody[SNAKE_MAX_LEN];
static int s_snakeLen;
static int s_snakeDirX, s_snakeDirY;
static Pt s_food;
static bool s_snakeAlive;
static int s_snakeScore;
static lv_obj_t *s_snakeCells[SNAKE_GRID_H][SNAKE_GRID_W];
static lv_obj_t *s_snakeScoreLbl;
static lv_timer_t *s_snakeTimer;

// Spawns food on a random free cell. Arduino's random(min, max) is
// upper-exclusive, hence the "+ 1" - without it the last row/column could
// never get food. The retry loop also guarantees we never drop food under
// the snake's own body (which would make the round unwinnable).
static void snake_place_food() {
  for (int attempt = 0; attempt < 32; attempt++) {
    s_food.x = random(0, SNAKE_GRID_W);
    s_food.y = random(0, SNAKE_GRID_H);
    bool occupied = false;
    for (int i = 0; i < s_snakeLen && !occupied; i++) {
      if (s_snakeBody[i].x == s_food.x && s_snakeBody[i].y == s_food.y) occupied = true;
    }
    if (!occupied) return;
  }
  // Board is full (snake won) - park the food outside the grid; snake_set_cell()
  // bounds-checks, so nothing is drawn.
  s_food.x = -1;
  s_food.y = -1;
}

static void snake_set_cell(int x, int y, lv_color_t c) {
  if (x < 0 || x >= SNAKE_GRID_W || y < 0 || y >= SNAKE_GRID_H) return;
  lv_obj_set_style_bg_color(s_snakeCells[y][x], c, 0);
}

static void snake_tick(lv_timer_t *t) {
  if (!s_snakeAlive) return;

  Pt head = s_snakeBody[0];
  Pt newHead = { (int8_t)(head.x + s_snakeDirX), (int8_t)(head.y + s_snakeDirY) };

  bool crashed = (newHead.x < 0 || newHead.x >= SNAKE_GRID_W || newHead.y < 0 || newHead.y >= SNAKE_GRID_H);
  for (int i = 0; i < s_snakeLen && !crashed; i++) {
    if (s_snakeBody[i].x == newHead.x && s_snakeBody[i].y == newHead.y) crashed = true;
  }

  if (crashed) {
    s_snakeAlive = false;
    lv_label_set_text_fmt(s_snakeScoreLbl, "OYUN BITTI - Skor: %d", s_snakeScore);
    if (s_snakeScore > game_loader_get_highscore("snake")) {
      game_loader_set_highscore("snake", s_snakeScore);
    }
    return;
  }

  bool ate = (newHead.x == s_food.x && newHead.y == s_food.y);

  Pt tail = s_snakeBody[s_snakeLen - 1];
  if (!ate) {
    snake_set_cell(tail.x, tail.y, lv_color_hex(0x1c2128)); // clear old tail
  } else {
    s_snakeScore += 10;
    if (s_snakeLen < SNAKE_MAX_LEN) s_snakeLen++;
  }

  // Shift first, then place the food: snake_place_food() has to see the
  // post-move body, otherwise it can drop food onto the head that is about
  // to be drawn over it.
  for (int i = s_snakeLen - 1; i > 0; i--) s_snakeBody[i] = s_snakeBody[i - 1];
  s_snakeBody[0] = newHead;
  snake_set_cell(newHead.x, newHead.y, lv_color_hex(0x7fd97a));

  if (ate) {
    snake_place_food();
    if (s_food.x < 0) {
      // No free cell left - the snake filled the board.
      s_snakeAlive = false;
      lv_label_set_text_fmt(s_snakeScoreLbl, "KAZANDIN! Skor: %d", s_snakeScore);
      if (s_snakeScore > game_loader_get_highscore("snake")) {
        game_loader_set_highscore("snake", s_snakeScore);
      }
      return;
    }
    snake_set_cell(s_food.x, s_food.y, lv_color_hex(0xd94f4f));
    lv_label_set_text_fmt(s_snakeScoreLbl, "Skor: %d", s_snakeScore);
  }
}

static void snake_dir_cb(lv_event_t *e) {
  intptr_t d = (intptr_t)lv_event_get_user_data(e);
  int dx = 0, dy = 0;
  switch (d) {
    case 0: dx =  0; dy = -1; break; // up
    case 1: dx =  0; dy =  1; break; // down
    case 2: dx = -1; dy =  0; break; // left
    case 3: dx =  1; dy =  0; break; // right
  }
  // ignore reversing straight into yourself
  if (s_snakeLen > 1 && dx == -s_snakeDirX && dy == -s_snakeDirY) return;
  s_snakeDirX = dx;
  s_snakeDirY = dy;
}

static void snake_game_start() {
  s_snakeLen = 3;
  s_snakeDirX = 1; s_snakeDirY = 0;
  s_snakeAlive = true;
  s_snakeScore = 0;
  for (int i = 0; i < s_snakeLen; i++) { s_snakeBody[i].x = 4 - i; s_snakeBody[i].y = SNAKE_GRID_H / 2; }
  snake_place_food();
  for (int y = 0; y < SNAKE_GRID_H; y++)
    for (int x = 0; x < SNAKE_GRID_W; x++)
      snake_set_cell(x, y, lv_color_hex(0x1c2128));
  for (int i = 0; i < s_snakeLen; i++) snake_set_cell(s_snakeBody[i].x, s_snakeBody[i].y, lv_color_hex(0x7fd97a));
  snake_set_cell(s_food.x, s_food.y, lv_color_hex(0xd94f4f));
  lv_label_set_text(s_snakeScoreLbl, "Skor: 0");
}

static void build_snake(lv_obj_t *parent) {
  lv_obj_t *grid = lv_obj_create(parent);
  lv_obj_remove_style_all(grid);
  lv_obj_set_size(grid, SNAKE_GRID_W * SNAKE_CELL_PX, SNAKE_GRID_H * SNAKE_CELL_PX);
  lv_obj_align(grid, LV_ALIGN_TOP_LEFT, 2, 2);

  for (int y = 0; y < SNAKE_GRID_H; y++) {
    for (int x = 0; x < SNAKE_GRID_W; x++) {
      lv_obj_t *cell = lv_obj_create(grid);
      lv_obj_remove_style_all(cell);
      lv_obj_set_size(cell, SNAKE_CELL_PX - 1, SNAKE_CELL_PX - 1);
      lv_obj_set_pos(cell, x * SNAKE_CELL_PX, y * SNAKE_CELL_PX);
      lv_obj_set_style_bg_color(cell, lv_color_hex(0x1c2128), 0);
      lv_obj_set_style_bg_opa(cell, LV_OPA_COVER, 0);
      s_snakeCells[y][x] = cell;
    }
  }

  int gridWidthPx = SNAKE_GRID_W * SNAKE_CELL_PX;
  int padX = gridWidthPx + 8;

  s_snakeScoreLbl = lv_label_create(parent);
  lv_label_set_text(s_snakeScoreLbl, "Skor: 0");
  lv_obj_set_pos(s_snakeScoreLbl, padX, 2);

  // Simple D-pad using 4 buttons stacked in the leftover width on the right.
  static const char *dpadSym[4] = { LV_SYMBOL_UP, LV_SYMBOL_DOWN, LV_SYMBOL_LEFT, LV_SYMBOL_RIGHT };
  for (int d = 0; d < 4; d++) {
    lv_obj_t *btn = lv_btn_create(parent);
    lv_obj_set_size(btn, 40, 34);
    lv_obj_set_pos(btn, padX + (d % 2) * 44, 28 + (d / 2) * 40);
    lv_obj_add_event_cb(btn, snake_dir_cb, LV_EVENT_CLICKED, (void *)(intptr_t)d);
    lv_obj_t *l = lv_label_create(btn);
    lv_label_set_text(l, dpadSym[d]);
    lv_obj_center(l);
  }

  snake_game_start();
  if (s_snakeTimer) lv_timer_del(s_snakeTimer);
  s_snakeTimer = lv_timer_create(snake_tick, 220, NULL);
}

// =====================================================================================
//  Screen switcher
// =====================================================================================

static void show_app(AppScreen app) {
  if (s_snakeTimer && app != APP_SNAKE) {
    lv_timer_del(s_snakeTimer);
    s_snakeTimer = NULL;
  }
  lv_obj_clean(s_content);

  // Everything below lived inside s_content and is now a dangling pointer.
  // Drop them so a late callback can never dereference freed widgets.
  s_wifiStatusLbl = NULL;
  s_netList       = NULL;
  s_wifiPassTa    = NULL;
  s_wifiConnectPanel = NULL;
  s_chatLog       = NULL;
  s_chatTa        = NULL;
  s_analyzerChart = NULL;
  s_analyzerSeries = NULL;
  s_analyzerTopList = NULL;
  s_bleList       = NULL;
  s_snakeScoreLbl = NULL;

  switch (app) {
    case APP_HOME:           build_home(s_content); break;
    case APP_SETTINGS:       build_settings(s_content); break;
    case APP_CHAT:           build_chat(s_content); break;
    case APP_WIFI_ANALYZER:  build_wifi_analyzer(s_content); break;
    case APP_BLE_SCANNER:    build_ble_scanner(s_content); break;
    case APP_GAMES:          build_games(s_content); break;
    case APP_SNAKE:          build_snake(s_content); break;
  }
}

// =====================================================================================
//  Public entry points
// =====================================================================================

void ui_init() {
  tft.begin();
  tft.setRotation(TFT_ROTATION);

  // Backlight: plain on/off to stay compatible across ESP32 Arduino core
  // versions (the ledcSetup/ledcAttachPin PWM API changed between core 2.x
  // and 3.x). See README.md if you want brightness dimming instead.
  pinMode(PIN_TFT_BL, OUTPUT);
  digitalWrite(PIN_TFT_BL, HIGH);

  // The SD card shares the SPI bus TFT_eSPI just configured above (same
  // SCLK/MOSI/MISO, its own CS), so it must be mounted only *after*
  // tft.begin() - never before.
  if (!sd_init()) {
    Serial.println("[CyberPhoneOS] UYARI: SD kart bulunamadi, kart olmadan devam ediliyor.");
  }

  lv_init();
  lv_disp_draw_buf_init(&draw_buf, buf1, NULL, TFT_HOR_RES * LVGL_BUF_LINES);

  static lv_disp_drv_t disp_drv;
  lv_disp_drv_init(&disp_drv);
  disp_drv.hor_res = TFT_HOR_RES;
  disp_drv.ver_res = TFT_VER_RES;
  disp_drv.flush_cb = disp_flush_cb;
  disp_drv.draw_buf = &draw_buf;
  lv_disp_drv_register(&disp_drv);

  static lv_indev_drv_t indev_drv;
  lv_indev_drv_init(&indev_drv);
  indev_drv.type = LV_INDEV_TYPE_POINTER;
  indev_drv.read_cb = touch_read_cb;
  lv_indev_drv_register(&indev_drv);

  // One-time touch calibration, stored on the SD card. Delete
  // /system/config.json if you ever need to recalibrate.
  sd_load_config(s_cfg);
  uint16_t calData[5];
  bool haveCal = false;
  for (int i = 0; i < 5; i++) if (s_cfg.touch_cal[i] != 0) haveCal = true;
  if (haveCal) {
    for (int i = 0; i < 5; i++) calData[i] = (uint16_t)s_cfg.touch_cal[i];
    tft.setTouch(calData);
  } else {
    tft.fillScreen(TFT_BLACK);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.setCursor(20, 100);
    tft.println("Dokunmatik kalibrasyon: koseleri sirayla dokunun");
    tft.calibrateTouch(calData, TFT_MAGENTA, TFT_BLACK, 20);
    tft.setTouch(calData);
    for (int i = 0; i < 5; i++) s_cfg.touch_cal[i] = calData[i];
    sd_save_config(s_cfg);
  }

  s_root = lv_scr_act();
  lv_obj_set_style_bg_color(s_root, lv_color_hex(0x05070a), 0);

  build_status_bar();
  build_nav_bar();

  s_content = lv_obj_create(s_root);
  lv_obj_remove_style_all(s_content);
  lv_obj_set_size(s_content, TFT_HOR_RES, TFT_VER_RES - 20 - 30);
  lv_obj_align(s_content, LV_ALIGN_TOP_MID, 0, 20);
  lv_obj_set_style_bg_color(s_content, lv_color_hex(0x05070a), 0);
  lv_obj_set_style_bg_opa(s_content, LV_OPA_COVER, 0);

  s_keyboard = lv_keyboard_create(s_root);
  lv_obj_set_size(s_keyboard, TFT_HOR_RES, 120);
  lv_obj_align(s_keyboard, LV_ALIGN_BOTTOM_MID, 0, 0);
  lv_obj_add_flag(s_keyboard, LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_event_cb(s_keyboard, kb_event_cb, LV_EVENT_READY, NULL);
  lv_obj_add_event_cb(s_keyboard, kb_event_cb, LV_EVENT_CANCEL, NULL);

  s_stack[0] = APP_HOME;
  s_stackTop = 0;
  show_app(APP_HOME);
}

void ui_handler() {
  lv_timer_handler();

  static unsigned long lastStatusRefresh = 0;
  if (millis() - lastStatusRefresh > 500) {
    status_bar_refresh();
    // The connection state machine runs on the network core, so mirror it
    // into Settings here - otherwise the label would stay on the value it
    // had when the screen was built.
    if (s_stack[s_stackTop] == APP_SETTINGS && s_wifiStatusLbl) {
      switch (wifi_manager_current_state()) {
        case WIFI_STATE_CONNECTED:   lv_label_set_text(s_wifiStatusLbl, "Bagli"); break;
        case WIFI_STATE_CONNECTING:  lv_label_set_text(s_wifiStatusLbl, "Baglaniliyor..."); break;
        case WIFI_STATE_FAILED:      lv_label_set_text(s_wifiStatusLbl, "Baglanamadi"); break;
        default:                     lv_label_set_text(s_wifiStatusLbl, "Bagli degil"); break;
      }
    }
    lastStatusRefresh = millis();
  }

  // Drain inter-core queues (non-blocking peeks).
  ChatMessage cm;
  while (xQueueReceive(g_chatInQueue, &cm, 0) == pdTRUE) {
    if (s_stack[s_stackTop] == APP_CHAT) chat_append(cm.sender, cm.text);
  }

  NetworkListResult nlr;
  if (xQueueReceive(g_wifiListQueue, &nlr, 0) == pdTRUE) {
    if (s_stack[s_stackTop] == APP_SETTINGS) settings_apply_scan_result(nlr);
  }

  WifiScanResult wsr;
  if (xQueueReceive(g_wifiScanQueue, &wsr, 0) == pdTRUE) {
    if (s_stack[s_stackTop] == APP_WIFI_ANALYZER) analyzer_apply_result(wsr);
  }

  BleScanResult bsr;
  if (xQueueReceive(g_bleScanQueue, &bsr, 0) == pdTRUE) {
    if (s_stack[s_stackTop] == APP_BLE_SCANNER) ble_apply_result(bsr);
  }
}
