#include "web_server.h"

// Implementation of helper functions for ServerClient
void webServerBroadcastLog(NFCWebServer* ws, const String& message, const String& level) {
    if (ws != nullptr) {
        ws->broadcastLog(message, level);
    }
}

void webServerBroadcastRaw(NFCWebServer* ws, const String& json) {
    if (ws != nullptr) {
        ws->broadcastRaw(json);
    }
}
