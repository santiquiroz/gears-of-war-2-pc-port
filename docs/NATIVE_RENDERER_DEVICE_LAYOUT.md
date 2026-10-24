# Guest Direct3D device layout (native renderer, phase S1a)

Static reading of the recompiled PowerPC (`generated/default/gow2_recomp.N.cpp`, each instruction kept as a
comment). Nothing was run and the game was not launched. Offsets are bytes from the device pointer unless
marked. Register numbers are Xenos dword indices, the same convention as `tools/native/reg_scan.py`.
References are `file:line`; `N.cpp` means `generated/default/gow2_recomp.N.cpp`.

Confidence: **high** = read directly and consistent across functions; **medium** = consistent, one link
inferred; **low** = a guess from one access pattern. Phase 0: [NATIVE_RENDERER_PHASE0.md](NATIVE_RENDERER_PHASE0.md).
Design: [superpowers/specs/2026-10-23-full-native-renderer-design.md](superpowers/specs/2026-10-23-full-native-renderer-design.md).

Address arithmetic: `lis rX,V` loads `V << 16` as a signed 16-bit value, so `lis rX,-31984` = 0x8310_0000
(`82.cpp:15163` shows the immediate `-2096103424` = 0x83100000) and `lis rX,-31982` = 0x8312_0000.

## 1. Device pointer

- **Draw functions take the device in `r3`** and copy it at once: `mr r31,r3` in `sub_82980B20` (`183.cpp:22206`),
  `sub_8297FD30` (`78.cpp:19923`), `sub_82980210` (`1.cpp:18870`), `sub_829813F0` (`57.cpp:20246`).
- **Other library functions:** `sub_8298F600` keeps it in `r30` (`120.cpp:20237`), `sub_8298F378` in `r27`
  (`22.cpp:20525`), Clear `sub_82981ED8` in `r31` (`48.cpp:20161`), Resolve `sub_8298A128` in `r31` (`179.cpp:19908`),
  Swap `sub_8298D430` in `r31` (`54.cpp:20016`), `sub_8298C760` in `r31` (`198.cpp:19033`). No library function
  reads a global for it.
- **Global: guest 0x83105D40** (= 0x83100000 + 23872). High.
  - Written by `sub_829778E0` (`155.cpp:19176`, device allocation, section 5): `lis r30,-31984`, `stw 0,23872(r30)`
    at entry (19188-19198), `stw r31,23872(r30)` with the new device on success (19304).
  - Read by the game before calls: `sub_82803380` does `lis r25,-31984` (`82.cpp:15162`) and `lwz r3,23872(r25)`
    before `sub_8297F4E0` (15172-15174, SetIndices), `sub_8297F3C0` (15305-15321, SetStreamSource) and
    `sub_82980B20` (15355-15365). `sub_8297FD30`'s caller does the same (`43.cpp:13690-13699`).
- Not traced: where `r30` for `sub_82980210` (`104.cpp:15618-15634`) and `r22` for `sub_829813F0`
  (`47.cpp:20528-20534`) come from. Probably the same global.
- **Object size: 24192 bytes allocated, 24064 zeroed, aligned to 128**, raw pointer stored at `dev-4`
  (`155.cpp:19193-19207`). Highest offset seen: 23980 (Swap `54.cpp:20823`, creation `155.cpp:19294`).

## 2. How the device drives the packets

The device holds **shadow copies of register values and 64-bit dirty masks**. Each draw flushes the dirty groups
(type-0 register writes copied from the shadows), then emits the draw packet.

- **Dirty masks** at 0, 8, 16, 24, 32, 40. `sub_82980B20` clears 0-32 after flushing (`183.cpp:22237, 22255, 22380,
  22425, 22512`). 40 is only ANDed with 16 (22280).
- **Writers:** `sub_8298E220(dev, mask, base, shadow)` (`157.cpp:19291`): each run of set bits is one register write,
  register `base+k`, value `shadow[k]`. `sub_8298E5B8` (`72.cpp:19964`): one bit = 16 dwords (4 vec4).
  `sub_8298E460(dev, mask, shadow)` (`210.cpp:20162`): fetch slots at 0x4800, 6 dwords each.
