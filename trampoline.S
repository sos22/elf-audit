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

#define VG_ESCAPE(x) $(0x45410000 | (x))
#define VALGRIND_ESCAPE_ENTER_MONITOR VG_ESCAPE(0)
#define VALGRIND_ESCAPE_EXIT_MONITOR VG_ESCAPE(1)
	
.macro VALGRIND_ESCAPE code
	pushq \code
	movq %rsp, %rax
	rolq $3, %rdi
	rolq $13, %rdi
	rolq $61, %rdi
	rolq $51, %rdi
	xchgq %rbx, %rbx
	addq $8, %rsp
.endm
	
	/* The first phase trampoline is just responsible for finding
	or allocating the second phase. */
trampoline_1_start:
	pushq %rax
	
	VALGRIND_ESCAPE VALGRIND_ESCAPE_ENTER_MONITOR
	
	pushq %rcx
	pushq %rdx
trampoline_1_load_target:
	movq $0xaaaaaf009dead, %rax
trampoline_1_load_name:
	movq $0xaaaaaf009dead, %rdx
trampoline_1_load_find_second_stage:
	movq $0xaaaaaf00adead, %rcx
	jmp *%rcx
trampoline_1_end:

	/* This is called from the first stage trampoline to find the
	second stage.  %rax contains the ultimate target address and
	rdx contains the target name.  rcx is saved but doesn't
	contain anything useful. The top of the stack is:

	Caller's rdx     <-- (%rsp)
	Caller's rcx     <-- 8(%rsp)
	Caller's rax     <-- 16(%rsp)
	Parent return address <-- 24(%rsp)
	Stack arguments
	*/
find_second_stage:
	/* We need to rearrange the stack to look like this:

	Caller's rdi		  (%rsp)
	Caller's rsi		 8(%rsp)
	Caller's rdx		16(%rsp)
	Caller's rcx		24(%rsp)
	Caller's r8		32(%rsp)
	Caller's r9		40(%rsp)
	Stack arguments

	The return address has to move to a register.
	
	*/
	
	pushq %rsi
	pushq %rdi
	
	/* Stack now looks like this:

	Caller's rdi		  (%rsp)
	Caller's rsi		 8(%rsp)
	Caller's rcx		16(%rsp)
	Caller's rdx		24(%rsp)
	Caller's rax		32(%rsp)
	Return address		40(%rsp)
	Stack arguments

	*/
	
	movq 40(%rsp), %rcx  /* Return address to rcx */
	movq %r9, 40(%rsp)   /* Save r9 to the right place */
	movq 32(%rsp), %r9   /* Caller's rax to r9 */
	movq %r8, 32(%rsp)   /* Save r8 in the right place */

	/* Stack layout now:

	Caller's rdi		  (%rsp)
	Caller's rsi		 8(%rsp)
	Caller's rdx		16(%rsp)
	Caller's rcx		24(%rsp)
	Caller's r8		32(%rsp)
	Caller's r9		40(%rsp)
	Stack arguments
	
        Registers:

	rax -- Target address
	rdx -- Pointer to target name
	rcx -- Return address
	r9 -- Caller's rax

	*/
	
	/* The stack now contains all of the arguments to the target function
	in order, so a pointer to it will be the second argument to the pre
	function. */
	movq %rsp, %rsi
	
	/* Save non-argument registers. */
	pushq %r9 /* Really caller's rax */
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

	with the return address in rcx, the target rip in rax, and the target
	name in rdx.
	*/

	/* Call pre_func_audit.  Its arguments are like so: rdi is a
	pointer to the target name, rsi is a pointer to the argument
	block, rdx is a pointer to the result area, and rcx is the
	target rip. */
	
	pushq %rcx
	pushq %rax
	pushq %rdx

	/* (%rsp) -> target name, 8(%rsp) -> target rip, 16(%rsp) -> return rip */
	
	movq 8(%rsp), %rcx
	subq $16, %rsp
	movq %rdx, %rdi
	movq %rsp, %rdx

	call pre_func_audit
	
	test %rax, %rax
	jnz skip_call

	addq $16, %rsp
	
	/* Okay, we're definitely going to run this call.  Find a second
	stage trampoline. */

	popq %rdx /* Target name */
	movq (%rsp), %rdi
	movq 8(%rsp), %rsi
	call find_second_stage_trampoline
	popq %rdi /* Target rip */
	addq $8, %rsp /* Discard return rip; it's in the second stage */
	
	/* %rax is the second stage trampoline.  Put the stack back
	together. */

	/* We need to arrange to restore all the registers, and then
	return with the stack looking like this:

	Target rip             (%rsp)
	Trampoline address    8(%rsp)
	Stack arguments
	
	We'll do the return and the target will then return to the
	start of the trampoline.

	We want to restore rax as late as possible, because that
	allows us to move the VG escape to the very end and minimise
	the amount of noise in traces.

	The stack currently looks like this:

	Caller's r11		(rsp)
	Caller's r10		8(rsp)
	Caller's rax		16(rsp)
	Caller's rdi 		24(rsp)
	Caller's rsi		32(rsp)
	Caller's rdx		40(rsp)
	Caller's rcx		48(rsp)
	Caller's r8		56(rsp)
	Caller's r9		64(rsp)
	Stack arguments

	*/
	
	movq 64(%rsp), %r9 /* Restore r9 */
	movq %rax, 64(%rsp) /* Stash the trampoline address */
	movq 56(%rsp), %r8 /* Restore r8 */
	movq %rdi, 56(%rsp) /* Stash target address */

	/* Restore rcx, and move rax down to its slot */
	movq 48(%rsp), %rcx
	movq 16(%rsp), %rax
	movq %rax, 48(%rsp)

	/* Pop the bulk of the registers */
	popq %r11
	popq %r10
	addq $8, %rsp
	popq %rdi
	popq %rsi
	popq %rdx

	/* Tell Valgrind what we're doing */
	VALGRIND_ESCAPE VALGRIND_ESCAPE_EXIT_MONITOR
	popq %rax

	/* And now we finally get to drop into the target */
	ret


