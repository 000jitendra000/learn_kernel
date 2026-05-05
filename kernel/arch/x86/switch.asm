[bits 32]

; void task_switch(uint32_t *old_esp, uint32_t new_esp)
;
; Called as a normal C function. At entry:
;   [esp+0] = return address
;   [esp+4] = old_esp  (uint32_t * — where to save current esp)
;   [esp+8] = new_esp  (uint32_t   — esp of task to restore)
;
; SAVE order (what we push onto the current stack):
;   pushf           → eflags          (+4 bytes)
;   pusha           → eax ecx edx ebx esp ebp esi edi  (+32 bytes)
;
; After 9 dwords (36 bytes) pushed, args are at:
;   [esp+36] = ret addr
;   [esp+40] = old_esp
;   [esp+44] = new_esp
;
; RESTORE order (mirror — what task_create must build):
;
;   HIGH (stk_top - 0)
;   [ eip    ]   ← ret pops this → jumps to task entry
;   [ eflags ]   ← popf
;   [ eax    ]   \
;   [ ecx    ]    |
;   [ edx    ]    |
;   [ ebx    ]    | popa restores these
;   [ esp_   ]    | (dummy, popa discards esp)
;   [ ebp    ]    |
;   [ esi    ]    |
;   [ edi    ]   /
;   LOW  ← esp points here when task is suspended

global task_switch

task_switch:
    pushf                       ; save eflags
    pusha                       ; save eax ecx edx ebx esp ebp esi edi

    ; 9 dwords pushed = 36 bytes
    mov eax, [esp + 40]         ; old_esp argument
    mov [eax], esp              ; save current esp into *old_esp

    mov eax, [esp + 44]         ; new_esp argument
    mov esp, eax                ; switch to new task's stack

    ; restore new task's context (exact mirror of save above)
    popa                        ; restore eax ecx edx ebx esp ebp esi edi
    popf                        ; restore eflags

    ret                         ; return to new task's eip