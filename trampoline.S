.global find_second_stage
.global call_post_func
	
.global trampoline_1_start
.global trampoline_1_load_target
.global trampoline_1_load_name
.global trampoline_1_load_find_second_stage
.global trampoline_1_end

.global trampoline_2_start
.global trampoline_2_loads_call_post_func
.global trampoline_2_loads_target_name
.global trampoline_2_loads_return_rip
.global trampoline_2_end
	
	/* The first phase trampoline is just responsible for finding
	or allocating the second phase. */
trampoline_1_start:
	pushq %r9
	pushq %r8
	pushq %rcx
trampoline_1_load_target:
	movq $0xaaaaaf009dead, %r9
trampoline_1_load_name:
	movq $0xaaaaaf009dead, %r8
trampoline_1_load_find_second_stage:
	movq $0xaaaaaf00adead, %rcx
	jmp *%rcx
trampoline_1_end:

	/* This is called from the first stage trampoline to find the
	second stage.  %r9 contains the ultimate target address and r8
	contains the target name.  The top of the stack is:

	Caller's rcx    <-- (%rsp)
	Caller's r8     <-- 8(%rsp)
	Caller's r9     <-- 16(%rsp)
	Parent return address <-- 24(%rsp)
	Stack arguments
	*/
find_second_stage:
	/* Rearrange the stack to look like this:

	Caller's rdx		(%rsp)
	Caller's rcx    	8(%rsp)
	Caller's r8		16(%rsp)
	Caller's r9		24(%rsp)
	Stack arguments

	with the return address in rdx. */
	pushq %rdx
	/* Stack now:
	Caller's rdx		(%rsp)
	Caller's rcx		8(%rsp)
	Caller's r8		16(%rsp)
	Caller's r9		24(%rsp)
	Return address		32(%rsp)
	Stack arguments

	Adjust to:

	Caller's rdx		(%rsp)
	Caller's rdx		8(%rsp)
	Caller's rcx		16(%rsp)
	Caller's r8		24(%rsp)
	Caller's r9		32(%rsp)
	Stack arguments
	*/
	movq 32(%rsp), %rdx /* return address to rdx */
	movq 24(%rsp), %rcx /* Old r9 to rcx */
	movq %rcx, 32(%rsp) /* Old r9 to new r9 slot */
	movq 16(%rsp), %rcx /* Old r8 to rcx */
	movq %rcx, 24(%rsp) /* Old r8 to new r8 slot */
	movq 8(%rsp), %rcx /* Old rcx to rcx */
	movq %rcx, 16(%rsp) /* Old rcx to new rcx slot */
	movq (%rsp), %rcx /* Old rdx to rcx */
	movq %rcx, 8(%rsp) /* Old rdx to new rdx slot */

	/* And finally remove the redundant copy of rdx */
	addq $8, %rsp
	
	/* Save the rest of the argument passing registers */
	pushq %rsi
	pushq %rdi

	/* The stack now contains all of the arguments to the target function
	in order, so a pointer to it will be the second argument to the pre
	function. */
	movq %rsp, %rsi
	
	/* Save non-argument registers. */
	pushq %rax
	pushq %r10
	pushq %r11

	/* We now have a stack like this:

	Caller's r11 <- rsp
	Caller's r10		8(rsp)
	Caller's rax		16(rsp)
	Caller's rdi <- rsi	24(rsp)
	Caller's rsi		32(rsp)
	Caller's rdx		40(rsp)
	Caller's rcx		48(rsp)
	Caller's r8		56(rsp)
	Caller's r9		64(rsp)
	Stack arguments

	with the return address in rdx, the target rip in r9, and the target
	name in r8.
	*/

	pushq %rdx
	pushq %r9
	pushq %r8

	/* (%rsp) -> target name, 8(%rsp) -> target rip, 16(%rsp) -> return rip */
	
	movq 8(%rsp), %rcx
	subq $16, %rsp
	movq %rsp, %rdx
	movq %r8, %rdi
	
	call pre_func_audit@PLT

	test %rax, %rax
	jnz skip_call

	addq $16, %rsp
	
	/* Okay, we're definitely going to run this call.  Find a second
	stage trampoline. */

	popq %rdx /* Target name */
	movq (%rsp), %rdi
	movq 8(%rsp), %rsi
	call find_second_stage_trampoline@PLT
	popq %rdi /* Target rip */
	addq $8, %rsp /* Discard return rip; it's in the second stage */
	
	/* %rax is the second stage trampoline.  Put the stack back
	together. */

	/* Phase 1: recover r9 and put the trampoline address into the
	return address slot on the stack */
	movq 64(%rsp), %r9
	movq %rax, 64(%rsp)

	/* Phase 2: recover r8 and stash the target address in the
	stack */
	movq 56(%rsp), %r8
	movq %rdi, 56(%rsp)

	/* Phase 2: Restore all other registers */
	popq %r11
	popq %r10
	popq %rax
	popq %rdi
	popq %rsi
	popq %rdx
	popq %rcx
	
	/* Phase 4: return into the target */
	ret


trampoline_2_start:
	pushq %r11
	pushq %rdi
	pushq %r10
trampoline_2_loads_target_name:
	movq $0xaaaabbbbcccddd, %rdi
trampoline_2_loads_return_rip:
	movq $0xaaaabbbbcccdde, %r11
trampoline_2_loads_call_post_func:
	movq $0xaaaabbbbccccc, %r10
	jmp *%r10
trampoline_2_end:
	
call_post_func:
	/* Have already saved rdi and r10.  Save the rest and call the
	post func. */
	pushq %r9
	pushq %r8
	pushq %rcx
	pushq %rsi

	pushq %rdx
	pushq %rax
	movq %rsp, %rsi /* Second argument -> return value */
	
	pushq %r11
	
	call post_func_audit@PLT

	popq %r11
	
	popq %rax
	popq %rdx
	popq %rsi
	popq %rcx
	popq %r8
	popq %r9
	popq %r10

	/* Stack contents:

	Old rdi  (%rsp)
	Old r11	 8(%rsp)

	r11 contains the return address

	Want to turn that into:

	Old rdi 	(%rsp)
	Return address  8(%rsp)

	r11 contains old r11
	*/
	
	movq 8(%rsp), %rdi
	movq %r11, 8(%rsp)
	movq %rdi, %r11
	popq %rdi
	ret

skip_call:
	/* Set up the return value */
	popq %rax
	popq %rdx
	
	/* Discard target name and target rip */
	addq $16, %rsp

	/* Restore r9 so that we can put the return address in its
	slot. */
	movq 72(%rsp), %r9

	/* Pop return rip and put it back into place */
	popq %r8
	movq %r8, 64(%rsp)

	/* Restore all other registers */
	popq %r11
	popq %r10
	popq %rax
	popq %rdi
	popq %rsi
	popq %rdx
	popq %rcx
	popq %r8

	/* And we're done */
	ret
