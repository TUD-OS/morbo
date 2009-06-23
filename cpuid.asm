        ;; CPU identification

        CPU P3
        
        GLOBAL  do_cpuid, read_msr, write_msr

        SECTION .text
do_cpuid:
        push edi
        mov eax, [esp + 8]
        mov edi, [esp + 12]
        cpuid
        cld
        stosd
        mov eax, ebx
        stosd
        mov eax, ecx
        stosd
        mov eax, edx
        stosd
        pop edi

read_msr:
        mov ecx, [esp + 4]
        rdmsr
        ret

write_msr:
        mov ecx, [esp + 4]
        mov eax, [esp + 8]
        mov edx, [esp + 12]
        wrmsr
        ret
        
        ;; EOF