/*
 *	Implement binary loading for 32bit platforms. We use a variant of
 *	a.out with a custom relocation format. The reloc format could be
 *	compacted and this may be worth doing evnetually.
 *
 *	Binaries are assumed to have a separate shareable text segment but
 *	whether this feature is used depends upon the memroy banking model
 *	that is being used.
 */

#include <kernel.h>
#include <kernel32.h>
#include <version.h>
#include <kdata.h>
#include <printf.h>
#include <exec.h>

static void close_on_exec(void)
{
	int j;
	for (j = UFTSIZE - 1; j >= 0; --j) {
		if (udata.u_cloexec & (1 << j))
			doclose(j);
	}
	udata.u_cloexec = 0;
}

static int valid_hdr(inoptr ino, struct exec *bf)
{
	uint32_t n = ntohl(bf->a_midmag);
	uint32_t feat = (n >> 16) & 0x3F0;
	uint32_t sub = (n >> 16) & 0xF;

	/* Wrong architecture */
	if (feat != CPU_MID)
		return 0;
	/* Sufficiently featured ? */
	if (sys_cpu_feat < sub)
		return 0;

	/* We only permit NMAGIC files */
	if ((n & 0xFFFF) != NMAGIC)
		return 0;
	if (bf->stacksize < 4096)
		bf->stacksize = 4096;
	/* Emtry must be within text */
	if (bf->a_entry >= bf->a_text)
		return 0;
	/* Wrapped */
	if (bf->a_text + bf->a_data < bf->a_text)
		return 0;
	if (bf->a_text + bf->a_data + bf->a_bss < bf->a_text)
		return 0;
	if (bf->a_data + bf->a_bss + bf->stacksize < bf->a_data)
		return 0;
	if (bf->a_data + bf->a_bss + bf->stacksize + bf->a_trsize < bf->a_data)
		return 0;
	/* Not enough file content. Only the first 0x20 bytes of the header are
	   not part of the binary */
	if (bf->a_text + bf->a_data + bf->a_trsize + 0x20 > ino->c_node.i_size)
		return 0;
	/* Revisit this for other ports. Avoid alignment traps */
	if (UNALIGNED(bf->a_text | bf->a_data | bf->a_entry | bf->a_bss | bf->a_trsize))
		return 0;
	/* Fix up the BSS so that it's big enough to hold the relocations
	   Factor in the stack as well. Generally the stack space is enough
	   to avoid any expansion. When we switch to compact relocations
	   the problem should go away entirely */
	/* TODO ? max sane number of relocs to stop wrap or silly expansion */
	if (bf->a_bss + bf->stacksize < bf->a_trsize)
		bf->a_bss = bf->a_trsize - bf->stacksize;

	return 1;
}

/*
 *	We have two blocks. block 0 is the code, block 1 is the data.
 *	We need to relocate accordingly. It's possible that the underlying
 *	memory manager used one map but if so the addresses will be
 *	contiguous and it'll just work treating it as two banks
 *
 *	NS32K needs some special handling as it can have fixups that
 *	are wrong endian and fixups that are right endian.
 */