- **Ring cursor:** `dev+48` write pointer (pre-increment `stwu`), `52` end, `56` high-water. When `48 > 56`,
  `sub_8298C1A0` runs (`201.cpp:19728`).
- **Mask bit → register** (rotate masks in `sub_82980B20` 22261-22525, and the same groups in Resolve
  `179.cpp:20550-20760`; group bases high, per-bit order medium):

| Mask | Bits | Writer, base | Shadow | Registers |
|---|---|---|---|---|
| 0 | all 64 | `E5B8`, 0x4000 | 1920 | vertex ALU constants, 256 vec4 |
| 8 | all 64 | `E5B8`, 0x4400 | 6016 | pixel ALU constants, 256 vec4 |
| 16 | 0-11 | `E220`, 0x2200 | 10548 | 0x2200 RB_DEPTHCONTROL, 0x2201 RB_BLENDCONTROL0, 0x2208 RB_MODECONTROL |
| 16 | 12-16 | `E220`, 0x2180 | 10528 | 0x2180 SQ_PROGRAM_CNTL, 0x2181 SQ_CONTEXT_MISC |
| 16 | 21-41 | `E220`, 0x2100 | 10444 | viewport group, 0x210F PA_CL_VPORT_XSCALE at 10504 |
| 16 | 42-57 | `E220`, 0x2000 | 10368 | 0x2000 RB_SURFACE_INFO, 0x2001 RB_COLOR_INFO, 0x2002 RB_DEPTH_INFO, 0x2003-5 RB_COLOR1-3_INFO |
| 24 | 0-31 | `E460`, 0x4800 | 1152 | 32 fetch slots × 6 dwords (texture **and** vertex fetch, section 3a) |
| 24 | 34-54 | `E220`, 0x2280 | 10596 | names unknown |
| 32 | 0-37 | `E220`, 0x2300 | 10680 | includes RB_COPY_* (section 6) |
| 32 | 38-45 | `E220`, 0x2380 | 10832 | names unknown |
| 32 | 49-54 | `E2E0`, 0x2388 | 10272 | 0x2388-0x239F: **4 registers (16 bytes) per bit**, 24 dwords (`56.cpp:20414-20641`, see below) |
| 32 | 56 | `E220`, 0x4900 | 10112 | 0x4900-0x4927: 8 BOOL + 32 LOOP constants. The mask is `lis r4,-256` = 0xFFFFFFFFFF000000 (40 bits), 183.cpp:22503 |

- **`sub_8298E2E0(dev, mask)`** (`56.cpp:20414`) has its base (r27 = 9096 = 0x2388) and shadow (r31 = dev+10272) built
  in. Each set bit copies 16 bytes with two `ld`/`std` (20616-20634) and advances the register by 4 (`rlwinm r10,r10,2`)
  and the shadow by 16 (`rlwinm r11,r10,4`). Before the writes it emits type-0 0x2007 (value 10396), 0x0A31 = 0x10000,
  0x0A2F-0x0A30 = {0, 0x1000} and a WAIT_REG_MEM `0xC0043C00` {3, 0xA31, 0, 0x80000000, 8}; it pads each register
  packet to 8 bytes with a type-2 filler (20597-20611). High.

- **Not in any mask: scissor.** `sub_8297CA20` (`173.cpp:20082`) writes a type-0 packet `0x00022080` (3 registers
  from 0x2080: PA_SC_WINDOW_OFFSET = 0, PA_SC_WINDOW_SCISSOR_TL, _BR) straight into the ring, from shadow dwords
  **10436 (TL) and 10440 (BR)**, 15-bit x in bits 0-14 and y in bits 16-30. Set by `sub_8297F248` (SetScissorRect,
  clamps, `17.cpp:20200-20214`). Also called by Clear, Resolve, and `184.cpp:19973`. It skips the write when the five
  surfaces 12440-12456 equal the copies at 12728-12744 (or are null) and `10940` bit 5 is set (`173.cpp` loop after
  20090). High for the registers, medium for the skip rule. 10432 is unexplained (window offset? guess).

### 2a. Other writers of the same shadows

- **Clear** (`48.cpp`) tests masks 16 and 40 (20185-20196, 20399) and flushes 0x2200 from 10548 (20365-20375) and
  0x2180 from 10528 (20391-20401) itself. **Resolve** flushes 0x2200, 0x2180, 0x2000, 0x2100, 0x2280, 0x2300 the
  same way (`179.cpp` relative lines 663-870 from 19888). A draw hook reading shadows can see values they left.
