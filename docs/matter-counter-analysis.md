# Anàlisi: Matter Message Counters aplicat a BlauProtocol v2

> Resposta de la comunitat Tasmota al nostre disseny de BlauProtocol v2.
> Fonts: [MATTER_1.4.1_CORE_SPEC §4.6](https://github.com/arendst/Tasmota/blob/development/lib/libesp32/berry_matter/specs_for_ai/MATTER_1.4.1_CORE_SPEC_COMPACT.md#46-message-counters)
> i [be_matter_counter.cpp](https://github.com/arendst/Tasmota/blob/development/lib/libesp32/berry_matter/src/be_matter_counter.cpp)

---

## 1. El problema que han identificat

Al nostre disseny de BlauProtocol v2 proposàvem:

> "El receptor guarda `last_nonce_seen` a NVS. Rebutja qualsevol paquet amb nonce ≤ last_nonce_seen."

El problema és que **guardar a NVS (flash) en cada paquet rebut és perillós**:

- La memòria flash de l'ESP32 té una vida útil de ~100.000 cicles d'escriptura per cel·la.
- Un atacant pot enviar milers de paquets falsos per segon (ESP-NOW no té rate limiting).
- Cada paquet fals forçaria una escriptura a NVS → la flash es desgastaria en pocs minuts o hores sota atac dirigit.

Exemple: a 1.000 paquets/segon → 100.000 cicles esgotats en **100 segons**.

---

## 2. Com ho resol Matter: sliding window + escriptura diferida

L'especificació Matter §4.6 i la seva implementació (`be_matter_counter.cpp`) utilitzen una estratègia en dues capes:

### 2.1 Sliding window bitmap en RAM

En lloc de validar únicament contra l'últim nonce vist, el receptor manté en **RAM** un registre dels últims N nonces rebuts com un **bitmap de 32 bits**:

```
max_counter = 150   ← el valor més alt vist fins ara
window_bitmap = 0b...11010111  ← bits = quins valors anteriors hem vist

Rang cobert: [150-31 .. 150] = [119 .. 150]
```

**Lògica de validació:**

| Cas | Acció |
|---|---|
| nonce > max_counter | Acceptar. Desplaçar la finestra, marcar com a vist |
| nonce dins la finestra i bit = 0 | Acceptar. Marcar bit |
| nonce dins la finestra i bit = 1 | Rebutjar (replay) |
| nonce < finestra (massa antic) | Rebutjar (massa lluny en el passat) |

Amb 32 bits de finestra, el receptor tolera **fins a 32 paquets fora d'ordre o perduts** sense falsos rebutjos.

### 2.2 Escriptura diferida a flash (ahead increment)

El truc clau per evitar el desgast és **no escriure a NVS en cada paquet**. En lloc d'això:

```
Quan el counter arriba a un nou màxim:
  Si (max_counter % SAVE_INTERVAL == 0):
    NVS ← max_counter + AHEAD_INCREMENT
```

On `SAVE_INTERVAL` = cada N missatges (ex: 1.000) i `AHEAD_INCREMENT` = un valor per davant (ex: 1.000).

**Efecte pràctic:**

- Flash s'escriu **1 vegada cada 1.000 missatges** → 100× menys desgast.
- Sota atac de 1.000 paquets/segon, la flash es desgastaria en ~100.000 segons (~27 hores) en lloc de 100 segons.
- Després d'un reinici, el receptor carrega el valor `max + AHEAD_INCREMENT` de NVS → acceptarà nonces futurs però rebutjarà tots els nonces antics que l'atacant pugui tenir capturats.

**Cost:** Després d'un reinici, els nonces del rang `[max_real .. max_real + AHEAD_INCREMENT]` es "perden" (el receptor els veurà com ja usats). L'emissor ha de continuar des del seu propi comptador persistent, que sol ser superior.

### 2.3 Variant 6B — Proposta pròpia (guardar N + escriptura màx. 1 cop/hora)

En lloc de guardar `N + AHEAD_INCREMENT`, es guarda el **nonce real rebut (N)**, i l'escriptura a flash es limita a **màxim un cop per hora** (i només si el valor ha canviat):

```
Nonce rebut: 100  → guarda 100 a flash  ← primera escriptura
Nonce rebut: 101–999  → no escriu (no ha passat 1 hora)
...1 hora després...
Nonce rebut: 512  → guarda 512 a flash  ← ha passat 1 hora, valor diferent
```

**Si es va la llum al nonce 612, i la flash té guardat 512:**
- BlauLux carrega 512
- Rep paquet #613 → 613 > 512 → ✅ acceptat

**Avantatge respecte a 6A (Matter):** no es "perden" nonces after reinici — qualsevol nonce per sobre del darrer guardat és acceptat.

**Únic risc:** després d'un tall de llum, els nonces enviats durant l'última hora (p.ex. #513–#612) podrien ser replay-ats. Per a una llum domèstica (50 pulsacions/hora), la finestra de replay és de ~50 nonces, el que és molt acceptable.

**Escriptures a flash:** màxim 24/dia → 100.000 cicles esgotats en **~11 anys** d'ús continu.

---

### 2.4 Inicialització aleatòria

Matter inicialitza el comptador amb un valor aleatori en el rang `[1 .. 2^28]`:

```cpp
counter = Crypto_DRBG(28 bits) + 1
```

Això evita que un atacant pugui predir el nonce inicial i preparar paquets falsos per avançat.

---

## 3. Aplicació a BlauProtocol v2

### Canvis al disseny original

| Aspecte | BlauProtocol v2 original | v2 millorat (Matter-style) |
|---|---|---|
| Validació anti-replay | `nonce > last_seen` | Sliding window 32-bit en RAM |
| Escriptura a NVS (receptor) | Cada paquet | Cada N paquets (N ≥ 100) |
| Valor guardat a NVS | `last_nonce_seen` | `max_counter + AHEAD_INCREMENT` |
| Inicialització counter (emissor) | 0 | Random `[1 .. 2^28]` |
| Tolerància a paquets fora d'ordre | Cap | Finestra de 32 paquets |

### Estructura del payload v2 — sense canvis

El payload continua sent **20 bytes**. El canvi és únicament en la **lògica de validació al receptor**, no en el format del paquet.

```
[0]    version (0x02)
[1–8]  ciphertext AES-128-GCM
[9–12] nonce (uint32, monotònic, inicialitzat aleatòriament)
[13–20] GCM tag (8 bytes)
```

### Pseudocodi receptor (BlauLux) actualitzat

```cpp
// A NVS (persistent):
//   nonce_ceiling  ← max_counter + AHEAD_INCREMENT (per defecte)

// En RAM:
uint32_t max_counter;
uint32_t window_bitmap;  // 32 bits

// A l'arrencada:
max_counter = NVS.read("nonce_ceiling");
window_bitmap = 0;

// En cada paquet rebut:
bool receive_packet(uint32_t nonce, uint8_t* ciphertext, uint8_t* tag) {
    // 1. Verificar autenticitat AES-GCM (primer, per descartar ràpid)
    if (!aes_gcm_verify(key, nonce, ciphertext, tag)) return false;

    // 2. Anti-replay: sliding window
    if (nonce > max_counter) {
        // Nou màxim: desplaçar finestra
        uint32_t shift = nonce - max_counter;
        window_bitmap = (shift >= 32) ? 0 : (window_bitmap << shift);
        window_bitmap |= 1;  // marcar el nou màxim com vist
        max_counter = nonce;

        // Escriptura diferida: només cada SAVE_INTERVAL
        if (max_counter >= nonce_ceiling) {
            nonce_ceiling = max_counter + AHEAD_INCREMENT;
            NVS.write("nonce_ceiling", nonce_ceiling);
        }
    } else {
        // Dins la finestra?
        uint32_t age = max_counter - nonce;
        if (age >= 32) return false;  // massa antic
        uint32_t bit = (1 << age);
        if (window_bitmap & bit) return false;  // replay
        window_bitmap |= bit;
    }

    // 3. Processar comanda
    handle_action(decrypt(ciphertext));
    return true;
}
```

**Valors recomanats per BlauLux:**
- `SAVE_INTERVAL`: no cal (s'escriu quan `max_counter >= nonce_ceiling`)
- `AHEAD_INCREMENT`: **1.000** → escriptura a NVS cada ~1.000 missatges vàlids
- Finestra: **32 bits** (32 missatges de profunditat, suficient per ESP-NOW)

---

## 4. Impacte en la resistència a atacs

| Escenari d'atac | v2 original | v2 Matter-style |
|---|---|---|
| Replay d'un paquet capturat | Bloquejat (nonce ≤ last) | Bloquejat (sliding window) |
| Flood de paquets falsos (wear-out) | Flash esgotada en ~100s | Flash esgotada en ~27h |
| Flood de paquets vàlids forjats | Impossible (sense la clau) | Impossible (sense la clau) |
| Reinici + replay de nonces antics | Vulnerable si AHEAD = 0 | Bloquejat (ceiling guarda +1000) |

> **Nota:** La resistència al flood és millor, però no infinita. Per a un ús domèstic de llums, la finestra de 27h és més que suficient — un atacant hauria de mantenir el flood activament durant hores per aconseguir alguna cosa, sense cap benefici pràctic.

---

## 5. Conclusió

La comunitat Tasmota té raó: el disseny original de BlauProtocol v2 tenia una vulnerabilitat de desgast de flash. La solució que proposen (basada en com funciona el protocol Matter) és:

1. **Sliding window en RAM** per validar anti-replay sense escriure a flash en cada missatge.
2. **Escriptura diferida a NVS** del valor `max_counter + AHEAD_INCREMENT` en lloc del valor exacte.
3. **Inicialització aleatòria del nonce** a l'emissor per evitar predictibilitat.

Aquests tres canvis no afecten el format del paquet (segueix sent 20 bytes) ni l'experiència d'usuari (contrasenya compartida), però fan el sistema significativament més robust davant d'atacs de desgast.
