/*
 *	Implement binary loading for 32bit platforms. We use the ucLinux binflat
 *	format with a simple magic number tweak to avoid confusion with ucLinux
 *
 *	TODO: Right now we do a classic bss/stack layout and dont' support
 *	fixed stack/expanding BSS, multiple segment loaders for flat binaries
 *	etc. We really ought to pick a saner format. There's much to be said
 *	for a.out with relocs over flat, or relocs packed into BSS and letting
 *	user space do the relocs (so anything that overflows or gets it wrong
 *	goes bang the right side of the kernel/user boundary). The a.out
 *	standard relocs are 8bytes each so a packed reloc from user space
 *	would be far better. That might also be the best way to handle
 *	shared code segment designs (aka 'resident' in Amiga speak)
 *
 *	Note: bFLT is actually broken by design for the corner case of
 *	splitting the code and data segment into two loads, when a binary
 *	computes an address that is a negative offset from the data segment
 *	(eg when biasing the start of an array)
 *
 *	FIXME: we should set the stack to the top of the available space
 *	we get allocated not just rely on the stack allocation given. For
 *	that we need a way to query the space available.
 */

#include <kernel.h>
#include <kernel32.h>
#include <version.h>
#include <kdata.h>
#include <printf.h>
#include <flat.h>

static void close_on_exec(void)
{
	int j;
	for (j = UFTSIZE - 1; j >= 0; --j) {
		if (udata.u_cloexec & (1 << j))
			doclose(j);
	}
	udata.u_cloexec = 0;
}

static int valid_hdr(inoptr ino, struct binfmt_flat *bf)
{
	if (bf->stack_size < 4096)
		bf->stack_size = 4096;
	if (bf->rev != FLAT_VERSION)
		return 0;
	if (bf->entry >= bf->data_start)
		return 0;
	if (bf->data_start > bf->data_end)
		return 0;
	if (bf->data_end > bf->bss_end)
		return 0;
	if (bf->bss_end + bf->stack_size < bf->bss_end)
		return 0;
	if (bf->data_end > ino->c_node.i_size)
		return 0;
	/* Revisit this for other ports. Avoid alignment traps */
	if (UNALIGNED(bf->reloc_start | bf->stack_size | bf->data_start | bf->data_end | bf->bss_end | bf->entry))
		return 0;
	/* Fix up the BSS so that it's big enough to hold the relocations
	   FIXME: this is a) ugly and b) overcautious as we should factor
	   in the stack space too */
	if (bf->bss_end - bf->data_end < 4 * bf->reloc_count)
		bf->bss_end = bf->data_end + 4 * bf->reloc_count;
	if (bf->reloc_start + bf->reloc_count * 4 > ino->c_node.i_size ||
		bf->reloc_start + bf->reloc_count * 4 < bf->reloc_start)
		return 0;
	if (bf->flags != 1)
		return 0;
	return 1;
}

/* For now we load the binary in one block, including code/data/bss. We can
   look at better formats, split binaries etc later maybe */
