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
- [x] Reescalado temporal FSR 4 y FSR 3.1 (`gow2_temporal`, `gow2_upscaler`; apagado por defecto). La versión publicada no trae DLSS
- [x] Teclas y botones del mouse que el juego recibe como en un PC mientras se juega un nivel (`gow2_native_keys`, parche SDK 0022); los menús, el menú de pausa y las escenas siguen pasando por el mando emulado
- [x] Indicaciones de teclado como teclas dibujadas que siguen tus teclas configuradas y vuelven a los botones del mando mientras haya uno conectado (`gow2_keycaps`; el paquete se crea con `scripts/build-keycaps.ps1`, ver [tools/keycaps](tools/keycaps/README.md))
- [ ] Campaña completa de principio a fin
- [x] Campaña cooperativa por System Link de hasta cuatro jugadores (`gow2_coop_players`, apagada por defecto): cuatro copias en un mismo PC (loopback) y un PC con un teléfono Android en la misma LAN para dos jugadores, verificado el 2026-10-14 y el 2026-10-15; Etapa 2 (cambios de capítulo, muertes, salida y reingreso, puntos de control y cinemáticas) verificada con cuatro copias en un mismo PC el 2026-10-16; un teléfono y un PC con 0.2.0 entraron al mismo lobby de System Link el 2026-10-16
- [x] Juego por internet, experimental (`net_peers`, `net_public_address`, `net_allow_peers`): un teléfono con datos móviles se unió a una campaña alojada en un PC a través de puertos abiertos por UPnP, verificado el 2026-10-15; todavía no se probó con un amigo en otro PC fuera de la red
- [x] Menú de opciones en el juego (Opciones de PC; Opciones del dispositivo en Android) ([docs/PC_OPTIONS.md](docs/PC_OPTIONS.md)): verificado en un PC y en un Galaxy S25 Ultra el 2026-10-16
- [x] Opciones de calidad del juego: Gráficos > Calidad del juego con perfiles Bajo/Medio/Alto/Personalizado y 24 ajustes del motor del propio juego (sombras, luces, bloom, desenfoque de movimiento, profundidad de campo, oclusión ambiental, destellos, distorsión, calcomanías, niebla, vegetación, nivel de detalle); las 76 claves en [docs/SYSTEM_SETTINGS.md](docs/SYSTEM_SETTINGS.md)
- [x] Comprobación de actualizaciones en PC y Android (GitHub Releases, como máximo dos veces al día, `gow2_update_check`)
- [x] Ajustes automáticos para GPU integradas (720p interno, filtrado anisotrópico 2x, sin re-subida de memoria guest por cuadro) para portátiles y consolas portátiles
- [x] Android: los objetos/efectos blancos del renderizador Vulkan están corregidos (0.2.3); las sombras de objetos son una opción, apagada por defecto en Android
- [x] VSync espera a la pantalla en D3D12 y el límite de 30 fps se mantiene con VSync apagado (0.2.4)
- [ ] Horda y Versus por System Link
- [ ] Android: una compilación del repositorio llega a jugar y se une a la campaña de un PC por System Link, pero todavía no está terminado; cada versión desde la v0.1.2 trae un APK experimental (ver [Android](#android-experimental)). Toda la escena se dibuja desde el 15 de octubre; corre a unos 30 fps durante los primeros minutos desde un arranque en frío y baja a unos 10-12,5 fps cuando el teléfono está caliente ([docs/ANDROID.md](docs/ANDROID.md))

Ver el plan completo en [ROADMAP.md](ROADMAP.md).

## Descarga y juego

La versión no incluye nada del juego. Necesitas tu propia copia comercial de Gears of War 2 para Xbox 360 (ID de título `4D53082D`), volcada a una carpeta que tenga `default.xex` y la carpeta `GearGame`, por ejemplo extrayendo tu ISO con [xdvdfs](https://github.com/antangelo/xdvdfs):

```text
xdvdfs unpack "Gears of War 2.iso" C:\Games\GoW2
```

No piratees el juego.

### PC

Requisitos: Windows 10/11 x64 y una GPU con DirectX 12 (desarrollado en una AMD Radeon RX 7800 XT).

1. Descarga `gow2-recomp-<versión>-win64.zip` de la [última versión](https://github.com/santiquiroz/gears-of-war-2-pc-port/releases/latest) (el `.apk` es la versión de Android).
2. Descomprímelo en cualquier lugar y ejecuta `gow2.exe`. El exe no está firmado, así que Windows puede mostrar «Windows protegió tu PC»: elige **Más información** y luego **Ejecutar de todas formas**.
3. El primer arranque pide la carpeta del juego (la que tiene `default.xex` y `GearGame`) y la recuerda en `gow2_game_path.txt`, junto al exe. Para elegir otra carpeta más adelante, borra ese archivo.

No pregunta si la carpeta del juego está dentro de la carpeta del exe o justo al lado, si la carpeta del exe está dentro de la del juego, o si pasas la carpeta tú:

```text
gow2.exe --game_data_root="C:\Games\GoW2"
```

La primera vez que juegues en cooperativo por System Link, Windows pregunta si `gow2.exe` puede usar la red: permítelo, o los demás jugadores no encontrarán tu partida.

### Android (experimental)

Requisitos: un teléfono arm64 con Android 10 o superior y Vulkan, y espacio libre para una copia completa de los archivos del juego.

1. Descarga `gow2-recomp-<versión>-android-arm64.apk` de la [última versión](https://github.com/santiquiroz/gears-of-war-2-pc-port/releases/latest) e instálalo (Android te pide permitir instalar apps desde el navegador o el gestor de archivos).
2. Copia al teléfono la carpeta del juego extraída.
3. Abre la app, toca **Elegir carpeta del juego** (**Choose game folder** con el teléfono en inglés) y elige esa carpeta. La app copia los archivos a su propio almacenamiento, lo que necesita tanto espacio libre como ocupa el juego, porque el juego no puede leer archivos a través del selector de carpetas. Con la carpeta del paso 2 son dos copias; borra la tuya cuando la app termine.

En lugar de los pasos 2 y 3 puedes copiar la carpeta por USB desde un PC, de modo que `default.xex` quede en `Android/data/com.santiquiroz.gow2recomp/files/game/`, y luego tocar **Volver a comprobar**.

En Android el menú de ajustes se llama Opciones del dispositivo (el engranaje de la esquina superior derecha). Si el juego se cierra inesperadamente, el siguiente arranque abre primero **Opciones de inicio**: algunos ajustes, **Modo seguro** (sin memoria GPU dispersa, resolución 1x, filtro bilineal), **Restablecer ajustes** y **Compartir último registro**. Para abrirla a propósito, mantén pulsado el icono de la app y elige **Opciones de inicio**. Estado y límites conocidos: [docs/ANDROID.md](docs/ANDROID.md).

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

Un mando funciona al mismo tiempo. Todas las asignaciones están en `gow2.toml`.

## Ajustes

Todo esto se puede cambiar en el juego desde Opciones > Opciones de PC (o F10, o Back+Start); ver [docs/PC_OPTIONS.md](docs/PC_OPTIONS.md). `gow2.toml` se encuentra junto al exe. Se lee al arrancar; las opciones de línea de comandos (`--nombre=valor`) tienen prioridad sobre él. Claves principales:

La pestaña Gráficos incluye **Calidad del juego**: perfiles Bajo / Medio / Alto (original) y 24 controles de calidad del motor; requieren reiniciar. Alto restaura la calidad original; Automático conserva los valores de la plataforma. Ver [el inventario de claves y perfiles](docs/SYSTEM_SETTINGS.md).

- `fullscreen` — pantalla completa o ventana.
- `resolution_scale` — resolución interna de renderizado como múltiplo de 720p: 1 = 720p, 2 = 1440p, 3 = 4K.
- `present_effect` — escalado de la imagen final: `bilinear` con el runtime estándar; `fsr`, `cas` y rutas temporales experimentales con un runtime compilado desde el código fuente (ver abajo).
- `vsync` — sincronización vertical.
- `gow2_unlock_fps` — elimina el límite de 30 fps del juego.
- `gow2_fps_cap` — límite de FPS con el juego desbloqueado (0 = sin límite).
- `gow2_audio_volume` — volumen general en por ciento.
- `gow2_black_shading_fix` — iluminación correcta de los personajes.
- `gow2_texture_streaming_fix` — reserva de texturas más grande, menos apariciones repentinas.
- `gow2_aniso16` — filtrado anisotrópico 16x.
- `gow2_ao_upscaling_fix` — oclusión ambiental sin artefactos al escalar.
- `gow2_dof_upscaling_fix` — profundidad de campo sin artefactos al escalar.
- `gow2_temporal`, `gow2_upscaler` — reescalado temporal: `gow2_temporal = true` con `gow2_upscaler = "fsr4"` (Radeon RX 7000/9000) o `"fsr3"`; apagado por defecto.
- `mnk_sensitivity` — sensibilidad del mouse.
- `gow2_save_dir` — carpeta de partidas guardadas, ajustes del perfil y logros; apúntala a una carpeta de OneDrive, Google Drive o Dropbox para tener la campaña en la nube ([docs/SAVES.md](docs/SAVES.md)).

Los parches comunitarios vienen de [xenia-canary/game-patches](https://github.com/xenia-canary/game-patches) (autores illusion y boma); los sitios y valores parcheados para este XEX se aplican al código generado, cada uno detrás de su ajuste `gow2_*`.

En Windows, el juego consulta GitHub para buscar una nueva versión como máximo dos veces al día y no envía ningún otro dato. Puedes desactivarlo en Opciones de PC > Sistema o poner `gow2_update_check = false` en `gow2.toml`.

## Rendimiento

Medido en un Ryzen 9 7900X3D + Radeon RX 7800 XT, Acto 1, `resolution_scale = 2` (1440p interno): unos 90 cuadros presentados por segundo en promedio, 1% bajo en 55, GPU unos 6 ms por cuadro (PresentMon 2.6, captura de 30 s). Estas cifras son cuadros presentados. Para reportes de errores, `gow2_frame_log = true` registra los tiempos de cuadro FRAMESTATS cada 5 s y `d3d12_stats_log = true` registra los contadores D3D12.

## Mejoras modernas

- Resolución nativa mediante `resolution_scale` (ver Ajustes).
- FSR: el runtime de la versión viene compilado con AMD FidelityFX. `present_effect = "fsr"` renderiza con un `resolution_scale` menor y reescala con FSR 1 más nitidez; `"cas"` solo aplica nitidez. Medido con `resolution_scale = 1` y FSR 1 a 1440p: unos 85 fps en el Acto 1.
- Reescalado temporal: FSR 4.1 en Radeon RX 7000/9000 y FSR 3.1 en otras GPU, con las DLL firmadas del FidelityFX SDK 2.3 de AMD, portados de la recompilación de Gears of War: Judgment. Apagado por defecto: pon `gow2_temporal = true` y `gow2_upscaler = "fsr4"` o `"fsr3"`, y baja `resolution_scale` para que el reescalador tenga margen. FSR 4 se comprobó en el juego con una Radeon RX 7800 XT (de 1280x704 a 2560x1408).
- La versión publicada no trae DLSS: la licencia del SDK de DLSS de NVIDIA no lo permite en una versión de código abierto (ver THIRD_PARTY_NOTICES.md). Solo las compilaciones hechas desde el código fuente con tu propia copia del SDK de DLSS de NVIDIA pueden usarlo.
- Paquetes de texturas: ver la siguiente sección. Probado con una textura reemplazada por una versión del doble de tamaño:

  ![Una textura de la pantalla de título reemplazada por una versión recoloreada de 2048x2048](docs/screenshots/texture-pack-test.jpg)

## Paquetes de texturas

Reemplaza las texturas del juego por versiones de mayor resolución (por ejemplo reescaladas 4x) para tu propio uso. Cada jugador crea sus paquetes desde su propia copia. Una textura reescalada o recoloreada sigue siendo una obra derivada del arte del juego, protegido por derechos de autor, así que no compartas paquetes hechos con las texturas del juego; este proyecto no los aloja ni los enlaza.

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

Horda y Versus por System Link no se han probado. Ver [ROADMAP.md](ROADMAP.md).

## Problemas conocidos

- **La puntería con el mouse cambió en la 0.1.1.** En la 0.1.0 el mouse se traducía a movimiento del stick derecho, y el juego ignoraba cualquier inclinación del stick menor al 30 % (su binding `DeadZone=0.3`). Ahora el mouse va por los ejes de mouse de PC del propio juego (`gow2_mouse_aim`), sin zona muerta, y los mandos conservan la puntería original. Las teclas todavía pasan por un mando emulado. Se agradece contar en un issue cómo se siente.
- **Las pistas muestran imágenes de botones de Xbox.** Los menús muestran las teclas ([Espacio], [E]...) mientras no haya un mando conectado y pasan a los botones de Xbox al conectar uno, y vuelven al desconectarlo (`gow2_prompts` = `auto`; `keyboard` y `controller` fijan una opción al arrancar; en Android la elección se sigue haciendo al arrancar). Un menú ya abierto cambia la próxima vez que se abre; las imágenes de botones dentro de las pistas del juego siguen siendo las de Xbox.
- **Se reportó iluminación verdosa** en la piel, las luces y las llamas del menú; todavía no se reproduce en el PC de desarrollo. Se agradecen capturas en un issue.
- **El cooperativo de tres o cuatro jugadores es nuevo.** La Etapa 2 se probó con cuatro copias en un mismo PC: los jugadores 3 y 4 conservan sus cuerpos y armas al cambiar de capítulo y viajar, reaparecen al morir, salen y vuelven a unirse, se restauran al cargar puntos de control, se ocultan durante las cinemáticas, no reciben daño a pie mientras un héroe conduce y el anfitrión los lleva junto a Marcus o Dom si quedan muy atrás. No se han probado las pantallas divididas, salir de un vehículo, la sección Ride-Reaver, unirse durante una cinemática, combates reales prolongados, cuatro PC por LAN o internet ni Android con 3 o más jugadores ([docs/COOP_N_PLAYERS.md](docs/COOP_N_PLAYERS.md)).
- **Todos los jugadores necesitan la misma versión publicada.** Todos deben usar la 0.1.2; con tres o más jugadores, las campañas con hashes cooperativos distintos permanecen ocultas, mientras que en cooperativo de dos jugadores no hay comprobación y se espera que la 0.1.1 falle al final del capítulo 2 del Acto 1.
- Solo se ha jugado el Acto 1 completo; las pruebas cooperativas llegaron a los capítulos 13, 16 y 27 mediante viajes entre capítulos.

## Compilar desde el código fuente

Ver [BUILDING.md](BUILDING.md). Los scripts principales son `scripts/build.ps1`, `scripts/build-sdk.ps1` (opcional: añade FidelityFX/FSR, reescaladores temporales y trazado de system link) y `scripts/package.ps1`.

## Cómo encontrar las funciones que codegen no detecta

`tools/harvest_hints.py` analiza la imagen descifrada (exportada desde Ghidra con `tools/ghidra/ExportImage.java`) contra una generación de código base sin pistas (`scripts/codegen-baseline.sh`) y emite `hints/harvested.toml`: thunks de ajuste, entradas `.pdata`, punteros de código guardados en `.rdata`/`.data` y direcciones construidas en el código con `lis`+`addi`/`ori`. Las pistas que dividirían una función existente se detectan desde el registro de codegen con `tools/hint_fixpoint.py` y se excluyen mediante `hints/denylist.txt`. Lo que siga faltando se encuentra en tiempo de ejecución con `scripts/run-harvest-loop.sh` (arrancar, leer el FATAL de "unregistered function", añadirlo a `hints/runtime.toml`, recompilar).

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
