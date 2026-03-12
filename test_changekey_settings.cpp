/**
 * Test ChangeKeySettings Implementation
 * 
 * This file demonstrates testing the changeKeySettings() function
 * Copy the relevant parts to your main code for testing
 */

// ============================================================================
// Test 1: Check Current Settings
// ============================================================================
bool testGetCurrentSettings(NTAG424Handler& ntag424) {
    Serial.println(F("\n╔════════════════════════════════════════════════════════════╗"));
    Serial.println(F("║  TEST 1: Read Current Key Settings                        ║"));
    Serial.println(F("╚════════════════════════════════════════════════════════════╝"));
    
    // Authenticate first
    NTAG424Handler::AuthResult authResult;
    const uint8_t* testKey = NTAG424Handler::DEFAULT_AES_KEY;
    
    if (!ntag424.authenticateEV2First(0, testKey, authResult)) {
        Serial.println(F("❌ Authentication failed"));
        return false;
    }
    Serial.println(F("✅ Authenticated"));
    
    // Get current settings
    uint8_t settings, maxKeys;
    if (!ntag424.getKeySettings(settings, maxKeys)) {
        Serial.println(F("❌ GetKeySettings failed"));
        return false;
    }
    
    Serial.print(F("Current Settings: 0x"));
    Serial.println(settings, HEX);
    Serial.print(F("Max Keys: "));
    Serial.println(maxKeys);
    
    // Decode settings
    bool configChangeable = (settings & 0x80) != 0;
    bool configFrozen = (settings & 0x08) != 0;
    uint8_t changeKeyRights = settings & 0x07;
    
    Serial.println(F("\n[Decoded Settings]"));
    Serial.print(F("  Bit 7 - Config changeable: "));
    Serial.println(configChangeable ? F("YES") : F("NO (FROZEN)"));
    Serial.print(F("  Bit 3 - Config locked: "));
    Serial.println(configFrozen ? F("YES") : F("NO"));
    Serial.print(F("  Bits 2-0 - ChangeKey rights: 0x"));
    Serial.println(changeKeyRights, HEX);
    
    if (changeKeyRights == 0x0E) {
        Serial.println(F("  ⚠️  ChangeKey is FROZEN (niemand mag keys wijzigen)"));
    } else if (changeKeyRights == 0x0F) {
        Serial.println(F("  ✅ ChangeKey is FREE (iedereen mag keys wijzigen)"));
    } else if (changeKeyRights <= 0x04) {
        Serial.print(F("  🔑 Only key "));
        Serial.print(changeKeyRights);
        Serial.println(F(" may change keys"));
    }
    
    return true;
}

// ============================================================================
// Test 2: Open Settings (Set to 0xFF)
// ============================================================================
bool testOpenSettings(NTAG424Handler& ntag424) {
    Serial.println(F("\n╔════════════════════════════════════════════════════════════╗"));
    Serial.println(F("║  TEST 2: Open Settings (0xFF)                              ║"));
    Serial.println(F("╚════════════════════════════════════════════════════════════╝"));
    
    // Authenticate first
    NTAG424Handler::AuthResult authResult;
    const uint8_t* testKey = NTAG424Handler::DEFAULT_AES_KEY;
    
    if (!ntag424.authenticateEV2First(0, testKey, authResult)) {
        Serial.println(F("❌ Authentication failed"));
        return false;
    }
    Serial.println(F("✅ Authenticated"));
    
    // Try to open settings
    Serial.println(F("\nSending ChangeKeySettings(0xFF)..."));
    if (!ntag424.changeKeySettings(0xFF)) {
        Serial.println(F("❌ ChangeKeySettings failed"));
        Serial.println(F("   Mogelijke oorzaken:"));
        Serial.println(F("   - Settings zijn al frozen (bit 7 = 0)"));
        Serial.println(F("   - Geen permissie om settings te wijzigen"));
        return false;
    }
    
    Serial.println(F("✅ Settings opened to 0xFF"));
    
    // Verify
    uint8_t settings, maxKeys;
    if (ntag424.getKeySettings(settings, maxKeys)) {
        Serial.print(F("\nVerification - Current Settings: 0x"));
        Serial.println(settings, HEX);
        
        if (settings == 0xFF) {
            Serial.println(F("✅ VERIFIED: Settings = 0xFF (volledig open)"));
            return true;
        } else {
            Serial.println(F("⚠️  Settings niet zoals verwacht"));
            return false;
        }
    }
    
    return false;
}

