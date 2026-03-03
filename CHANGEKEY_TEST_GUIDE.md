# NTAG424 DNA ChangeKey Test Suite
## Systematische Test voor SW=911E Error Resolution

**Datum:** 2 maart 2026  
**Status:** Test Suite Geïmplementeerd ✅  
**Doel:** Identificeer correcte APDU structuur voor ChangeKey command

---

## 📋 Test Overzicht

De test suite implementeert **6 verschillende APDU configuraties** om te bepalen welke variant de SW=911E (length error) oplost:

| Test | Beschrijving | Hypothese |
|------|-------------|-----------|
| **test0** | Current implementation (baseline) | Native mode, P1=0x00, KeyNo in data, Le=0x00 |
| **test1** | ISO-wrapped mode | P1=KeyNo, GEEN KeyNo in data, Le=0x00 |
| **test2** | Case 3 APDU | P1=0x00, KeyNo in data, GEEN Le |
| **test3** | CmdCtr=0 forced | Native mode, maar CmdCtr=0 i.p.v. 1 |
| **test4** | Combo: Wrapped + No Le | P1=KeyNo, geen KeyNo in data, geen Le |
| **test5** | Combo: No Le + CmdCtr=0 | Native mode, geen Le, CmdCtr=0 |

---

## 🔧 Voorbereiding

### Hardware Setup
1. **Seeed XIAO ESP32S3** aangesloten via USB
2. **PN5180 NFC reader** correct geconnecteerd (pins volgens platformio.ini)
3. **NTAG424 DNA kaart** met **factory key** (00...00)
   - ⚠️ **LET OP:** Kaart moet factory status hebben of gebruik test kaart

### Software Setup
```bash
# 1. Code uploaden naar ESP32
pio run -t upload -e seeed_xiao_esp32s3

# 2. Open Serial Monitor (baudrate 115200)
pio device monitor -b 115200
```

---

## 🧪 Test Procedure

### Stap 1: System Startup
Wacht tot je dit ziet in serial monitor:
```
==================================
✅ System Ready!
Hold D0 button for 3s to enter WiFi config

🔬 TEST COMMANDS (via Serial Monitor):
  test0 = Current implementation (baseline)
  test1 = P1=KeyNo (ISO-wrapped mode)
  test2 = omitLe=true (Case 3 APDU)
  test3 = CmdCtr=0 forced
  test4 = Combo: P1=KeyNo + omitLe
  test5 = Combo: omitLe + CmdCtr=0
==================================
```

### Stap 2: Plaats Kaart
- Leg NTAG424 DNA kaart op de NFC reader
- Wacht tot kaart is gedetecteerd (zie LED/logs)

### Stap 3: Run Test
Typ in serial monitor:
```
test0
```
(of test1, test2, test3, test4, test5)

### Stap 4: Observeer Output

#### Bij SUCCESS (0x9100):
```
╔═══════════════════════════════════════════════════════╗
║                  🎉 TEST SUCCESS! 🎉                  ║
╚═══════════════════════════════════════════════════════╝

Test Mode 2 resolved the 0x911E error!
This APDU configuration is correct.
```

#### Bij FALEN (0x911E):
```
╔═══════════════════════════════════════════════════════╗
║                    ❌ TEST FAILED                     ║
╚═══════════════════════════════════════════════════════╝

Status Word: 0x911E
Result: STILL 0x911E (Length Error)
```

#### Bij FALEN (0x917E):
```
Result: 0x917E (MAC Error) - structure may be correct!
```
⚠️ **LET OP:** 0x917E betekent dat de APDU structuur waarschijnlijk **correct** is, maar de cryptografie mismatch heeft!

---

## 📊 Test Matrix

Voer systematisch alle tests uit:

| Test | Command | Expected Result | Actual SW | Notes |
|------|---------|----------------|-----------|-------|
| test0 | `test0` | Baseline (0x911E) | _____ | Current impl |
| test1 | `test1` | ? | _____ | ISO-wrapped |
| test2 | `test2` | ? | _____ | No Le |
| test3 | `test3` | ? | _____ | CmdCtr=0 |
| test4 | `test4` | ? | _____ | Wrapped+NoLe |
| test5 | `test5` | ? | _____ | NoLe+CmdCtr0 |

### Vul resultaten in:
- **0x9100** = SUCCESS ✅
- **0x911E** = Length Error (zoals baseline)
- **0x917E** = MAC Error (structuur OK, crypto issue!)
- **0x91xx** = Andere error

---

## 🔍 Interpretatie Resultaten

### Scenario A: Test geeft 0x9100 (Success)
✅ **OPGELOST!**
- Deze APDU configuratie is correct
- Implementeer deze variant in productie code
- Update `changeKey()` functie met werkende configuratie

