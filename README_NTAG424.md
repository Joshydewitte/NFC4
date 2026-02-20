# NTAG424 DNA Integration - Quick Start

> **Status:** 95% Complete - Integration & Testing Required  
> **Last Update:** February 19, 2026

---

## 🎯 What Works

✅ AES-128 crypto library (ESP32 mbedtls)  
✅ EV2 First authentication protocol  
✅ ChangeKey command implementation  
✅ Web UI for card personalization  
✅ Node.js server for key derivation  
✅ Complete documentation

## ⚠️ What's Missing

✅ **PN5180 ISO-DEP/RATS integration** ← FIXED! (See ISO_DEP_FIX.md)  
✅ NTAG424Handler instantiation in main.cpp  
✅ NFCReader::getNFC() getter method  
⏳ Hardware testing met NTAG424 DNA kaart ← START HERE

---

## 🚀 Quick Start for Next Developer

### 1. Read This First
**[DEVELOPER_HANDOFF.md](DEVELOPER_HANDOFF.md)** - Complete implementation guide (15 min read)

### 2. Implementation Order

```
Step 1: Implement transceive() in ntag424_handler.cpp
        ↓ (2-4 hours)
Step 2: Add getNFC() to nfc_reader.h
        ↓ (5 minutes)
Step 3: Instantiate NTAG424Handler in main.cpp
        ↓ (1-2 hours)
Step 4: Test with factory fresh NTAG424 DNA card
        ↓ (3-6 hours)
```

### 3. Key Files to Edit

| File | What to Do | Lines |
|------|-----------|-------|
| `src/ntag424_handler.cpp` | Implement `sendCommand()` using PN5180 API | ~160-180 |
| `include/nfc_reader.h` | Add `getNFC()` getter | ~50 |
| `src/main.cpp` | Instantiate handler + implement config mode | ~100, ~200 |

---

## 📚 Documentation Structure

```
DEVELOPER_HANDOFF.md          ← Main implementation guide
├─ System Overview
├─ Architecture Diagram
├─ Step-by-step Implementation
├─ Testing Procedures
└─ Troubleshooting

NTAG424_IMPLEMENTATION.md     ← Technical deep-dive
├─ Protocol Specifications
├─ Crypto Details
├─ Command Reference
└─ Test Procedures

QUICK_START_NTAG424.md        ← Fast reference
├─ Quick Integration Guide
├─ Code Examples
└─ Common Pitfalls

CODE_SNIPPETS_NTAG424.md      ← Copy-paste ready code
├─ transceive() skeleton
├─ main.cpp integration
├─ CRC32 implementation
└─ Error handling examples
```

---

## 🧪 Testing

### Hardware Required
- Seeed XIAO ESP32S3
- PN5180 NFC Reader
- Factory fresh NTAG424 DNA cards
- USB-C cable

### Software Required
```bash
# Check these are working:
pio --version                          # PlatformIO CLI
node --version                         # Node.js for server
cd nfc-server && node server.js        # Should start on port 3000
```

### Test Workflow
1. Upload firmware: `pio run --target upload --upload-port COM9`
2. Start server: `node nfc-server/server.js`
3. Open web UI: `http://[ESP32-IP]/config-card`
4. Follow 5-step wizard to personalize card
5. Verify in machine mode

---

## 🔍 Finding the PN5180 API

```bash
# The PN5180ISO14443 class is here:
.pio/libdeps/seeed_xiao_esp32s3/PN5180-Library/PN5180ISO14443.h
.pio/libdeps/seeed_xiao_esp32s3/PN5180-Library/PN5180ISO14443.cpp

# Key method to use:
ISO14443ErrorCode issueISO14443Command(
    uint8_t *cmd,       // Command to send
    uint16_t cmdLen,    // Length of command
    uint8_t **result,   // Response buffer (output)
    uint16_t *resultLen // Response length (output)
);
```

**Your job:** Wrap this in `NTAG424Handler::sendCommand()` with proper error handling.

---

## 📞 Getting Help

### Compilation Issues
- Clean build: `pio run --target clean`
- Verbose: `pio run -v`
- Check logs in Serial Monitor

