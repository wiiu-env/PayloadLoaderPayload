#define BAT_SETUP_HOOK_ADDR         0xFFF1D624

 .globl KernelPatchesRevertHook
KernelPatchesRevertHook:
    # store the old DBAT0
    mfdbatu r5, 0
    mfdbatl r6, 0

    # memory barrier
    eieio
    isync

    # setup DBAT0 for access to kernel code memory
    lis r3, 0xFFF0
    ori r3, r3, 0x0002
    mtdbatu 0, r3
    lis r3, 0xFFF0
    ori r3, r3, 0x0032
    mtdbatl 0, r3

    # memory barrier
    eieio
    isync

    # restore kernel hook
    lis r3, BAT_SETUP_HOOK_ADDR@h
    ori r3, r3, BAT_SETUP_HOOK_ADDR@l

    lis r4, 0x80a3                      # lwz        r5 ,0x34 (r3 )
    ori r4, r4, 0x0034
    stw r4, 0x00(r3)
    lis r4, 0x80c3      				#   lwz        r6 ,0x38 (r3 )
    ori r4, r4, 0x0038
    stw r4, 0x04(r3)
    lis r4, 0x80e3      				#   lwz        r7 ,0x3c (r3 )
    ori r4, r4, 0x003c
    stw r4, 0x08(r3)
    lis r4, 0x8103      				#   lwz        r8 ,0x40 (r3 )
    ori r4, r4, 0x0040
    stw r4, 0x0C(r3)
    lis r4, 0x8123      				#   lwz        r9 ,0x44 (r3 )
    ori r4, r4, 0x0044
    stw r4, 0x10(r3)
    lis r4, 0x8143      				#   lwz        r10 ,0x48 (r3 )
    ori r4, r4, 0x0048
    stw r4, 0x14(r3)
    lis r4, 0x8163      				#   lwz        r11 ,0x4c (r3 )
    ori r4, r4, 0x004c
    stw r4, 0x18(r3)
    lis r4, 0x8063      				#   lwz        r3 ,0x50 (r3 )
    ori r4, r4, 0x0050
    stw r4, 0x1C(r3)
    lis r4, 0x4c00     				    #   isync
    ori r4, r4, 0x012c
    stw r4, 0x20(r3)
    lis r4, 0x7ca7                      #   mtsr       sr7 ,r5
    ori r4, r4, 0x01a4
    stw r4, 0x24(r3)

    # flush and invalidate the replaced instructions
    lis r3, (BAT_SETUP_HOOK_ADDR & ~31)@h
    ori r3, r3, (BAT_SETUP_HOOK_ADDR & ~31)@l
    dcbf 0, r3
    icbi 0, r3
    lis r3, ((BAT_SETUP_HOOK_ADDR + 0x20) & ~31)@h
    ori r3, r3, ((BAT_SETUP_HOOK_ADDR + 0x20) & ~31)@l
    dcbf 0, r3
    icbi 0, r3
    sync

    # memory barrier
    eieio
    isync

    # restore DBAT 0 and return from interrupt
    mtdbatu 0, r5
    mtdbatl 0, r6

    # memory barrier
    eieio
    isync

    blr

