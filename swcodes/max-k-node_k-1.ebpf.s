mov r3, 0x0
stxw [r10-4], r3
stxw [r10-8], r1
stxdw [r10-16], r2
stxdw [r10-64], r1
mov r1, r3
stxdw [r10-72], r2
stxdw [r10-80], r3
call 0x1	                    /* Call getState() */
stxw [r10-20], r0
mov r1, 0x1
call 0x1	                    /* Call getState() */
stxw [r10-24], r0
ldxdw r1, [r10-80]
stxb [r10-32], r1
lddw r2, 0x65756c61762f6564	    /* Load ritValuePath[] into stack. */
stxdw [r10-40], r2
lddw r2, 0x6f6e6c61636f6c2f	    /* Cont. */
stxdw [r10-48], r2
mov r1, r10
add r1, 0xffffffd0
call 0x3	                    /* Call ritGet() */
stxw [r10-52], r0
ldxdw r1, [r10-72]
ldxdw r2, [r10-64]
ldxw r3, [r10-20]
lsh r3, 0x20
arsh r3, 0x20
mov r4, 0x15
stxdw [r10-88], r1
stxdw [r10-96], r2
jsgt r4, r3, +12
ja +0
ldxw r1, [r10-52]
lsh r1, 0x20
arsh r1, 0x20
ldxw r2, [r10-24]
lsh r2, 0x20
arsh r2, 0x20
jsge r2, r1, +4
ja +0
call 0x4	                    /* Call deliverMsg() */
stxdw [r10-104], r0
ja +17
ldxw r1, [r10-20]
add r1, 0x1
stxw [r10-20], r1
mov r2, 0x0
stxdw [r10-112], r1
mov r1, r2
ldxdw r2, [r10-112]
call 0x2	                    /* Call setState() */
ldxw r2, [r10-52]
ldxw r1, [r10-24]
stxdw [r10-120], r0
call 0x5	                    /* Call MAX() */
mov r1, 0x1
mov r2, r0
call 0x2	                    /* Call setState() */
stxdw [r10-128], r0
ja +0
mov r0, 0x0
exit
ja +3
ldxw r1, [r10-52]
stxdw [r10-128], r1
ja +0
ldxdw r1, [r10-128]
mov r2, 0x1
stxdw [r10-136], r1
mov r1, r2
ldxdw r2, [r10-136]
call 0x2	                    /* Call setState() */
stxdw [r10-144], r0
ja +0
mov r0, 0x0
exit
stxdw [r10-152], r0
ja +0
mov r0, 0x0
exit
