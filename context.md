# Proyecto TP Multiplayer Mod - Contexto

## ¿Qué es esto?
Este repositorio contiene un prototipo inicial de un mod multijugador para **The Legend of Zelda: Twilight Princess** ejecutado a través del motor **Dusklight**. 
El objetivo principal de este proyecto es demostrar la viabilidad de sincronizar estados del juego a través de la red (posiciones, animaciones) inyectando código en el motor.

## Lo que se ha logrado hasta ahora
1. **Conexión de Red UDP estable:** Se integró la librería **ENet** exitosamente dentro del ciclo de vida del juego (`mod_initialize`, `mod_update`, `mod_shutdown`).
2. **Sistema de Servidor:** Se usa un `server_simulator.exe` que actúa como servidor maestro para aceptar conexiones y hacer eco de las coordenadas. La comunicación cliente-servidor funciona correctamente y los paquetes de posición son recibidos y procesados.
3. **Lectura de Memoria en Vivo:** Se logró acceder de manera segura a la memoria del juego mediante la función `dComIfGp_getPlayer(0)` y leer los valores de `x, y, z` en tiempo real, así como la vida y estatus máximo.
4. **Prevención de Cierres (Crashes) por red:**
   - Se removieron todas las dependencias de `std::cout` en el código del cliente para evitar que la aplicación de ventana (GUI) se aborte de inmediato al imprimir.
   - Se configuraron retardos (timeouts) en ENet para prevenir que el cliente se desconecte mientras el jugador atraviesa pantallas de carga (cambios de mapa).

## Problemas Resueltos y Conocimientos Clave
- **Limitación del Port de PC vs Dolphin (GX Rendering):** Los comandos puros de gráficos GameCube/Wii directamente **NO FUNCIONAN** en el port nativo de PC (Dusklight).
- **El problema del Maniquí (Clon del jugador):** Intentar forzar la aparición de un `ALINK` o un `TestCube` con `fopAcM_fastCreate` causa crashes por falta de recursos (`.arc` files) en el escenario actual.
- **Representación Segura Exitosa:** Como alternativa al modelo 3D, se implementó exitosamente el uso del sistema de partículas de Dusklight (`dComIfGp_particle_set`). Ahora el jugador 2 se representa de manera 100% segura mediante una **partícula de Hada Luminosa** (`0x72F` / `0x01A`). Esto funciona en todos los mapas porque las partículas base siempre están cargadas en la memoria.
  - **Mejora:** Se añadió una segunda hada más pequeña frente a la principal para indicar la dirección (rotación Y) a la que mira el jugador.
- **Inyecciones seguras (Hooks):** Se utiliza `mod_update` con un retardo de 120 fotogramas para asegurar la inicialización completa.
- **Experimento de Renderizado 3D (Inestable):** Se ha habilitado la invocación forzada del actor `ALINK` (`fpcNm_ALINK_e`) mediante `fopAcM_fastCreate` en la función `SpawnRemotePlayer()`. Como se esperaba, esto causa inestabilidad y crashes abruptos en el juego debido a la falta de archivos `.arc` para dos personajes en memoria. Se mantiene activado únicamente para propósitos de prueba/debugging.

## Estado de la Red y Servidor
- El simulador de servidor (`server_simulator.exe`) ha sido modificado exitosamente y ahora funciona como un **Servidor Dedicado de Broadcast Real** (`tp_server.exe`).
- Cualquier paquete recibido de un cliente es re-transmitido a todos los demás clientes conectados (Multi-client routing).
- **Modo Fantasma (Pruebas Locales):** Si el servidor detecta que hay solo 1 jugador conectado, le devolverá un eco de sus paquetes con un desplazamiento de +150 en X/Z para simular a otro jugador y probar modelos o efectos en solitario.

## Pasos Obligatorios de Compilación y Despliegue
Siempre que se compila el mod con CMake (`cmake --build .`), el archivo resultante `.dusk` se genera en `build/mods/tp_multiplayer_mod.dusk`. **Este archivo DEBE ser copiado manualmente** a la carpeta de pruebas interna: `C:\Games\tp-multiplayer-mod\build\dusklight\mods\` para que los cambios tengan efecto en el entorno de pruebas local. Se ha creado el script `build_and_deploy.bat` para automatizar esto. No pongas nada fuera de la carpeta `tp-multiplayer-mod`.

## Próximos Pasos (Lo que falta)
1. **Sincronización Bidireccional de Animaciones y Rotación:** Actualizar animaciones, posiblemente usando el sistema de estatus actual (salud) y ampliándolo a más variables.
2. **Representación 3D (Opcional Futuro):** En el futuro, podríamos estudiar el uso de `J3DModelData` o generar un `SimpleModel` para cargar mallas arbitrarias si la comunidad desarrolla herramientas para ello, aunque la partícula luminosa actual cumple su función sin peligro de cierres.
3. **Gestión de Sesiones (Lobby):** Un menú dentro del juego para alojarse a un servidor por IP en vez de hardcodearla.
