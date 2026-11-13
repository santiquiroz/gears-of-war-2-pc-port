# Gears of War 2 — port nativo para PC (no oficial)

[![Última versión](https://img.shields.io/github/v/release/santiquiroz/gears-of-war-2-pc-port?label=versi%C3%B3n)](https://github.com/santiquiroz/gears-of-war-2-pc-port/releases/latest)
[![Descargas](https://img.shields.io/github/downloads/santiquiroz/gears-of-war-2-pc-port/total?label=descargas)](https://github.com/santiquiroz/gears-of-war-2-pc-port/releases)
[![Licencia: BSD-3-Clause](https://img.shields.io/github/license/santiquiroz/gears-of-war-2-pc-port?label=licencia)](LICENSE)
[![Estrellas en GitHub](https://img.shields.io/github/stars/santiquiroz/gears-of-war-2-pc-port?style=social)](https://github.com/santiquiroz/gears-of-war-2-pc-port/stargazers)

**Sitio web:** [santiquiroz.github.io/gears-of-war-2-pc-port/es](https://santiquiroz.github.io/gears-of-war-2-pc-port/es/)

Gears of War 2 de Xbox 360, versión comercial, recompilado estáticamente desde el `default.xex` del propio jugador a código nativo x64 con el SDK de ReXGlue. No es emulación. Es necesario poseer el juego. Un port anterior hecho por fans ("Hollow") se construyó a partir de una versión de desarrollo filtrada de 2008; este ejecuta el juego comercial completo.

El nombre corto del proyecto, `gow2-recomp`, es el del zip de cada versión; el repositorio es `gears-of-war-2-pc-port`.

Read in English: [README.md](README.md)

## Capturas de pantalla

![Pantalla de título](docs/screenshots/title.jpg)
![Menú principal](docs/screenshots/main-menu.jpg)
![Menú de opciones de PC](docs/screenshots/pc-options.jpg)
![Acto 1, Jacinto Med](docs/screenshots/act1-jacintomed.jpg)
![Acto 1 con renderizado interno 4K (resolution_scale = 3)](docs/screenshots/act1-4k-internal.jpg)

*Acto 1 con renderizado interno 4K (resolution_scale = 3).*

![Renderizado original 720p (izquierda) vs renderizado interno 4K (derecha)](docs/screenshots/scale-1x-vs-3x.jpg)

*Renderizado original 720p (izquierda) vs renderizado interno 4K (derecha).*

## Estado

- [x] Acto 1 jugable con teclado y mouse o con mando
- [x] Alta resolución nativa (`resolution_scale` 2 = 1440p, 3 = 4K interno)
- [x] Parches comunitarios del juego
- [x] Reescalado FSR 1 y nitidez CAS (`present_effect`)
- [x] Paquetes de texturas: volcar las texturas del juego y cargar tus propias versiones en mayor resolución
- [x] Puntería con el mouse por los ejes de mouse de PC del propio juego, sin zona muerta de stick (`gow2_mouse_aim`, activado por defecto)
- [x] Reescalado temporal FSR 4 y FSR 3.1 (Opciones de PC > Gráficos > Reescalador; `gow2_temporal`, `gow2_upscaler`; apagado por defecto). La versión publicada no trae DLSS; DLSS aparece en el menú con DLL del SDK que compiles tú mismo (0.2.8)
- [x] HDR10, generación de fotogramas FSR 3, capturas con F12 y carpeta de mods (0.4.0, se buscan testers)
- [x] Teclas y botones del mouse que el juego recibe como en un PC mientras se juega un nivel (`gow2_native_keys`, parche SDK 0022); los menús, el menú de pausa y las escenas siguen pasando por el mando emulado
- [x] Botones en pantalla que siguen el último dispositivo que usaste, en vivo: teclas dibujadas con tus teclas configuradas para teclado y mouse, los botones de Xbox, o botones de PlayStation (PS4: Share/Options, PS5: Create/Options) para un DualShock o DualSense (idea y parche del SDK de la recompilación de Gears of War 3 de @Flippage, issue #25) (`gow2_keycaps`; el paquete se crea desde tu propia copia con `Build keycap pack.cmd` del zip, sin Python desde la 0.2.9, o con `scripts/build-keycaps.ps1` desde el código, ver [tools/keycaps](tools/keycaps/README.md))
- [x] Ultrawide: una imagen realmente más ancha (Hor+) en vez de 16:9 con barras, hasta 1680 píxeles (`gow2_widescreen`, ver [docs/WIDESCREEN.md](docs/WIDESCREEN.md)); funciona con el renderizador nativo (nativo solo en Android por defecto desde la 0.3.15; en PC una opción de Gráficos > Renderizador > Nativo) y con la ruta de render targets ROV (Intel por defecto)
- [x] Renderizador nativo (0.3.0; dibujos tomados de las llamadas Direct3D del juego desde la 0.3.4): los render targets son superficies del host y los resolves escriben texturas directamente en vez de emular la EDRAM de la Xbox 360. Activado por defecto solo en Android desde la 0.3.15 (antes: AMD, NVIDIA desde la 0.3.8, y Android); en PC es una opción de Gráficos > Renderizador > Nativo, porque deja manchas en algunas tarjetas (issue #26) (`gow2_native_renderer`)
- [x] Antialiasing: FXAA y FXAA Extreme (Gráficos > Antialiasing, 0.3.4), en PC y Android
- [x] API gráfica en PC: Direct3D 12 (por defecto) o Vulkan, experimental (Opciones de PC > Gráficos > API gráfica, `gpu_backend`, 0.3.8). Con Vulkan no están disponibles los reescaladores temporales; FSR 1 y CAS sí
- [x] Instalación desde tu propio `.iso` en PC (0.3.6) y en Android (0.2.6)
- [x] Voces y subtítulos en idiomas separados (0.3.7): voces en inglés, español latino o francés; textos en 9 idiomas ([docs/LANGUAGES.md](docs/LANGUAGES.md))
- [ ] Campaña completa de principio a fin
- [x] Campaña cooperativa por System Link de hasta cuatro jugadores (`gow2_coop_players`, apagada por defecto): cuatro copias en un mismo PC (loopback) y un PC con un teléfono Android en la misma LAN para dos jugadores, verificado el 2026-10-14 y el 2026-10-15; Etapa 2 (cambios de capítulo, muertes, salida y reingreso, puntos de control y cinemáticas) verificada con cuatro copias en un mismo PC el 2026-10-16; un teléfono y un PC con 0.2.0 entraron al mismo lobby de System Link el 2026-10-16; 8 y 10 jugadores son experimentales (0.3.0, ocho copias se unieron en un mismo PC)
- [x] Juego por internet, experimental (`net_peers`, `net_public_address`, `net_allow_peers`): un teléfono con datos móviles se unió a una campaña alojada en un PC a través de puertos abiertos por UPnP, verificado el 2026-10-15; todavía no se probó con un amigo en otro PC fuera de la red
- [x] Fuego amigo y "Ir con el jugador 1" en la campaña cooperativa (0.4.2): los jugadores solo se pueden herir entre sí mientras todos activan PC Options > Multijugador > Fuego amigo (cooperativo), y solo entre jugadores humanos; "Ir con el jugador 1" (mismo menú) lleva a un invitado atorado junto al jugador 1 ([docs/PC_OPTIONS.md](docs/PC_OPTIONS.md#friendly-fire-and-go-to-player-1), en inglés). Todos los jugadores necesitan la 0.4.2
- [x] Menú de opciones en el juego (Opciones de PC; Opciones del dispositivo en Android) ([docs/PC_OPTIONS.md](docs/PC_OPTIONS.md)): verificado en un PC y en un Galaxy S25 Ultra el 2026-10-16
- [x] Opciones de calidad del juego: Gráficos > Calidad del juego con perfiles Bajo/Medio/Alto/Personalizado y 24 ajustes del motor del propio juego (sombras, luces, bloom, desenfoque de movimiento, profundidad de campo, oclusión ambiental, destellos, distorsión, calcomanías, niebla, vegetación, nivel de detalle); las 76 claves en [docs/SYSTEM_SETTINGS.md](docs/SYSTEM_SETTINGS.md)
- [x] Comprobación de actualizaciones en PC y Android (GitHub Releases, como máximo dos veces al día, `gow2_update_check`)
- [x] Ajustes automáticos para GPU integradas (720p interno, filtrado anisotrópico 2x, sin re-subida de memoria guest por cuadro) para portátiles y consolas portátiles
- [x] Android: los objetos/efectos blancos del renderizador Vulkan están corregidos (0.2.3); las sombras de objetos son una opción, apagada por defecto en Android
- [x] VSync espera a la pantalla en D3D12 y el límite de 30 fps se mantiene con VSync apagado (0.2.4)
- [x] Campaña cooperativa en pantalla dividida en un mismo PC (Opciones de PC > Multijugador > Jugadores en pantalla dividida, luego Campaña cooperativa > Campaña en pantalla dividida; dos jugadores, el límite del propio juego; el segundo jugador necesita un mando, o el teclado como jugador 1), verificada el 2026-10-22 con dos jugadores
- [x] Versus por System Link: dos copias en un mismo PC se unieron a un grupo y jugaron Zona de guerra en Avalanche el 2026-10-22
- [x] Horda por System Link: dos copias en un mismo PC jugaron la oleada 1 en Avalanche el 2026-10-22 (parches SDK 0049 y 0050)
- [ ] Android: una compilación del repositorio llega a jugar y se une a la campaña de un PC por System Link, pero todavía no está terminado; cada versión desde la v0.1.2 trae un APK experimental (ver [Android](#android-experimental)). El renderizador nativo está activado por defecto desde la 0.3.1. En un Galaxy S25 Ultra el hospital del capítulo 2 corre a unos 46-53 fps desde un arranque en frío y a unos 37 fps cuando el teléfono ya está tibio (batería a 38 °C), con la tasa de cuadros desbloqueada (resolución interna 720p); el límite es la GPU, que baja su velocidad al calentarse. Las GPU Mali sin la función de Vulkan `vertexPipelineStoresAndAtomics` (por ejemplo la Mali-G720 del Dimensity 8300) arrancan desde la 0.3.8 (issue #27). Desde la 0.4.3 el juego aguanta salir y volver (Inicio, otra app, la pantalla que se apaga); hasta la 0.3.18 la vuelta lo cerraba ([docs/ANDROID.md](docs/ANDROID.md))

Ver el plan completo en [ROADMAP.md](ROADMAP.md).

## Descarga y juego

La versión no incluye nada del juego. Necesitas tu propia copia comercial de Gears of War 2 para Xbox 360 (ID de título `4D53082D`): una imagen de disco (`.iso`) de ella, o una carpeta que tenga `default.xex` y la carpeta `GearGame`, que todavía puedes crear extrayendo tu ISO con [xdvdfs](https://github.com/antangelo/xdvdfs):

```text
xdvdfs unpack "Gears of War 2.iso" C:\Games\GoW2
```

El port se recompiló desde el disco retail de EE. UU. (su default.xex); otras ediciones, como el disco europeo, no están probadas y pueden fallar, y el log dice al arrancar cuál tienes (`XEX default.xex ...`).

No piratees el juego.

### PC

Requisitos: Windows 10/11 x64 y una GPU con DirectX 12, o una GPU con Vulkan para la opción experimental de Vulkan (desarrollado en una AMD Radeon RX 7800 XT).

1. Descarga `gow2-recomp-<versión>-win64.zip` de la [última versión](https://github.com/santiquiroz/gears-of-war-2-pc-port/releases/latest) (el `.apk` es la versión de Android).
2. Descomprímelo en cualquier lugar y ejecuta `gow2.exe`. El exe no está firmado, así que Windows puede mostrar «Windows protegió tu PC»: elige **Más información** y luego **Ejecutar de todas formas**.
3. El primer arranque pide tu `.iso`, o el `default.xex` de una carpeta del juego ya extraída (que también tenga `GearGame`), y recuerda la carpeta del juego en `gow2_game_path.txt`, junto al exe. Con un `.iso` lo extrae una sola vez en una carpeta `game` junto a `gow2.exe`, lo que necesita unos 8 GB libres en esa unidad; después puedes borrar el `.iso` si quieres. Para elegir otra carpeta más adelante, borra `gow2_game_path.txt`.

No pregunta si la carpeta del juego está dentro de la carpeta del exe o justo al lado, si la carpeta del exe está dentro de la del juego, o si pasas la carpeta tú (`--game_data_root` también acepta el .iso):

```text
gow2.exe --game_data_root="C:\Games\GoW2"
```

La primera vez que juegues en cooperativo por System Link, Windows pregunta si `gow2.exe` puede usar la red: permítelo, o los demás jugadores no encontrarán tu partida.

El primer arranque, y el primer arranque después de una actualización que cambia los gráficos, vuelve a compilar los shaders: la ventana puede quedarse unos minutos en la pantalla de título y Windows puede decir que «No responde», y el primer nivel tarda un poco más en cargar. Déjalo correr; los arranques siguientes usan los shaders guardados.

### Linux y SteamOS (experimental)

Una build nativa de Linux para Steam Deck, Legion Go con SteamOS y distribuciones de escritorio con Vulkan. Necesita la carpeta del juego ya extraída (no extrae el `.iso` por sí misma; usa xdvdfs o copia la carpeta que hizo la build de Windows).

1. Descarga `gow2-recomp-<versión>-linux-x86_64.tar.gz` del [último release](https://github.com/santiquiroz/gears-of-war-2-pc-port/releases/latest), extráelo en el propio dispositivo (no en Windows) y copia todo lo que hay dentro de su carpeta en la carpeta del juego, junto a `default.xex`.
2. Ejecuta `./gow2.sh`. En SteamOS, agrega `gow2.sh` a Steam como juego que no es de Steam (en Examinar, cambia el tipo de archivo a Todos los archivos) y deja Proton desactivado. Si Steam dice "Unsupported file type", los archivos perdieron el permiso de ejecución, casi siempre por extraerlos en Windows: abre una terminal en la carpeta del juego y ejecuta `chmod +x gow2 gow2.sh`.

Desde la 0.4.2 necesita un procesador x86-64-v3: Steam Deck, Legion Go, Intel desde 2013 (Haswell) y todos los AMD Ryzen; `gow2.sh` lo avisa en un procesador más viejo. El aviso de RenderDoc "Capturing Vulkan" que SteamOS mostraba con la 0.4.1 está apagado desde la 0.4.2.

Funciona con Vulkan, así que las funciones de Direct3D 12 (escalado FSR y DLSS, generación de cuadros, HDR10, paquetes de texturas) no están disponibles. Detalles, dónde quedan las partidas y cómo compilarla: [docs/LINUX.md](docs/LINUX.md) (en inglés).

### Android (experimental)

Requisitos: un teléfono arm64 con Android 10 o superior y Vulkan, y espacio libre para una copia completa de los archivos del juego.

1. Descarga `gow2-recomp-<versión>-android-arm64.apk` de la [última versión](https://github.com/santiquiroz/gears-of-war-2-pc-port/releases/latest) e instálalo (Android te pide permitir instalar apps desde el navegador o el gestor de archivos).
2. Copia al teléfono la carpeta del juego extraída, o el `.iso` de tu disco.
3. Abre la app y toca **Elegir carpeta del juego** (**Choose game folder** con el teléfono en inglés) y elige esa carpeta, o **Elegir archivo .iso** (la app extrae la imagen por sí misma; sirven imágenes planas, XGD2 y XGD3). La app copia los archivos a su propio almacenamiento, lo que necesita tanto espacio libre como ocupa el juego, porque el juego no puede leer archivos a través del selector de carpetas. Con la carpeta del paso 2 son dos copias; borra la tuya cuando la app termine.

En lugar de los pasos 2 y 3 puedes copiar la carpeta por USB desde un PC, de modo que `default.xex` quede en `Android/data/com.santiquiroz.gow2recomp/files/game/`, y luego tocar **Volver a comprobar**.

En Android el menú de ajustes se llama Opciones del dispositivo (el engranaje de la esquina superior derecha). Si el juego se cierra inesperadamente, el siguiente arranque abre primero **Opciones de inicio**: algunos ajustes, un **Driver de GPU** (en Adreno, un .zip de Turnip hecho para AdrenoTools en lugar del driver del teléfono), **Modo seguro** (sin memoria GPU dispersa, resolución 1x, filtro bilineal, el driver del teléfono), **Restablecer ajustes** y **Compartir último registro**. Para abrirla a propósito, mantén pulsado el icono de la app y elige **Opciones de inicio**. Estado y límites conocidos: [docs/ANDROID.md](docs/ANDROID.md). En Opciones del dispositivo, desliza un dedo sobre la lista para desplazarla (0.3.8).

## Controles

| Acción | Tecla |
|---|---|
| Moverse | W / A / S / D |
| Mirar | Mouse (o flechas del teclado) |
| Disparar | Botón izquierdo del mouse |
| Apuntar | Botón derecho del mouse |
| Cobertura / carrera roadie / voltereta | Espacio |
| Cuerpo a cuerpo / motosierra (mantener) | F o V |
| Usar / recoger / reanimar | E |
| Punto de interés | Q |
| Recargar | R |
| Pantalla táctica | G |
| Zoom | Botón central del mouse o Z |
| Armas | 1–4 |
| Pausa | Enter o Esc |
| Atrás | Tab |
| Menú de opciones de PC | F10 |

Un mando funciona al mismo tiempo. Sus botones se reasignan en Opciones de PC > Controles > Botones del mando (cualquier mando que SDL reconozca, Xbox incluido; se guarda como `pad_map_*`). Todas las asignaciones están en `gow2.toml`.

## Ajustes

Todo esto se puede cambiar en el juego desde Opciones > Opciones de PC (o F10, o Back+Start); ver [docs/PC_OPTIONS.md](docs/PC_OPTIONS.md). En Windows, Opciones de PC también se abre antes de cargar el juego, con un botón Jugar (se desactiva en Sistema > Mostrar opciones al iniciar). `gow2.toml` se encuentra junto al exe. Se lee al arrancar; las opciones de línea de comandos (`--nombre=valor`) tienen prioridad sobre él. Claves principales:

La pestaña Gráficos incluye **Calidad del juego**: perfiles Bajo / Medio / Alto (original) y 24 controles de calidad del motor; requieren reiniciar. Alto restaura la calidad original; Automático conserva los valores de la plataforma (en PC, los originales, que ya son la opción más alta de cada fila; Android apaga las sombras de objetos). Ver [el inventario de claves y perfiles](docs/SYSTEM_SETTINGS.md).

Las voces (inglés, español latino o francés; `gow2_voice_language`) y los subtítulos y textos (9 idiomas; `user_language`) se eligen por separado en Opciones de PC > Sistema y se aplican al reiniciar.

- `fullscreen` — pantalla completa o ventana.
- `resolution_scale` — resolución interna de renderizado como múltiplo de 720p: 1 = 720p, 2 = 1440p, 3 = 4K.
- `present_effect` — escalado de la imagen final: `bilinear`, `fsr` (AMD FSR 1 con nitidez, desde un `resolution_scale` menor) o `cas` (solo nitidez).
- `vsync` — sincronización vertical.
- `gow2_native_renderer` — `auto` (nativo solo en Android desde la 0.3.15; emulado en PC), `on` u `off`.
- `gpu_backend` — `any` (Direct3D 12), `d3d12` o `vulkan` (experimental); requiere reiniciar.
- `gow2_native_frontend` — origen de los draws: `auto` (comandos de consola en PC y en teléfonos con Mali, llamadas Direct3D en los demás teléfonos), `on` u `off`.
- `gow2_skip_intro_movies` — salta los videos con los logos de Epic Games y Microsoft Game Studios al arrancar.
- `gow2_skip_hint` — mientras se ve una cinemática, una nota en la esquina muestra el botón que la salta (Back en el mando, Tab en el teclado; solo el anfitrión puede saltarla). Activado por defecto.
- `gow2_launcher` — Windows: abre Opciones de PC antes de cargar el juego, con un botón Jugar (`false` entra directo al juego).
- `gow2_texture_pack` — Windows: la carpeta del pack de texturas dentro de `texture_packs` junto al exe, `textures` para la carpeta `textures` antigua, `off`, o vacío para seguir `texture_replace`; requiere reiniciar.
- `gow2_texture_build_scale`, `gow2_texture_build_quality` — Windows: escala (`2` o `4`) y formato (`disc` mantiene el de cada textura, `bc7`) de Crear pack de texturas.
- `present_letterbox` — Android: `false` llena una pantalla más ancha (recorta hasta un 10 % arriba y abajo y estira a lo ancho el resto).
- `gow2_unlock_fps` — elimina el límite de 30 fps del juego.
- `gow2_fps_cap` — límite de FPS con el juego desbloqueado (0 = sin límite).
- `gow2_show_fps` — muestra en una esquina los FPS del juego y el tiempo por cuadro.
- `gow2_audio_volume` — volumen general en por ciento.
- `gow2_black_shading_fix` — iluminación correcta de los personajes.
- `gow2_texture_streaming_fix` — reserva de texturas más grande, menos apariciones repentinas.
- `gow2_aniso16` — filtrado anisotrópico 16x.
- `gow2_ao_upscaling_fix` — oclusión ambiental sin artefactos al escalar.
- `gow2_dof_upscaling_fix` — profundidad de campo sin artefactos al escalar.
- `gow2_temporal`, `gow2_upscaler` — reescalado temporal: `gow2_temporal = true` con `gow2_upscaler = "fsr4"` (Radeon RX 7000/9000) o `"fsr3"`; apagado por defecto (solo Direct3D 12).
- `mnk_sensitivity` — sensibilidad del mouse.
- `gow2_save_dir` — carpeta de partidas guardadas, ajustes del perfil y logros; apúntala a una carpeta de OneDrive, Google Drive o Dropbox para tener la campaña en la nube ([docs/SAVES.md](docs/SAVES.md)).

Los parches comunitarios vienen de [xenia-canary/game-patches](https://github.com/xenia-canary/game-patches) (autores illusion y boma); los sitios y valores parcheados para este XEX se aplican al código generado, cada uno detrás de su ajuste `gow2_*`.

En Windows, el juego consulta GitHub para buscar una nueva versión como máximo dos veces al día y no envía ningún otro dato. Puedes desactivarlo en Opciones de PC > Sistema o poner `gow2_update_check = false` en `gow2.toml`.

Para actualizar, usa Opciones de PC > Sistema > Descargar (o la página de Releases), abre el zip nuevo y copia todo lo que hay dentro de su carpeta `gow2-recomp-<versión>-win64` en la carpeta del juego, reemplazando archivos. Tus ajustes de `gow2.toml`, tus partidas y la caché de shaders se conservan: el zip trae sus valores por defecto como `gow2.default.toml` y el juego crea `gow2.toml` a partir de él solo si falta.

## Rendimiento

Hospital del capítulo 2, tasa de cuadros desbloqueada: unos 260 fps a 1440p internos en un Ryzen 9 7900X3D + Radeon RX 7800 XT con el renderizador nativo (0.3.8; de la 0.3.3 a la 0.3.7 iba a unos 150 porque el front end nativo venía activado por defecto); 60 fps (el límite) con la GPU a cerca del 55 % en una GTX 1070; unos 46-53 fps desde un arranque en frío en un Galaxy S25 Ultra a 720p. Dónde se va el tiempo de cada cuadro en PC y Android, por qué el port necesita más GPU que "Hollow" y el plan ordenado para bajarlo: [docs/PERFORMANCE.md](docs/PERFORMANCE.md) (en inglés). Para reportar errores, `gow2_frame_log = true` registra los tiempos de cuadro FRAMESTATS cada 5 s y `d3d12_stats_log = true` los contadores de D3D12. Desde la 0.4.3 el código recompilado guarda los registros de la CPU de la Xbox 360 en registros del procesador en vez de en memoria: `gow2.exe` pasó de 90 a 72 MB y en Linux el hilo principal del juego ejecuta cerca de un tercio menos de instrucciones por cuadro; la tasa de cuadros del PC de arriba no cambió, porque la marca el lado de la GPU.

## Mejoras modernas

- Resolución nativa mediante `resolution_scale` (ver Ajustes).
- FSR: el runtime de la versión viene compilado con AMD FidelityFX. `present_effect = "fsr"` renderiza con un `resolution_scale` menor y reescala con FSR 1 más nitidez; `"cas"` solo aplica nitidez. Medido con `resolution_scale = 1` y FSR 1 a 1440p: unos 85 fps en el Acto 1. Desde la 0.4.2 el suavizado de FSR, la nitidez de CAS y el tramado cambian al instante desde PC Options, sin reiniciar.
- Reescalado temporal: FSR 4.1 en Radeon RX 7000/9000 y FSR 3.1 en otras GPU, con las DLL firmadas del FidelityFX SDK 2.3 de AMD, portados de la recompilación de Gears of War: Judgment. Apagado por defecto: elígelo en Opciones de PC > Gráficos > Reescalador (o pon `gow2_temporal = true` y `gow2_upscaler = "fsr4"` o `"fsr3"`), y baja `resolution_scale` para que el reescalador tenga margen. FSR 4 se comprobó en el juego con una Radeon RX 7800 XT (de 1280x704 a 2560x1408).
- HDR10 (0.4.0, Direct3D 12): Opciones de PC > Pantalla > HDR y reiniciar, con HDR activado en Windows. El blanco de papel y el brillo máximo se ajustan; el log dice `HDR10: on (...)` o por qué quedó apagado. Se buscan testers: [#47](https://github.com/santiquiroz/gears-of-war-2-pc-port/issues/47).
- Generación de cuadros FSR (0.4.0, Direct3D 12): Opciones de PC > Gráficos > Generación de cuadros = FSR (4 o 3), con el reescalador FSR 3.1 o FSR 4, y reiniciar. Usa la generación de FSR 4 donde el driver la ofrece (hoy Radeon RX 9000) y FSR 3 en las demás. Duplica los fotogramas mostrados; con el tope para VRR (`gow2_fps_cap = -1`) el tope queda a la mitad para que la tasa duplicada no pase la del monitor. Se buscan testers: [#48](https://github.com/santiquiroz/gears-of-war-2-pc-port/issues/48).
- Capturas con F12 (0.4.0): la imagen del juego sin menús en PNG en `Imágenes\Gears of War 2 PC` (Opciones de PC > Sistema).
- Mods (0.4.0): una carpeta `mods` junto a `gow2.exe` para packs de texturas apilados y archivos sueltos del juego con la estructura de COGBox, más Opciones de PC > Gráficos > Menús y textos originales. Ver [docs/MODDING.md](docs/MODDING.md). Se buscan testers: [#49](https://github.com/santiquiroz/gears-of-war-2-pc-port/issues/49).
- DLSS: la versión publicada no lo trae (la licencia del SDK de DLSS de NVIDIA no lo permite en una versión de código abierto; ver THIRD_PARTY_NOTICES.md), igual que el proyecto no publica arte del juego. Dos formas de agregarlo tú mismo: [OptiScaler](https://github.com/optiscaler/OptiScaler) con FSR 3.1 o FSR 4 elegido en el port (lo reportan jugadores; en OptiScaler los presets K o J de DLSS se ven más nítidos que L y M), o compilar las DLL del SDK desde el código con tu propia copia del SDK de DLSS de NVIDIA: `scripts/build-sdk.ps1` (sin `-NoDlss`) y copiar `rexruntime.dll`, `rexgpu-xenos.dll` y `nvngx_dlss.dll` junto al `gow2.exe` de la versión publicada; Opciones de PC ofrece entonces DLSS. Confirmado en una RTX 5060 Ti (issue #18).
- Paquetes de texturas: ver la siguiente sección. Probado con una textura reemplazada por una versión del doble de tamaño:

  ![Una textura de la pantalla de título reemplazada por una versión recoloreada de 2048x2048](docs/screenshots/texture-pack-test.jpg)

## Paquetes de texturas

La forma fácil en Windows: vuelca con Opciones de PC → Gráficos → Pack de texturas en Desactivado y luego **Crear pack de texturas** escala tu volcado con Real-ESRGAN en tu GPU y escribe un pack que eliges en Pack de texturas. Los pasos y la memoria de video que cuesta están en [docs/TEXTURE_PACKS.md](docs/TEXTURE_PACKS.md) (en inglés). Los pasos de abajo hacen un pack a mano.

Reemplaza las texturas del juego por versiones de mayor resolución (por ejemplo reescaladas 4x). Los paquetes salen de la copia de cada jugador; hacerlos y compartirlos queda en manos de sus autores. Este proyecto no publica ni enlaza paquetes hechos con las texturas del juego.

1. Activa `texture_dump = true` en `gow2.toml` y juega. Los volcados quedan en `textures\4D53082D\dump` junto a `gow2.exe`, un archivo `.dds` por textura única, con el nombre `<hash>_<formato>_<ancho>x<alto>.dds`. Los videos y otras texturas que cambian en cada frame se omiten.
2. Convierte los volcados a PNG con `tools\texpack.py` (viene en la versión; necesita Python 3 y Pillow, `pip install pillow`): `python tools\texpack.py to-png textures\4D53082D\dump work` convierte la carpeta completa.
3. Reescala el PNG con cualquier herramienta (Upflow en GPUs AMD, chaiNNer, Upscayl).
4. Convierte de vuelta con la cadena de mipmaps completa: `python tools\texpack.py from-png work textures\4D53082D\replace`. Los nombres se mantienen; el juego reconoce cada archivo por el hash al inicio del nombre. Esto genera DDS sin comprimir; para archivos unas 4 veces más chicos, comprímelos con texconv de Microsoft: `texconv -f BC7_UNORM -m 0 archivo.dds`.
5. Activa `texture_replace = true` en `gow2.toml` y juega. Antes de reescalar todo, comprueba que el reemplazo funciona con un color sólido: `python tools/texpack.py tint <volcado.dds> <reemplazo.dds>` y busca la textura teñida en el juego.

Un reemplazo se aplica cuando tiene al menos tantos niveles de mipmap como la textura del juego (la cadena completa hasta 1x1, que `from-png` ya genera), mantiene la misma proporción de aspecto y es al menos tan grande como la original.

## System Link

La campaña cooperativa por System Link funciona entre PC y entre varias copias en un mismo PC; una compilación de Android se unió a un PC para el cooperativo de dos jugadores. Viene apagada: pon `net_link_active = true` en `gow2.toml` (o pasa `--net_link_active=true`) en la copia de cada jugador y usa la misma compilación en todas; `gow2_coop_players` 3 o más la enciende sola. El puerto UDP 1000 del juego pasa a ser 41000 en todas las plataformas (`net_port_offset`, porque Android no puede usar puertos por debajo de 1024) y la baliza de System Link usa UDP 14001, así que un PC anfitrión necesita una regla del Firewall de Windows para esos dos puertos. `system_link_trace = true` registra cada llamada de red y de sesión del juego (líneas `[netlink]`).

- [docs/NETWORK_PLAY.md](docs/NETWORK_PLAY.md): un PC y un teléfono en la misma red, los ajustes, varias copias en un mismo PC y solución de problemas.
- [docs/COOP_N_PLAYERS.md](docs/COOP_N_PLAYERS.md): la Etapa 2 del cooperativo con más de dos jugadores cubre cambios de capítulo, muertes, salida y reingreso, puntos de control y cinemáticas; aún faltan las pantallas divididas, salir de vehículos, la sección Ride-Reaver y cuatro PC por internet.
- [docs/ONLINE.md](docs/ONLINE.md): jugar por internet. Desde el repositorio (no viene en el zip de la versión publicada), [`scripts/host-online.ps1`](https://github.com/santiquiroz/gears-of-war-2-pc-port/blob/master/scripts/host-online.ps1) `-Start` abre por UPnP los puertos UDP 14001 y 41000 en cada router entre el PC y internet, con un plazo (6 h por defecto, 24 h como máximo), y programa un `-Stop` automático para el final del plazo; `-Stop` los cierra. Ejecuta `-Stop` después de cada sesión; algunos routers ignoran el plazo. Sin UPnP: reenvío manual; sin abrir puertos: Tailscale o ZeroTier. Verificado el 2026-10-15: un teléfono con datos móviles se unió a un PC anfitrión detrás de dos routers en cadena.

Versus funciona por System Link: en Multijugador pon el Tipo de partida en System Link, crea el grupo en una copia y únete desde el Explorador de System Link en las demás (dos copias en un mismo PC jugaron Zona de guerra el 2026-10-22). Horda funciona igual desde el menú de Horda (dos copias jugaron la oleada 1 el 2026-10-22). Ver [ROADMAP.md](ROADMAP.md).

## Problemas conocidos

- **«Bad Image ... rexruntime.dll ... 0xc0e90002» en Windows 11.** Es el Control inteligente de aplicaciones bloqueando las DLL de la versión publicada porque todavía no tienen firma de código; los archivos no están dañados. Desactivar el Control inteligente de aplicaciones (Seguridad de Windows → Control de aplicaciones y navegador) permite que el juego arranque, pero Windows no puede volver a activarlo sin reinstalar, así que decide teniendo eso en cuenta. Se está estudiando firmar las versiones.

- **La puntería con el mouse cambió en la 0.1.1.** En la 0.1.0 el mouse se traducía a movimiento del stick derecho, y el juego ignoraba cualquier inclinación del stick menor al 30 % (su binding `DeadZone=0.3`). Ahora el mouse va por los ejes de mouse de PC del propio juego (`gow2_mouse_aim`), sin zona muerta, y los mandos conservan la puntería original. Las teclas todavía pasan por un mando emulado. Se agradece contar en un issue cómo se siente.
- **Las pistas muestran imágenes de botones de Xbox.** Los menús siguen el último dispositivo que usaste y cambian en vivo: teclas ([Espacio], [E]...) tras usar teclado o mouse, los botones de Xbox tras un mando de Xbox, botones de PlayStation (PS4: Share/Options, PS5: Create/Options) tras un DualShock o DualSense (`gow2_prompts` = `auto`; `keyboard`, `controller` y `playstation` fijan una opción al arrancar; en Android la elección se sigue haciendo al arrancar y no hay `playstation`). Los mandos de PlayStation ahora se detectan por SDL (antes solo contaban los mandos XInput); sus botones vienen del paquete de keycaps, así que recréalo con `Build keycap pack.cmd` (sin un paquete de la 0.3.5 muestran botones de Xbox). Idea y parche del SDK de la recompilación de Gears of War 3 de @Flippage (issue #25). Un menú ya abierto cambia la próxima vez que se abre; las imágenes de las pistas del tutorial pierden los círculos de botones de Xbox mientras se muestran teclas (el texto de la pista nombra tu tecla), y cualquier otro arte con botones pintados sigue mostrando botones de Xbox. Las indicaciones durante el juego (recoger, recargar, cubrirse) dibujan el botón directamente desde la fuente de botones del juego (issue #21): con el paquete de keycaps muestran tus teclas (confirmado en el juego en #21 con la v0.3.4), y con etiquetas de texto conservan los botones de Xbox ([docs/PROMPTS_GAMEPLAY_FINDINGS.md](docs/PROMPTS_GAMEPLAY_FINDINGS.md)).
- **Se reportó iluminación verdosa** en la piel, las luces y las llamas del menú; todavía no se reproduce en el PC de desarrollo. Se agradecen capturas en un issue.
- **El cooperativo de tres o cuatro jugadores es nuevo.** La Etapa 2 se probó con cuatro copias en un mismo PC: los jugadores 3 y 4 conservan sus cuerpos y armas al cambiar de capítulo y viajar, reaparecen al morir, salen y vuelven a unirse, se restauran al cargar puntos de control, se ocultan durante las cinemáticas, no reciben daño a pie mientras un héroe conduce y el anfitrión los lleva junto a Marcus o Dom si quedan muy atrás. No se han probado las pantallas divididas, salir de un vehículo, la sección Ride-Reaver, unirse durante una cinemática, combates reales prolongados, cuatro PC por LAN o internet ni Android con 3 o más jugadores ([docs/COOP_N_PLAYERS.md](docs/COOP_N_PLAYERS.md)).
- **Todos los jugadores necesitan la misma versión publicada.** Todos deben usar la misma versión; con tres o más jugadores, las campañas con hashes cooperativos distintos permanecen ocultas, y en cooperativo de dos jugadores no hay comprobación, así que mezclar versiones puede hacer que el juego falle.
- Solo se ha jugado el Acto 1 completo; las pruebas cooperativas llegaron a los capítulos 13, 16 y 27 mediante viajes entre capítulos. El 2026-10-22 `scripts/chapter-sweep.ps1` viajó a cada uno de los 29 capítulos y jugó en cada uno un minuto de movimiento y disparos programados sin ningún fallo por función faltante; una pasada tuvo un timeout de la GPU (TDR) en el capítulo 23 que no se repitió en tres pasadas posteriores. Jugar los actos 2 a 5 de principio a fin sigue pendiente (issue #8).
- **El renderizador nativo deja manchas en algunas GPU** (RX 7900 XT, RX 9070 XT, RTX 3070 Ti; issue #26); desde la 0.3.15 Automático usa Emulado (EDRAM) en PC.

## Compilar desde el código fuente

Ver [BUILDING.md](BUILDING.md). Los scripts principales son `scripts/build.ps1`, `scripts/build-sdk.ps1` (opcional: añade FidelityFX/FSR, reescaladores temporales y trazado de system link) y `scripts/package.ps1`.

## Cómo encontrar las funciones que codegen no detecta

`tools/harvest_hints.py` analiza la imagen descifrada (exportada desde Ghidra con `tools/ghidra/ExportImage.java`) contra una generación de código base sin pistas (`scripts/codegen-baseline.sh`) y emite `hints/harvested.toml`: thunks de ajuste, entradas `.pdata`, punteros de código guardados en `.rdata`/`.data` y direcciones construidas en el código con `lis`+`addi`/`ori`. Las pistas que dividirían una función existente se detectan desde el registro de codegen con `tools/hint_fixpoint.py` y se excluyen mediante `hints/denylist.txt`. Lo que siga faltando se encuentra en tiempo de ejecución con `scripts/run-harvest-loop.sh` (arrancar, leer el FATAL de "unregistered function", añadirlo a `hints/runtime.toml`, recompilar). `scripts/chapter-sweep.ps1` hace lo mismo con los 29 capítulos en una sola pasada viajando a cada uno (ver [docs/TEST_TOOLING.md](docs/TEST_TOOLING.md)).

## Contribuir

Las contribuciones son bienvenidas mediante pull requests en GitHub, un pull request por función o corrección. Ver [CONTRIBUTING.md](CONTRIBUTING.md) para las reglas. Los errores se reportan en issues con el `gow2_*.log` más reciente adjunto (en Android: Opciones de inicio > Compartir último registro).

## Créditos

- [ReXGlue SDK](https://github.com/rexglue/rexglue-sdk) (BSD-3-Clause, derivado de Xenia): runtime y generador de código.
- [OverkillLabs3/gears-judgement-recomp](https://github.com/OverkillLabs3/gears-judgement-recomp) (Gears of War: Judgment en ReXGlue, BSD-3-Clause): este repositorio copia `tools/patch_switch_traps.py` y `tools/check_objects_fresh.py` de él y adapta `scripts/build.ps1` de su `scripts/build.ps1`. Ver [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md).
- [XenonRecomp](https://github.com/hedge-dev/xenonrecomp) (hedge-dev).
- [Xenia](https://github.com/xenia-project/xenia).
- AMD FidelityFX SDK: FSR 1 y CAS (MIT). Las DLL de FSR 4 y FSR 3.1 de la versión publicada son binarios firmados de AMD, redistribuidos bajo la licencia de AMD, no MIT (ver [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md)).
- NVIDIA DLSS SDK (solo en compilaciones desde el código fuente; no viene en la versión publicada).
- [PresentMon](https://github.com/GameTechDev/PresentMon) (usado para las mediciones).

## Legal

Proyecto no oficial. Sin afiliación ni respaldo de Microsoft, Xbox Game Studios, The Coalition o Epic Games. Todas las marcas pertenecen a sus dueños. El código propio de este repositorio tiene licencia BSD-3-Clause; ver [LICENSE](LICENSE).

Las capturas (`docs/screenshots`, los archivos de `docs/assets/img` salvo `icon-320.webp`, `docs/assets/og-image*.jpg` y `assets/icon/og-image*.png`) muestran contenido de Gears of War 2, © Microsoft Corporation (originalmente © 2008 Epic Games, Inc.), tomado del juego ejecutándose en este port. No están cubiertas por la licencia BSD-3-Clause. El zip de cada versión también trae DLL de AMD y de Microsoft bajo sus propias licencias ([THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md)).
