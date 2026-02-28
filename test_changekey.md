# ChangeKey Test met AN12196 Example Data

## Input Data (uit gebruiker log):
```
Session ENC Key: E19ACDDAD6ACD2273BD326DF66CCDD57
Session MAC Key: 8A990749D1B24A3CB686F1FFBD718DDC  
Transaction ID: BB28674F
CmdCtr: 0
New Key: 10532710706128B62A990EE06ECC86A6
```

## Stap-voor-stap berekening:

### 1. Plaintext (CORRECT ✓):
```
NewKey: 10532710706128B62A990EE06ECC86A6
Version: 01
Padding: 800000000000000000000000000000
Result: 10532710706128B62A990EE06ECC86A601800000000000000000000000000000
```
**Gebruiker log**: `Plain Data: 10532710706128B62A990EE06ECC86A601800000000000000000000000000000` ✓

### 2. IV Input (CORRECT ✓):
```
A5 5A || BB28674F || 0000 || 0000000000000000
= A55ABB28674F00000000000000000000
```

### 3. IV =  E(SessionENCKey, IV_Input):
```
E(E19ACDDAD6ACD2273BD326DF66CCDD57, A55ABB28674F00000000000000000000)
= ?
```
**Gebruiker log**: `IV: 1901810B28DD4F1188B9679FCCD25931`

### 4. Encrypted Data = E(SessionENCKey, IV, PlainData):
```
E(E19ACDDAD6ACD2273BD326DF66CCDD57, 1901810B28DD4F1188B9679FCCD25931, plaintext)
= ?
```
**Gebruiker log**: `Encrypted Key Data: 9D12925E36422805EAC2285BC45E8BA557E7D63ADBE9E763ADC09DF67AE6007A` 

### 5. MAC Input:
```
C4 || 0000 || BB28674F || 00 || 9D12925E36422805EAC2285BC45E8BA557E7D63ADBE9E763ADC09DF67AE6007A
= C40000BB28674F009D12925E36422805EAC2285BC45E8BA557E7D63ADBE9E763ADC09DF67AE6007A
```

### 6. CMAC = CMACFull(SessionMACKey, MAC_Input):
```
CMAC_Full = CMAC(8A990749D1B24A3CB686F1FFBD718DDC, C40000BB28674F009D12...)
```

### 7. CMACt (truncated):
```
Take bytes at indices 1,3,5,7,9,11,13,15 from CMAC_Full
```
**Gebruiker log**: `CMAC: 88834E91169B91DB`

## Probleem:
De kaart retourneert SW=911E (INTEGRITY_ERROR), wat betekent dat de CMAC die we berekenen NIET overeenkomt met wat de kaart verwacht!

## Mogelijke oorzaken:
1. ❓ Session keys zijn verkeerd afgeleid
2. ❓ MAC input formatting is verkeerd  
3. ❓ CMAC berekening is verkeerd
4. ❓ CMAC truncatie is verkeerd

## Test benodigd:
Ik moet de EXACTE AN12196 waarden gebruiken om te verifiëren dat onze crypto implementatie correct werkt!
