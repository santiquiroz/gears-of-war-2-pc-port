# S3: dónde van los draws no indexados (sub_82980210 / sub_8297FD30)

Todas las referencias son `generated/default/gow2_recomp.N.cpp:línea`. `dev` = dispositivo D3D (r31). Hallazgos solo por lectura del código generado; no se ejecutó el juego. Lo marcado "hipótesis" no está verificado.

## Resumen

1. **El draw SÍ se escribe en el anillo, en las dos rutas (inline y segmento).** Ninguna ruta lo manda a otro sitio.
2. **Ninguna de las dos funciones confirma el puntero de escritura `dev+48` en el camino de éxito.** Dejan el nuevo puntero en `dev+13444`. El *llamador* hace `dev+48 = dev+13444` después de copiar los vértices (memcpy `sub_82A92B70`). Son funciones tipo "BeginVertices": devuelven r3 = 0 y un puntero de vértices (`dev+13448`).
3. Por eso un hook que lea `dev+48` justo al volver de la función no ve nada de lo que esa llamada escribió. Todo lo escrito en esa llamada (estado + draw) queda entre el `dev+48` de entrada y `dev+13444`.
4. En la ruta de segmento (`10940` bit 0), el draw no va suelto: lo envuelve una secuencia con `0xC0006000`, y el recorrido de paquetes puede estar parando ahí (hipótesis, ver 3.3).

## 1. Dónde escribe cada función el draw

### sub_82980210 (`gow2_recomp.1.cpp`)

- El puntero de anillo local es r30. Se lee de `dev+48` en la línea 19227 (`lwz r30,48(r31)`; offset 375 dentro del rango 18852+). Si pasa del límite `dev+56` se llama a `sub_8298C1A0`.
- Selector: `lbz r11,10940(r31); clrlwi. r11,r11,31` (~19445). Bit 0 limpio es el camino inline. Bit 0 puesto es el camino de segmento (`bne 0x82980614`).
- **Inline** (~19447-19453): `0xC0032201` (lis -16381 / ori 8705; DRAW_INDX con 4 palabras de payload), luego r24, r29, r25, r27 con `stwu` sobre r30. Salta a `loc_829806D8`.
- **Segmento** (1.cpp:19455-19590). Es el mismo flujo que `183.cpp:22685-22714`:
  - Reserva un registro de 16 bytes en la lista `dev+13248`: cursor `13256`, fin `13260`. Si `cursor+16 > fin` llama a `sub_8298B080(dev+13248)` para obtener un bloque nuevo.
  - **`record[0] = r30`**, es decir, la dirección del anillo justo antes del segmento (la última palabra escrita). Los 12 bytes restantes los rellena la GPU (ver abajo). Avanza `dev+13256 += 16`.
  - Palabras que se escriben en el anillo, en orden:
    1. `0xC0006000`, `0xFFFFFFFF` (SET_BIN_MASK, máscara -1).
    2. `0xC0032201`, r24, r29, r25, r27 (el DRAW_INDX, igual que el inline).
    3. `0xC0006000`, `0x80000000` (SET_BIN_MASK, restaura).
    4. `0xC0015A01`, `26`, `phys(record+4) | 1` (EVENT_WRITE_EXT, evento 26 escribe en `record+4`; la dirección va con la conversión de dirección física `rlwinm 12,20,31 / +512 / &0x1000 / + (addr & 0x1FFFFFFE)`).
    5. `0xC0004601`, `25` (EVENT_WRITE, evento 25).
  - Los opcodes/eventos son lo que dice la aritmética de las constantes; no verifiqué sus nombres contra una tabla de PM4.
- Cierre (`loc_829806D8`, 1.cpp:19592-19605): `dev+24 |= 1`, `dev+13444 = r11` (último puntero escrito, el nuevo wptr), `*(out) = r28`, `dev+13448 = r28` (puntero de vértices), `dev+13452 = r26`, `13456 = r19`, `13460 = r18`. Devuelve r3 = 0.
- **El único `stw r30,48(r31)` está en el camino de error** (`loc_8298070C`, ~19620).

