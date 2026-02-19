#include "web_server.h"

// Implementation of helper function for ServerClient logging
void webServerBroadcastLog(NFCWebServer* ws, const String& message, const String& level) {
    if (ws != nullptr) {
        ws->broadcastLog(message, level);
    }
}
