# Complete ChangeKey Analyse - AN12196 Table 27 vs Implementatie

## AN12196 Example (Case 2: KeyNo == AuthKey)

### Input Parameters (Step 1-9):
- Old Key: `00000000000000000000000000000000`
- New Key: `5004BF991F408672B1EF00F08F9E8647`
- Key Version: `01`
- TI (Transaction ID): `7614281A` (MSB first, 4 bytes)
- CmdCtr (Command Counter): `0300` (value = 3)
- SessionMACKey: `5529860B2FC5FB6154B7F28361D30BF9`
- SessionENCKey: `4CF3CB41A22583A61E89B158D252FC53`

### Step 10: Padding
Padding = `800000000000000000000000000000`

### Step 11: Plaintext
Plaintext = NewKey || KeyVer || Padding
= `5004BF991F408672B1EF00F08F9E8647` || `01` || `800000000000000000000000000000`
= `5004BF991F408672B1EF00F08F9E86470180000000000000000000000000000000`

### Step 8: Current IV Input
**BELANGRIJK**: Dit is de INPUT voor IV berekening, niet de IV zelf!
IV_Input = A55A || TI || CmdCtr || Padding
= `A55A` || `7614281A` || `0300` || `0000000000000000`
= `A55A7614281A03000000000000000000`

**ANALYSE BYTE ORDER:**
- `A55A` = Label (2 bytes)
- `7614281A` = TI in MSB-first (4 bytes) ✓
- `0300` = CmdCtr - IS DIT LSB of MSB?
  - Als waarde 3 in LSB-first: bytes zijn 03 00 ✓
  - Als waarde 3 in MSB-first: bytes zijn 00 03 ✗

**CONCLUSIE**: CmdCtr in IV input is **LSB first** ✓

### Step 12: IVc (Calculated IV)
IVc = E(SessionENCKey, IV_Input)
= E(`4CF3CB41A22583A61E89B158D252FC53`, `A55A7614281A03000000000000000000`)
= `01602D579423B2797BE8B478B0B4D27B`

### Step 13: Encrypted Data
E(SessionENCKey, IVc, Plaintext)
= `C0EB4DEEFEDDF0B513A03A95A75491818580503190D4D05053FF75668A01D6FD`

### Step 14: MAC Input
**CRUCIALE ANALYSE**: Wat is de byte order van CmdCtr in MAC input?

MAC_Input = Cmd || CmdCtr || TI || CmdHeader || EncData

De specificatie toont:
`C403007614281A00C0EB4DEEFEDDF0B513A03A95A75491818580503190D4D05053FF75668A01D6FD`

Laten we dit parseren:
- `C4` = CMD (1 byte)
- `0300` = CmdCtr (2 bytes)
- `7614281A` = TI (4 bytes)
- `00` = CmdHeader/KeyNo (1 byte)
- Rest = Encrypted data

**ANALYSE**: CmdCtr waarde = 3
- Hex string toont: `03 00`
- Dit betekent: byte[0]=0x03, byte[1]=0x00
- LSB first: laagste byte (03) eerst ✓
- MSB first: hoogste byte (00) eerst ✗

**CONCLUSIE**: CmdCtr in MAC input is **LSB first**, NIET MSB first!

### Step 15-16: CMAC
Full CMAC = `B7A60161F202EC3489BD4BEDEF64BB32`
CMACt = `A6610234BDED6432`

**TRUNCATIE ANALYSE:**
Full CMAC bytes: B7 A6 01 61 F2 02 EC 34 89 BD 4B ED EF 64 BB 32
Indices:         0  1  2  3  4  5  6  7  8  9  10 11 12 13 14 15

CMACt bytes: A6 61 02 34 BD ED 64 32
Dit zijn indices: 1, 3, 5, 7, 9, 11, 13, 15 ✓

**CONCLUSIE**: CMAC truncatie neemt ODD indices (1,3,5,7,9,11,13,15) in volgorde ✓

## FOUTEN GEVONDEN:

### ❌ FOUT 1: CmdCtr byte order in CMAC was VERKEERD GEFIXED!
**HUIDIGE CODE (FOUT):**
```cpp
macInput[1] = (commandCounter >> 8) & 0xFF;  // MSB first - FOUT!
macInput[2] = commandCounter & 0xFF;
```

**CORRECTE CODE:**
```cpp
macInput[1] = commandCounter & 0xFF;         // LSB first
macInput[2] = (commandCounter >> 8) & 0xFF;
```

### ✓ CORRECT: CmdCtr in IV input is LSB first
```cpp
ivInput[6] = commandCounter & 0xFF;        // LSB first ✓
ivInput[7] = (commandCounter >> 8) & 0xFF;
```

### ✓ CORRECT: CMAC truncatie
```cpp
mac[0] = T[1];  // Indices 1,3,5,7,9,11,13,15 ✓
mac[1] = T[3];
// etc.
```

### ✓ CORRECT: Session Key Derivation
- Gebruikt NIST SP 800-108 KDF met correcte Session Vector ✓

### ✓ CORRECT: Plaintext formatting
```cpp
memcpy(plainData, newKey, 16);      // NewKey ✓
plainData[16] = 0x01;               // Version ✓
plainData[17] = 0x80;               // Padding ✓
memset(plainData + 18, 0x00, 14);   // Zeros ✓
```

## CONCLUSIE:
De LAATSTE "fix" voor CmdCtr byte order in CMAC was VERKEERD en moet worden teruggedraaid!
CmdCtr is **ALTIJD LSB first**, zowel in IV input als in MAC input!