### sub_8297FD30 (`gow2_recomp.78.cpp:19909+`)

- Mismo patrón. `lwz r30,48(r31)` en 20268. El `stw r30,48(r31)` de 20298 está **solo en el camino de fallo** de `sub_8298B5B0` (reserva de memoria de vértices).
- Draw inline (20431-20437): `0xC0012201` (lis -16383 / ori 8705; DRAW_INDX con 2 palabras), r29, `((r22&63)|(r24<<16)|128)`.
- Draw segmento (20458-20530): mismo registro de 16 bytes en `13256` y la misma envoltura `0xC0006000`/`0xC0012201`/`0xC0006000`/`0xC0015A01`/…
- Cierre (20595-20602): `dev+24 |= 1`, `dev+13444 = r11`, `dev+13448 = r28` (puntero de vértices devuelto en r3), `dev+13456 = r23`.

### DrawIndexed (`gow2_recomp.183.cpp:22649-22714`)

Idéntico selector y registro (también hay copias en `150.cpp:20315-20343` y `88.cpp:20685-20713`; el estado de `78.cpp:20458` es el de sub_8297FD30). El layout de `NATIVE_RENDERER_DEVICE_LAYOUT.md` ("registro de 16 bytes en 13256") es correcto.

## 2. Quién consume los registros

- La lista en `dev+13248` es un asignador de bloques: `+0` primer bloque, `+4` bloque actual, `+8` cursor, `+12` fin, `+16` dueño (`dev`). Se inicializa en `67.cpp:19960-19976` (`stw r31,13264`, ... `13248`). Hay listas gemelas en `13268`, `13288`, `13348` (cada una tras un `bl 0x8298b080`).
- `sub_8298B080` (`169.cpp:19925`) pide un bloque de 4224 bytes con `sub_8298B5B0`, escribe en el bloque anterior un enlace a la dirección física del nuevo (`(phys) - 0x40000000`), y deja `+4 = bloque`, `+8 = bloque+4`, `+12 = bloque+4220`. O sea, bloques encadenados (263 registros de 16 bytes por bloque, tras 4 bytes de enlace).
- **Consumidor de la lista 13248**: solo `sub_82989438` (`118.cpp:20166+`; lectura en 20346). No copia los draws al anillo. Cierra el segmento escribiendo, en la lista `dev+13368` (cursor `13376`, fin `13380`), una entrada `{0x89000000, phys(head de 13248), phys(head de 13348)}`, y entradas `0x88000000, dev+...` / `0x87000000` (118.cpp:20300-20440). También limpia bits de `10940`/`10943` (20449). Es decir, se construye una **tabla de segmentos** (cabeza de lista de registros por segmento) que consume un reproductor posterior. No trazé ese reproductor (candidatos: lectores de `13376`/`13380` en `195.cpp:20235`, `214.cpp:20486`, y `sub_8298C1A0`).
- Los otros accesos a `13256/13260/...` en `200.cpp`, `3.cpp`, `51.cpp`, `7.cpp`, `20.cpp`, `145.cpp`, `162.cpp`, `67.cpp:4837+` usan `r31 = 0x8312xxxx` (un global distinto: `lis r31,-31982`), **no** son el dispositivo. No confundirlos.
- Cómo llegan los registros a la GPU: el draw ya está en el anillo, así que la GPU lo ve por el camino normal. Los registros (`record[0]` = posición en el anillo del segmento) sirven para *replay por tile* (predicated tiling): los `SET_BIN_MASK` envolventes y el `EVENT_WRITE_EXT` (que escribe contadores en `record+4`) son los elementos de esa maquinaria. No hay INDIRECT_BUFFER (`sub_8298BAB0`) en este camino.

## 3. ¿El estado visto pertenece al mismo draw? Dónde enganchar

### 3.1 Sí
El estado que ve el recorrido (constantes 0x03FF4000/0x03FF4400, IM_LOAD, fetch, BOOL/LOOP) lo escribe la propia función en r30 antes del draw (`1.cpp` 19226+ a 19450). El draw va a continuación en el mismo bloque contiguo. No hay flush posterior que lo emita.