- **`sub_8298F600` (flush)** writes registers directly from shadows (`120.cpp`): RB_MODECONTROL from 10580
  (21008-21025), RB_COLOR_INFO from 10372 (21062-21079), SQ_PROGRAM_CNTL/CONTEXT_MISC from 10528/10532 (21120-21137).

## 3. Shaders and shader constants

Two shader objects: `dev+12692` (**A**) and `dev+12696` (**B**), read at the top of `sub_8298F600` (`120.cpp:20261-20266`).
B null skips the block at 20269.

- `S_A = A + *(A+64)` (20420-20428), `S_B = B + *(B+896) + 872` (20277-20286). `S_A+40` microcode offset, `S_A+44`
  microcode bytes (`>>2`, 20527-20530). `S_B+8 | S_A+48` → shadow 10528 (SQ_PROGRAM_CNTL), `S_B+12 | S_A+52` → 10532
  (SQ_CONTEXT_MISC) (20914-20924). Field meanings medium.
- **IM_LOAD (0x27) for A:** header `0xC0012700` (20493), payload `[(S_A+40 + *(A+24)) | 0x1000-flag | 1]`,
  `[(S_A+44) >> 2]` (20497-20534). Under the Xenia convention (type 0 vertex, 1 pixel) A is the pixel shader —
  low-medium: the middleware builder in section 7 emits IM_LOAD with type 2, so the convention is not settled.
  No IM_LOAD for B found in `sub_8298F600`.
- **Embedded microcode** (`sub_8298F210`, `18.cpp:19104`): INVALIDATE_STATE (0x3B) + IM_LOAD_IMMEDIATE (0x2B) with
  the microcode copied into the ring (19153-19226). Shader cache `sub_8298F4C0` (`7.cpp:20195`): 416-byte entries.
- **LOAD_ALU_CONSTANT** (`sub_8298F378`, `22.cpp:20511`, args `r4` descriptor, `r5` data base): records
  `{u16 start, u16 count, u32 offset}` after `*(r4+20)` (20523-20693); packet `0xC0022F00`,
  `[offset+r5 | 0x1000-flag, start·4, count]` (20655-20688), or a ring copy when `10940` bits 0-6 are set
  (20558-20633). Called for A (`A+40`, base `A+24`) and B (`B+872`, base `B+32`) (`120.cpp:20468-20474, 20720-20728,
  20870-20878`). **These constants are read from guest memory through the descriptor, not from shadows 1920/6016.**

### 3a. Vertex streams, index buffer, fetch constants

- **SetStreamSource = `sub_8297F3C0(dev, n, vb, offset, stride, maskbit)`** (`213.cpp:18733`). High.
  - Fetch dword 0 = `(vb+24) + offset` with the physical-address fixup (`clrlwi 3`, `+0x1000` for high addresses),
    stored at `dev + 1912 − 8n`; dword 1 = `(vb+28) − offset` (size) at `dev + 1916 − 8n`. Since the fetch shadow
    starts at 1152, that is **vertex fetch constant 95 − n** (2 dwords each, three per 6-dword slot).
  - The caller passes the mask bit: `(95 − n) / 3` via `mulli 21846` (`82.cpp:15300-15318`); `ORed into mask 24`.
  - So the "96 vertex fetch constants" are **the same 0x4800 block** as the 32 texture fetches (32 slots × 6 dwords
    = 96 × 2), not a separate block.
  - Stream buffer pointer at `12460 + 4n`; stride/4 byte at `12528 + n`. If that differs from `11832 + n`, mask 16
    gets `0x80000` (`oris 8`). 11832 looks like the stride the vertex shader/declaration expects (medium-low).
- **SetIndices = `sub_8297F4E0(dev, ib)`** (`49.cpp:19402`): stores `ib` at **12436** (high). Old buffer: if `10908`
  ≠ 0 it writes that fence value into `old+8`; otherwise, if `10912 & old+0`, appends an 8-byte record to a
  pending list `13524` (write) / `13528` (end), flushing with `sub_82989780` when full. Same code in SetStreamSource.
