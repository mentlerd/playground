Recently acquired some flipdot hardware, with software based on Intel 8051 - Ghidra "supports" it, but most operations involving multiple bytes come out iffy.

Case in point:
```
90 56 bb  MOV   DPTR,#0x56bb
e0        MOVX  A,@DPTR=>DAT_EXTMEM_56bb
fc        MOV   R4,A
a3        INC   DPTR
e0        MOVX  A,@DPTR=>DAT_EXTMEM_56bc
fd        MOV   R5,A
a3        INC   DPTR
ec        MOV   A,R4
f0        MOVX  @DPTR=>DAT_EXTMEM_56bd,A
a3        INC   DPTR
ed        MOV   A,R5
f0        MOVX  @DPTR=>DAT_EXTMEM_56be,A
24 e6     ADD   A,#0xe6
f5 82     MOV   DPL,A
74 56     MOV   A,#0x56
3c        ADDC  A,R4
f5 83     MOV   DPH,A
74 ff     MOV   A,#0xff
f0        MOVX  @DPTR,A
```

Decompiler output:
```
DAT_EXTMEM_56bd = DAT_EXTMEM_56bb;
DAT_EXTMEM_56be = DAT_EXTMEM_56bc;
*(undefined *) CONCAT11((DAT_EXTMEM_56bb - (((0x19 < DAT_EXTMEM_56bc) << 7) >> 7)) + 'V', DAT_EXTMEM_56bc - 0x1a) = 0xff;
```
Gnarly.

Lets do a quick breakdown of what is happening:
```
90 56 bb  MOV   DPTR,#0x56bb                 | __xdata byte * DPTR = 0x56bb;
e0        MOVX  A,@DPTR=>SHORT_EXTMEM_56bb   |
fc        MOV   R4,A                         |
a3        INC   DPTR                         | byte R4 = *(DPTR++);
e0        MOVX  A,@DPTR=>SHORT_EXTMEM_56bb+1 |
fd        MOV   R5,A                         |
a3        INC   DPTR                         | byte R5 = *(DPTR++);
ec        MOV   A,R4                         |
f0        MOVX  @DPTR=>SHORT_EXTMEM_56bd,A   |
a3        INC   DPTR                         | *(DPTR++) = R4;
ed        MOV   A,R5                         |
f0        MOVX  @DPTR=>SHORT_EXTMEM_56bd+1,A | *(DPTR) = R5;
```
8051 is a peculiar architecture for someone coming from a more high level background. It has different memory spaces. As such a raw pointer value may mean multiple values in physical memory depending on which memory space we are addressing with it.

The processor is built in a way that it can only read/write external memory (`__xdata`) by setting the 16bit `DPTR` register to a memory address in the region, and performing reads/writes (of a single byte!) via dedicated instructions.

Now, the output of the decompiler makes perfect sense, it got rid of all the DPTR gymnastics, and decompiled the instructions into two perfectly reasonable assignments:
```
DAT_EXTMEM_56bd = DAT_EXTMEM_56bb;
DAT_EXTMEM_56be = DAT_EXTMEM_56bc;
```

These writes are quite convenient though, if I designate the memory addresses as arrays it becomes even easier to see:
```
BYTE_ARRAY_EXTMEM_56bd[0] = BYTE_ARRAY_EXTMEM_56bb[0];
BYTE_ARRAY_EXTMEM_56bd[1] = BYTE_ARRAY_EXTMEM_56bb[1];
```

Consecutive writes, this is likely a copy between two `int16_t` variables located in memory. However if I annotate them as such we get gnarly output once more:
```
SHORT_EXTMEM_56bd._0_1_ = SHORT_EXTMEM_56bb._0_1_;
SHORT_EXTMEM_56bd._1_1_ = (byte)SHORT_EXTMEM_56bb;
```
Not great, not terrible.

The rest of the code though...
```
*(undefined *) CONCAT11((SHORT_EXTMEM_56bb._0_1_ - (((0x19 < (byte)SHORT_EXTMEM_56bb) << 7) >> 7)) + 'V',(byte)SHORT_EXTMEM_56bb - 0x1a) = 0xff;
```

Lets dive in to the assembly once more, leaving some variables where we left off:
```
__xdata byte * DPTR = 0x56be;

byte R4 = SHORT_EXTMEM_56bb._0_1_
byte R5 = SHORT_EXTMEM_56bb._1_1_
byte A  = SHORT_EXTMEM_56bb._1_1_

24 e6  ADD   A,#0xe6  | 
f5 82  MOV   DPL,A    | DPL = 0xe6 + SHORT_EXTMEM_56bb._1_1_
74 56  MOV   A,#0x56  |
3c     ADDC  A,R4     |
f5 83  MOV   DPH,A    | DPH = 0x56 + SHORT_EXTMEM_56bb._0_1_ + CARRY(0xe6, SHORT_EXTMEM_56bb._1_1_)
74 ff  MOV   A,#0xff  | 
f0     MOVX  @DPTR,A  | *DPTR = 0xff;
```

We need some context: `DPL` and `DPH` are 8 bit registers, aliasing the lower/higher bytes of the `DPTR` register. We know this from the ISA manual, and the `.slaspec`:
```
define register offset=0x82  size=2 [ DPTR ];
define register offset=0x82  size=1 [ DPH DPL ]; # relocated to facilitate DPTR 16-bit access
```

So whats really going on is that we are writing the upper/lower half of DPTR like so:
```
DPTR._1_1_ = 0xe6 + SHORT_EXTMEM_56bb._1_1_
DPTR._0_1_ = 0x56 + SHORT_EXTMEM_56bb._0_1_ + CARRY(0xe6, SHORT_EXTMEM_56bb._1_1_)
```

But hold on, this is just some standard 16 bit addition followed by an assignment! This is most likely an array access:
```
((__xdata byte[]) 0x56e6)[SHORT_EXTMEM_56bb] = 0xFF;
```

We can sort-of make out the individual components of the correct answer in the decompilation, but you have to look _hard_:
```
*(undefined *) CONCAT11(
  (
    SHORT_EXTMEM_56bb._0_1_
    -
    (((0x19 < (byte) SHORT_EXTMEM_56bb) << 7) >> 7) ; <-- Looks vaguely like a carry
  )
  +
  0x56,
  (byte)SHORT_EXTMEM_56bb - 0x1a
) = 0xff;
```

As to why ghidra is unable to recognize this, I am unsure.

It seems I am not the only one to run into this sort of problem though. There is [an open issue/bug](https://github.com/NationalSecurityAgency/ghidra/issues/1243) on github from 2019. With no comments and 3 likes including mine..

Unfortunately this problem is quite hard to ignore. Array accesses are quite common, and the code which I am trying to reverse is chocked full of them.

I guess I will be debugging a decompiler soon. Unfortunately my home setup is quite different from what I am used to. Once you get acquianted with LLVM tools and their pretty nice Xcode integration it is hard to let go of the scriptability aspect. Time to see whether LLVM tools can shine on Windows too.
