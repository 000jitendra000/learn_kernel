global task_switch

task_switch:

    pushf
    pusha

    mov eax, [esp + 40]
    mov [eax], esp

    mov eax, [esp + 44]
    mov esp, eax

    popa
    popf
    ret