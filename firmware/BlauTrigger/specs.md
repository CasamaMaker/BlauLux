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
- On/Off - per controlar un relé, led, mosfet ...
- Digital led, tipus WS2812 - ja sigui per un sól led a mode de testimoni o una tira led sencera - es pot configurar color i brillo %
- PWM - per controlar una llum dimmer, o una tira leds, o utilitzant-ne dos sortides per una llum amb Cold White i Warm White ... - es pot configurar freqüència i dutty cycle %
- Triac per cicle - per controlar una càrrega AC, ja sigui una bombeta o el que sigui - es pot configurar dutty cycle %, ja que la freqüència serà 50Hz
- Triac per fase - per controlar una càrrega AC, ja sigui una bombeta o el que sigui més precís - es pot configurar dutty cycle, potència % i l'entrada del zero crossing detector (ZCD)

# Microcontroladors utilitzats inicialment:
- ESP32-C3
- ESP32-S3