
.global main
main:

// TODO: [p0-task1] Add your code here!
	li sp,0x50700000
	li t0,1		#t0: current num
	li t2,200	#t1: max num
	
loop:
	mv s0,t0	#s0:input parameter
	jal ra,is_prime

	#return value is in a0,1 for prime,0 for non-prime
	
	addi t0,t0,1
	ble t0,t2,loop
	ebreak

is_prime:
	addi sp,sp,-24
	sd ra,16(sp)	#return address
	sd t0,8(sp)
	sd s0,0(sp)
	

	li t0,2		#t0:loop i starts from 2
	li a0,1		#default prime
    	
	ble s0,t0,check_two	#if num <= 2 

loop_div:
	rem t1,s0,t0	#t1 = num % i
	beq t1,zero,non_prime
	addi t0,t0,1
	mul t2,t0,t0
	ble t2,s0,loop_div	#i^2 <= num 

	j end

check_two:
	li a0,0
	li t1,2
	beq s0,t1,prime_two
	j end

prime_two:
	li a0,1
	j end

non_prime:
	li a0,0

end:
	ld ra,16(sp)
	ld t0,8(sp)
	ld s0,0(sp)
	addi sp,sp,16
	ret
