	.equ RESULT_ADDR, 0x50400000
	.text
	.global main
main:
        // TODO: [p0-task1] Add your code here!
        
	li t0,1
	li t1,50
	li t2,0
loop:
	add t2, t2,t0
	addi t0,t0,1
	ble t0,t1,loop

	la t3,RESULT_ADDR
	sw t2,0(t3)
	ebreak

    