// ============================================================================
// Test 3: Lock Settings (Set to 0x0E)
// ============================================================================
bool testLockSettings(NTAG424Handler& ntag424) {
    Serial.println(F("\n╔════════════════════════════════════════════════════════════╗"));
    Serial.println(F("║  TEST 3: Lock Settings (0x0E)                              ║"));
    Serial.println(F("║  ⚠️  WARNING: Dit is permanent! Keys kunnen daarna         ║"));
    Serial.println(F("║     NIET meer gewijzigd worden!                            ║"));
    Serial.println(F("╚════════════════════════════════════════════════════════════╝"));
    
    // ⚠️  SAFETY CHECK
    Serial.println(F("\n⚠️⚠️⚠️  DANGER ZONE  ⚠️⚠️⚠️"));
    Serial.println(F("This will PERMANENTLY lock key settings!"));
    Serial.println(F("Keys can NEVER be changed after this!"));
    Serial.println(F("Comment out the 'return false' to enable this test."));
    Serial.println();
    return false;  // SAFETY: Verwijder deze regel om test uit te voeren
    
    // Authenticate first
    NTAG424Handler::AuthResult authResult;
    const uint8_t* testKey = NTAG424Handler::DEFAULT_AES_KEY;
    
    if (!ntag424.authenticateEV2First(0, testKey, authResult)) {
        Serial.println(F("❌ Authentication failed"));
        return false;
    }
    Serial.println(F("✅ Authenticated"));
    
    // Lock settings
    Serial.println(F("\nSending ChangeKeySettings(0x0E)..."));
    if (!ntag424.changeKeySettings(0x0E)) {
        Serial.println(F("❌ ChangeKeySettings failed"));
        return false;
    }
    
    Serial.println(F("✅ Settings locked to 0x0E"));
    
    // Verify
    uint8_t settings, maxKeys;
    if (ntag424.getKeySettings(settings, maxKeys)) {
        Serial.print(F("\nVerification - Current Settings: 0x"));
        Serial.println(settings, HEX);
        
        if (settings == 0x0E) {
            Serial.println(F("✅ VERIFIED: Settings = 0x0E (volledig locked)"));
            Serial.println(F("   ⚠️  Keys kunnen nu NIET meer gewijzigd worden!"));
            return true;
        } else {
            Serial.println(F("⚠️  Settings niet zoals verwacht"));
            return false;
        }
    }
    
    return false;
}

// ============================================================================
// Test 4: Managed Mode (Set to 0x08)
// ============================================================================
bool testManagedMode(NTAG424Handler& ntag424) {
    Serial.println(F("\n╔════════════════════════════════════════════════════════════╗"));
    Serial.println(F("║  TEST 4: Set Managed Mode (0x08)                          ║"));
    Serial.println(F("║  Config frozen, maar master key kan nog keys wijzigen     ║"));
    Serial.println(F("╚════════════════════════════════════════════════════════════╝"));
    
    // Authenticate first
    NTAG424Handler::AuthResult authResult;
    const uint8_t* testKey = NTAG424Handler::DEFAULT_AES_KEY;
    
    if (!ntag424.authenticateEV2First(0, testKey, authResult)) {
        Serial.println(F("❌ Authentication failed"));
        return false;
    }
    Serial.println(F("✅ Authenticated"));
    
    // Set managed mode
    Serial.println(F("\nSending ChangeKeySettings(0x08)..."));
    if (!ntag424.changeKeySettings(0x08)) {
        Serial.println(F("❌ ChangeKeySettings failed"));
        return false;
    }
    
    Serial.println(F("✅ Settings set to managed mode (0x08)"));
    
    // Verify
    uint8_t settings, maxKeys;
    if (ntag424.getKeySettings(settings, maxKeys)) {
        Serial.print(F("\nVerification - Current Settings: 0x"));
        Serial.println(settings, HEX);
        
        if (settings == 0x08) {
            Serial.println(F("✅ VERIFIED: Managed mode active"));
            Serial.println(F("   - Config is frozen"));
            Serial.println(F("   - Alleen key 0 (master) mag keys wijzigen"));
            return true;
        } else {
            Serial.println(F("⚠️  Settings niet zoals verwacht"));
            return false;
        }
    }
    
    return false;
}

