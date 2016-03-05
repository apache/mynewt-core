    .text
    .code32
    .p2align 4, 0x90    /* align on 16-byte boundary and fill with NOPs */

    .globl _os_arch_frame_init
    /*
     * void os_arch_frame_init(struct stack_frame *sf)
     */
_os_arch_frame_init:
    push    %ebp                    /* function prologue for backtrace */
    mov     %esp,%ebp
    push    %esi                    /* save %esi before using it as a tmpreg */

    /*
     * At this point we are executing on the main() stack:
     * ----------------
     * stack_frame ptr      0xc(%esp)
     * ----------------
     * return address       0x8(%esp)
     * ----------------
     * saved ebp            0x4(%esp)
     * ----------------
     * saved esi            0x0(%esp)
     * ----------------
     */
    movl    0xc(%esp),%esi          /* %esi = 'sf' */
    movl    %esp,0x0(%esi)          /* sf->mainsp = %esp */

    /*
     * Switch the stack so the stack pointer stored in 'sf->sf_jb' points
     * to the task stack. This is slightly complicated because OS X wants
     * the incoming stack pointer to be 16-byte aligned.
     *
     * ----------------
     * sf (other fields)
     * ----------------
     * sf (sf_jb)           0x4(%esi)
     * ----------------
     * sf (sf_mainsp)       0x0(%esi)
     * ----------------
     * alignment padding    variable (0 to 12 bytes)
     * ----------------
     * pointer to sf_jb     %esp
     * ----------------
     */
    movl    %esi,%esp
    subl    $0x4,%esp               /* make room for setjmp() argument */
    andl    $0xfffffff0,%esp        /* align %esp on 16-byte boundary */
    leal    0x4(%esi),%eax          /* %eax = &sf->sf_jb */
    movl    %eax,0x0(%esp)
    call    __setjmp                /* _setjmp(sf->sf_jb) */
    test    %eax,%eax
    jne     1f
    movl    0x0(%esi),%esp          /* switch back to the main() stack */
    pop     %esi
    pop     %ebp
    ret                             /* return to os_arch_task_stack_init() */
1:
    lea     2f,%ecx
    push    %ecx                    /* retaddr */
    push    $0                      /* frame pointer */
    movl    %esp,%ebp               /* handcrafted prologue for backtrace */
    push    %eax                    /* rc */
    push    %esi                    /* sf */
    call    _os_arch_task_start     /* os_arch_task_start(sf, rc) */
    /* never returns */
2:
    nop
