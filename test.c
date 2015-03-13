__asm__("mov $0x7fffa0000, %rax");
__asm__("call 0x5	\n"
	"pop %rbx	\n"
	"sub %rbx, %rax");
__asm__("mov $0xffffa7a7a7, %rax");
__asm__("mov $0xffffa7a7a7, %rbx");
__asm__("mov $0xffffa7a7a7, %rcx");
__asm__("mov $0xffffa7a7a7, %rdx");
__asm__("mov $0xffffa7a7a7, %rdi");
__asm__("mov $0xffffa7a7a7, %rsi");
__asm__("mov $0xffffa7a7a7, %r15");
__asm__("mov $0xffffa7a7a7, %r14");
__asm__("mov $0xffffa7a7a7, %r13");
__asm__("mov $0xffffa7a7a7, %r12");
__asm__("mov $0xffffa7a7a7, %r11");
__asm__("mov $0xffffa7a7a7, %r10");
__asm__("mov $0xffffa7a7a7, %r9");
__asm__("mov $0xffffa7a7a7, %r8");
__asm__("mov $0xffffa7a7a7, %rsp");
__asm__("mov $0x7fffa7a7ac, %r15");
__asm__("jmp *r15");

