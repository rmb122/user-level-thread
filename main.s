	.file	"main.c"
	.text
	.comm	curr_handler,4,4
	.section	.rodata
.LC0:
	.string	"asdas"
	.text
	.globl	handler
	.type	handler, @function
handler:
.LFB0:
	.cfi_startproc
	pushq	%rbp
	.cfi_def_cfa_offset 16
	.cfi_offset 6, -16
	movq	%rsp, %rbp
	.cfi_def_cfa_register 6
	movl	$1, %esi
	movl	$14, %edi
	call	signal@PLT
	movl	curr_handler(%rip), %eax
	testl	%eax, %eax
	je	.L3
	movl	$1, curr_handler(%rip)
	leaq	.LC0(%rip), %rdi
	movl	$0, %eax
	call	printf@PLT
.L3:
	nop
	popq	%rbp
	.cfi_def_cfa 7, 8
	ret
	.cfi_endproc
.LFE0:
	.size	handler, .-handler
	.globl	set_timer
	.type	set_timer, @function
set_timer:
.LFB1:
	.cfi_startproc
	pushq	%rbp
	.cfi_def_cfa_offset 16
	.cfi_offset 6, -16
	movq	%rsp, %rbp
	.cfi_def_cfa_register 6
	subq	$48, %rsp
	movq	%fs:40, %rax
	movq	%rax, -8(%rbp)
	xorl	%eax, %eax
	leaq	handler(%rip), %rsi
	movl	$14, %edi
	call	signal@PLT
	movq	$0, -32(%rbp)
	movq	$10000, -24(%rbp)
	movq	$0, -48(%rbp)
	movq	$10000, -40(%rbp)
	leaq	-48(%rbp), %rax
	movl	$0, %edx
	movq	%rax, %rsi
	movl	$0, %edi
	call	setitimer@PLT
	nop
	movq	-8(%rbp), %rax
	xorq	%fs:40, %rax
	je	.L5
	call	__stack_chk_fail@PLT
.L5:
	leave
	.cfi_def_cfa 7, 8
	ret
	.cfi_endproc
.LFE1:
	.size	set_timer, .-set_timer
	.section	.rodata
.LC1:
	.string	"%lld\n"
.LC2:
	.string	"Hello, World!"
	.text
	.globl	main
	.type	main, @function
main:
.LFB2:
	.cfi_startproc
	pushq	%rbp
	.cfi_def_cfa_offset 16
	.cfi_offset 6, -16
	movq	%rsp, %rbp
	.cfi_def_cfa_register 6
	subq	$992, %rsp
	movq	%fs:40, %rax
	movq	%rax, -8(%rbp)
	xorl	%eax, %eax
	movl	$0, %eax
	call	set_timer
	leaq	-976(%rbp), %rax
	movq	%rax, %rdi
	call	getcontext@PLT
	movl	$0, -980(%rbp)
	jmp	.L8
.L9:
	movl	-980(%rbp), %eax
	cltq
	addq	$4, %rax
	movq	-968(%rbp,%rax,8), %rax
	movq	%rax, %rsi
	leaq	.LC1(%rip), %rdi
	movl	$0, %eax
	call	printf@PLT
	addl	$1, -980(%rbp)
.L8:
	cmpl	$22, -980(%rbp)
	jle	.L9
	leaq	.LC2(%rip), %rdi
	call	puts@PLT
	leaq	-976(%rbp), %rax
	movq	%rax, %rdi
	call	setcontext@PLT
	movl	$0, %eax
	movq	-8(%rbp), %rdx
	xorq	%fs:40, %rdx
	je	.L11
	call	__stack_chk_fail@PLT
.L11:
	leave
	.cfi_def_cfa 7, 8
	ret
	.cfi_endproc
.LFE2:
	.size	main, .-main
	.ident	"GCC: (GNU) 9.2.0"
	.section	.note.GNU-stack,"",@progbits
