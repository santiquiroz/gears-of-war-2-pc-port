# S1d findings (parity probe)

## Dumps
`GOW2_PARITY_DUMP_FRAME=<n>`: for frame n (it doesn't have to be a multiple of 300), draws d < 64, the GAME line is followed by
`PARITY GAMEDUMP f d blk=vsc|psc|fetch hex=...`. vsc/psc are the reconstructed 2048 constants (shadows 1920/6016
plus the LOAD_ALU_CONSTANT overlays of A and B). fetch is the fetch shadow (dev+1152) plus the ring overlay when that
overlay was built. `tools/native/parity_dump_diff.py <log>` diffs them against `PARITY CPDUMP` (`--selftest` passes).

## Pixel shader when A == 0
sub_8298F600 (120.cpp:20218+) loads B, then runs `lwz r29,12692(r30); cmplwi cr6,r29,0; bne 0x8298f760`. When A is
0, the code falls through to 8298F6C0. There it sets the low 3 bits of RB_MODECONTROL (shadow 10580) to 5 (depth only;
`rlwimi r11,r22,0,29,31` with r22 = 5), and the only packet is an optional type-0 write to register 0x05C8 (value
0x20000, emitted when byte 10943 bit 0 is set). sub_8298F210 (18.cpp:19104) emits only a type-0 (vertex)
IM_LOAD_IMMEDIATE: `0xC0003B00,0x100` then `0xC0002B00|(n+1)<<16, 0, n`. **No pixel shader is loaded**, so the
command processor keeps the last one it got. The only 9-dword type-1 IM_LOAD_IMMEDIATE (header 0xC00A2B00,
`lis -16374; ori 11008; li 1; li 9; li r5,36`) comes from four places:
- Clear sub_82981ED8 (48.cpp:20615-20700): source `lis r10,-32247; addi r29,r10,-10608` = **0x8208D690**.
- sub_82982718 (130.cpp:20070-20110): `lis r6,-32247; addi r30,r6,-12476; mr r4,r30` = **0x8208CF44**. Its vertex
  shader is at +1484 (0x8208D510), 96 bytes.
- sub_82976EE0 (128.cpp:20343, source r29) and sub_82994830 (207.cpp:19237, source r28), both register-relative. I did
  not resolve these two.

The probe now tracks `g_last_pixel_shader`. A draw with A != 0 sets it to A's IM_LOAD, sub_82982718 sets it to
0x8208CF44, and Clear sets it to 0x8208D690 when it returns. Each GAME line now ends with `Paddr= Plen=`. Concern: I
assumed Clear's load is unconditional after the 10943 test and did not check the branch. Draws with A != 0 in
functions the probe doesn't hook also replace the CP shader, so Paddr is a best effort.

## sub_829813F0 (57.cpp:20227-20470): 0 % explained
It's the rectangle-list draw. Args: r3 dev, r4 = number of rects, r5 = int4 rects, r6 = float4 color, f1 = z,
r8/r9/r10 plus a stack arg. It **calls neither the flush (sub_8298E5B8) nor sub_8298F600**: no shader, constant or
dirty-group flush. It doesn't restore state either. It reserves r4·21 dwords of vertices (sub_8298B5B0), then writes
straight into the ring at dev+48:
`0x00054800` + fetch dwords 0-5 (`Phys|3`, `0x10000002|size<<2`, 0,0,0,0); `0x00025000` + 0,0,0 (register index
0x5000, not decoded); `0x00012000` + r8, r9 (RB_SURFACE_INFO, RB_COLOR_INFO); `0x00002301` + (r8>>16 & 3: 0, 0x8001 or
0xC003); `0x00002200` + r10 (RB_DEPTHCONTROL); `0x00002208` + stack arg (RB_MODECONTROL); `0xC0003601` DRAW_INDX_2 +
initiator `(3·r4)<<16 | 0x88` (prim 8, auto index). That is **one draw per call, emitted by itself**. Afterwards it
only ORs dirty bits (dev+24 |= 1<<31; dev+16 |= 1<<57, 1<<56, 0x800, 8; dev+32 |= 1<<36), so the next normal draw
re-flushes from the shadows, which it leaves untouched.

So the CP state at this draw is the state of the last flush plus these direct writes. The shadows may hold pending
changes that nothing has flushed yet. Its prim and n were also logged as `-`, which can throw off frame alignment.

Probe change: for sub_829813F0 it logs prim=8 and n=3·r4. When the 22-dword layout checks out at write-84, the surf
hash covers shadow 10368 with dwords 0-1 replaced by the ring values, rs covers 10548 with dwords 0 and 8 replaced, and
fetch2/fetch dump cover the fetch shadow with dwords 0-5 replaced. No inner hook is needed because it emits the draw
itself. parity_compare.py only checks fetch2 for 82980210/8297FD30, so for this function read the dump diff. The
remaining groups (prog, vp, scis, constants) can still differ wherever shadows changed without a flush.

## sub_82980210 (1.cpp:18852-19629)
It flushes ALU constants itself: `ld r4,0(r31); li r5,16384; bl sub_8298E5B8` (0x4000, mask 0), then `li r5,17408`
(0x4400, mask 8). Then it calls sub_8298F600, sub_8298DE88 and sub_8298E220 for 0x2200, 0x2180, 0x2000, 0x2100,
0x2280, 0x2300, 0x2380 and 0x4900 (bool/loop). Direct ring writes:
- `0x00002102` + r20.
- `0x000548BA` + 6 dwords (r9, r7, r6, r5, then `Phys|3` and `0x10000002|size<<2`), i.e. fetch dwords 186-191. The
  vertex stream is fetch constant 95 = dwords 190-191.
- `0x00025000` + r3, r16, r15.
- `0xC0032201` DRAW_INDX + 4 dwords (r24, r29, r25, r27).

The second path (lines ~292-330) repeats the same DRAW_INDX between `0xC0006000` SET_BIN_MASK_LO (-1 and then
0x80000000), plus `0xC0015A01` (26, x|1) and `0xC0004601` EVENT_WRITE (25). **It writes no ALU constant directly**,
so the 0 % in vsc2/psc2 has to come from the flush and the overlay. Check whether r7 really is the count (pairing) and
read `parity_dump_diff.py` on a dump frame. fetch2 misses at dwords other than 186-191 would point to the fetch shadow.

## Not run
Not built or run in game (budget). Both Python selftests pass.
