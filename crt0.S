	.text
	.globl _start
	.type _start, @function
_start:
	/* Clear the frame pointer.  The ABI suggests this be done, to mark
	   the outermost frame obviously.  */
	xorl %ebp, %ebp

	/* Extract the arguments as encoded on the stack and set up
	   the arguments for `main': argc, argv.  envp will be determined
	   later in __libc_start_main.  */
	popl %esi		/* Pop the argument count.  */
	movl %esp, %ecx		/* argv starts just at the current stack top.*/

	/* Before pushing the arguments align the stack to a 16-byte
	(SSE needs 16-byte alignment) boundary to avoid penalties from
	misaligned accesses.  Thanks to Edward Seidl <seidl@janed.com>
	for pointing this out.  */
	andl $0xfffffff0, %esp
	pushl %eax		/* Push garbage because we allocate
				   28 more bytes.  */

	/* Provide the highest stack address to the user code (for stacks
	   which grow downwards).  */
	pushl %esp

	pushl %edx		/* Push address of the shared library
				   termination function.  */

	/* Push address of our own entry points to .fini and .init.  */
	pushl $__libc_csu_fini
	pushl $__libc_csu_init

	pushl %ecx		/* Push second argument: argv.  */
	pushl %esi		/* Push first argument: argc.  */

	pushl $main

	/* Call the user's main function, and exit with its value.
	   But let the libc call main.    */
	call __libc_start_main

	hlt			/* Crash if somehow `exit' does return.  */

#ifdef SHARED
1:	movl	(%esp), %ebx
	ret
#endif

/* To fulfill the System V/i386 ABI we need this symbol.  Yuck, it's so
   meaningless since we don't support machines < 80386.  */
	.section .rodata
	.globl _fp_hw
_fp_hw:	.long 3
	.size _fp_hw, 4
	.type _fp_hw,@object

/* Define a symbol for the first piece of initialized data.  */
	.data
	.globl __data_start
__data_start:
	.long 0
	.weak data_start
	data_start = __data_start