static unsigned relocate(struct exec *bf)
{
#if defined(__ns32k__)
	unsigned arseendian;
	uint32_t sizebits;
#endif
	/* Relocations lie over the BSS as loaded */
	uint32_t *rp = (uint32_t *)(udata.u_database + bf->a_data);
	uint32_t n = bf->a_trsize / sizeof(uint32_t);
	uint32_t codebase = udata.u_codebase;
	uint32_t database = udata.u_database;

	uint32_t relend = bf->a_text + bf->a_data;

	/* We can use _uput/_uget as we set up the memory map so we know
	   it is valid */
	while (n--) {
		uint32_t *mp;
		uint32_t mv;
		uint32_t v = _ugetl(rp++);
#if defined(__ns32k__)
		if (v & 0x80000000)
			arseendian = 1;
		else
			arseendian = 0;
		v &= 0x7FFFFFFF;
#endif
		if (v > relend - 3) {	/* Bad relocation - should we fail ? */
/*			kprintf("R0 %d left, %p relendd %p", n, v, relend - 3 ); */
			return 1;
		}
		/* Which block holds the offset ? */
		if (v <= bf->a_text - 3)
			mp = (uint32_t *)(codebase + v);
		else if (v >= bf->a_text)
			mp = (uint32_t *)(database + v - bf->a_text);
		else {	/* Bad */
/*			kprintf("R1 %d left, %p a_data %p", n, v, bf->a_data); */
			return 1;
		}
		/* Now what are we relocating against */
		mv = _ugetl(mp);
#if defined(__ns32k__)
		if (arseendian)
			mv = ntohl(mv);
		sizebits = 0;
		if ((mv & 0xC0000000) == 0xC0000000)
			sizebits = 1;
		/* We don't deal with underflows on the sized 30 bit signed relocs
		   We should never ever get one ; TODO review */
		mv &= 0x3FFFFFFF;
#endif
/*		kprintf("Reloc %x:%p (%p) to ", v, mp, mv); */
		if (mv >= bf->a_text)
			mv += database - bf->a_text;
		else
			mv += codebase;
		/* Write the updated value */
/*		kprintf("%p\n", mv); */
#if defined(__ns32k__)
		/* Put back the field size info */
		if (sizebits)
			mv |= 0xC0000000;
		if (arseendian)
			mv = ntohl(mv);
#endif
		_uputl(mv, mp);
	}
	return 0;
}


/* User's execve() call. All other flavors are library routines. */
/*******************************************
execve (name, argv, envp)        Function 23
char *name;
char *argv[];
char *envp[];
********************************************/
#define name (uint8_t *)udata.u_argn
#define argv (uint8_t **)udata.u_argn1
#define envp (uint8_t **)udata.u_argn2