- **Vertex declaration:** not located as a separate field. The stride compare above is the only declaration-like
  state the draw path touches.

## 4. Draw arguments and DRAW_INDX fields

`sub_82980B20` (`183.cpp:22192`), DrawIndexed:

- `r4` → `r18`; `r18 & 0x3F` = primitive type, into word 2.
- `r5`: not read in the prologue; the game passes 0 (`82.cpp:15351`). Guess: base vertex index (XDK signature).
- `r6` → `r21`: start index, scaled by 2 or 4 (22578-22579, 22628-22629) and added to the index base.
- `r7` → `r19`: index count; above 65535 the draw is split (22549-22574). `r8` = device byte 12187 (22577).

Inline packet (`10940` bit 0 clear, 22656-22681), header `0xC0032201` (type 3, opcode 0x22 DRAW_INDX):

| Word | Value | Meaning (medium) |
|---|---|---|
| 1 | `(lookup(r23+r8) | 0x100) & 0x13F` | draw initiator (source select, index size) |
| 2 | `(count << 16) | prim` (`| 0x800` if bit 31 of `*(12436)`) | count and primitive type |
| 3 | `(start << scale) + *(12436+24)` | index buffer address + start |
| 4 | `(count & 0xFFFFFF) | bits 31-30 of *(12436)` | count and index format |

Segment path (`10940` bit 0 set, 22684-22714): same words into a 16-byte record at `13256`.
`sub_829813F0` (`57.cpp:20227`): user-pointer draw; allocates `21 × arg2` dwords (20307), copies 4 floats per vertex
from arg6 (20321-20336), emits DRAW_INDX_2. `sub_8297FD30`/`sub_82980210` read `11832`/write `12528` (stream 0
stride, section 3a) before drawing (`78.cpp:19930-19941`, `1.cpp:18896-18917`).

## 5. Device offset table (beyond section 2)

