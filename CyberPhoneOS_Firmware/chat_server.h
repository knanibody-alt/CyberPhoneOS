#ifndef CHAT_SERVER_H
#define CHAT_SERVER_H

#include <Arduino.h>

// Starts the ESP32 as a Wi-Fi Access Point, mounts SD:/www as the web
// root, and opens a WebSocket at /ws. Any phone joining the AP and
// browsing to 192.168.4.1 gets the chat page from the SD card.
void chat_server_start(const char *apSsid, const char *apPass);

void chat_server_stop();

// Called by the network task's loop to pump WebSocket cleanup etc.
void chat_server_loop();

// Sends a message that originated on the ESP32's own touchscreen keyboard
// out to every connected phone, and echoes it into the on-device queue too.
void chat_server_send_from_device(const char *sender, const char *text);

bool chat_server_is_running();

#endif // CHAT_SERVER_H