### Runtime Issues
- Enable verbose logging in `ntag424_handler.cpp`
- Use WebSocket logs in `/config-card` UI
- Compare output with expected in DEVELOPER_HANDOFF.md

### Protocol Issues
- Read NTAG424 datasheet (section 10: Security)
- Check command codes in NTAG424_IMPLEMENTATION.md
- Verify with logic analyzer if needed

---

## ✅ Definition of Done

The implementation is complete when:

1. ✅ Factory fresh NTAG424 DNA card can be personalized via web UI
2. ✅ New master key is successfully written to card
3. ✅ Card is registered in server database
4. ✅ Machine mode can authenticate with personalized card
5. ✅ No errors in Serial Monitor during full workflow
6. ✅ All compilation warnings resolved (optional)

---

## 🎓 Learning Resources

### NTAG424 DNA Protocol
- [NXP NTAG424 DNA Datasheet](https://www.nxp.com/docs/en/data-sheet/NTAG424_DNA.pdf)
- [Application Note: AES Authentication](https://www.nxp.com/docs/en/application-note/AN12196.pdf)

### PN5180 NFC Frontend
- [PN5180 Library GitHub](https://github.com/ATrappmann/PN5180-Library)
- [PN5180 Datasheet](https://www.nxp.com/docs/en/data-sheet/PN5180A0XX.pdf)

### ESP32 Development
- [ESP32 Arduino Core](https://docs.espressif.com/projects/arduino-esp32/en/latest/)
- [mbedtls Crypto Library](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/protocols/mbedtls.html)

---

## 🏗️ Project Structure

```
NFC4/
├── src/
│   ├── main.cpp                    ⚠️ Needs NTAG424Handler integration
│   ├── nfc_reader.cpp              ✅ Working
│   ├── ntag424_crypto.cpp          ✅ Complete
│   ├── ntag424_handler.cpp         ⚠️ Needs transceive() implementation
│   ├── system_config.cpp           ✅ Working
│   └── web_server.cpp              ✅ Working
│
├── include/
│   ├── nfc_reader.h                ⚠️ Needs getNFC() method
│   ├── ntag424_crypto.h            ✅ Complete
│   ├── ntag424_handler.h           ✅ Complete
│   ├── config_card_page.h          ✅ Complete (web UI)
│   ├── settings_page.h             ✅ Updated with config link
│   └── [other headers]             ✅ Working
│
├── platformio.ini                  ✅ Configured for ESP32S3
├── DEVELOPER_HANDOFF.md            📖 Your main guide
├── NTAG424_IMPLEMENTATION.md       📖 Technical reference
├── QUICK_START_NTAG424.md          📖 Quick reference
├── CODE_SNIPPETS_NTAG424.md        📋 Code examples
└── README_NTAG424.md               📖 This file

External:
└── nfc-server/                     ✅ Node.js server (running)
    ├── server.js                   ✅ Key derivation API
    └── cards.json                  📦 Card database
```

---

## 🚨 Important Notes

1. **Don't skip transceive() implementation** - Everything depends on it
2. **Test incrementally** - Start with getVersion(), then auth, then changeKey
3. **Use verbose logging** - Makes debugging 10x easier
4. **Check PN5180 library version** - We're using 1.1.0+sha.7879748
5. **Factory keys are all zeros** - DEFAULT_AES_KEY[16] = {0x00, ...}

---

## 💡 Pro Tips

- Use Serial Monitor at 115200 baud for debugging
- WebSocket logs in `/config-card` show real-time progress
- PN5180 library examples are in `.pio/libdeps/.../examples/`
- ESP32 watchdog timer is 5 seconds - avoid long blocking operations
- NTAG424 commands timeout after ~500ms - handle gracefully

---

**Ready to start? Open [DEVELOPER_HANDOFF.md](DEVELOPER_HANDOFF.md) and follow Step 1!**

---

*Built with ❤️ on ESP32 + PN5180 + NTAG424 DNA*  
*Crypto by NXP, Implementation by GitHub Copilot, Integration by You!*
