# S3: Clear (sub_82981ED8) packet inventory

Read-only static analysis. Files: `48.cpp` = gow2_recomp.48.cpp (sub_82981ED8, 20141-21140), `99.cpp` (sub_82981970, 19118-19910),
`47.cpp` (sub_829816E0, 20284-20645), `57.cpp` (sub_829813F0), `76.cpp` (sub_8298DE88, 20917), `157.cpp` (sub_8298E220, 19291),
`201.cpp` (sub_8298C1A0, 19728 = ring-space check/wrap). Line numbers are the `// instruction` comments.
Ring writes are `stwu rX,4(ptr)` with ptr = dev+48 (pre-increment). "hdr" = packet header dword.
Not decoded: the inside of sub_8298DE88 and sub_8298E220 (see 3.3). Register names marked (?) are unverified.

## 0. Call structure

```
Clear sub_82981ED8 (dev r3, flags r4, rects r5, colour r6, z f1, stencil = r8 (r22), 20161-20165)
 |- clamp rects (20198-20331), early outs: no depth surface (12456==0) drops flags &= ~0xF0, flags==0 -> return (20186-20195)
 |- flush pending shadows (sub_8298DE88 / sub_8298E220), see 2
 |- prelude packets A-F directly into the ring (3)
 |- if flags&0xF == 0 : sub_82981970(dev, flags, r5=-1, ...)          (20934-20955)  depth/stencil only
 |  else: type-0 pre-packet G, then for rt=0..3 with (flags>>rt)&1 and RT surface dev+12440+4*rt != 0:
 |        sub_82981970(dev, flags, rt, ...); flags &= ~0xF0 afterwards  (21036-21080)   depth/stencil goes with the first RT
 |- sub_8297CA20 (scissor from shadows 10436/10440) (21087-21113), dirty bits (4)
sub_82981970 (99.cpp): per call: state packets (3.2) then sub_829816E0 once per rect (or once for the whole rect)
sub_829816E0 (47.cpp): tiles one rect, calls sub_829813F0 once or twice
sub_829813F0 (57.cpp): the rectangle-list draw (S1D); emits fetch 0-5, 0x2000/0x2001, 0x2301, 0x2200, 0x2208, DRAW_INDX_2
```

Clear's own arguments to sub_82981970: r4 flags, r5 rt index or -1, r6..r9 = clamped x0,y0,x1,y1 (r24,r25,r26,r28 in Clear),
r10 = colour ptr (r23; if the caller passed NULL it points to a 16-byte constant at 0x820xxxxx, `lis -32227; addi 2656`, 20171-20182),
f1 = z, stack 92(r1) = r22 (stencil value).

## 1. Shaders Clear uses (IM_LOAD_IMMEDIATE 0x2B, embedded microcode, memcpy via sub_82A92B70)

No IM_LOAD (0x2A) and no INVALIDATE_STATE (0x3B) with microcode. Table base is the global block `r29 = 0x82090000 - 10608 = 0x8208D690`
(`lis r10,-32247; addi r29,r10,-10608`, 20615/20623).

| Shader | Packet | Source | Size |
|---|---|---|---|
| vertex | hdr 0xC01C2B00 (count 28 = 29 payload dwords) {0 (type 0), 27 (dwords), 27 dwords copy} (20603-20643) | **0x8208D6D8** (`r4 = r29+72`, 20625) | **27 dwords = 108 bytes** (`li r5,108`, 20617) |
| pixel | hdr 0xC00A2B00 (count 10) {1 (type 1), 9 (dwords), 9 dwords copy} (20690-20718) | **0x8208D690** (`r4 = r29`, 20702) | **9 dwords = 36 bytes** (`li r5,36`, 20704) |

Both are fixed constants (no table indexing); microcode type convention: 0 = vertex, 1 = pixel (the first payload dword).
Both are written once per Clear call (prelude), not per rectangle. The shadows dev+12692/12696 (shader objects A/B) are untouched:
`sub_8298F600` is never called by Clear or by sub_829813F0, so the CP keeps Clear's shaders until the next normal draw
(which re-flushes because of the dirty bits in 3.4; bit 0 of dev+10943 is cleared, see packet C).

## 2. Flushes of pending shadows Clear does before its own packets (all from device shadows)