| Offset | Meaning | Conf. | Evidence |
|---|---|---|---|
| 0-40 | dirty masks | high (0-24), medium (32), low (40) | section 2 |
| 48 / 52 / 56 | ring write / end / end − 160 bytes | high | creation `198.cpp:19417-19423` |
| 60 | set to 1 at allocation (ref count?) | low | `155.cpp:19213` |
| 1152-1919 | fetch constants, 32 × 24 bytes; vertex fetch 95−n at 1912−8n | high | section 3a |
| 1920 / 6016 / 10112 | vertex ALU / pixel ALU / BOOL shadows | high / high / medium-high | section 2 |
| 10368-10391 | RB_SURFACE_INFO, RB_COLOR_INFO, RB_DEPTH_INFO, RB_COLOR1-3_INFO | high | Resolve picks 10372 or `4·(2594+i)` by copy source `179.cpp` +532..+560 |
| 10436 / 10440 | scissor TL / BR (packed) | high | section 2 |
| 10444-10527 | viewport group (0x2100-), 10504 = VPORT_XSCALE | medium-high | `183.cpp:22370-22377` |
| 10528 / 10532 | SQ_PROGRAM_CNTL / SQ_CONTEXT_MISC | high | section 3 |
| 10548 / 10552 / 10580 | RB_DEPTHCONTROL / RB_BLENDCONTROL0 / RB_MODECONTROL | high | section 2a |
| 10560 | shadow 0x2203, passed to `sub_8298DE88` | low | `183.cpp:22287`; Resolve +663 |
| 10776-10788 | RB_COPY_CONTROL, _DEST_BASE, _DEST_PITCH, _DEST_INFO (0x2318-0x231B) | high | Resolve `179.cpp` +478..+502, mask 32 `0x3C00` |
| 10800 / 10804 / 10820 | 0x231E / 0x231F / 0x2323 (guess: RB_COLOR_CLEAR, _LO, a copy field) | low (names) | Resolve +554..+569, +442 |
| 10880 / 10888 / 10892 | copied global / handle / `process type == 2` flag | low | `155.cpp:19214-19222` |
| 10896 / 10900 | two allocated buffers (freed and reallocated by `8298C760`) | medium | `198.cpp:19143-19159, 19284-19297` |
| 10908 | fence value (init 3); written into released resources at `+8` | medium | `198.cpp:19429-19431`; section 3a |
| 10912 | resource-flag mask for deferred release | low-medium | `49.cpp` SetIndices |
| 10940 / 10941-10943 | flags (bit 0 = segment draws, bit 5 = scissor-skip rule) / state bytes | medium / low | `183.cpp:22649`; `173.cpp` |
| 12187 / 12122 | prim-type lookup byte / table | low | `183.cpp:22518-22598` |
| 12436 | index buffer (`+0` bit 31 = 32-bit, `+24` address) | high | SetIndices; draw |
| 12440-12452 | color render targets 0-3 (`+28` = RB_COLOR_INFO value) | medium-high | `120.cpp:20566-20577`; Resolve +890..+944 |
| 12456 | depth-stencil surface (`+24` low 2 bits = format class) | medium | 5th entry; copy source 4 = depth; Clear `48.cpp:20186` |
| 12460 + 4n / 12528 + n / 11832 + n | stream n buffer / stride÷4 / expected stride | high / high / medium-low | `213.cpp` SetStreamSource |
| 12648-12660 / 12676-12688 | float / int clamp bounds used by Clear and Resolve | low-medium | `48.cpp:20198-20317` |
| 12692 / 12696 / 12700 | shader A / shader B / last A id | low-medium | section 3 |
| 12728-12744 | copies of 12440-12456 (last surfaces set into EDRAM state?) | low | `173.cpp`, Resolve +890 |
| 13248-13260 | draw segment allocator | medium | `183.cpp:22684-22714` |
| 13524 / 13528 | deferred-release list write / end | medium | section 3a |
| 13544 / 13548 | display width / height (compared with 720, 480, 576) | medium | Swap `54.cpp:20527-20541` |
| 13608 / 13612 | front-buffer format (low 6 bits, 7 or 54) / flags | low | Swap `54.cpp:20505-20525` |
| 14836 / 14840 / 14896-14936 | ring buffer allocation and bookkeeping (base 14904, end 14908 with `0x0BADF00D`) | medium | `198.cpp:19366-19425` |
| 16560 | frame counter, +1 each Swap | high | `54.cpp:20024-20034` |
| 16708 / 16728 | Swap-exchanged word / 4800-byte buffer | low | `54.cpp:20492`; `155.cpp:19224` |
| 21544-21552 / 21624-21632 | display mode halfwords / range checked in Swap | low | `54.cpp:20568-20572, 20111-20150` |
| 23980 | `0x0C000001` at creation; read in Swap | low | `155.cpp:19229`; `54.cpp:20823` |

## 6. Resolve, Swap, creation

- **Resolve `sub_8298A128`** (`179.cpp:19888-22059`). Computes RB_COPY_CONTROL into 10776 (low 3 bits = copy source:
  0-3 color, 4 depth), DEST_BASE 10780, DEST_PITCH 10784, DEST_INFO 10788, sets mask 32 `|= 0x3C00`; optional clear
  values 10800/10804 (`|= 0xC0`) from the RB_COLOR_INFO of the source (10372 or 10380-10388). Clamps the rect against
  scissor 10436/10440. Flushes groups itself, then writes direct packets (registers 0x2208, 0x2200, 0x2293, 0x2312,
  0x200D seen as immediates near +1262..+1412; not decoded) and copies a block via `sub_82A92B70` (+1243).
- **Swap `sub_8298D430`** (`54.cpp:20003`). Frame counter 16560; `0xC0025800` = type 3 opcode 0x58 (guess:
  EVENT_WRITE_SHD) with payload `0x80000003, address, 0xDEADBEEF` (20205-20245); later a type-0 write to register
  0x1844 (byte 0x6110, the AVIVO primary surface address — guess) with the front buffer address (20854-20880).
- **Creation.** `sub_829778E0` (`155.cpp:19176`) allocates and zeroes the device, sets the global, then calls
  `sub_82990B80` (init, not read) and returns `0x8007000E` on failure. `sub_8298C760` (`198.cpp:19020`) is the
  **ring (re)initialiser**: waits on 10984/10988, frees 14836/14840/10896/10900, allocates the ring (default 0x200000,
  `lis r28,32`), writes `0x0BADF00D` at the end, sets 48/52/56, and inits 10908 = 3.

## 7. Phase-0 "inlined" list: what each writes

