# Projecte:
Blautrigger

# Descripció:
Blautrigger forma part del sistema Blau, el qual es compón de dos dispositius, BlauLink i BlauTrigger. BlauLink és un interruptor que va amb bateria i la seva funció és disparàr l'acció, que es transmet per ESPNOW amb el protocol BlauProtocol al receptor BlauTrigger. I BlauTrigger és el que executa l'acció d'encendre o aparar la llum/s configurada/es.

# Objectiu principal:
Controlar la/es llum/s. 

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
- A la pàgina web de configuració del hardware te les següents parts (de dalt a baix): desplegable per seleccionar plantilles de hardware predefinit (SONOF_MINI_R4, PICO_CLICK ...), si escollim "Personalitzar" aleshores apareix un altre desplegable amb els microcontroladors preconfigurats (ESP32-C3, ESP32-S3 ...). Sota de tot això, apareix una taula amb on cada fila correspon a un gpio utilitzable del microcontrolador seleccionat. I les columnes de: "Nom" que l'usuari li donarà a la funció d'aquell pin (editable), "Número gpio" real (no editable), desplegable de funcions del pin (@Tipus de control de llums), "Canal" per agrupar els pins si es desitja (editable). I finalment un boto de guardar i un altre per esborrar la configuració del hardware.
- Vull poder utilitzar tots els gpios, cadascún amb la seva propia funció i configuració, independent als altres
- Cada gpio ha de poder tenir el seu propi canal independent o compartir-lo
- Només commutaràn les sortides mitjançant l'entrade del botó pulsador les sortides que comparteixin el mateix canal que el boto.
- Cada canal ha de tenir la seva pròpia entitat al home assistant.
- Per definició, en activacions mitjançant ESPNOW, només afectarà les sortides del canal 1

# Monitoritzacio estats (web principal)
- Canviar l'apartat de "Hardware test" per una que sigui algo així com "Estats actuals". S'han de mostrar els estats actuals de totes les sortides configurades.
- Cada canal ha de tenir el seu propi subapartat dins d'"estats actuals". Amb un unic interruptor general On/Off del subgrup del canal. I si tenen altres paràmetres com dutty cicle, color, també s'han de poder modificar instantàneament. Tot això s'ha de veure reflectit al mqtt/home assistant...