// ============================================================================
// Complete Personalization Flow Test
// ============================================================================
bool testCompletePersonalization(NTAG424Handler& ntag424, 
                                 const uint8_t* newMasterKey) {
    Serial.println(F("\n╔════════════════════════════════════════════════════════════╗"));
    Serial.println(F("║  TEST 5: Complete Personalization Flow                    ║"));
    Serial.println(F("╚════════════════════════════════════════════════════════════╝"));
    
    const uint8_t* oldKey = NTAG424Handler::DEFAULT_AES_KEY;
    
    // Step 1: Authenticate
    Serial.println(F("\n[1/4] Authenticating..."));
    NTAG424Handler::AuthResult authResult;
    if (!ntag424.authenticateEV2First(0, oldKey, authResult)) {
        Serial.println(F("❌ Auth failed"));
        return false;
    }
    Serial.println(F("✅ Authenticated"));
    
    // Step 2: Open settings
    Serial.println(F("\n[2/4] Opening settings (0xFF)..."));
    if (!ntag424.changeKeySettings(0xFF)) {
        Serial.println(F(" ℹ️  Settings already open or failed"));
        // Continue anyway - might already be open
    } else {
        Serial.println(F("✅ Settings opened"));
    }
    
    // Step 3: Change master key
    Serial.println(F("\n[3/4] Changing master key..."));
    if (!ntag424.changeKey(0, oldKey, newMasterKey)) {
        Serial.println(F("❌ Key change failed"));
        return false;
    }
    Serial.println(F("✅ Master key changed"));
    
    // Step 4: Lock settings
    Serial.println(F("\n[4/4] Locking settings (0x0E)..."));
    Serial.println(F("⚠️  SAFETY: Skipping lock for test - comment out return"));
    return true;  // SAFETY: Verwijder deze regel om daadwerkelijk te locken
    
    if (!ntag424.changeKeySettings(0x0E)) {
        Serial.println(F("❌ Lock failed"));
        return false;
    }
    Serial.println(F("✅ Settings locked"));
    
    // Verify
    uint8_t settings, maxKeys;
    if (ntag424.getKeySettings(settings, maxKeys)) {
        Serial.print(F("\n✅ Final settings: 0x"));
        Serial.println(settings, HEX);
    }
    
    Serial.println(F("\n╔════════════════════════════════════════════════════════════╗"));
    Serial.println(F("║  ✅ PERSONALIZATION COMPLETE                               ║"));
    Serial.println(F("╚════════════════════════════════════════════════════════════╝"));
    
    return true;
}

// ============================================================================
// Main Test Runner
// ============================================================================
void runChangeKeySettingsTests(NTAG424Handler& ntag424) {
    Serial.println(F("\n\n"));
    Serial.println(F("╔══════════════════════════════════════════════════════════════╗"));
    Serial.println(F("║                                                              ║"));
    Serial.println(F("║        NTAG424 ChangeKeySettings Test Suite                  ║"));
    Serial.println(F("║                                                              ║"));
    Serial.println(F("╚══════════════════════════════════════════════════════════════╝"));
    
    delay(2000);
    
    // Test 1: Check current settings
    testGetCurrentSettings(ntag424);
    delay(1000);
    
    // Test 2: Open settings (safe to test)
    testOpenSettings(ntag424);
    delay(1000);
    
    // Test 3: Lock settings (DANGEROUS - disabled by default)
    // Uncomment alleen als je weet wat je doet!
    // testLockSettings(ntag424);
    
    // Test 4: Managed mode
    testManagedMode(ntag424);
    delay(1000);
    
    Serial.println(F("\n\n╔══════════════════════════════════════════════════════════════╗"));
    Serial.println(F("║  Tests Complete                                              ║"));
    Serial.println(F("╚══════════════════════════════════════════════════════════════╝"));
}

/*
 * USAGE IN MAIN.CPP:
 * 
 * #include "test_changekey_settings.cpp"
 * 
 * void setup() {
 *     // ... initialize hardware, NFC, etc ...
 *     
 *     // Run tests
 *     runChangeKeySettingsTests(ntag424);
 * }
 */
