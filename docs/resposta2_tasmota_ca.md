Gràcies per indicar-ho — tens tota la raó. L'enfocament de guardar a NVS en cada paquet rebut és una vulnerabilitat real: a 1.000 paquets/s un atacant pot esgotar la flash de l'ESP32 en aproximadament 100 segons.

---

**Què fa realment Matter (Tasmota)**

Mirant `be_matter_counter.cpp`, Matter manté la protecció contra replay íntegrament en RAM mitjançant un bitmap de 32 bits — sense cap escriptura a flash:

```
max_counter = 150
window_bitmap = 0b...11010111   ← quins dels últims 32 nonces s'han vist
Rang cobert: [119 .. 150]
```

Un paquet s'accepta si el seu nonce és superior a `max_counter` (desplaça la finestra) o cau dins la finestra amb el seu bit a zero. Sense NVS involucrat — un atac de flood no pot desgastar la flash.

El compromís: després d'un reinici, tant `max_counter` com `window_bitmap` es perden. Matter deixa el problema del comptador post-reinici a les capes superiors del protocol (gestió de sessions). Per a un dispositiu IoT sense sessions persistents com BlauLux, aquest buit queda sense resoldre si s'usa el comptador de Matter tal qual.

---

**La meva proposta: escriptura diferida + testimoni + finestra d'acceptació**

Per cobrir el buit post-reinici sense el cost d'experiència d'usuari d'un esquema ahead-increment, combino dos mecanismes:

**1. Escriptura diferida a NVS, limitada a 1 escriptura/hora**

El receptor guarda el `max_nonce` real (no `max_nonce + N`) a NVS.

Condicions d'escriptura: `(max_nonce != flash_nonce) AND (millis() - last_write >= 3.600.000)`.
Això limita les escriptures a flash a **24/dia**, donant una vida útil de la flash de ~11 anys en ús normal. Combinat amb rate limiting (màx. 10 paquets/s + backoff exponencial: 10 s → 60 s → 10 min), un atac de flood no pot provocar més d'una escriptura a NVS per hora.

**2. Post-reinici: paquet testimoni + finestra d'acceptació**

Després d'un reinici, es carrega `X` (el darrer nonce guardat) des de NVS. En lloc d'acceptar cegament qualsevol nonce `> X` (cosa que permetria el replay de paquets enviats des de l'última hora de guardada), el receptor usa un handshake en dos passos:

```
Finestra d'acceptació:  X < nonce < X + 100

Pas 1 — Testimoni:
  Primer paquet amb nonce dins (X, X+100):
    → verificació AES-GCM ✅
    → emmagatzemat com a witness_nonce en RAM
    → NO executat (comanda descartada)

Pas 2 — Confirmació:
  Proper paquet on: B > witness_nonce  I  X < B < X + 100:
    → acceptat i executat ✅
    → boot_phase = false → reprèn operació monotònica normal
```

Des del punt de vista de l'usuari: premes el botó una vegada (absorbit com a testimoni), tornes a prémer — el dispositiu respon. Dues pulsacions per recuperar-se d'un reinici.

**Per què és important el límit superior de la finestra**

Sense el límit superior (`X + 100`), qualsevol paquet capturat amb `nonce > X` seria acceptat després del reinici. La finestra `(X, X+100)` limita la superfície de replay a com a màxim els últims ~100 nonces enviats abans del reinici (aproximadament l'última hora de pulsacions). Els paquets capturats anteriorment — o molt per davant del comptador guardat — queden fora de la finestra i es rebutgen.

Perquè un atacant pogués explotar aquesta finestra residual necessitaria: (a) dos paquets AES-GCM vàlids capturats prèviament, (b) tots dos amb nonces dins `(X, X+100)`, i (c) un event de reinici per activar la fase de boot. Per a un llum domèstic, és una amenaça negligible.

---

**Comparativa: Matter (Tasmota) vs la meva proposta**

| | Matter (Tasmota) — sliding window RAM | La meva proposta — escriptura diferida + testimoni |
|---|---|---|
| Escriptures flash sota flood | ✅ cap (només RAM) | Màx. 1/hora (independentment de la taxa) |
| Vida útil flash (ús normal) | N/A | ~11+ anys |
| Protecció replay (normal) | ✅ sliding window 32 bits | ✅ monotònic estricte |
| Protecció replay post-reinici | ❌ no resolt | ⚠️ finestra de ~100 nonces |
| Recuperació després de reinici | ❌ no resolt | ✅ 2 pulsacions del botó |

---

Aquesta és encara una proposta de disseny — no he començat la implementació. Qualsevol comentari sobre l'enfocament testimoni + finestra, o vulnerabilitats que pugui haver passat per alt, és molt benvingut.