arg_t _execve(void)
{
	/* Not ideal on stack */
	struct exec aout;
	inoptr ino;
	uint8_t **nargv;	/* In user space */
	uint8_t **nenvp;	/* In user space */
	struct s_argblk *abuf, *ebuf;
	int argc;
	uaddr_t top;
	uaddr_t go;
	uint32_t true_brk;
	uint_fast8_t mflags;

	if (!(ino = n_open_lock(name, NULLINOPTR)))
		return (-1);

	if (!((getperm(ino) & OTH_EX) &&
	      (ino->c_node.i_mode & F_REG) &&
	      (ino->c_node.i_mode & (OWN_EX | OTH_EX | GRP_EX)))) {
		udata.u_error = EACCES;
		goto nogood;
	}

	mflags = fs_tab[ino->c_super].m_flags;
	if (mflags & MS_NOEXEC) {
		udata.u_error = EACCES;
		goto nogood;
	}

	setftime(ino, A_TIME);

	udata.u_offset = 0;
	udata.u_count = sizeof(struct exec);
	udata.u_base = (void *)&aout;
	udata.u_sysio = true;

	readi(ino, 0);
	if (udata.u_done != sizeof(struct exec)) {
		udata.u_error = ENOEXEC;
		goto nogood;
	}

	/* FIXME: ugly - save this as valid_hdr modifies it */
	true_brk = aout.a_data + aout.a_bss;

	if (!valid_hdr(ino, &aout)) {
		udata.u_error = ENOEXEC;
		goto nogood2;
	}

	/* Gather the arguments, and put them in temporary buffers. */
	abuf = (struct s_argblk *) tmpbuf();
	/* Put environment in another buffer. */
	ebuf = (struct s_argblk *) tmpbuf();

	/* Read args and environment from process memory */
	if (rargs(argv, abuf) || rargs(envp, ebuf))
		goto nogood3;

	/* This must be the last test as it makes changes if it works */
	/* FIXME: ENOMEM fix needs to go to 16bit ? */
	if ((udata.u_error = pagemap_realloc(&aout, 0 /* unused */)) != 0)
		goto nogood3;

#ifdef CONFIG_PLATFORM_UDMA
	plt_udma_kill(p);
#endif
	/* Core dump and ptrace permission logic */
#ifdef CONFIG_LEVEL_2
	/* Q: should uid == 0 mean we always allow core */
	if ((!(getperm(ino) & OTH_RD)) ||
		(ino->c_node.i_mode & (SET_UID | SET_GID)))
		udata.u_flags |= U_FLAG_NOCORE;
	else
		udata.u_flags &= ~U_FLAG_NOCORE;
#endif

	if (!(mflags & MS_NOSUID)) {
		/* setuid, setgid if executable requires it */
		if (ino->c_node.i_mode & SET_UID)
			udata.u_euid = ino->c_node.i_uid;
		if (ino->c_node.i_mode & SET_GID)
			udata.u_egid = ino->c_node.i_gid;
	}

	top = udata.u_database + aout.a_data + aout.a_bss + aout.stacksize;

	udata.u_top = top;
	udata.u_ptab->p_top = top;

//	kprintf("user space at %p\n", progbase);
//	kprintf("top at %p\n", progbase + bin_size);

	go = (uint32_t)udata.u_codebase + aout.a_entry;

	close_on_exec();

	/*
	 *  Read in the rest of the program, block by block. We rely upon
	 *  the optimization path in readi to spot this is a big move to user
	 *  space and move it directly.
	 *
	 *  There is some magic here as the binary contains a 32 byte header
	 *  in the crt0 that is replaced with the kernel vdso but can contain
	 *  header info a.out is lacking. Right now it contains the stack size
	 *  which we already loaded. We don't need to put the stack into the
	 *  user map. The user will never see it as the vdso arrives before they
	 *  can look.
	 */

	udata.u_base = (uint8_t *)udata.u_codebase + 0x20;
	udata.u_count = aout.a_text - 0x20;
	udata.u_sysio = false;
	/* As we allocated this space we know the range is valid */
	readi(ino, 0);
	if (udata.u_done != aout.a_text - 0x20)
		goto nogood4;

	/* Now the data (and relocations) */
	udata.u_base = (uint8_t *) udata.u_database;
	udata.u_count = aout.a_data + aout.a_trsize;
	udata.u_sysio = false;
	readi(ino, 0);
	if (udata.u_done != aout.a_data + aout.a_trsize)
		goto nogood4;

	if (relocate(&aout))
		goto nogood4;

	/* This may wipe the relocations */	
	uzero((uint8_t *)udata.u_database + aout.a_data,
		aout.a_bss + aout.stacksize);

	/* Use of brk eats into the stack allocation */

	/* Use the temporary we saved (hack) as we mangled bss_end */
	udata.u_break = udata.u_database + true_brk;

	/* Turn off caught signals */
	memset(udata.u_sigvec, 0, sizeof(udata.u_sigvec));

	/* place the arguments, environment and stack at the top of userspace memory. */

	/* Write back the arguments and the environment */
	nargv = wargs(((uint8_t *) top - 4), abuf, &argc);
	nenvp = wargs((uint8_t *) (nargv), ebuf, NULL);

	/* Fill in udata.u_name with Program invocation name */
	uget((void *) ugetl(nargv, NULL), udata.u_name, 8);
	memcpy(udata.u_ptab->p_name, udata.u_name, 8);

	tmpfree(abuf);
	tmpfree(ebuf);
	i_unlock_deref(ino);

	/* Shove argc and the address of argv just below envp */
	uputl((uint32_t) nargv, nenvp - 1);
	uputl((uint32_t) argc, nenvp - 2);

	// Set stack pointer for the program
	udata.u_isp = nenvp - 2;

	/*
	 * Sort of - it's a good way to deal with all the stupidity of
	 * random 68K platforms we will have to handle, and a nice place
	 * to stuff the signal trampoline 8)
	 */
	install_vdso();

	kprintf("Code at %p , Data at %p\n",
		udata.u_codebase, udata.u_database);
	kprintf("Go = %p ISP = %p\n", go, udata.u_isp);

	doexec(go);

nogood4:
	/* Must not run userspace */
	ssig(udata.u_ptab, SIGKILL);
nogood3:
	tmpfree(abuf);
	tmpfree(ebuf);
nogood2:
nogood:
	i_unlock_deref(ino);
	return (-1);
}

#undef name
#undef argv
#undef envp
