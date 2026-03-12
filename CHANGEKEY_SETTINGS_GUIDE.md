# ChangeKeySettings Implementation Guide

## ✅ Implementation Complete

De `changeKeySettings()` functie is nu volledig geïmplementeerd volgens AN12196 § 6.17.

## Functionaliteit

```cpp
bool NTAG424Handler::changeKeySettings(uint8_t settings);
```

### Werking
1. **Vereist authenticatie** - `authenticateEV2First()` moet eerst succesvol zijn
2. **Encrypteert** KeySettings byte + CRC32 + padding (16 bytes totaal)
3. **Berekent CMAC** over volledige command
4. **Stuurt commando** 0x54 naar kaart
5. **Update session state** (commandCounter, currentIV) na succes

## KeySettings Byte Betekenis (AN12196 Table 20)

| Bit | Naam | 1 = | 0 = |
|-----|------|-----|-----|
| 7 | Configuration changeable | Allowed | **Frozen** |
| 6 | AuthKey changeable without master | YES | Needs master |
| 5 | Free Directory List access | Public | Needs auth |
| 4 | Free Create/Delete files | Public | Needs master |
| 3 | Configuration frozen | **LOCKED** | Not frozen |
| 2-0 | ChangeKey rights | 0x0-0x4=key#, 0xE=**FROZEN**, 0xF=FREE |

### Belangrijke Waarden

#### 0xFF - Volledig Open (Setup Fase)
```
Bits: 1111 1111
- Bit 7 = 1: Configuration nog wijzigbaar
- Bit 3 = 1: Configuration NIET frozen
- Bits 2-0 = 111 (0xF): ChangeKey is FREE (iedereen mag)
```
**Gebruik:** Tijdens initiële personalisatie

#### 0x0E - Volledig Gelocked (Productie)
```
Bits: 0000 1110
- Bit 7 = 0: Configuration FROZEN (niet meer wijzigbaar)
- Bit 3 = 1: Configuration LOCKED
- Bits 2-0 = 110 (0xE): ChangeKey is FROZEN (niemand mag meer)
```
**Gebruik:** Na personalisatie, voor eindgebruiker

#### 0x08 - Config Frozen, ChangeKey Met Master
```
Bits: 0000 1000
- Bit 7 = 0: Configuration FROZEN
- Bit 3 = 1: Configuration LOCKED
- Bits 2-0 = 000 (0x0): Alleen master key (key0) mag ChangeKey
```
**Gebruik:** Voor beheerd systeem waar centrale server nog keys kan wijzigen

## Gebruik Scenario's

### Scenario 1: Personalisatie Flow

```cpp
// Stap 1: Authenticeer met factory default key
NTAG424Handler::AuthResult authResult;
if (!ntag424.authenticateEV2First(0, DEFAULT_KEY, authResult)) {
    Serial.println("Auth failed");
    return false;
}

// Stap 2: Open settings volledig (als nog niet open)
if (!ntag424.changeKeySettings(0xFF)) {
    Serial.println("Could not open settings");
    return false;
}

// Stap 3: Change keys (0-4)
if (!ntag424.changeKey(0, DEFAULT_KEY, NEW_MASTER_KEY)) {
    Serial.println("Master key change failed");
    return false;
}

// Stap 4: Change andere keys indien nodig
if (!ntag424.changeKey(1, DEFAULT_KEY, NEW_APP_KEY1)) {
    Serial.println("App key 1 change failed");
    return false;
}

// Stap 5: Lock settings volledig
if (!ntag424.changeKeySettings(0x0E)) {
    Serial.println("Could not lock settings");
    return false;
}

Serial.println("✅ Personalisatie compleet!");
```

### Scenario 2: Check Huidige Settings

