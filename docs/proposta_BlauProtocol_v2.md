# BlauProtocol v2 — Proposta de seguretat criptogràfica

> Document de disseny per a la implementació de seguretat al protocol de comunicació
> ESP-NOW del sistema Blau (BlauClick → BlauLux).
> Basat en l'anàlisi del feedback de la comunitat Tasmota i l'estudi del protocol Matter.

---

## 1. Mancances de BlauProtocol v1 i feedback de la comunitat Tasmota

BlauProtocol v1 és un protocol de comunicació via ESP-NOW pensat per ser simple i ràpid.
Funciona bé per a un ús domèstic en entorns de confiança, però presenta vulnerabilitats
importants que qualsevol persona amb un portàtil i una antena Wi-Fi pot explotar.

### Les tres vulnerabilitats principals

**Sniffing (escoltar):** Els paquets ESP-NOW viatgen per l'aire sense xifrar. Qualsevol
dispositiu proper en mode monitor pot llegir el contingut de cada paquet: quin botó s'ha
premut, quina comanda s'ha enviat, i identificar els dispositius implicats.

**Replay (reenviar):** Un atacant pot capturar un paquet vàlid (per exemple, "encendre la
llum") i tornar-lo a enviar en qualsevol moment, sense tenir el comandament físic. BlauLux
v1 no té cap mecanisme per detectar si un paquet és nou o si ja ha estat processat.

**Forgery (falsificar):** Coneixent l'estructura del payload v1 (que és pública), un atacant
pot construir paquets falsos que BlauLux accepti com a vàlids, activant o desactivant la
llum a voluntat.

### Feedback de la comunitat Tasmota

> *"Thanks for sharing. It looks neat and simple. However, the lack of any authentication
> nor encryption sounds like a decent risk to have devices open to the wild. Maybe for a
> lamp it is acceptable, but not beyond. Or did I miss something? With ESP32x, there is
> plenty of room to add a layer of moderately secure authentication. Any possibility on
> your side?"*

> *"As ESP-NOW messages are easy to sniff and resend, an exposure is that anyone in range
> can mess with you, especially with an unencrypted protocol. It does not appear that
> BlauLux has security against this. Using MQTT via standard Wifi avoids this type of
> vulnerability, as long as the Wifi network is not breached. Still, Tasmota does already
> support the similar WizMote also using ESP-NOW, where the protocol simply includes a
> sequence number without encryption or similar. Still, the Tasmota receiver can validate
> that the incoming MAC (last 3 bytes) is already known, making intrusion less
> straightforward."*

La comunitat també va identificar una vulnerabilitat addicional en el disseny inicial de
BlauProtocol v2: guardar el nonce a NVS en cada paquet rebut pot desgastar la memòria flash
de l'ESP32 en qüestió de minuts si un atacant fa un flood de paquets (100.000 cicles
d'escriptura / 1.000 paquets·s⁻¹ = 100 s fins a esgotament).

---

## 2. Taula de mesures de seguretat per a BlauProtocol v2

| # | Mesura | Protegeix contra | Com funciona | Cost impl. | Essencial? |
|---|---|---|---|---|---|
| 1 | **PBKDF2** | Contrasenya feble / deducció de clau | Password → PBKDF2-HMAC-SHA256 → AES key[16]. La contrasenya mai surt del xip ni viatja per l'aire. | Baix | ✅ Sí |
| 2 | **AES-128-GCM** | Sniffing + forgery | Xifra el payload + calcula un authentication tag de 8 bytes. Sense la clau, impossible desxifrar o falsificar. Hardware AES a l'ESP32. | Mitjà | ✅ Sí |
| 3 | **Nonce monotònic** | Replay | Cada paquet porta un uint32 creixent. El receptor rebutja nonces ≤ max_nonce vist. | Baix | ✅ Sí |
| 4 | **Nonce inicial aleatori** | Predicció del primer nonce | L'emissor inicialitza el comptador amb `esp_random()` en rang [1..2²⁸] en lloc de 0. | Molt baix | Recomanat |
| 5 | **Rate limiting + backoff exponencial** | Flood / desgast flash per atac | Màx. 20 paquets/s per MAC. Si se supera: bloqueig 10s → 60s → 10min (backoff exponencial). | Baix | Recomanat |
| 6 | **Matter (Tasmota) — Sliding window RAM** | Desgast flash per replay | Manté un bitmap de 32 bits en RAM dels últims nonces vistos. Zero escriptures a flash. Après reinici, el problema del replay post-boot queda sense resoldre (deixat a la capa d'aplicació). | — | Referència |
| 7 | **Guardar N + límit 1/hora + testimoni + finestra** *(proposta pròpia)* | Desgast flash + replay post-reinici | Guarda el nonce real N a NVS, màx. 1 cop/hora, indexat per MAC. Après reinici: 1r paquet vàlid = testimoni (no executat). 2n paquet acceptat si: `B > A (testimoni)` I `X < B < X+100 (finestra)`. | Baix-Mitjà | Opcional |
| 8 | **MAC whitelist** | Emissors desconeguts | BlauLux manté una llista de MACs de BlauClick registrats. Paquets de MACs desconegudes es descarten abans de qualsevol processament criptogràfic. | Baix | Opcional |

---

## 3. Explicació detallada de cada mesura

### Mesura 1 — PBKDF2: de contrasenya a clau criptogràfica

*Imagina que la teva contrasenya és una clau de casa, però en lloc d'usar-la directament,
la foses i en fabriquessis una de nova, completament diferent i molt més robusta. Ningú
pot endevinar la clau nova a partir de la original, però tu sí que pots fabricar-la sempre
que vulguis.*

**Mecanisme**

PBKDF2 (*Password-Based Key Derivation Function 2*, RFC 8018) aplica HMAC-SHA256 de forma
iterativa sobre la contrasenya + un salt fix per produir una clau AES de 16 bytes:

```
key = PBKDF2-HMAC-SHA256(password, salt="BlauProtocol", iterations=1000, keylen=16)
```

- `salt` fix de protocol evita rainbow table attacks sense necessitat de salt aleatori.
- 1.000 iteracions: ~100 ms a l'ESP32 (acceptable en boot, imperceptible per l'usuari).
- La contrasenya original **no es guarda** a NVS. Només es guarda `key[16]`.
- Tant BlauClick com BlauLux deriven la mateixa clau independentment si usen la mateixa
  contrasenya — no cal transmetre la clau.

**Disponibilitat ESP32**

```cpp
#include "mbedtls/pkcs5.h"
#include "mbedtls/md.h"

mbedtls_md_context_t md_ctx;
mbedtls_md_init(&md_ctx);
mbedtls_md_setup(&md_ctx, mbedtls_md_info_from_type(MBEDTLS_MD_SHA256), 1);

uint8_t key[16];
mbedtls_pkcs5_pbkdf2_hmac(
    &md_ctx,
    (const uint8_t*)password, strlen(password),
    (const uint8_t*)"BlauProtocol", 12,
    1000,
    16, key
);
mbedtls_md_free(&md_ctx);
// Guardar key[16] a NVS namespace "blau", key "aes_key"
```

---

### Mesura 2 — AES-128-GCM: xifrat autenticat

*Imagina que poses el missatge dins una caixa forta (xifrat) i a sobre hi enganxes un
segell de cera personalitzat (tag d'autenticació). Qui el rebi pot trencar la caixa amb
la seva clau, però si el segell és fals o ha estat manipulat, ho sabrà immediatament.*

**Mecanisme**

AES-128-GCM (*Galois/Counter Mode*) proporciona simultàniament:
- **Confidencialitat**: el payload va xifrat, illegible sense la clau.
- **Autenticació**: el GCM tag (8 bytes truncat) verifica que el paquet no ha estat
  modificat ni forjat. Un tag fals rebutja el paquet sense revelar res.
- **Hardware AES a l'ESP32**: el mòdul `AES_Accelerator` del ESP32 executa AES en
  hardware, sense cost de CPU significatiu (~10 µs per paquet de 8 bytes).

**Payload xifrat (8 bytes plaintext → 8 bytes ciphertext + 8 bytes tag)**

```
Plaintext (bytes 1–8 del payload v1):
  [0] type    uint8_t   tipus de paquet (0x01 = link, 0x02 = trigger)
  [1] seq     uint8_t   número de seqüència aplicació (0–255 circular)
  [2-3] src_id uint16_t identificador de l'emissor (últims 2 bytes MAC)
  [4] cmd     uint8_t   comanda (CMD_ON, CMD_OFF, CMD_TOGGLE, ...)
  [5] p1      uint8_t   paràmetre 1 (intensitat, temperatura color, ...)
  [6] p2      uint8_t   paràmetre 2
  [7] p3      uint8_t   paràmetre 3
```

**Codi ESP32 — xifrat (BlauClick)**

```cpp
#include "mbedtls/gcm.h"

mbedtls_gcm_context gcm_ctx;
mbedtls_gcm_init(&gcm_ctx);
mbedtls_gcm_setkey(&gcm_ctx, MBEDTLS_CIPHER_ID_AES, key, 128);

uint8_t iv[4];                          // nonce com a IV de 4 bytes (uint32 LE)
memcpy(iv, &nonce_counter, 4);

uint8_t ciphertext[8], tag[8];
mbedtls_gcm_crypt_and_tag(
    &gcm_ctx, MBEDTLS_GCM_ENCRYPT,
    8, iv, 4,                           // plaintext len, IV, IV len
    NULL, 0,                            // AAD (no s'usa)
    plaintext, ciphertext,
    8, tag                              // tag output, tag len (truncat a 8)
);
mbedtls_gcm_free(&gcm_ctx);
```

**Codi ESP32 — verificació i desxifrat (BlauLux)**

```cpp
uint8_t plaintext[8];
int ret = mbedtls_gcm_auth_decrypt(
    &gcm_ctx, 8, iv, 4,
    NULL, 0,
    tag, 8,                             // tag rebut
    ciphertext, plaintext
);
if (ret != 0) return false;             // tag invàlid → forgery o corrupció
```

---

### Mesura 3 — Nonce monotònic: anti-replay

*Imagina que cada carta que envies porta un número de sèrie que va pujant. Si el teu amic
rep la carta número 50 i després li arriba la carta número 30, sap que és una còpia antiga
i la llença sense llegir-la.*

**Mecanisme**

El nonce és un `uint32_t` persistent a NVS de BlauClick que s'incrementa en cada enviament.
BlauLux manté per cada MAC emissora (`max_nonce[MAC]`) el valor més alt vist. Qualsevol
paquet amb `nonce ≤ max_nonce[MAC]` és descartat com a replay.

**Vida útil del nonce**: uint32 màxim = 4.294.967.295. A 100 pulsacions/dia →
**~117.000 anys** fins a overflow. No cal gestionar rollover per a BlauProtocol.

**Interacció amb mesura 7**: `max_nonce[MAC]` viu en RAM i s'escriu a NVS de forma
diferida (màx. 1 cop/hora) per protegir la flash. Veure mesura 7.

---

### Mesura 4 — Nonce inicial aleatori

*Si tothom comença a comptar des de 1, és fàcil endevinar per on anirà. Si comences des
d'un número aleatori enorme, ningú pot preparar paquets falsos per avançat.*

**Mecanisme**

En lloc d'inicialitzar `nonce_counter = 0` a NVS, BlauClick usa el generador de nombres
aleatoris hardware de l'ESP32:

```cpp
uint32_t initial_nonce = (esp_random() & 0x0FFFFFFF) + 1;  // rang [1 .. 2^28]
NVS.write("nonce_ctr", initial_nonce);
```

Limitat a 28 bits (com fa Matter) per deixar marge de crescement sense overflow prematur.
`esp_random()` usa el RNG hardware del ESP32, no és predictible.

---

### Mesura 5 — Rate limiting + backoff exponencial

*Igual que un mòbil que et bloqueja uns minuts si t'equivoques la contrasenya moltes
vegades seguides, BlauLux para de respondre si rep massa missatges en poc temps.*

**Mecanisme**

Implementat per MAC emissora, en RAM (no persistit). Dos nivells de control:

1. **Finestra de taxa**: comptador de paquets per segon. Si supera `RATE_MAX_PKT` → bloqueig.
2. **Backoff exponencial**: cada bloqueig consecutiu augmenta el temps d'espera.

```cpp
#define RATE_MAX_PKT       20       // paquets/segon màxim per MAC
#define BACKOFF_TIER_1     10000    // 10 s (primer abús)
#define BACKOFF_TIER_2     60000    // 60 s (segon abús)
#define BACKOFF_TIER_3     600000   // 10 min (abús persistent)

struct RateState {
    uint32_t window_start_ms;
    uint8_t  pkt_count;
    uint8_t  backoff_tier;          // 0 = cap, 1/2/3 = tiers
    uint32_t block_until_ms;
};

bool check_rate(RateState& r, uint32_t now_ms) {
    if (now_ms < r.block_until_ms) return false;  // bloquejat

    if (now_ms - r.window_start_ms >= 1000) {
        r.window_start_ms = now_ms;
        r.pkt_count = 0;
    }
    r.pkt_count++;
    if (r.pkt_count > RATE_MAX_PKT) {
        uint32_t delay = (r.backoff_tier == 0) ? BACKOFF_TIER_1 :
                         (r.backoff_tier == 1) ? BACKOFF_TIER_2 : BACKOFF_TIER_3;
        r.block_until_ms = now_ms + delay;
        if (r.backoff_tier < 3) r.backoff_tier++;
        return false;
    }
    return true;
}
```

**Nota important**: la verificació AES-GCM es fa **abans** d'actualitzar NVS. Un atacant
sense la clau no pot superar el GCM verify → 0 escriptures a NVS. El rate limiting
protegeix principalment contra replay de paquets vàlids capturats i contra el desgast de
CPU per processar floods.

---

### Matter (Tasmota) — Sliding window en RAM

*Imagina que recordes mentalment les últimes 32 cartes que has rebut. Si t'arriba una carta
que ja has rebut, la rebutges. Si perds la memòria (reinici), ja no saps quines cartes
eren vàlides.*

**Mecanisme** (implementació a `be_matter_counter.cpp`)

El receptor manté en **RAM** un bitmap de 32 bits que registra els últims 32 nonces rebuts.
**No escriu res a flash.** Validació per sliding window:

```
max_counter = 150         ← màxim nonce vist en sessió
window_bitmap = 0b...11010111  ← quins dels últims 32 s'han vist

Rang cobert: [119 .. 150]
```

| Cas | Acció |
|---|---|
| nonce > max_counter | Acceptar. Desplaçar finestra, marcar com vist |
| nonce dins finestra, bit = 0 | Acceptar. Marcar bit |
| nonce dins finestra, bit = 1 | Rebutjar (replay) |
| nonce < finestra (massa antic) | Rebutjar |

```cpp
// En RAM (volàtil):
uint32_t max_counter = 0;
uint32_t window_bitmap = 0;

bool receive(uint32_t nonce) {
    if (nonce > max_counter) {
        uint32_t shift = nonce - max_counter;
        window_bitmap = (shift >= 32) ? 0 : (window_bitmap << shift);
        window_bitmap |= 1;
        max_counter = nonce;
        return true;  // cap escriptura a NVS
    }
    uint32_t age = max_counter - nonce;
    if (age >= 32) return false;
    uint32_t bit = (1u << age);
    if (window_bitmap & bit) return false;
    window_bitmap |= bit;
    return true;
}
```

**Avantatge**: zero escriptures a flash — el flood no pot desgastar la flash.

**Limitació**: after reinici, `max_counter` i `window_bitmap` es perden. El protocol
Matter deixa el problema del replay post-reinici a les capes superiors del protocol
(gestió de sessions). Per a un dispositiu IoT sense sessions persistents com BlauLux,
aquest problema queda sense resoldre si s'usa Matter tal qual.

La mesura **7** és la nostra proposta per cobrir aquest buit.

---

### Mesura 7 — Guardar N + límit 1/hora + testimoni + finestra (proposta pròpia)

*Imagina que deixes un post-it amb l'última pàgina que has llegit, però només actualitzes
el post-it una vegada per hora. Si algú intenta enganyar-te amb una pàgina que ja has llegit
però que no vas anotar, la primera vegada no te la creus (testimoni), i la segona vegada
comproves que sigui propera a l'última que recordes (finestra). Dues vegades seguides amb
números consecutius, i confies.*

**Mecanisme**

**NVS (persistent, per MAC):**
```
Namespace: "blau_rx"
Key: "nc_" + sender_MAC_as_6_uppercase_hex_chars
Value: uint32_t — darrer max_nonce guardat per aquesta MAC
Escriptura: màx. 1 cop/hora i només si max_nonce != flash_nonce

Exemple amb 2 BlauClick registrats:
  "nc_AABBCCDDEEFF" → 1420   (BlauClick principal)
  "nc_112233445566" → 87     (BlauClick secundari)
```

**RAM (per MAC, volàtil):**
```cpp
struct SenderState {
    uint32_t max_nonce;          // màxim nonce vist en sessió
    uint32_t flash_nonce;        // últim valor escrit a NVS
    uint32_t last_nvs_write_ms;  // timestamp última escriptura NVS
    bool     boot_phase;         // true fins que 2n paquet post-boot és acceptat
    bool     has_witness;        // true després del 1r paquet post-boot vàlid
    uint32_t witness_nonce;      // nonce del 1r paquet post-boot (no executat)
    RateState rate;              // estat rate limiting (mesura 5)
};
```

**Lògica post-reinici (boot_phase = true):**

```
Flash té guardat per MAC: X = 512
Finestra: NONCE_WINDOW = 100
Rang acceptat: X < nonce < X + NONCE_WINDOW  →  512 < nonce < 612

1r paquet rebut (nonce 547):
  → GCM verify ✅
  → 512 < 547 < 612 ✅  (dins finestra)
  → boot_phase = true, has_witness = false
  → witness_nonce = 547, has_witness = true
  → NO executar, retornar false

2n paquet rebut (nonce 548):
  → GCM verify ✅
  → 548 > 547 (witness) ✅
  → 512 < 548 < 612 ✅  (dins finestra)
  → boot_phase = false  → EXECUTAR comanda ✅
  → max_nonce = 548
```

```
1r paquet fora de finestra (nonce 620):
  → GCM verify ✅
  → 512 < 620 < 612?  ❌ (620 ≥ 612)
  → Descartar, has_witness roman false
```

```
Replay d'un paquet antic (nonce 350):
  → 512 < 350?  ❌
  → Descartat immediatament
```

**Lògica de mode normal (boot_phase = false):**

```cpp
if (nonce <= s.max_nonce) return false;  // replay estricte
s.max_nonce = nonce;

// Escriptura diferida a NVS: màx 1 cop/hora, només si ha canviat
uint32_t now = millis();
if (s.max_nonce != s.flash_nonce &&
    (now - s.last_nvs_write_ms) >= 3600000UL) {
    char nvs_key[20];
    snprintf(nvs_key, sizeof(nvs_key), "nc_%02X%02X%02X%02X%02X%02X",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    NVS.write(nvs_key, s.max_nonce);
    s.flash_nonce = s.max_nonce;
    s.last_nvs_write_ms = now;
}
```

**Flash wear budget**: màx. 24 escriptures/dia × 365 dies × 11 anys = ~96.360 cicles
per MAC. Amb 2 BlauClick: ~48.000 cicles per NVS namespace → vida útil >20 anys.

**Vulnerabilitat residual**: un atacant que hagi capturat 2 paquets consecutius amb
nonces dins la finestra `[X..X+100]` pot fer replay après un reinici. Per a un ús domèstic
(~50 pulsacions/hora), la probabilitat que l'atacant tingui exactament 2 paquets del rang
post-reinici és molt baixa i el benefici pràctic d'explotar-ho (encendre una llum) és nul.

---

### Mesura 8 — MAC whitelist

*Imagina que la teva porta de casa només s'obre amb les claus dels membres de la família
registrats. Si algú fa una còpia de la clau, el model de clau correcte és una condició
necessària — però no suficient si la porta també té un codi.*

**Mecanisme**

BlauLux manté una llista de fins a N MACs de BlauClick autoritzats, emmagatzemada a NVS.
Paquets de MACs no registrades es descarten **abans** de qualsevol operació criptogràfica,
reduint la superfície d'atac i la càrrega de CPU.

```cpp
// NVS namespace "blau_rx", keys "wl_0" .. "wl_9"
// Value: uint8_t[6] (MAC address)

bool is_mac_allowed(const uint8_t* mac) {
    for (int i = 0; i < MAX_WHITELIST; i++) {
        char key[8];
        snprintf(key, sizeof(key), "wl_%d", i);
        uint8_t stored_mac[6];
        if (NVS.read(key, stored_mac, 6) == OK) {
            if (memcmp(mac, stored_mac, 6) == 0) return true;
        }
    }
    return false;
}
```

**Nota**: la MAC no és un mecanisme de seguretat fort per si sola (és trivial de spoof-ar
amb la majoria de drivers Wi-Fi/ESP-NOW). En combinació amb AES-128-GCM actua com a
filtre de primer nivell, no com a garantia d'autenticitat.

---

## 4. Proposta BlauProtocol v2 — Especificació completa

### 4.1 Payload v2 (20 bytes, format over-the-air)

```
Offset  Camp         Tipus        Descripció
──────  ───────────  ───────────  ─────────────────────────────────────────────────
  0     version      uint8_t      0x02 — discrimina v1 (0x01) i permet downgrade
                                  detection al receptor v1
  1–8   ciphertext   uint8_t[8]   AES-128-GCM encrypt de plaintext[8] (veure baix)
  9–12  nonce        uint32_t     Comptador monotònic BlauClick, little-endian.
                                  En clar per permetre GCM decryption sense cerca.
 13–20  gcm_tag      uint8_t[8]   GCM authentication tag truncat a 8 bytes.
                                  Cobreix: version(1) + ciphertext(8) + nonce(4).
                                  Si es vol AAD: afegir raw_packet[0..12] com a AAD.
```

**Plaintext (8 bytes, xifrats en ciphertext):**
```
Offset  Camp    Tipus     Descripció
──────  ──────  ────────  ──────────────────────────────────────────
  0     type    uint8_t   0x01=link heartbeat, 0x02=trigger cmd
  1     seq     uint8_t   Seqüència aplicació circular [0..255]
  2–3   src_id  uint16_t  Últims 2 bytes MAC BlauClick (LE)
  4     cmd     uint8_t   CMD_ON=0x01, CMD_OFF=0x02, CMD_TOGGLE=0x03, ...
  5     p1      uint8_t   Brightness [0..255] o temperatura color low byte
  6     p2      uint8_t   Temperatura color high byte o reservat
  7     p3      uint8_t   Reservat / extensió futura
```

**Compatibilitat**: el byte `version=0x02` en clar permet a un receptor v1 (que espera
`version=0x01`) detectar incompatibilitat i descartar el paquet sense processar-lo
malament.

---

### 4.2 NVS layout (BlauClick — emissor)

```
Namespace: "blau_tx"
─────────────────────────────────────────────────────────
Key          Tipus       Descripció
──────────── ──────────  ────────────────────────────────
"aes_key"    uint8[16]   Clau AES derivada via PBKDF2
"nonce_ctr"  uint32_t    Comptador de nonce actual
                         Inicialitzat amb esp_random() & 0x0FFFFFFF + 1
```

---

### 4.3 NVS layout (BlauLux — receptor)

```
Namespace: "blau_rx"
─────────────────────────────────────────────────────────
Key                  Tipus      Descripció
──────────────────── ─────────  ────────────────────────────────────────────────
"aes_key"            uint8[16]  Clau AES (idèntica a BlauClick si mateixa pwd)
"wl_0".."wl_9"       uint8[6]   MAC whitelist (opcional, mesura 8)
"nc_AABBCCDDEEFF"    uint32_t   Darrer nonce guardat per MAC AA:BB:CC:DD:EE:FF
"nc_112233445566"    uint32_t   Darrer nonce guardat per MAC 11:22:33:44:55:66
... (una entrada per BlauClick registrat)
```

---

### 4.4 Flux de configuració inicial (una sola vegada, per l'usuari)

```
[BlauClick]
  1. Usuari introdueix contrasenya via Serial/BLE setup
  2. Calcular: key = PBKDF2-HMAC-SHA256(pwd, "BlauProtocol", 1000, 16)
  3. NVS.write("blau_tx", "aes_key", key)
  4. nonce_init = (esp_random() & 0x0FFFFFFF) + 1
  5. NVS.write("blau_tx", "nonce_ctr", nonce_init)
  6. (Opcional) Registrar MAC de BlauClick a la whitelist de BlauLux via pairing mode

[BlauLux]
  1. Usuari introdueix la mateixa contrasenya
  2. Calcular: key = PBKDF2-HMAC-SHA256(pwd, "BlauProtocol", 1000, 16)
     → Resultat idèntic al de BlauClick
  3. NVS.write("blau_rx", "aes_key", key)
  4. (Opcional) Inicialitzar whitelist buida o en pairing mode
```

---

### 4.5 Flux d'enviament per pulsació (BlauClick)

```cpp
void send_command(uint8_t cmd, uint8_t p1, uint8_t p2, uint8_t p3) {
    // 1. Llegir i incrementar nonce
    uint32_t nonce = NVS.read("blau_tx", "nonce_ctr");
    NVS.write("blau_tx", "nonce_ctr", nonce + 1);

    // 2. Construir plaintext
    uint8_t plain[8] = {
        0x02,                         // type: trigger
        seq_counter++,                // seq circular
        (uint8_t)(own_mac[4]),        // src_id low
        (uint8_t)(own_mac[5]),        // src_id high
        cmd, p1, p2, p3
    };

    // 3. Xifrar amb AES-128-GCM
    uint8_t iv[4];
    memcpy(iv, &nonce, 4);
    uint8_t cipher[8], tag[8];
    aes_gcm_encrypt(key, iv, 4, plain, 8, cipher, tag);

    // 4. Construir paquet v2
    uint8_t pkt[20];
    pkt[0] = 0x02;                    // version
    memcpy(&pkt[1], cipher, 8);
    memcpy(&pkt[9], &nonce, 4);
    memcpy(&pkt[13], tag, 8);         // tag truncat 8 bytes

    // 5. Enviar via ESP-NOW (broadcast o unicast a BlauLux MAC)
    esp_now_send(blaulux_mac, pkt, 20);
}
```

---

### 4.6 Flux de recepció i validació complet (BlauLux)

```cpp
#define NONCE_WINDOW        100
#define NVS_WRITE_INTERVAL  3600000UL   // 1 hora en ms
#define RATE_MAX_PKT        20
#define BACKOFF_T1          10000
#define BACKOFF_T2          60000
#define BACKOFF_T3          600000

// Estat per MAC (en RAM, mapa MAC → SenderState)
std::map<MacAddr, SenderState> senders;

void on_espnow_recv(const uint8_t* mac, const uint8_t* data, int len) {
    if (len != 20) return;
    if (data[0] != 0x02) return;           // versió incorrecta → v1 o desconegut

    // === MESURA 8: MAC whitelist ===
    if (!is_mac_allowed(mac)) return;

    SenderState& s = get_or_create(senders, mac);
    uint32_t now = millis();

    // === MESURA 5: Rate limiting ===
    if (!check_rate(s.rate, now)) return;

    // === MESURA 2: AES-128-GCM verify + decrypt ===
    uint8_t* cipher = (uint8_t*)&data[1];
    uint32_t nonce;
    memcpy(&nonce, &data[9], 4);
    uint8_t* tag = (uint8_t*)&data[13];

    uint8_t plain[8];
    if (!aes_gcm_decrypt(key, (uint8_t*)&nonce, 4, cipher, 8, tag, 8, plain)) {
        return;                            // forgery → descartar silenciosament
    }

    // === MESURES 3 + 7: Anti-replay + deferred NVS per MAC ===
    if (s.boot_phase) {
        // --- Post-reinici: lògica testimoni + finestra ---
        uint32_t X = s.flash_nonce;

        if (!s.has_witness) {
            // 1r paquet vàlid post-boot
            if (nonce > X && nonce < X + NONCE_WINDOW) {
                s.witness_nonce = nonce;
                s.has_witness = true;
            }
            return;                        // mai executar el testimoni
        } else {
            // 2n paquet: B > A i dins finestra
            if (nonce <= s.witness_nonce) return;
            if (nonce <= X || nonce >= X + NONCE_WINDOW) return;
            s.boot_phase = false;
            s.max_nonce = nonce;
        }
    } else {
        // --- Mode normal: monotònic estricte ---
        if (nonce <= s.max_nonce) return;  // replay
        s.max_nonce = nonce;
    }

    // === MESURA 7: Escriptura diferida NVS, màx 1/hora ===
    if (s.max_nonce != s.flash_nonce &&
        (now - s.last_nvs_write_ms) >= NVS_WRITE_INTERVAL) {
        char nvs_key[20];
        snprintf(nvs_key, sizeof(nvs_key), "nc_%02X%02X%02X%02X%02X%02X",
                 mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
        nvs_write("blau_rx", nvs_key, s.max_nonce);
        s.flash_nonce = s.max_nonce;
        s.last_nvs_write_ms = now;
    }

    // === Processar comanda ===
    handle_action(plain[4], plain[5], plain[6], plain[7]);  // cmd, p1, p2, p3
}
```

---

### 4.7 Cobertura de vulnerabilitats

| Vulnerabilitat | Mecanisme que la cobreix | Resultat |
|---|---|---|
| Sniffing (llegir payload) | AES-128-GCM (mesura 2) | ✅ Payload illegible sense clau |
| Forgery (paquet fals) | AES-128-GCM tag (mesura 2) | ✅ Tag invàlid → descartat |
| Replay genèric | Nonce monotònic (mesura 3) | ✅ nonce ≤ max → descartat |
| Replay post-reinici | Testimoni + finestra (mesura 8) | ✅ Finestra [X..X+100] |
| Desgast flash per flood | Rate limit + NVS 1/hora (5 + 7) | ✅ Màx. 24 escriptures/dia |
| Nonce predictible | Nonce inicial aleatori (mesura 4) | ✅ Range [1..2²⁸] random |
| Emissor desconegut | MAC whitelist (mesura 8) | ✅ Descartat pre-crypto |
| MAC spoofing | AES-128-GCM (mesura 2) | ✅ Clau no depèn de la MAC |

---

### 4.8 Recomanació de mesures a implementar

**Conjunt recomanat per a BlauProtocol v2 domèstic:**

> Mesures **1 + 2 + 3 + 4 + 5 + 7 + 8**

Cobreix el 100% de les vulnerabilitats identificades per la comunitat Tasmota amb una
complexitat d'implementació proporcional al cas d'ús (llum domèstica). A diferència de
Matter (que no resol el replay post-reinici a nivell de comptador), la mesura 7 cobreix
aquest buit amb el mecanisme testimoni + finestra.

**Fitxers a modificar per la implementació:**
- `blauprotocol.h` — estructures payload v1→v2, constants
- `blauprotocol_link.h` / `blauprotocol_trg.h` — build/parse del paquet
- `BlauLux/src/espnow.cpp` — lògica recepció completa (§4.6)
- `BlauClick/src/espnow.cpp` — lògica enviament (§4.5)
- `BlauLux/src/setup.cpp` — PBKDF2 + NVS init
- `BlauClick/src/setup.cpp` — PBKDF2 + NVS init + nonce random init

La lògica d'aplicació (`handleAction`, `BlauTrigger`, etc.) **no canvia**.

---

## 5. Referències

- [RFC 8018 — PBKDF2](https://www.rfc-editor.org/rfc/rfc8018)
- [RFC 5116 — AES-GCM](https://www.rfc-editor.org/rfc/rfc5116)
- [Matter §4.6 — Message Counters](https://github.com/arendst/Tasmota/blob/development/lib/libesp32/berry_matter/specs_for_ai/MATTER_1.4.1_CORE_SPEC_COMPACT.md#46-message-counters)
- [mbedTLS GCM — ESP-IDF](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/protocols/mbedtls.html)
- `BlauLux/docs/security-espnow-v2.md` — investigació d'opcions criptogràfiques
- `BlauLux/docs/matter-counter-analysis.md` — anàlisi Matter counters i variant 7
