# Projecte:
BlauLux

# Descripció:
BlauLux forma part del sistema Blau, el qual es compón de dos dispositius, BlauLink i BlauLux. BlauLink és un interruptor que va amb bateria i la seva funció és disparàr l'acció, que es transmet per ESPNOW amb el protocol BlauProtocol al receptor BlauLux. I BlauLux és el que executa l'acció d'encendre o aparar la llum/s configurada/es.

# Objectiu principal:
Controlar la/es llum/s via espnow/blauprotocol

# Formes de disparador:
- ESPNOW (BlauProtocol)
- Botó físic
- MQTT (Home Assistant)
- Web allotjada local

# Configuració general:
Es fa a través de la web local. Es configura el wifi, el mqtt, el hardware i altres

# Formes d'accedir a la web:
- Al arrencar el microcontrolador, si no està configurat cap botó, aleshores es crear un wifi en mode AP i s'habilita la web captive local
- En qualsevol moment, si es presiona el botó més de 3 segons, aleshores es crear un wifi en mode AP i s'habilita la web captive local
- En qualsevol moment, si està configurat i conectat al wifi de la xarxa local, aleshores es pot accedir a la web mitjançant una ip tipus: 192.168.xxx.xxx

# Tipus de control de llums:
- **On/Off** - per controlar un relé, led, mosfet ...
- **Digital led**, tipus WS2812 - ja sigui per un sól led a mode de testimoni o una tira led sencera - es pot configurar color i brillo %
- **PWM** - per controlar una llum dimmer, o una tira leds, o utilitzant-ne dos sortides per una llum amb Cold White i Warm White ... - es pot configurar freqüència i dutty cycle %
- **Triac per cicle** - per controlar una càrrega AC, ja sigui una bombeta o el que sigui - es pot configurar dutty cycle %, ja que la freqüència serà 50Hz
- **Triac per fase** - per controlar una càrrega AC, ja sigui una bombeta o el que sigui més precís - es pot configurar dutty cycle, potència % i l'entrada del zero crossing detector (ZCD)

# Microcontroladors utilitzats inicialment:
- ESP32-C3
- ESP32-S3