```cpp
// Check wat de huidige settings zijn
uint8_t settings, maxKeys;
if (ntag424.getKeySettings(settings, maxKeys)) {
    Serial.print("Current settings: 0x");
    Serial.println(settings, HEX);
    
    // Decode settings
    bool configChangeable = (settings & 0x80) != 0;
    bool configFrozen = (settings & 0x08) != 0;
    uint8_t changeKeyRights = settings & 0x07;
    
    Serial.print("Config changeable: ");
    Serial.println(configChangeable ? "YES" : "NO (FROZEN)");
    Serial.print("Config locked: ");
    Serial.println(configFrozen ? "YES" : "NO");
    Serial.print("ChangeKey rights: 0x");
    Serial.println(changeKeyRights, HEX);
    
    if (changeKeyRights == 0x0E) {
        Serial.println("⚠️  ChangeKey is FROZEN - keys cannot be changed!");
    }
}
```

### Scenario 3: Managed System (Server Kan Keys Wijzigen)

```cpp
// Voor systemen waar centrale server beheer behoudt
if (!ntag424.changeKeySettings(0x08)) {
    Serial.println("Could not set managed mode");
    return false;
}

// Nu kan alleen key 0 (master) nog changeKey uitvoeren
// Config is wel frozen maar key wijzigingen zijn mogelijk via master
```

## Technische Details

### Command Format
```
Command Code: 0x54
Input:  [CMD:1] [EncryptedData:16] [MAC:8]
Output: SW status word only (9100 = success)
```

### EncryptedData Format (voor encryptie)
```
[KeySettings:1] [CRC32:4] [Padding:11]

Bijvoorbeeld voor 0xFF:
  FF              Settings byte
  70CBA71D        CRC32 over 0xFF (LSB first)
  80000000000000000000  Padding (0x80 + zeros)
```

### Encryption/MAC Process
1. **IV berekening**: `E(SesEncKey, A55A || TI || CmdCtr || 00...00)`
2. **Data encryptie**: `AES-CBC(SesEncKey, IV, plainData)`
3. **CMAC berekening**: `CMAC(SesMacKey, 54 || CmdCtr || TI || EncData)`

## Error Handling

### Authentication Required
```cpp
if (!ntag424.changeKeySettings(0xFF)) {
    // Check of authenticated is
    Serial.println("Error: Must authenticate first");
}
```

### Settings Frozen
Als de kaart settings frozen zijn (bit 7 = 0), geeft de kaart een error terug:
```
SW = 91CA  // Command not allowed (settings frozen)
```

### Permission Denied
Als ChangeKey rights niet voldoen:
```
SW = 919D  // Permission denied
```

## Verificatie

Na `changeKeySettings()` kun je verifiëren met `getKeySettings()`:

```cpp
// Set settings
ntag424.changeKeySettings(0x0E);

// Verify
uint8_t settings, maxKeys;
if (ntag424.getKeySettings(settings, maxKeys)) {
    if (settings == 0x0E) {
        Serial.println("✅ Settings correct: LOCKED");
    } else {
        Serial.print("⚠️  Unexpected settings: 0x");
        Serial.println(settings, HEX);
    }
}
```

## Volgorde Best Practices

### ✅ Correcte Volgorde
1. Authenticate met oude key
2. Open settings (0xFF) - indien nodig
3. Change keys (één voor één)
4. Lock settings (0x0E)
5. Verify met getKeySettings

### ❌ Foute Volgorde
- NIET eerst locken en dan keys proberen te wijzigen
- NIET authenticatie vergeten
- NIET commandCounter resetten tussen commando's

## Debug Output

De implementatie geeft uitgebreide debug output:
```
[CHANGE KEY SETTINGS]
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
Settings:       0xFF
Full APDU:      54[16 bytes encrypted][8 bytes MAC]
  [00] Cmd:     0x54
  [01-16] Enc:  [encrypted data]
  [17-24] MAC:  [CMAC]
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
```

## Referenties

- **AN12196**: NTAG 424 DNA and NTAG 424 DNA TagTamper features and hints
- **Section 6.17**: ChangeKeySettings Command
- **Table 20**: Key Settings Byte Format
- **Section 9.1**: Secure Messaging (EV2)

## Status

✅ **Volledig Geïmplementeerd**
- Encryptie volgens AN12196 § 9.1.4
- CMAC volgens AN12196 § 9.1.9
- Session state management (commandCounter, IV)
- Error handling
- Debug logging