trampoline_2_start:
	subq $8, %rsp
	pushq %rax
	VALGRIND_ESCAPE VALGRIND_ESCAPE_ENTER_MONITOR
	
	pushq %rdx
	pushq %r10
trampoline_2_loads_target_name:
	movq $0xaaaabbbbcccddd, %rax
trampoline_2_loads_return_rip:
	movq $0xaaaabbbbcccdde, %r10
trampoline_2_loads_call_post_func:
	movq $0xaaaabbbbccccc, %rdx
	jmp *%rdx
trampoline_2_end:
	
call_post_func:
	/* Stack layout:

	Caller's r10		(%rsp)
	Caller's rdx		8(%rsp)
	Caller's rax		16(%rsp)
	Pad			24(%rsp)
	Caller's area
	
	with the target name pointer in rax, the return address in
	r10, and nothing useful in rdx
	*/
	movq %r10, 24(%rsp)
	
	pushq %rsi

	lea 24(%rsp), %rsi /* Second argument -> return value */
	
	pushq %r8
	pushq %r9
	pushq %r11
	pushq %rcx
	pushq %rdi

	movq %rax, %rdi
	
	call post_func_audit@PLT
	
	popq %rdi
	popq %rcx
	popq %r11
	popq %r9
	popq %r8
	popq %rsi
	popq %r10
	popq %rdx

	VALGRIND_ESCAPE VALGRIND_ESCAPE_EXIT_MONITOR
	
	popq %rax

	ret

skip_call:
	/* Stack layout:

	Return rax		(%rsp)
	Return rdx		8(%rsp)
	Target name		16(%rsp)
	Target rip		24(%rsp)
	Return rip		32(%rsp)
	Caller's r11		40(%rsp)
	Caller's r10		48(%rsp)
	Caller's rax		56(%rsp)
	Caller's rdi		64(%rsp)
	Caller's rsi		72(%rsp)
	Caller's rdx		80(%rsp)
	Caller's rcx		88(%rsp)
	Caller's r8		96(%rsp)
	Caller's r9		104(%rsp)
	Stack arguments
	*/

	/* Shuffle the stack to look like this: 
	<stuff>			(%rsp)
	<stuff>			8(%rsp)
	<stuff>			16(%rsp)
	<stuff>			24(%rsp)
	Caller's r9		32(%rsp)
	Caller's r11		40(%rsp)
	Caller's r10		48(%rsp)
	Caller's r8		56(%rsp)
	Caller's rdi		64(%rsp)
	Caller's rsi		72(%rsp)
	Return rdx		80(%rsp)
	Caller's rcx		88(%rsp)
	Return rax		96(%rsp)
	Return rip		104(%rsp)
	Stack arguments
	*/
	movq 32(%rsp), %r8
	movq 104(%rsp), %r9
	movq %r9, 32(%rsp)
	movq %r8, 104(%rsp)
	movq 96(%rsp), %r8
	movq %r8, 56(%rsp)
	movq (%rsp), %rax
	movq %rax, 96(%rsp)
	movq 8(%rsp), %rdx
	movq %rdx, 80(%rsp)

	/* Clear out the <stuff> */
	addq $32, %rsp

	/* Restore registers and get out of here */
	popq %r9
	popq %r11
	popq %r10
	popq %r8
	popq %rdi
	popq %rsi
	popq %rdx
	popq %rcx

	VALGRIND_ESCAPE VALGRIND_ESCAPE_EXIT_MONITOR
	popq %rax
	ret
