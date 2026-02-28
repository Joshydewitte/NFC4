# Session Vector Constructie Check

## Specificatie (NT4H2421Gx datasheet):

```
SV1 = A5h||5Ah||00h||01h||00h||80h||RndA[15..14]||
      (RndA[13..8] # RndB[15..10])||RndB[9..0]||RndA[7..0]

SV2 = 5Ah||A5h||00h||01h||00h||80h||RndA[15..14]||
      (RndA[13..8] # RndB[15..10])||RndB[9..0]||RndA[7..0]
```

## Huidige implementatie analyse:

```cpp
// Build SV1 for Encryption Key
uint8_t sv1[32];
sv1[0] = 0xA5;  // ✓
sv1[1] = 0x5A;  // ✓
sv1[2] = 0x00;  // Counter ✓
sv1[3] = 0x01;  // ✓
sv1[4] = 0x00;  // Length ✓
sv1[5] = 0x80;  // ✓

// Context (26 bytes):
// RndA[15..14]
sv1[6] = rndA[15];  // ✓
sv1[7] = rndA[14];  // ✓

// RndA[13..8] XOR RndB[15..10] (6 bytes)
for (int i = 0; i < 6; i++) {
    sv1[8 + i] = rndA[13 - i] ^ rndB[15 - i];
}
```

**PROBLEEM GEVONDEN!**

Loop iteratie:
- i=0: sv1[8] = rndA[13] XOR rndB[15]  ✓
- i=1: sv1[9] = rndA[12] XOR rndB[14]  ✓
- i=2: sv1[10] = rndA[11] XOR rndB[13]  ✓
- i=3: sv1[11] = rndA[10] XOR rndB[12]  ✓
- i=4: sv1[12] = rndA[9] XOR rndB[11]   ✓
- i=5: sv1[13] = rndA[8] XOR rndB[10]   ✓

Dit klopt!

```cpp
// RndB[9..0] (10 bytes)
for (int i = 0; i < 10; i++) {
    sv1[14 + i] = rndB[9 - i];
}
```

Loop iteratie:
- i=0: sv1[14] = rndB[9]  ✓
- i=1: sv1[15] = rndB[8]  ✓
- i=2: sv1[16] = rndB[7]  ✓
- i=3: sv1[17] = rndB[6]  ✓
- i=4: sv1[18] = rndB[5]  ✓
- i=5: sv1[19] = rndB[4]  ✓
- i=6: sv1[20] = rndB[3]  ✓
- i=7: sv1[21] = rndB[2]  ✓
- i=8: sv1[22] = rndB[1]  ✓
- i=9: sv1[23] = rndB[0]  ✓

Dit klopt!

```cpp
// RndA[7..0] (8 bytes)
for (int i = 0; i < 8; i++) {
    sv1[24 + i] = rndA[7 - i];
}
```

Loop iteratie:
- i=0: sv1[24] = rndA[7]  ✓
- i=1: sv1[25] = rndA[6]  ✓
- i=2: sv1[26] = rndA[5]  ✓
- i=3: sv1[27] = rndA[4]  ✓
- i=4: sv1[28] = rndA[3]  ✓
- i=5: sv1[29] = rndA[2]  ✓
- i=6: sv1[30] = rndA[1]  ✓
- i=7: sv1[31] = rndA[0]  ✓

Dit klopt!

## CONCLUSIE:
Session Vector constructie is CORRECT! ✓

Het probleem moet ergens anders zitten...