- `sub_8227A648` (`111.cpp:781-871`): **SHA-1** (constants 0x67452301 … 0xC3D2E1F0, then update/final, 20-byte copy).
  No PM4; the scan hit is a false positive. High.
- `sub_82C67850` / `sub_82C67A28` / `sub_82C67C08` (`117.cpp:26863`, `220.cpp:26735`, `93.cpp:27429`): a
  middleware **command-buffer builder** with assert strings. Gets a 16-dword buffer from a callback at `obj+236`
  (`220.cpp` up to 26860), writes `0xC00B2500` (SET_STATE, 12 dwords: six `*(*(r+32)+4) | *(r+16)` resource
  addresses) and `0xC0012700` IM_LOAD with type field 2; C08 also `0xC0002B00` IM_LOAD_IMMEDIATE (27726). Does
  not touch the device or its shadows; its packets reach the CP only if the buffer is later executed. Medium.
- `sub_82529CC0` (`152.cpp:7422-12339`): game code, no call into the D3D library found. Reads globals at
  0x83120000 (`lis -31982`: 14348 → +80 → +60, 7463-7474), increments counters 13416-13428 and stores 13016 of
  globals. The `ori r12` immediates (9081-9310) are vector masks, not packets. No device write seen. Medium.
- `sub_824C97C0` (`143.cpp:7044-7154`): game code; creates an object via `sub_824C4168(*(0x8312380C))`, stores it at
  `this+280` and fills fields 536 and 172. No device access. Medium.
- `sub_82EB7700` (`158.cpp:33850`): popcount. High.

## 8. Verdict (gate G1)

- **Readable at the four draws (high/medium-high):** ALU constant shadows (1920, 6016, 10112) — but the shaders'
  own constants come by LOAD_ALU_CONSTANT from the descriptors at A+40/B+872; fetch constants (1152-1919, texture and
  vertex); RB_SURFACE/COLOR/DEPTH_INFO and COLOR1-3 (10368-10391); viewport (10444-); scissor (10436/10440);
  RB_DEPTHCONTROL/BLENDCONTROL0/MODECONTROL (10548/10552/10580); SQ_PROGRAM_CNTL/CONTEXT_MISC (10528/10532);
  render targets (12440-12456); index buffer (12436); streams (12460+, 12528+); draw arguments.
- **With caveat (medium):** shader A microcode (S_A+40/44); the stage of A.
- **Unknown:** B's microcode location; IM_LOAD type convention; names in groups 0x2280, 0x2300 (beyond RB_COPY_*),
  0x2380, 0x2388 (PA_SU_SC_MODE_CNTL is 0x2205 in 0x2200 group by Xenia numbering — not confirmed from code);
  vertex declaration object; `r5` of DrawIndexed; the middleware builder's buffer consumer.

## 9. Cheapest runtime check per unknown

1. **Global.** At the first draw read `*(uint32_t*)0x83105D40` (big-endian) and check it equals `r3` of the draw.
2. **Shadow = CP state.** At each draw copy 10368-10391, 10436-10443, 10528-10535, 10548-10595 and compare with the
   CP register file at the following DRAW_INDX. Repeat on the first draw after a Clear and after a Resolve.
3. **Fetch.** Compare `dev+1152..1919` with CP registers 0x4800-0x48BF at the draw; check stream 0 = slot 95.
4. **Shader A/B and stage.** Log 12692/12696 and the IM_LOAD payload; compare the type bit with which shader the CP
   binds as pixel (`active_pixel_shader_`); search the ring for an IM_LOAD whose address matches B.
5. **LOAD_ALU source.** Decode A+40/B+872 records at a draw and compare with the next `0xC0022F00` payloads.
6. **Unnamed groups.** Log masks 24/32 and shadows 10596-10911 at draws that change blend/cull/stencil only.
7. **DrawIndexed `r5`.** Log `r5` across callers; any non-zero value tells whether it offsets the vertex fetch.

## Limits

No run, no packet trace. Not read: `sub_82990B80`, `sub_8298E710`, `sub_8298E2E0`, `sub_8298DE88`, `sub_8298C1A0`,
`sub_8298AFC0`, `sub_8298B5B0`, the tail of Resolve past its group flushes, the middle of Swap (20250-20830).
Relative Resolve lines (`+N`) are counted from `179.cpp:19888`.