static void relocate(struct binfmt_flat *bf, uaddr_t progbase, uint32_t size)
{
	uint32_t *rp = (uint32_t *)(progbase + bf->reloc_start);
	uint32_t n = bf->reloc_count;

	/* TODO: check should this be -0x43 */
	size -= 3;		/* We work in 32bit chunks */
	progbase += 0x40;	/* Offset is relative the header end */

	/* We can use _uput/_uget as we set up the memory map so we know
	   it is valid */
	while (n--) {
		uint32_t v = ntohl(_ugetl(rp++));
		if (v < size) {
			uint32_t *mp = (uint32_t *)(progbase + v);
			_uputl(_ugetl(mp) + progbase, mp);
		}
	}
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
	struct binfmt_flat binflat;
	inoptr ino;
	uint8_t **nargv;	/* In user space */
	uint8_t **nenvp;	/* In user space */
	struct s_argblk *abuf, *ebuf;
	int argc;
	uint32_t bin_size;	/* Will need to be bigger on some cpus */
	uaddr_t progbase, top;
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
	udata.u_count = sizeof(struct binfmt_flat);
	udata.u_base = (void *)&binflat;
	udata.u_sysio = true;

	readi(ino, 0);
	if (udata.u_done != sizeof(struct binfmt_flat)) {
		udata.u_error = ENOEXEC;
		goto nogood;
	}

	binflat.rev = ntohl(binflat.rev);
	binflat.entry = ntohl(binflat.entry);
	binflat.data_start = ntohl(binflat.data_start);
	binflat.data_end = ntohl(binflat.data_end);
	binflat.bss_end = ntohl(binflat.bss_end);
	binflat.stack_size = ntohl(binflat.stack_size);
	binflat.reloc_start = ntohl(binflat.reloc_start);
	binflat.reloc_count = ntohl(binflat.reloc_count);
	binflat.flags = ntohl(binflat.flags);

	/* FIXME: ugly - save this as valid_hdr modifies it */
	true_brk = binflat.bss_end;

	/* Hard coded for our 68K format. We don't quite use the ucLinux
	   names, we don't want to load a ucLinux binary in error! */
	if (memcmp(binflat.magic, FLAT_FUZIX_MAGIC, 4) || !valid_hdr(ino, &binflat)) {
		udata.u_error = ENOEXEC;
		goto nogood2;
	}

	/* Memory needed */
	bin_size = binflat.bss_end + binflat.stack_size;

	/* Overflow ? */
	if (bin_size < binflat.bss_end) {
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
	/* FIXME: need to update this to support split code/data and to fix
	   stack handling nicely */
	/* FIXME: ENOMEM fix needs to go to 16bit ? */
	/* NULL for exec is a hack - we need the binfmt_flat to be
	   our exec structure in this case I think */
	if ((udata.u_error = pagemap_realloc(NULL, bin_size)) != 0)
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

	udata.u_codebase = progbase = pagemap_base();
	/* From this point on we are commmited to the exec() completing
	   so we can start writing over the old program */
	uput(&binflat, (uint8_t *)progbase, sizeof(struct binfmt_flat));

	if (!(mflags & MS_NOSUID)) {
		/* setuid, setgid if executable requires it */
		if (ino->c_node.i_mode & SET_UID)
			udata.u_euid = ino->c_node.i_uid;
		if (ino->c_node.i_mode & SET_GID)
			udata.u_egid = ino->c_node.i_gid;
	}

	top = progbase + bin_size;

	udata.u_top = top;
	udata.u_ptab->p_top = top;

//	kprintf("user space at %p\n", progbase);
//	kprintf("top at %p\n", progbase + bin_size);

	bin_size = binflat.reloc_start + 4 * binflat.reloc_count;
	go = (uint32_t)progbase + binflat.entry;

	close_on_exec();

	/*
	 *  Read in the rest of the program, block by block. We rely upon
	 *  the optimization path in readi to spot this is a big move to user
	 *  space and move it directly.
	 */

	 if (bin_size > sizeof(struct binfmt_flat)) {
		/* We copied the header already */
		bin_size -= sizeof(struct binfmt_flat);
		udata.u_base = (uint8_t *)progbase +
					sizeof(struct binfmt_flat);
		udata.u_count = bin_size;
		udata.u_sysio = false;
		/* As we allocated this space we know the range is valid */
		readi(ino, 0);
		if (udata.u_done != bin_size)
			goto nogood4;
	}

	/* Header isn't counted in relocations */
	relocate(&binflat, progbase, bin_size);
	/* This may wipe the relocations */	
	uzero((uint8_t *)progbase + binflat.data_end,
		binflat.bss_end - binflat.data_end + binflat.stack_size);

	/* Use of brk eats into the stack allocation */

	/* Use the temporary we saved (hack) as we mangled bss_end */
	udata.u_break = udata.u_codebase + true_brk;

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

//	kprintf("Go = %p ISP = %p\n", go, udata.u_isp);

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
