.global call_func

call_func:
	/* %rdi is the address of the function to call, %rsi
	is a pointer to the result area, %rdx is the argument count,
	and %rcx is a pointer to the argument array. */
	pushq %rbp
	
	pushq %rsi
	pushq %r12
	pushq %r13
	movq %rdi, %r12
	movq %rcx, %r13
	movq $0, %rax
	
	movq %rsp, %rbp
	
	testq %rdx, %rdx
	jz do_call

	cmpq $1, %rdx
	je call_1_arg
	cmpq $2, %rdx
	je call_2_arg
	cmpq $3, %rdx
	je call_3_arg
	cmpq $4, %rdx
	je call_4_arg
	cmpq $5, %rdx
	je call_5_arg
	cmpq $6, %rdx
	je call_6_arg

	subq $6, %rdx
	movq %rdx, %rcx
	shlq $3, %rdx

	subq %rdx, %rsp
	movq %rsp, %rdi
	lea 48(%r13), %rsi
	rep movsq
	
call_6_arg:
	movq 40(%r13), %r9
call_5_arg:
	movq 32(%r13), %r8
call_4_arg:
	movq 24(%r13), %rcx
call_3_arg:
	movq 16(%r13), %rdx
call_2_arg:
	movq 8(%r13), %rsi
call_1_arg:
	movq (%r13), %rdi
do_call:
	call *%r12

	movq %rbp, %rsp
	popq %r13
	popq %r12
	popq %rsi

	movq %rax, (%rsi)
	movq %rdx, 8(%rsi)
	
	popq %rbp
	ret