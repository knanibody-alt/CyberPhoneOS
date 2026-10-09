#include "chat_server.h"
#include "shared_types.h"
#include "config.h"
#include <WiFi.h>
#include <SD.h>
#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>

static AsyncWebServer server(AP_CHAT_PORT);
static AsyncWebSocket ws("/ws");
static bool s_running = false;
static bool s_routesRegistered = false;

static void push_to_ui(const char *sender, const char *text) {
  ChatMessage msg;
  strlcpy(msg.sender, sender, sizeof(msg.sender));
  strlcpy(msg.text, text, sizeof(msg.text));
  // Never block the network task waiting on a full queue.
  xQueueSend(g_chatInQueue, &msg, 0);
}

static void broadcast_chat(const char *sender, const char *text) {
  StaticJsonDocument<256> doc;
  doc["sender"] = sender;
  doc["text"] = text;
  String out;
  serializeJson(doc, out);
  ws.textAll(out);
}

static void handle_ws_message(AsyncWebSocketClient *client, uint8_t *data, size_t len) {
  StaticJsonDocument<256> doc;
  DeserializationError err = deserializeJson(doc, data, len);
  if (err) return;

  const char *sender = doc["sender"] | "phone";
  const char *text   = doc["text"]   | "";
  if (strlen(text) == 0) return;

  push_to_ui(sender, text);
  broadcast_chat(sender, text);
}

static void onWsEvent(AsyncWebSocket *server, AsyncWebSocketClient *client,
                      AwsEventType type, void *arg, uint8_t *data, size_t len) {
  switch (type) {
    case WS_EVT_CONNECT:
      push_to_ui("system", "bir telefon sohbete katildi");
      break;
    case WS_EVT_DISCONNECT:
      push_to_ui("system", "bir telefon ayrildi");
      break;
    case WS_EVT_DATA: {
      AwsFrameInfo *info = (AwsFrameInfo*)arg;
      if (info->final && info->index == 0 && info->len == len && info->opcode == WS_TEXT) {
        handle_ws_message(client, data, len);
      }
      break;
    }
    default: break;
  }
}

void chat_server_start(const char *apSsid, const char *apPass) {
  if (s_running) return;

  WiFi.mode(WIFI_AP_STA); // AP for chat clients, STA left available for internet if already connected
  WiFi.softAP(apSsid, apPass);

  // Route registration is permanent for the lifetime of the AsyncWebServer
  // object - re-adding them on every start() would stack duplicate handlers,
  // so the static tree is wired up exactly once.
  if (!s_routesRegistered) {
    ws.onEvent(onWsEvent);
    server.addHandler(&ws);

    // Serve the whole SD:/www tree, default to index.html.
    server.serveStatic("/", SD, "/www/").setDefaultFile("index.html");

    server.onNotFound([](AsyncWebServerRequest *request) {
      request->send(404, "text/plain", "Dosya bulunamadi (SD:/www icinde arandi)");
    });
    s_routesRegistered = true;
  }

  server.begin();
  s_running = true;
  String banner = String("AP baslatildi: ") + apSsid;
  push_to_ui("system", banner.c_str());
}

void chat_server_stop() {
  if (!s_running) return;
  ws.closeAll();
  server.end();
  WiFi.softAPdisconnect(true);
  s_running = false;
}

void chat_server_loop() {
  if (s_running) {
    ws.cleanupClients();
  }
}

void chat_server_send_from_device(const char *sender, const char *text) {
  if (!s_running) return;
  broadcast_chat(sender, text);
  push_to_ui(sender, text); // so it also appears in the ESP32's own chat window
}

bool chat_server_is_running() { return s_running; }
