.code

SyscallIndirectAdvanced PROC
    push    rbp
    mov     rbp, rsp
    mov     eax, ecx
    mov     r10, rcx
    mov     rcx, r8
    mov     rdx, r9
    mov     r8,  [rbp+0x28]
    mov     r9,  [rbp+0x30]
    jmp     rdx
SyscallIndirectAdvanced ENDP

SyscallIndirectAdvancedEx PROC
    push    rbp
    mov     rbp, rsp
    sub     rsp, 32
    push    rsi
    push    rdi
    mov     eax, ecx
    mov     r10, rcx
    mov     rsi, r8
    mov     rcx, [rsi]
    cmp     r9, 1
    jl      done
    mov     rdx, [rsi+8]
    cmp     r9, 2
    jl      done
    mov     r8,  [rsi+16]
    cmp     r9, 3
    jl      done
    mov     r9,  [rsi+24]
    cmp     r9, 4
    jle     done
    mov     rax, r9
    sub     rax, 4
    shl     rax, 3
    sub     rsp, rax
    mov     rdi, rsp
    mov     rax, 4
    shl     rax, 3
    add     rsi, rax
    mov     rcx, r9
    sub     rcx, 4
    shl     rcx, 3
    rep movsb
done:
    jmp     rdx
    pop     rdi
    pop     rsi
    mov     rsp, rbp
    pop     rbp
    ret
SyscallIndirectAdvancedEx ENDP

END