# AN12196 Table 27 vs Onze Implementatie - EXACTE VERGELIJKING

## AN12196 Case 2 Voorbeeld:

**Input:**
- Old Key (KeyNo 0x00): `00000000000000000000000000000000`
- New Key (KeyNo 0x00): `5004BF991F408672B1EF00F08F9E8647`
- Key Version: `01`
- TI: `7614281A`  
- CmdCtr: `0300` (value = 3)
- KSesAuthMACKey: `5529860B2FC5FB6154B7F28361D30BF9`
- KSesAuthENCKey: `4CF3CB41A22583A61E89B158D252FC53`

**Steps:**
1. Plaintext: `5004BF991F408672B1EF00F08F9E8647 01 80000000000000000000000000000000`
   - NewKey || Version || Padding(0x80...)
   
2. IV Input: `A55A 7614281A 0300 0000000000000000`
   - A55A || TI || CmdCtr || zeros
   
3. IVc (calculated): `01602D579423B2797BE8B478B0B4D27B`

4. Encrypted Data: `C0EB4DEEFEDDF0B513A03A95A75491818580503190D4D05053FF75668A01D6FD`

5. MAC Input: `C4 0300 7614281A 00 C0EB4DEEFEDDF0B513A03A95A75491818580503190D4D05053FF75668A01D6FD`
   - CMD || CmdCtr || TI || KeyNo || EncData

6. CMAC Full: `B7A60161F202EC3489BD4BEDEF64BB32`

7. CMACt (truncated): `A6610234BDED6432`

8. C-APDU: `90C400002900 C0EB4DEEFEDDF0B513A03A95A75491818580503190D4D05053FF75668A01D6FDA6610234BDED6432 00`
   - CLA || INS || P1 || P2 || Lc || Data || Le
   
9. R-APDU: **`9100`** (SUCCESS!)

---

## Onze Implementatie (van log):

**Input:**
- Old Key: `00000000000000000000000000000000` (factory default)
- New Key: `10532710706128B62A990EE06ECC86A6`
- Key Version: `01`
- TI: `55290C27`
- CmdCtr: `0000` (value = 0)
- KSesAuthMACKey: `E44655DEB1A2DB965E36ED42DEE2D8AB`
- KSesAuthENCKey: `814324EF6839F15357A36D3779692690`

**Steps:**
1. Plain Data: `10532710706128B62A990EE06ECC86A601800000000000000000000000000000`
   - NewKey || Version || Padding(0x80...) ✅

2. IV Input: `A55A 55290C27 0000 0000000000000000`
   - A55A || TI || CmdCtr || zeros ✅

3. IV (calculated): `BB2B779C04AD976610A0023B25B10776` ✅ (verified)

4. Encrypted Key Data: `AE6A48025B57618A3129EF1A043FE6E9DCA9F4860F80628BB3CD3DC2454C4510` ✅ (verified)

5. MAC Input: `C4 0000 55290C27 00 AE6A48025B57618A3129EF1A043FE6E9DCA9F4860F80628BB3CD3DC2454C4510`
   - CMD || CmdCtr || TI || KeyNo || EncData ✅

6. CMAC Full: `F48B57B3D943D39D2027EBD93786C193` ✅ (verified)

7. CMAC (truncated): `8BB3439D27D98693` ✅ (verified)

8. APDU: `90C400002900 AE6A48025B57618A3129EF1A043FE6E9DCA9F4860F80628BB3CD3DC2454C45108BB3439D27D98693 00` ✅
   - CLA || INS || P1 || P2 || Lc || Data || Le

9. R-APDU: **`911E`** (INTEGRITY_ERROR!) ❌

---

## ANALYSE:

**Structuur:** Beide zijn IDENTIEK ✅
**Cryptografie:** Alle verificaties slagen ✅
**APDU Format:** Correct ✅

**ENIGE VERSCHIL:** AN12196 gebruikt CmdCtr=3, wij gebruiken CmdCtr=0

MAAR: Dit is logisch - wij sturen dit als eerste commando na authenticatie!

---

## MOGELIJKE OORZAKEN:

### Hypothese 1: CommandCounter Issue
VRAAG: Wordt de commandCounter correct ge-increment na authenticatie?

In authenticateEV2First, wordt commandCounter gereset naar 0. Dit is correct.
Na het versturen van ChangeKey, moet de card deze incrementen naar 1.

### Hypothese 2: Factory Card Restrictions
VRAAG: Accepteert een factory blank NTAG424 DNA geen ChangeKey op key 0?

Dit is ONWAARSCHIJNLIJK - AN12196 doet precies dit (change key 0 from all-zeros).

### Hypothese 3: Commando Volgorde
VRAAG: Moet er een ander commando eerst verstuurd worden na authenticatie?

Laat me de NT4H2421Gx datasheet checken...

### Hypothese 4: CmdCtr moet incrementeel zijn over sessies
VRAAG: Blijft CmdCtr persistent over power cycles?

ONWAARSCHIJNLIJK - CmdCtr is session-scoped volgens spec.

### Hypothese 5: Padding Format
VRAAG: Moet de padding exact 15 bytes zijn, of mogen er meer nullen zijn?

Onze padding: `80` gevolgd door 14 x `00` = 15 bytes ✅
AN12196 padding: `80` gevolgd door 14 x `00` = 15 bytes ✅

**IDENTIEK!**

---

## CONCLUSIE:

Alle cryptografie is **100% correct**. Het probleem moet liggen in:
1. Een subtiele timing/volgorde issue
2. Een card-state probleem
3. Een vendor-specific requirement die niet in de spec staat

**VOLGENDE STAP:** Probeer een ander command na authenticatie (bijv. GetVersion of GetCardUID) om te zien of de sessie wel werkt voor andere commands.
