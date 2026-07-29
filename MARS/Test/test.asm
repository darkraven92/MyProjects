.text
main:
    li $t0, 5          # Ladda (Load Immediate) talet 5 i register $t0
    li $t1, 3          # Ladda talet 3 i register $t1
    add $t2, $t0, $t1   # Addera $t0 och $t1, spara resultatet i $t2

    # Avsluta programmet snyggt (Syscall 10 = exit)
    li $v0, 10
    syscall