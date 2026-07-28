# Proyecto TP Multiplayer Mod - Contexto

## ¿Qué es esto?
Este repositorio contiene un prototipo inicial de un mod multijugador para **The Legend of Zelda: Twilight Princess** ejecutado a través del motor **Dusklight**. 
El objetivo principal de este proyecto es demostrar la viabilidad de sincronizar estados del juego a través de la red (posiciones, animaciones) inyectando código en el motor original de GameCube/Wii.

## Lo que se ha logrado hasta ahora
1. **Conexión de Red UDP estable:** Se integró la librería **ENet** exitosamente dentro del ciclo de vida del juego (`mod_initialize`, `mod_update`, `mod_shutdown`).
2. **Sistema de Servidor:** Se creó un `server_simulator.exe` que actúa como servidor maestro para aceptar conexiones y hacer eco de las coordenadas.
3. **Lectura de Memoria en Vivo:** Se logró acceder de manera segura a la memoria del juego mediante la función `dComIfGp_getPlayer(0)` y leer los valores de `x, y, z` en tiempo real.
4. **Prevención de Cierres (Crashes):**
   - Se removieron todas las dependencias de `std::cout` en el código del cliente para evitar que la aplicación de ventana (GUI) se aborte de inmediato al imprimir.
   - Se configuraron retardos (timeouts) en ENet para prevenir que el cliente se desconecte mientras el jugador atraviesa pantallas de carga (cambios de mapa).
5. **Rendimiento Optimo:** Se ajustó la configuración de CMake (`-DCMAKE_BUILD_TYPE=Release`) para asegurar que el motor de Dusklight corra con optimizaciones completas, recuperando los 60-80 FPS originales y reduciendo los tiempos de carga de shaders ("pipelines").

## Problemas Resueltos y Limitaciones Actuales
- **Problema de los 10 FPS:** Solucionado reconstruyendo el binario completo de Dusklight en modo Release en lugar de Debug.
- **El problema del Maniquí (Clon del jugador):** 
  - **Limitación del Motor:** El motor de Twilight Princess está fuertemente diseñado alrededor del patrón "Singleton" para el actor del jugador (`ALINK`). Intentar forzar la aparición de un segundo actor de tipo `ALINK` a través de `fopAcM_fastCreate` corrompe los punteros internos del juego (ya que ambos actores pelean por ser el Jugador 1), causando un cierre instantáneo (Access Violation `0xc0000005`).
  - **Uso de Accesorios/Items:** Si intentamos aparecer un objeto de reemplazo (como una rupia o caja), el juego también se cierra si el modelo 3D de ese objeto específico no está precargado en el mapa (`.arc` file) actual.
  - **Solución temporal:** La generación visual del clon remoto está **desactivada**. Actualmente, el mod solo envía las posiciones locales y valida la red, sin intentar dibujar al otro jugador para asegurar la estabilidad del motor.

## Próximos Pasos (Lo que falta)
1. **Representación Visual del Jugador Remoto:** En lugar de intentar usar el marco de Actores nativo del juego (`fopAcM_fastCreate`), la solución ideal será inyectar una llamada de dibujado directamente en el hilo de renderizado (usando las librerías `J3D` o `GX` de Dusklight) para dibujar un cubo, esfera, o un modelo de Link personalizado que sea independiente del sistema de actores del juego.
2. **Sincronización Bidireccional de Animaciones:** Una vez que el jugador remoto sea visible, se deben interceptar y aplicar los códigos de estado (`currentAnimation`, `health`, etc.) para que se vea reflejado el movimiento real.
3. **Gestión de Sesiones (Lobby):** Desarrollar un menú dentro del juego o vía consola para permitir introducir la IP del servidor al que conectarse, en lugar de usar `127.0.0.1` de forma estricta en el código.
