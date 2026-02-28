# AN12196 Case 2 Analyse

## AN12196 Table 27: ChangeFileSettings Example (related to ChangeKey)

Het voorbeeld in AN12196 gebruikt **CommandCounter = 3**, niet 0!

Van AN12196 Table 27:
```
Step 14: Calculate MAC
MAC input = C4 03 00 76 14 28 1A 00 C0EB4DE...
```

Parsing:
- CMD: C4
- CmdCtr: 03 00 (value 3, LSB first)
- TI: 76 14 28 1A
- **Rest is encrypted data**

## PROBLEEM GEVONDEN!

In onze log staat:
```
MAC input = C4 00 00 55 29 0C 27 00 AE6A48...
```

Parsing:
- CMD: C4  ✓
- CmdCtr: 00 00 (value 0)  <-- Dit is CORRECT voor eerste command!
- TI: 55 29 0C 27  ✓
- KeyNo: 00  ✓
- Encrypted: AE6A48...  ✓

**WACHT!** Laat me AN12196 opnieuw lezen...

In AN12196 Table 27 staat dit NIET over ChangeKey, maar over **ChangeFileSettings**!

Laat me zoeken naar het echte ChangeKey voorbeeld...

## ChangeKey Specificatie (Section 11.6.2)

Voor EV2 authenticated state:
- Case 1: KeyNo ≠ AuthKeyNo → **XOR + CRC32**
- Case 2: KeyNo == AuthKeyNo → **NO CRC**!

Voor Case 2:
```
CmdData = E(KSesAuthENC, IVn, NewKey || Ver || 0x80 || padding)
```

Dit klopt met onze implementatie!

## Maar wacht...

Laat me de **commando volgorde** controleren. Misschien moet de key versie ANDERS zijn dan 0x01?

Of misschien moet de padding anders?

Laat me de exacte specification lezen over padding...