### 3.2 Hook recomendado
Enganchar **a la salida de sub_82980210 y sub_8297FD30** y recorrer los paquetes de `[dev+48 de entrada, dev+13444 de salida)` en lugar de `dev+48` de salida:

- Argumentos: dispositivo en r3 (`dev`); los parámetros de draw los dejan los llamadores en r4... (vértices: tamaño/cuenta, `*(r1+308)` es el puntero de salida).
- Puntero de escritura "después": **`dev+13444`** (no `dev+48`). Es una dirección virtual de anillo, igual que `dev+48`.
- Solo vale si el retorno es `r3 == 0` (éxito). En el fallo se escribe `dev+48` y no hay draw.
- Alternativa equivalente: enganchar el commit del llamador (`lwz r11,13444(r31); stw r11,48(r31)`) en `78.cpp:15951` (sub_82803830), `79.cpp:2774` (sub_82336C00), `84.cpp:18341` (sub_829801C8), `104.cpp:15676`, `43.cpp:13726/15226`, `126.cpp:17500`. Es más frágil, hay muchos llamadores.

### 3.3 Por qué el recorrido actual ve estado y no draw (hipótesis, sin verificar)
- Los datos son consistentes con estas dos explicaciones; no puedo distinguirlas solo con el código:
  1. El recorrido termina en `dev+48`, que sigue en el valor de entrada. Con esto no vería **nada** de esta llamada. Si ve el estado, puede que el hook lea `dev+48` en el *siguiente* hook, o que `sub_8298C1A0` (llamada cuando se llena el anillo) haya movido el puntero. Revisar cómo calcula el "después" `src/gow2_native_frontend.cpp`.
  2. Para el camino de segmento, el recorrido llega al estado pero **no reconoce `0xC0006000`** (opcode 0x60, SET_BIN_MASK, un solo dword de payload) y se detiene antes del draw. Conviene comprobar que el parser tiene este opcode (tamaño = `((hdr>>16)&0x3FFF)+2` palabras, como cualquier tipo 3) y que `0xC0015A01` / `0xC0004601` también se saltan bien.
- Prueba barata para decidirlo: registrar `dev+13444`, `dev+48` y `*(u8*)(dev+10940) & 1` a la entrada y salida de una llamada, y volcar las palabras del anillo entre el `dev+48` de entrada y `dev+13444`. Se debería ver `0xC0032201` (sub_82980210) o `0xC0012201` (sub_8297FD30), con o sin envoltura `0xC0006000 ... 0xC0006000 ... 0xC0015A01 ... 0xC0004601`.

### 3.4 Cómo reescribir
Como el draw está en el anillo, el patrón de S2 (reescribir DRAW_INDX en un NOP con puntero a la instantánea) sirve igual. En la ruta de segmento hay que conservar la longitud exacta: sustituir solo el DRAW_INDX y dejar la envoltura `SET_BIN_MASK`/`EVENT_WRITE_EXT`/`EVENT_WRITE` intacta, porque el registro `record[0]` ya apunta a la posición del anillo y el EVENT_WRITE_EXT escribe en `record+4`.

## Puntos sin resolver
- Quién fija el bit 0 de `10940`: la candidata es `118.cpp:20449-20596` (`sub_82989438`: `andi 223`, luego `rlwimi r11,r10,0,24,30` con r11 = 1 si se cumplen las condiciones sobre `12440-12456`/`12728-12744`, `12187`, bits 5-6 de `10940`). Otros `stb ...,10940` en `119.cpp:16237`, `155.cpp:19714`, `181.cpp:20357`, `184.cpp:20385`, `195.cpp:20167/20230`, `37.cpp:18910` sin revisar.
- El reproductor de la tabla de segmentos (`dev+13368`) no está trazado.
- No he comprobado qué hacen los demás llamadores de sub_82980210 (`104.cpp:15634`, `32.cpp:15337/15416`) tras volver; solo se confirmó el patrón de commit en `104.cpp:15676`.