### Scenario B: Test geeft 0x917E (MAC Error)
⚠️ **STRUCTUUR CORRECT, CRYPTO ISSUE**
- APDU length en format zijn goed
- Probleem ligt in:
  - MAC berekening details
  - IV berekening
  - Encryption padding
- Focus op crypto layer, niet APDU structuur

### Scenario C: Test geeft 0x911E (Length Error)
❌ **NIET OPGELOST**
- Deze configuratie lost het niet op
- Probeer volgende test
- Als ALLE tests 0x911E geven → contact NXP support

### Scenario D: Test geeft andere error (0x91xx)
🔍 **NIEUWE INFORMATIE**
- Noteer de status word
- Check AN12196 voor betekenis
- Mogelijk nieuwe hypothese

---

## 🎯 Volgende Stappen

### Als test slaagt (0x9100):
1. **Documenteer welke test werkt**
   ```
   Test X (beschrijving) oplost de issue!
   ```

2. **Update productie code**
   - Pas `changeKey()` functie aan
   - Gebruik werkende APDU configuratie
   - Verwijder test code (optioneel)

3. **Verify met echte key change**
   - Test met factory → derived key
   - Test met derived → nieuwe derived key
   - Verify authenticatie werkt na key change

### Als ALLE tests falen (0x911E):
1. **Contact NXP Support**
   - Stuur complete test results
   - Vermeld firmware versie: 04 04 02 30 00 11 05
   - Vraag naar undocumented quirks

2. **Check hardware**
   - Test met andere NTAG424 DNA kaart
   - Verify PN5180 firmware versie
   - Test met NXP reference reader (indien beschikbaar)

3. **Advanced debugging**
   - Capture raw APDU met logic analyzer
   - Compare met NXP reference implementation
   - Check ISO-DEP layer timing

---

## 📝 Test Log Template

```
NTAG424 DNA ChangeKey Test Results
==================================
Datum: __________
Hardware:
  - ESP32: Seeed XIAO ESP32S3
  - Reader: PN5180 (firmware: _______)
  - Card: NTAG424 DNA (firmware: 04 04 02 30 00 11 05)
  - Card UID: __________________________

Test Results:
-------------
test0: SW=0x____ [SUCCESS/FAIL] - ________________________
test1: SW=0x____ [SUCCESS/FAIL] - ________________________
test2: SW=0x____ [SUCCESS/FAIL] - ________________________
test3: SW=0x____ [SUCCESS/FAIL] - ________________________
test4: SW=0x____ [SUCCESS/FAIL] - ________________________
test5: SW=0x____ [SUCCESS/FAIL] - ________________________

Conclusie:
---------
☐ OPGELOST: Test ___ (beschrijving) werkt!
☐ NIET OPGELOST: Alle tests geven 0x911E
☐ NIEUW INZICHT: _________________________________

Volgende actie:
--------------
_______________________________________________________
_______________________________________________________
```

---

## 🆘 Troubleshooting

### "No card detected"
- Verifieer kaart ligt op reader
- Check PN5180 connections
- Restart ESP32

### "ISO-DEP activation failed"
- Kaart is geen NTAG424 DNA
- Reader error - check wiring
- Restart system

### "NDEF selection failed"
- Kaart is niet geformateerd
- Try factory fresh card
- Check card type

### "Authentication failed"
- Kaart heeft geen factory key meer
- Gebruik test kaart
- Of pas `oldKey` aan in test code

### Test hangt / crash
- Serial buffer vol - reset ESP32
- Memory issue - check heap
- Restart en probeer opnieuw

---

## 📚 Referenties

- **AN12196**: NTAG424 DNA Contact and Contactless Multi-Application IC with AES Authentication Security
  - Section 9.1.4: IV Calculation for Secured Messaging
  - Section 11.6.2: ChangeKey Command
  - Table 27: ChangeKey Native Mode Example

- **ISO/IEC 7816-4**: Smart card APDU structure
  - Case 3: Command with data, no response expected
  - Case 4: Command with data, response expected

- **Project Docs**:
  - `CHANGEKEY_TROUBLESHOOTING_SUMMARY.md` - Test history
  - `AN12196_case2_analysis.md` - Case 2 analysis
  - `DEVELOPER_HANDOFF.md` - Technical details

---

## ✅ Success Criteria

De test suite is succesvol als:
1. ✅ Minstens 1 test geeft 0x9100 (success)
2. ✅ Werkende configuratie is geïdentificeerd
3. ✅ Productie code kan worden geüpdatet
4. ✅ ChangeKey werkt consistent

---

**Veel succes met testen! 🚀**

*Bij vragen of problemen: documenteer in test log en overleg met team.*
