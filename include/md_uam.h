
#ifndef _MACHINE_MD_VAR_H_
#define	_MACHINE_MD_VAR_H_

#include <machine/reg.h>

/*
 * Miscellaneous machine-dependent declarations.
 */
extern	long	Maxmem;
extern	char	sigcode[];
extern	int	szsigcode, szosigcode;
extern	uint32_t *vm_page_dump;
extern	int vm_page_dump_size;

extern vm_offset_t kstack0;
extern vm_offset_t kernel_kseg0_end;

void	MipsSaveCurFPState(struct thread *);
void	fork_trampoline(void);
void	cpu_swapin(struct proc *);
uintptr_t MipsEmulateBranch(struct trapframe *, uintptr_t, int, uintptr_t);
void MipsSwitchFPState(struct thread *, struct trapframe *);
u_long	kvtop(void *addr);
int	is_cacheable_mem(vm_paddr_t addr);
void	mips_generic_reset(void);

#define	MIPS_DEBUG   0

#if MIPS_DEBUG
#define	MIPS_DEBUG_PRINT(fmt, args...)	printf("%s: " fmt "\n" , __FUNCTION__ , ## args)
#else
#define	MIPS_DEBUG_PRINT(fmt, args...)
#endif

void	mips_vector_init(void);
void	mips_cpu_init(void);
void	mips_pcpu0_init(void);
void	mips_proc0_init(void);
void	mips_postboot_fixup(void);

/* Platform call-downs. */
void	platform_identify(void);

extern int busdma_swi_pending;
void	busdma_swi(void);

struct	dumperinfo;
void	dump_add_page(vm_paddr_t);
void	dump_drop_page(vm_paddr_t);
void	minidumpsys(struct dumperinfo *);
#endif /* !_MACHINE_MD_VAR_H_ */