Each uses `sub_8298E220(dev, mask_slice, base_reg, shadow_ptr)` then clears the bits in dev+16/24/32:
- 20347-20354: `sub_8298DE88(dev, dev+16 mask & dev+40, 10560)` only if `dev+16 & dev+40` is non-zero; result stored back to dev+16.
- 20365-20375: 0x2200 group from dev+10548 (`li r5,8704`). 20391-20399: 0x2180 from 10528 (`8576`).
- 20427-20435: 0x2000 from 10368 (`8192`). 20463-20471: 0x2100 from 10444 (`8448`).
- 20505-20513: 0x2280 from 10596 (`8832`, mask dev+24). 20541-20549: 0x2300 from 10680 (`8960`, mask dev+32).
- Each only when its mask slice is non-zero; the masks are then cleared. So the shadows are flushed to the CP **before** Clear overrides
  them; the device shadows are not rewritten by Clear afterwards (only dirty bits set, 3.4).
- 20559-20569: ring space check (`dev+48 > dev+56` -> sub_8298C1A0).

## 3. Packets, in order

### 3.1 Prelude (Clear, 48.cpp)

| # | hdr | Meaning | Payload (source) | Lines |
|---|---|---|---|---|
| A | 0xC0006000 SET_BIN_MASK_LO (opcode 0x60) | only if `dev+10940 & 0x20` | {dev+12708 (shadow)} | 20575-20592 |
| B | 0xC0003B00 INVALIDATE_STATE (opcode 0x3B) | always | {0x300 (const)} | 20597-20619 |
| C | 0xC01C2B00 IM_LOAD_IMMEDIATE (vertex) | always | {0, 27, 27 dwords from 0x8208D6D8} | 20603-20643 |
| D | type-0 0x000005C8 (1 reg: 0x5C8) | only if `dev+10943 & 1`, then clears that bit | {0x00020000 const} | 20664-20687 |
| E | 0xC00A2B00 IM_LOAD_IMMEDIATE (pixel) | always | {1, 9, 9 dwords from 0x8208D690} | 20690-20718 |
| F | type-0 0x00012180 (regs 0x2180,0x2181) | always | {0x10000001, 0} const = SQ_PROGRAM_CNTL, SQ_CONTEXT_MISC (the doc's 0x2180 group) | 20723-20741 |
| G | type-0 0x00022100 (0x2100-0x2102) | always | {0xFFFF, 0, 0} const (viewport group start) | 20761-20797 |
| H | type-0 0x00002293 | | {0} | 20779-20783 |
| I | type-0 0x00022204 (0x2204-0x2206) | | {0x10000, 0x10000, 0x300} const (PA_SU_SC_MODE_CNTL area (?)) | 20787-20845 |
| J | type-0 0x00002312 | | {0xFFFF} | 20801-20851 |
| K | type-0 0x0000200D | | {0} | 20805-20857 |

Notes: the payloads of G-K are all immediates (`li/lis/ori`), i.e. fixed values, independent of the device shadows. The ring pointer `r3`
is stored back to dev+48 at 20930.

### 3.2 Colour path pre-packet, in the loop (only when flags & 0xF != 0, once, before the per-RT loop; 20961-21019)
type-0 0x00002104 {0xFFFF}, 0x00002202 {0}, 0x00012201 {0x10001} (hdr `r11` values 8452, 8706, 8705; payload r30=0xFFFF, 0, 0x10001).
Pending bits then: dev+16 |= 1<<37, 0x200, 0x400 (21021-21033).

### 3.3 sub_82981970, per call (99.cpp)

State derivation (no packets): `r15 = dev+10368` (RB_SURFACE_INFO shadow, 19151); `r21 = rt_surface(rt)->+28` (19169-19179, rt = *(dev+12440+4*rt)),
colour format class `(r21>>16)&0xF`, with two format remaps (19181-19202); `r30 = dev+10560` (shadow 0x2203, passed to sub_8298DE88) with
flags 0x10 -> `r20=118 (0x76)`, `r30|=1`; flags 0x20 -> `r20|=0x8701`, `r30|=4`; flags 0x40/0x80 merge the stencil value (stack arg) into r30
(19205-19253). `r14 = 5` (depth-only call, rt=-1) or `4` (colour RT) = the RB_MODECONTROL value passed on.

| # | hdr | Condition | Payload (source) | Lines |
|---|---|---|---|---|
| L1 | type-0 0x0000210D (1 reg) | flags & 0x20 | {(stencil value, stack arg) with byte 2 forced to 0xFF} | 19268-19282 |
| L2 | whatever sub_8298DE88(dev, 0x100, r30) emits (dirty mask 0x100 + shadow 10560 with the new value) | always | not decoded; it writes 0x2200-group/depth-control related registers incl. 0xC0006000 SET_BIN_MASK from dev+12708/12712, 0x2203 (8707), 0x2000, 0x2104 from dev+10460 etc. (76.cpp 21096-21420) | 19291-19297 |
| L3 | type-0 0x00022080 (0x2080,0x2081,0x2082) | always | {0, 0, 0x20002000} const = window offset 0, scissor TL 0, BR 8192x8192 (fully open) | 19314-19347 |
| L4 | type-0 0x00002203 {0}, 0x00002002 {r21 & 0xFFF}, 0x0000210D {(packed colour & 0xFF) \| 0x00FF0000} | only the **colour fast path**: flags & 0x30 == 0, format class not 5/7/15, rect >= 80x16 / 80x8 / 40x16 depending on MSAA bits of dev+10368; colour packed by sub_82989D00 from the float4 colour | 19402-19483 |
| L5 | SET_BIN_MASK 0xC0006000 {dev+12708 & 0x55555555} | only if `dev+10943 & 0x20`, `dev+10940 & 0x40`, and the mask is non-zero (multi-bin / predicated tiling), once for the whole rect, then calls 829816E0; later the same 0xC0006000 with `{3<<(2i)} & mask` per tile bin around each rect-subset | 19635-19884 |

Dirty bits set in sub_82981970: dev+16 |= 1<<28 (0x10000000) after L1 (19284-19288); on the fast path |= 0x100, 1<<55, 0x10000000 (19487-19499).
Which of L1/L4/L5 apply: L1 = stencil flag, L4 = colour fast path, L5 = binning. It then loops over the rects (dev+12748 = count, rects at dev+12752.., 12756
step 16, clamping each against the call's rect, 19744-19885) or, without binning, calls sub_829816E0 once (19602-19629).

### 3.4 sub_829816E0 and sub_829813F0

sub_829816E0 args: r3 dev, r4..r7 = x0,y0,x1,y1, r8 = colour ptr, r10 = RB_SURFACE_INFO shadow (dev+10368), f1 = z, stack: 84 = surface/colour-info `r21`,
92 = depth-control `r20` (0x76/0x8701 or 0), 100 = `r17` (depth-control for the fast-clear piece: 0x8777 or r20), 108 = mode `r14`, 116 = `r18`
(mode for the fast piece: 5 or r14). It rounds the rect to the EDRAM tile (40x16, 80x16 or 80x8 by MSAA bits `(r10>>16)&3`, 20314-20406, `divw/mullw`):
- aligned interior non-empty: up to **3 unaligned border rects** (left, top/bottom, right: 20431-20505) go out in **one** call to sub_829813F0
  (r4 = count <= 4, r5 = stack rect array, args r8=r10 shadow, r9=r21, r10=r20, stack84=r14, 20507-20532), plus **one** call for the aligned interior
  (r4=1, coordinates `sraw` by the tile shift, r10 bit 17 set = fast-clear/“hi” bit, r9, depth-control r17, mode r18, 20536-20601).
- otherwise (rect too small/thin): **one** call to sub_829813F0 with the whole rect (20606-20636).
So per rectangle: 1 or 2 calls to sub_829813F0, i.e. 1-2 draws each; per Clear call with N rect and 1 RT: 1-2*N draws (+ the depth-only call if no colour flag; with colour flags,
one set of draws per RT at index 0..3 and the first one carries depth/stencil). sub_829813F0 writes (S1D) fetch 0-5, 0x2000/0x2001, 0x2301, 0x2200, 0x2208 and DRAW_INDX_2 with prim 8,
3*count indices; the vertex buffer (position + colour + z, from `r6` colour float4 and `f1`) is allocated by sub_8298B5B0 and filled by it: **colour and z reach the shader through
vertices, not through ALU constants**.

### 3.5 End of Clear
`sub_8297CA20(dev, x,y from shadows 10436/10440)` (21087-21113) re-emits the scissor 0x2080 group, from shadows. Then `dev+10941 |= 4` and `dev+16 |= 0x20000` (21116-21126).

## 4. Answers

1. Shaders: fixed globals, vertex 0x8208D6D8 (27 dw), pixel 0x8208D690 (9 dw), written with IM_LOAD_IMMEDIATE 0x2B twice per Clear; no 0x2A / 0x3B-with-microcode; INVALIDATE_STATE 0x3B only as a separate 1-dword packet {0x300}.
2. ALU constants: **none** are written by Clear or its helpers (no 0x4000+/0x4400+ type-0, no LOAD_ALU 0x2F); colour/z are in the vertex stream.
   So a rectangle draw has to use whatever ALU constants the CP had, which Clear does not need (the clear vertex shader of 27 dwords uses none, to be confirmed by decoding the microcode at 0x8208D6D8).
3. Viewport / scissor / surface / depth / blend / mode:
   - computed immediates: 0x2100-0x2102, 0x2104, 0x2201, 0x2202, 0x2204-0x2206, 0x2293, 0x2312, 0x200D, 0x2180/0x2181, 0x5C8, scissor 0x2080-0x2082 (open to 8192).
   - from shadows: RB_SURFACE_INFO / colour info (dev+10368 -> r8 of the rect draw), RT format from the RT surface objects (dev+12440+4*rt -> +28), depth-control and 0x2203 shadow (dev+10560), SET_BIN_MASK (dev+12708/12712).
   - computed from arguments: stencil ref (0x210D), packed colour (L4), depth-control r20 from flags, mode r14.
   - at the end: scissor from shadows (sub_8297CA20).
4. sub_829813F0 is called indirectly through sub_829816E0, 1-2 per rect (see 3.4), arguments above. Dirty bits: Clear ORs dev+16 |= bits 41, 40, 39, 0x80, 0x40, 0x20, 44, 0x80000, 0x100000, 0x20000 (and 37, 0x200, 0x400 on the colour path, 28 in 82981970),
   dev+24 |= 1<<35, dev+32 |= 0x80000 (20860-20926, 21021-21033, 21122-21126); sub_829813F0 additionally ORs dev+24 |= 1<<31, dev+16 |= 1<<57, 1<<56, 0x800, 8, dev+32 |= 1<<36 (S1D). It never clears masks except through the pre-flush in 2 (those groups are flushed first and cleared), so later normal draws re-flush the groups that Clear overrode.
   The shaders: A/B shader shadows are not cleared; the next sub_8298F600 re-emits them (it writes if A/B are non-null; that is existing behaviour for every draw).
5. Recommendation: see 5.

## 5. Recommendation for the native front end

**Option 1 (one snapshot per rectangle draw, built from shadows + the registers Clear wrote + Clear's shaders)** needs the front end to track the *whole* CP register stream between the
Clear's entry and each draw: ~25 type-0 groups (3.1-3.3, many immediates), two IM_LOAD_IMMEDIATE with embedded microcode, SET_BIN_MASK and the nested sub_8298DE88 packets (which I did not decode),
and the overrides of the shadows (shader A/B, 0x2180, 0x2200 etc.) that are only "restored" by dirty bits. This is the same kind of CP register file replay the design already needs for normal draws, but every
Clear touches registers that the device shadow does not mirror (e.g. 0x2293, 0x2312, 0x200D, 0x5C8), so the snapshot would have to be built from the ring, not from the device.

**Option 2 (dedicated clear command) is simpler** given this packet list: the semantic content of a Clear is small - render target index (or -1), rect list (after clamping, 20198-20331, or the final
tile-split boxes passed to 829813F0), colour float4 (r6), z (f1), stencil (r8), flags. All packet traffic is derived from those plus the shadow RB_SURFACE_INFO/format/depth-control. Hook at **sub_82981ED8 entry**
(args r3..r8, f1, flags already in r4) and emit one `ClearCmd{flags, rects[], colour[4], z, stencil, rt surface ptrs (dev+12440.., 12456), msaa (dev+10368)}`; suppress the nested sub_829813F0 draws
(hook it, or gate on a "in clear" flag set between Clear's entry and exit) so they are not turned into snapshots a second time. The command processor needs: a clear of the current colour RT(s) (by flag bits 0..3) over the
rect list with the colour, a depth clear with z for flag 0x10, stencil clear for 0x20 (0x40/0x80 select the stencil mask/ref merge path), and the leave-state note that after Clear the CP state is Clear's, so the
next normal draw must not rely on the previous CP register values (i.e. its snapshot must be complete from the shadows, as the dirty bits ask; the native snapshot already is, because it reads the shadows).
Caveats: (a) the tile/hi-clear split and EDRAM fast clear (3.4) disappear in a native renderer, only the final coverage matters; (b) the unaligned border pieces and interior produce the same pixels as the unclamped rect, but the interior uses the MSAA tile shift and a different mode (5), so do not emit draws from those calls; (c) if the front end has to cooperate with the existing probe/parity tools, treat sub_82981ED8 as the owner of the draws it contains.

## 6. Concerns / unverified
- sub_8298DE88 and sub_8298E220 were not decoded; their emitted registers are inferred from partial instruction scans (76.cpp 21096-21420).
- Register names (0x2204-0x2206, 0x2293, 0x2312, 0x200D, 0x5C8) unknown; microcode of the two shaders was not disassembled.
- The flag bit meaning for 0x40/0x80 is only inferred from the code (19221-19253). Clear's stencil value is taken from r8 (`mr r22,r8`, 20165), not r7.
