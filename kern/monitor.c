// Simple command-line kernel monitor useful for
// controlling the kernel and exploring the system interactively.

#include <inc/stdio.h>
#include <inc/string.h>
#include <inc/memlayout.h>
#include <inc/assert.h>
#include <inc/x86.h>

#include <kern/console.h>
#include <kern/monitor.h>
#include <kern/kdebug.h>
#include <kern/trap.h>
#include <kern/pmap.h>

#define CMDBUF_SIZE	80	// enough for one VGA text line


struct Command {
	const char *name;
	const char *desc;
	// return -1 to force monitor to exit
	int (*func)(int argc, char** argv, struct Trapframe* tf);
};

static struct Command commands[] = {
	{ "help", "Display this list of commands", mon_help },
	{ "kerninfo", "Display information about the kernel", mon_kerninfo },
	{ "backtrace", "Display the trace infomation", mon_backtrace},
	{ "showmapping", "Displace the mapping of given virtual addressr", show_mapping},
};

/***** Implementations of basic kernel monitor commands *****/

int
mon_help(int argc, char **argv, struct Trapframe *tf)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(commands); i++)
		cprintf("%s - %s\n", commands[i].name, commands[i].desc);
	return 0;
}

int
mon_kerninfo(int argc, char **argv, struct Trapframe *tf)
{
	extern char _start[], entry[], etext[], edata[], end[];

	cprintf("Special kernel symbols:\n");
	cprintf("  _start                  %08x (phys)\n", _start);
	cprintf("  entry  %08x (virt)  %08x (phys)\n", entry, entry - KERNBASE);
	cprintf("  etext  %08x (virt)  %08x (phys)\n", etext, etext - KERNBASE);
	cprintf("  edata  %08x (virt)  %08x (phys)\n", edata, edata - KERNBASE);
	cprintf("  end    %08x (virt)  %08x (phys)\n", end, end - KERNBASE);
	cprintf("Kernel executable memory footprint: %dKB\n",
		ROUNDUP(end - entry, 1024) / 1024);
	return 0;
}

struct c_frame {
	uintptr_t caller_ebp;
	uintptr_t eip;
	uintptr_t args[5];
};

int
mon_backtrace(int argc, char **argv, struct Trapframe *tf)
{
	struct c_frame* cf = (struct c_frame *) read_ebp();
	while (NULL != cf) {
		cprintf("ebp %x  eip %x  args", cf, cf->eip);
		for (size_t i=0; i<5; ++i) {
			cprintf(" %08.x", cf->args[i]);
		}
		cprintf("\n");

		struct Eipdebuginfo info;
		// kern/monitor.c:143: monitor+106
		int ret = debuginfo_eip(cf->eip, &info);	
		if (ret == 0) {
			cprintf("       %s:%d: %.*s+%d\n", info.eip_file, info.eip_line,
				info.eip_fn_namelen, info.eip_fn_name, cf->eip - info.eip_fn_addr);
		}	
		
		cf = (struct c_frame *)cf->caller_ebp;
	}
	return 0;
}

uint32_t 
xtoi(char* buf) 
{
	uint32_t res = 0;
	buf += 2; 
	while (*buf) { 
		if (*buf >= 'a') *buf = *buf-'a'+'0'+10;
		res = res*16 + *buf - '0';
		++buf;
	}
	return res;
}

int
show_mapping(int argc, char **argv, struct Trapframe *tf)
{
	if (argc != 3) {
		cprintf("show mapping usage: showmapping va1 va2.\n");
		return 0;
	}
	uintptr_t start_va = xtoi(argv[1]), end_va = xtoi(argv[2]);
	assert(start_va < end_va);
	assert(end_va <= 0xffffffff);
	cprintf("virtual address begin: %x, end: %x \n", start_va, end_va);

	size_t cnt = 0;
	for (size_t va = start_va; va <= end_va && va >= start_va; va += PGSIZE) {
		cnt ++;
		if (cnt == 20) {
			cprintf("Too large range. show the first 20 entries. \n");
			break;
		}
		pte_t *pt = pgdir_walk(kern_pgdir, (void*)va, 0);
		if (!pt) {
			cprintf("va:%x no mapping to pa. \n", va);
		} else {
			physaddr_t pa = PTE_ADDR(*pt);
			uint32_t p = pa & PTE_P, w = pa & PTE_W, u = pa & PTE_U;
			cprintf("va:%x map to pa:%x. PET_P:%d, PET_W:%d, PET_U:%d \n", 
				va, pa, p, w, u);
		}
	}

	return 0;
}



/***** Kernel monitor command interpreter *****/

#define WHITESPACE "\t\r\n "
#define MAXARGS 16

static int
runcmd(char *buf, struct Trapframe *tf)
{
	int argc;
	char *argv[MAXARGS];
	int i;

	// Parse the command buffer into whitespace-separated arguments
	argc = 0;
	argv[argc] = 0;
	while (1) {
		// gobble whitespace
		while (*buf && strchr(WHITESPACE, *buf))
			*buf++ = 0;
		if (*buf == 0)
			break;

		// save and scan past next arg
		if (argc == MAXARGS-1) {
			cprintf("Too many arguments (max %d)\n", MAXARGS);
			return 0;
		}
		argv[argc++] = buf;
		while (*buf && !strchr(WHITESPACE, *buf))
			buf++;
	}
	argv[argc] = 0;

	// Lookup and invoke the command
	if (argc == 0)
		return 0;
	for (i = 0; i < ARRAY_SIZE(commands); i++) {
		if (strcmp(argv[0], commands[i].name) == 0)
			return commands[i].func(argc, argv, tf);
	}
	cprintf("Unknown command '%s'\n", argv[0]);
	return 0;
}

void
monitor(struct Trapframe *tf)
{
	char *buf;

	cprintf("Welcome to the JOS kernel monitor!\n");
	cprintf("Type 'help' for a list of commands.\n");

	if (tf != NULL)
		print_trapframe(tf);

	while (1) {
		buf = readline("K> ");
		if (buf != NULL)
			if (runcmd(buf, tf) < 0)
				break;
	}
}
