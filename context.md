# Proyecto TP Multiplayer Mod - Contexto

## ¿Qué es esto?
Este repositorio contiene un prototipo inicial de un mod multijugador para **The Legend of Zelda: Twilight Princess** ejecutado a través del motor **Dusklight**. 
El objetivo principal de este proyecto es demostrar la viabilidad de sincronizar estados del juego a través de la red (posiciones, animaciones) inyectando código en el motor.

## Arquitectura del Proyecto

### Archivos Fuente
- `src/main.cpp` — Punto de entrada del mod (lifecycle: initialize/update/shutdown)
- `src/game/ActorInterface.h/.cpp` — Sincronización de jugadores y renderizado
- `src/network/Client.h/.cpp` — Cliente ENet para conexión al servidor
- `src/network/PacketSerializer.h/.cpp` — Serialización/deserialización de paquetes
- `src/network/NetworkTypes.h` — Tipos de paquetes, structs de datos, RemotePlayerState
- `src/network/ConnectionConfig.h` — Configuración de host/puerto vía env vars
- `src/server_simulator.cpp` — Servidor dedicado relay
- `src/test_main.cpp` — Tests unitarios

### Protocolo de Red
Todos los paquetes siguen el formato: `[PacketType:1][PlayerID:1][Payload:N]`

| Tipo | ID | Payload | Descripción |
|---|---|---|---|
| PACKET_POSITION | 0 | 16 bytes (x,y,z,rotY como f32 net-order) | Sincronización de posición |
| PACKET_STATUS | 1 | 8 bytes (health,maxHealth,animID) | Sincronización de estado |
| PACKET_PLAYER_ASSIGN | 2 | 0 bytes (playerID en header) | Servidor asigna ID al conectar |
| PACKET_PLAYER_DISCONNECT | 3 | 0 bytes (playerID en header) | Servidor notifica desconexión |

## Lo que se ha logrado hasta ahora

### Fase 1 — Red y Comunicación ✅
1. **Conexión de Red UDP estable:** ENet integrado en el ciclo de vida del mod.
2. **Servidor Dedicado con PlayerIDs:** El servidor asigna IDs únicos (0-254) a cada cliente y los sobreescribe en paquetes relay para prevenir spoofing.
3. **Broadcast Multi-Cliente:** Cualquier paquete de un cliente se retransmite a todos los demás.
4. **Modo Fantasma:** Si hay solo 1 jugador, el servidor devuelve eco con offset +150 X/Z usando playerID=200.
5. **Auto-reconexión:** El cliente reintenta conexión automáticamente cada ~2 segundos.
6. **Control de Tasa de Envío:** Envía a 15 Hz (cada 2 frames a 30fps) en lugar de cada frame.

### Fase 2 — Lectura de Estado del Juego ✅
1. **Lectura de posición:** Usa `dComIfGp_getPlayer(0)` con verificación de perfil (`fpcNm_ALINK_e = 0x0FD`).
2. **Lectura de vida:** Vía `g_dComIfG_gameInfo.info.getPlayer().getPlayerStatusA()`.
3. **Protección contra crashes:** Retardo de 120 frames después de cargar, validación de `maxHealth > 0`.

### Fase 3 — Interpolación de Movimiento ✅
1. **Interpolación lineal (lerp):** Las posiciones remotas se suavizan entre paquetes.
2. **Interpolación de ángulo:** Usa shortest-path para rotación Y (evita giros de 360°).
3. **Modelo de interpolación:** prevPos → targetPos con factor LERP_SPEED=0.15 por frame.
4. **Timeout automático:** Jugadores remotos se eliminan tras 300 frames (~10s) sin actualización.

### Fase 4 — Renderizado de Partículas ✅
1. **Hada luminosa persistente:** Cada jugador remoto se muestra como partícula 0x01A.
2. **Emitters persistentes:** Se crean una vez y se actualizan vía `JPABaseEmitter::setGlobalTranslation()`.
3. **Indicador de dirección:** Hada pequeña al frente del jugador remoto mostrando rotación.
4. **Limpieza segura:** Emitters se invalidan con `becomeInvalidEmitter()` en map change/disconnect.
5. **Multi-jugador:** `std::map<uint8_t, RemotePlayerState>` soporta N jugadores simultáneos.

### Fase 5 — Modelo 3D (Preparado, No Activo)
1. **Clonación de modelo:** `mDoExt_J3DModel__create` con `J3DModelData` compartido de Link.
2. **Para activar:** Requiere hook en pipeline de draw vía `dusk::mods::hook_add_post`.
3. **Offset verificado:** `mpLinkModel` está en offset `0x650` de `daAlink_c` (confirmado en header).

## Problemas Resueltos y Conocimientos Clave
- **`DUSK_CONST` en `leaf_methods`:** En el port PC, `leaf_methods` es `const`. Hijackear los métodos del actor dummy causaba violaciones de acceso. **Solución:** Se eliminó el enfoque de actor dummy por completo.
- **Crash del modelo 3D:** El clon de `J3DModel` sin `McaMorf` (animación) tenía bones sin inicializar. **Solución:** Se usa partículas como representación visual mientras se prepara el hook approach.
- **`mDoExt_modelUpdateDL` en contexto incorrecto:** Llamar desde un actor hijackeado donde la GX pipeline no estaba lista causaba crashes. **Solución:** Para renderizado 3D futuro, usar `mDoExt_modelEntryDL` o hooks post-draw.
- **Spam de partículas:** Crear emitters cada frame causaba acumulación. **Solución:** Crear una vez, actualizar posición vía `setGlobalTranslation()`, invalidar en cleanup.
- **Timeout de ENet en pantallas de carga:** 60 segundos de timeout configurado.
- **`std::cout` en aplicación GUI:** Removido del cliente para evitar abort.

## Pasos Obligatorios de Compilación y Despliegue
Siempre que se compila el mod con CMake (`cmake --build .`), el archivo resultante `.dusk` se genera en `build/mods/tp_multiplayer_mod.dusk`. **Este archivo DEBE ser copiado manualmente** a la carpeta de pruebas interna: `C:\Games\tp-multiplayer-mod\build\dusklight\mods\` para que los cambios tengan efecto en el entorno de pruebas local. Se ha creado el script `build_and_deploy.bat` para automatizar esto. No pongas nada fuera de la carpeta `tp-multiplayer-mod`.

## Próximos Pasos (Lo que falta)
1. **Renderizado 3D del jugador remoto (Fase 5):** Usar `dusk::mods::hook_add_post` en el draw de `daAlink_c` para renderizar el modelo clonado después del Link local. Requiere copiar bone matrices o implementar `mDoExt_McaMorf` para animaciones.
2. **Sincronización de Animaciones:** Enviar el ID de animación actual y reproducirla en el clon remoto.
3. **Gestión de Sesiones (Lobby):** Menú in-game para conectarse a un servidor por IP.
4. **Sincronización de Estado Extendido:** Espada desenvainada, escudo, transformación lobo.
5. **Envío UDP Unreliable para posición:** Cambiar posición a UNRELIABLE para menor latencia.