# Configuració del hardware (web configuració hardware)
- A la pàgina web de configuració del hardware te les següents parts (de dalt a baix): desplegable per seleccionar plantilles de hardware predefinit (SONOF_MINI_R4, PICO_CLICK ...), si escollim "Personalitzar" aleshores apareix un altre desplegable amb els microcontroladors preconfigurats (ESP32-C3, ESP32-S3 ...). Sota de tot això, apareix una taula amb on cada fila correspon a un gpio utilitzable del microcontrolador seleccionat. I les columnes de: "Nom" que l'usuari li donarà a la funció d'aquell pin (editable), "Número gpio" real (no editable), desplegable de funcions del pin (sense selecció + Botó + [Tipus de control de llums](#tipus-de-control-de-llums)), "Canal" per agrupar els pins si es desitja (editable). I finalment un boto de guardar i un altre per esborrar la configuració del hardware.
- Vull poder utilitzar tots els gpios, cadascún amb la seva propia funció i configuració, independent als altres
- Cada gpio ha de poder tenir el seu propi canal independent o compartir-lo
- Només commutaràn les sortides mitjançant l'entrade del botó pulsador les sortides que comparteixin el mateix canal que el boto.
- Cada canal ha de tenir la seva pròpia entitat al home assistant.
- Per definició, en activacions mitjançant ESPNOW, només afectarà les sortides del canal 1

# Monitoritzacio estats (web principal)
- Apartat d'"Estats actuals": s'han de mostrar els estats actuals de totes les sortides configurades.
- S'han de poder modificar els paràmetres i aplicar-se en el moment.

# Limits
- Hi ha un màxim de **canals**, definit en un paràmetre del config.h . El primer canal és el 1.
- El canal 1 és el canal
- El **número màxim de gpio** està definit per les definicions dels microcontroladors i/o templates. No ha d'estar hardcodejat
- els **limits de canals per LEDC**, estarà definits també juntament amb les definicions del pinout dels microcontroladors

# Definició variables/paràmetres

- gpio: és el número del pin

vull que es defineixin "Array of Structs (AoS)":
struct GpioConfig {
    name;
    function;
    param1;
    param2;
    param3;
    state;
};

GpioConfig gpiomap[MAX_GPIO];


.function: és un arrai on es defineix cada funció segons el pin:
> 0. N/A  no utilitzat
> 1. Botó
> 2. On/Off
> 3. Led digital
> 4. PWM
> 5. Triac/cicle
> 6. Triac/fase

.parameter1: dependrà la funció que tingui assignada el pin (si la funció no requereix un paràmetre extra, ometre aquest valor):
> - **Botó**: pull-up
> - **On/Off**
> - **Led digital**: color
> - **PWM**: dutty cycle
> - **Triac/cicle**: dutty cycle
> - **Triac/fase**: fase %

.parameter2: dependrà la funció que tingui assignada el pin (si la funció no requereix un paràmetre extra, ometre aquest valor):
> - **Botó**
> - **On/Off**
> - **Led digital**: brillo
> - **PWM**: freqüència
> - **Triac/cicle**: freqüència
> - **Triac/fase**: pin Zero Crossing Detector

.parameter3: dependrà la funció que tingui assignada el pin (si la funció no requereix un paràmetre extra, ometre aquest valor):
> - **Botó**
> - **On/Off**
> - **Led digital**: nº leds
> - **PWM**
> - **Triac/cicle**
> - **Triac/fase**



## Taula resumida
| funcio | parametre1 | parametre2 | parametre3 |
| - | - | - | - |
| NA | | |
| Botó | pull-up | |
| On/Off | | |
| Led digital | color | brillo | nº leds |
| Pwm | dutty cycle | freqúència |
| Triac/cicle | dutty cycle | freqúència |
| Triac/fase | fase % | pin Zero Crossing Detector |


# Definició de les funcions dels drivers
D'aquesta manera vull poder definir i controlar cada sortida independentment de les altres. Actualment 

```
on_off(gpio, state){    //exemple
    writePin(gpio, state)
}

led_digital(gpio, state){
    color = parameter1[gpio]
    brillo = parameter2[gpio]

    set_led_digital(color, brillo)
}

pwm(gpio, state){
    dutty_cycle = parameter1[gpio]
    freqüència = parameter2[gpio]

    set_pwm(dutty_cycle, freqüència)
}

triac_cicle(gpio, state){
    dutty_cycle = parameter1[gpio]
    freqüència = parameter2[gpio]

    set_tria_cicle(dutty_cycle, freqüència)
}

triac_fase(gpio, state){
    fase_% = parameter1[gpio]
    zero_crossing_detector = parameter2[gpio]

    set_tria_fase(fase_%, zero_crossing_detector)
}
```

# Definició dels logs

- 3 nivells: 

    0. **silenci**: no imprimir res pel port sèrie
    1. **error**: imprimir errors detectats, cada error ha de tenir un número d'identificació d'on s'ha produit, per poder saber en quina part del codi s'ha ocasionat. també ha de tenir una breu descripció 
    2. **info**: informar dels estats, exemple: "Connectat al wifi XXX", "conectat a mqtt", "Canal 1 activat via espnow", "Canal 2 activat via mqtt" ...
    3. **debug**: s'ha d'imprimir tota la informació nova sobre: configuració, estats, wifi, mqtt, gpio, web... ha d'estar bén estructurada i visual per humans



exemple:
```
[D] [WIFI] MAC AP: 6055F969B3DD
[I] [WIFI] AP ok: BlauLux_B3DD
[D] [WIFI] AP ok: BlauLux_B3DD, ch=1, MAC=60:55:F9:69:B3:DD, 192.168.4.1

[I] [WIFI] STA connected to: CanGoita
[D] [WIFI] STA connected to: CanGoita, IP: 192.168.1.138, canal STA: 11
[I] [WIFI] esp_wifi_set_channel(1) -> FAIL (radio bloquejada al canal STA) (canal actual: 11)

[I] [MQTT] ...

[I] [WEB] Hardware configurat
[I] [WEB] Configuració hardware resetejada
[I] [WEB] Configuració wifi resetejada
[I] [WEB] Equip reiniciat

[I] [CONFIG] Configuració hardware actualitzat
[D] [CONFIG] Hardware config: gpio 1 | Boto (Nom) | pull-up: no |
[D] [CONFIG] Hardware config: gpio 4 | Triac cicle (bombeta) | duty: 30%
[D] [CONFIG] Hardware config: gpio 5 | LED digital (led indicado) | color: #FF8800 | brillo: 75%
[D] [CONFIG] Hardware config: gpio 10 | On/Off (ssr)


[D] [DRV] Sortida ON: (bombeta) gpio 4, Triac cicle [duty: 30%, 100 Hz]
[D] [DRV] Sortida ON: (led indicado) gpio 5, LED digital [color: #FF8800, brillo: 75%]
[D] [DRV] Sortida ON: (ssr) gpio 10, On/Off

[D] [DRV] Sortida OFF: (bombeta) gpio 4, Triac cicle [duty: 30%, 100 Hz]
```


7. API web (endpoints principals) >> POST per guardar [web > uC]; GET per visualitzar valors actuals [uC > web] més o menys
GET / → pàgina web principal
GET /version → versió firmware
<!-- GET /acfreq → freqüència AC mesurada (JSON) -->
POST /gpiomap → configuració GPIO completa (escriptura)
GET /gpiomap → configuració GPIO actual (lectura JSON)
POST /wifi → credencials WiFi STA
POST /mqtt → configuració MQTT
POST /driver → per modificar en temps real els estats de les sortides

# Guardar paràmetres via POST des de la web:
l'enviament des de la web ha de seguir la mateixa estructura que [Definició variables/paràmetres](#definició-variablesparàmetres):

    POST// /driver?funcio:xfx/parametre1:xp1x/parametre2:xp2x

i la recepció per part del codi ha de ser:

funció[gpio] = xfx
parametre1[gpio] = xp1x
parametre2[gpio] = xp2x
parametre3[gpio] = xp3x


8. Persistència (NVS / Preferences)
- Namespace: "blau"
- Versió d'esquema: clau schema = 2 (s'incrementa quan canvien les claus)
- Claus principals:
    - f{0-21}: funció Function per GPIO
    - a{0-21}: parameter1 per GPIO (uint32)
    - b{0-21}: parameter2 per GPIO (uint16)
    - c{0-21}: parameter3 per GPIO (uint16)
    - n{0-21}: nom del GPIO (string, màx 12 chars)
    - sta_ssid, sta_pass: credencials WiFi STA
    - mqtt_host/port/user/pass/client/topic/fulltopic: config MQTT
    - devname: nom del dispositiu

### 9. Comportament del sistema
- Seqüència d'arrencada (boot → loadConfig → initGPIOs → ESPNOW → WiFi → MQTT → loop)
- Mode AP: s'activa si no hi ha botó configurat (o premunt 3s)
  - SSID: `BlauLux_{últims4MAC}`
  - IP: 192.168.4.1
  - Timeout: 120 s → reinici automàtic
- Mode STA: opcional, connexió a xarxa domèstica
- Coexistència ESP-NOW i WiFi STA: canal Wi-Fi fix = 1 (ESPNOW_CHANNEL)
  - Si la xarxa domèstica usa un altre canal, ESP-NOW queda al canal STA actiu

### 10. Senyalització visual (LED d'estat, si és un led digital)
* colors defints a config.h
- Verd: arrencada correcta (blink 500 ms un sol cop)
- Lila: efecte parpellenjant suau mode AP actiu
- Groc: MQTT connectat
- Blanc: color per defecte de la llum configurada

# Fitxers
- button: buttonPressed()
- config
- wifi: apMode_enable(), apMode_disable(), staMode_enable(), staMode_disable()
- mqtt
- espnow: 
- nvsconfig: config_load(), config_safe(), config_clear()
- driver: drv_onoff(), drv_
- log: 
- webserver: setupServer()
- watchdog



# Altres característiques
- Watchdog
- guardar paràmetres de configuració amb Preference



# Funcionament drivers

```
void driver_setup(int gpio){
    _function = function[gpio]
    _pin = pin[gpio]
    _parameter1 = parameter1[gpio]
    _parameter2 = parameter2[gpio]
    _parameter3 = parameter3[gpio]

    switch(_function):
        case DRV_BOTO:
            pinMode(_pin, INPUT);
        case DRV_ONOFF:
            pinMode(_pin, OUTPUT);
        case DRV_LED_DIGITAL:
            ...
}
```

```
void driver_toogle(int gpio){
    _function = function[gpio]
    _state = state[gpio]

    switch(_function):
        case DRV_ONOFF:
            driver_onoff(gpio, !_state)
            state[gpio] = !_state
        ...
}
```

```
void driver_onoff(int gpio, bool on){
    _function = function[gpio]
    _pin = pin[gpio]
    _parameter1 = parameter1[gpio]
    _parameter2 = parameter2[gpio]
    _parameter3 = parameter3[gpio]
    _state = state[gpio]

    digitalWrite(_pin, on)
    state[gpio] = on
        ...
}
```