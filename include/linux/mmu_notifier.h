#ifndef _LINUX_MMU_NOTIFIER_H
#define _LINUX_MMU_NOTIFIER_H

#include <linux/list.h>
#include <linux/spinlock.h>
#include <linux/mm_types.h>
#include <linux/srcu.h>

struct mmu_notifier;
struct mmu_notifier_ops;

/* MMU Events report fine-grained information to the callback routine, allowing
 * the event listener to make a more informed decision as to what action to
 * take. The event types are:
 *
 *   - MMU_MIGRATE: memory is migrating from one page to another, thus all write
 *     access must stop after invalidate_range_start callback returns.
 *     Furthermore, no read access should be allowed either, as a new page can
 *     be remapped with write access before the invalidate_range_end callback
 *     happens and thus any read access to old page might read stale data. There
 *     are several sources for this event, including:
 *
 *         - A page moving to swap (various reasons, including page reclaim),
 *         - An mremap syscall,
 *         - migration for NUMA reasons,
 *         - balancing the memory pool,
 *         - write fault on COW page,
 *         - and more that are not listed here.
 *
 *   - MMU_MPROT: memory access protection is changing. Refer to the vma to get
 *     the new access protection. All memory access are still valid until the
 *     invalidate_range_end callback.
 *
 *   - MMU_MUNMAP: the range is being unmapped (outcome of a munmap syscall or
 *     process destruction). However, access is still allowed, up until the
 *     invalidate_range_free_pages callback. This also implies that secondary
 *     page table can be trimmed, because the address range is no longer valid.
 *
 *   - MMU_WRITE_BACK: memory is being written back to disk, all write accesses
 *     must stop after invalidate_range_start callback returns. Read access are
 *     still allowed.
 *
 *   - MMU_WRITE_PROTECT: memory is being writte protected (ie should be mapped
 *     read only no matter what the vma memory protection allows). All write
 *     accesses must stop after invalidate_range_start callback returns. Read
 *     access are still allowed.
 *
 *   - MMU_STATUS memory status change, like soft dirty, or huge page
 *     splitting flag being set on pmd.
 *
 * If in doubt when adding a new notifier caller, please use MMU_MIGRATE,
 * because it will always lead to reasonable behavior, but will not allow the
 * listener a chance to optimize its events.
 */
enum mmu_event {
	MMU_MIGRATE = 0,
	MMU_MPROT,
	MMU_MUNMAP,
	MMU_STATUS,
	MMU_WRITE_BACK,
	MMU_WRITE_PROTECT,
};

#ifdef CONFIG_MMU_NOTIFIER

/*
 * The mmu notifier_mm structure is allocated and installed in
 * mm->mmu_notifier_mm inside the mm_take_all_locks() protected
 * critical section and it's released only when mm_count reaches zero
 * in mmdrop().
 */
struct mmu_notifier_mm {
	/* all mmu notifiers registerd in this mm are queued in this list */
	struct hlist_head list;
	/* to serialize the list modifications and hlist_unhashed */
	spinlock_t lock;
};

struct mmu_notifier_ops {
	/*
	 * Called either by mmu_notifier_unregister or when the mm is
	 * being destroyed by exit_mmap, always before all pages are
	 * freed. This can run concurrently with other mmu notifier
	 * methods (the ones invoked outside the mm context) and it
	 * should tear down all secondary mmu mappings and freeze the
	 * secondary mmu. If this method isn't implemented you've to
	 * be sure that nothing could possibly write to the pages
	 * through the secondary mmu by the time the last thread with
	 * tsk->mm == mm exits.
	 *
	 * As side note: the pages freed after ->release returns could
	 * be immediately reallocated by the gart at an alias physical
	 * address with a different cache model, so if ->release isn't
	 * implemented because all _software_ driven memory accesses
	 * through the secondary mmu are terminated by the time the
	 * last thread of this mm quits, you've also to be sure that
	 * speculative _hardware_ operations can't allocate dirty
	 * cachelines in the cpu that could not be snooped and made
	 * coherent with the other read and write operations happening
	 * through the gart alias address, so leading to memory
	 * corruption.
	 */
	void (*release)(struct mmu_notifier *mn,
			struct mm_struct *mm);

	/*
	 * clear_flush_young is called after the VM is
	 * test-and-clearing the young/accessed bitflag in the
	 * pte. This way the VM will provide proper aging to the
	 * accesses to the page through the secondary MMUs and not
	 * only to the ones through the Linux pte.
	 */
	int (*clear_flush_young)(struct mmu_notifier *mn,
				 struct mm_struct *mm,
				 unsigned long address);

	/*
	 * test_young is called to check the young/accessed bitflag in
	 * the secondary pte. This is used to know if the page is
	 * frequently used without actually clearing the flag or tearing
	 * down the secondary mapping on the page.
	 */
	int (*test_young)(struct mmu_notifier *mn,
			  struct mm_struct *mm,
			  unsigned long address);

	/*
	 * change_pte is called in cases that pte mapping to page is changed:
	 * for example, when ksm remaps pte to point to a new shared page.
	 */
	void (*change_pte)(struct mmu_notifier *mn,
			   struct mm_struct *mm,
			   unsigned long address,
			   pte_t pte,
			   enum mmu_event event);

	/*
	 * Before this is invoked any secondary MMU is still ok to
	 * read/write to the page previously pointed to by the Linux
	 * pte because the page hasn't been freed yet and it won't be
	 * freed until this returns. If required set_page_dirty has to
	 * be called internally to this method.
	 */
	void (*invalidate_page)(struct mmu_notifier *mn,
				struct vm_area_struct *vma,
				unsigned long address,
				enum mmu_event event);

	/*
	 * invalidate_range_start() and invalidate_range_end() must be
	 * paired and are called only when the mmap_sem and/or the
	 * locks protecting the reverse maps are held. If the subsystem
	 * can't guarantee that no additional references are taken to
	 * the pages in the range, it has to implement the
	 * invalidate_range() notifier to remove any references taken
	 * after invalidate_range_start().
	 *
	 * Invalidation of multiple overlapping ranges may be optionally
	 * permitted by the driver. Either way the establishment of sptes
	 * should be avoided in the range passed to invalidate_range_start/
	 * end for the whole duration of the invalidate_range_start/end
	 * critical section.
	 *
	 * invalidate_range_start() is called when all pages in the
	 * range are still mapped and have at least a refcount of one.
	 *
	 * invalidate_range_free_pages() is called when a bunch of pages
	 * are unmapped but not yet freed by the VM.
	 *
	 * invalidate_range_end() is called when all pages in the
	 * range have been unmapped and the pages have been freed by
	 * the VM.
	 *
	 * The VM will remove the page table entries and potentially
	 * the page between invalidate_range_start() and
	 * invalidate_range_end(). If the page must not be freed
	 * because of pending I/O or other circumstances then the
	 * invalidate_range_start() callback (or the initial mapping
	 * by the driver) must make sure that the refcount is kept
	 * elevated.
	 *
	 * If the driver increases the refcount when the pages are
	 * initially mapped into an address space then either
	 * invalidate_range_start() or invalidate_range_end() may
	 * decrease the refcount. If the refcount is decreased on
	 * invalidate_range_start() then the VM can free pages as page
	 * table entries are removed.  If the refcount is only
	 * droppped on invalidate_range_end() then the driver itself
	 * will drop the last refcount but it must take care to flush
	 * any secondary tlb before doing the final free on the
	 * page. Pages will no longer be referenced by the linux
	 * address space but may still be referenced by sptes until
	 * the last refcount is dropped.
	 */
	void (*invalidate_range_start)(struct mmu_notifier *mn,
				       struct vm_area_struct *vma,
				       unsigned long start,
				       unsigned long end,
				       enum mmu_event event);
	void (*invalidate_range_free_pages)(struct mmu_notifier *mn,
					    struct vm_area_struct *vma,
					    unsigned long start,
					    unsigned long end);
	void (*invalidate_range_end)(struct mmu_notifier *mn,
				     struct vm_area_struct *vma,
				     unsigned long start,
				     unsigned long end,
				     enum mmu_event event);

	/*
	 * invalidate_range() is either called between
	 * invalidate_range_start() and invalidate_range_end() when the
	 * VM has to free pages that where unmapped, but before the
	 * pages are actually freed, or outside of _start()/_end() when
	 * a (remote) TLB is necessary.
	 *
	 * If invalidate_range() is used to manage a non-CPU TLB with
	 * shared page-tables, it not necessary to implement the
	 * invalidate_range_start()/end() notifiers, as
	 * invalidate_range() alread catches the points in time when an
	 * external TLB range needs to be flushed.
	 *
	 * The invalidate_range() function is called under the ptl
	 * spin-lock and not allowed to sleep.
	 *
	 * Note that this function might be called with just a sub-range
	 * of what was passed to invalidate_range_start()/end(), if
	 * called between those functions.
	 */
	void (*invalidate_range)(struct mmu_notifier *mn, struct mm_struct *mm,
				 unsigned long start, unsigned long end);
};

/*
 * The notifier chains are protected by mmap_sem and/or the reverse map
 * semaphores. Notifier chains are only changed when all reverse maps and
 * the mmap_sem locks are taken.
 *
 * Therefore notifier chains can only be traversed when either
 *
 * 1. mmap_sem is held.
 * 2. One of the reverse map locks is held (i_mmap_mutex or anon_vma->rwsem).
 * 3. No other concurrent thread can access the list (release)
 */
struct mmu_notifier {
	struct hlist_node hlist;
	const struct mmu_notifier_ops *ops;
};

static inline int mm_has_notifiers(struct mm_struct *mm)
{
	return unlikely(mm->mmu_notifier_mm);
}

extern int mmu_notifier_register(struct mmu_notifier *mn,
				 struct mm_struct *mm);
extern int __mmu_notifier_register(struct mmu_notifier *mn,
				   struct mm_struct *mm);
extern void mmu_notifier_unregister(struct mmu_notifier *mn,
				    struct mm_struct *mm);
extern void __mmu_notifier_unregister(struct mmu_notifier *mn,
				      struct mm_struct *mm);
extern void mmu_notifier_synchronize(void);
extern void __mmu_notifier_mm_destroy(struct mm_struct *mm);
extern void __mmu_notifier_release(struct mm_struct *mm);
extern int __mmu_notifier_clear_flush_young(struct mm_struct *mm,
					  unsigned long address);
extern int __mmu_notifier_test_young(struct mm_struct *mm,
				     unsigned long address);
extern void __mmu_notifier_change_pte(struct mm_struct *mm,
				      unsigned long address,
				      pte_t pte,
				      enum mmu_event event);
extern void __mmu_notifier_invalidate_page(struct vm_area_struct *vma,
					   unsigned long address,
					   enum mmu_event event);
extern void __mmu_notifier_invalidate_range_start(struct vm_area_struct *vma,
						  unsigned long start,
						  unsigned long end,
						  enum mmu_event event);
extern void __mmu_notifier_invalidate_range_free_pages(struct vm_area_struct *vma,
						       unsigned long start,
						       unsigned long end);
extern void __mmu_notifier_invalidate_range_end(struct vm_area_struct *vma,
						unsigned long start,
						unsigned long end,
						enum mmu_event event);
extern void __mmu_notifier_invalidate_range(struct mm_struct *mm,
				  unsigned long start, unsigned long end);

static inline void mmu_notifier_release(struct mm_struct *mm)
{
	if (mm_has_notifiers(mm))
		__mmu_notifier_release(mm);
}

static inline int mmu_notifier_clear_flush_young(struct mm_struct *mm,
					  unsigned long address)
{
	if (mm_has_notifiers(mm))
		return __mmu_notifier_clear_flush_young(mm, address);
	return 0;
}

static inline int mmu_notifier_test_young(struct mm_struct *mm,
					  unsigned long address)
{
	if (mm_has_notifiers(mm))
		return __mmu_notifier_test_young(mm, address);
	return 0;
}

static inline void mmu_notifier_change_pte(struct mm_struct *mm,
					   unsigned long address,
					   pte_t pte,
					   enum mmu_event event)
{
	if (mm_has_notifiers(mm))
		__mmu_notifier_change_pte(mm, address, pte, event);
}

static inline void mmu_notifier_invalidate_page(struct vm_area_struct *vma,
						unsigned long address,
						enum mmu_event event)
{
	if (mm_has_notifiers(vma->vm_mm))
		__mmu_notifier_invalidate_page(vma, address, event);
}

static inline void mmu_notifier_invalidate_range_start(struct vm_area_struct *vma,
						       unsigned long start,
						       unsigned long end,
						       enum mmu_event event)
{
	if (mm_has_notifiers(vma->vm_mm))
		__mmu_notifier_invalidate_range_start(vma, start,
						      end, event);
}

static inline void mmu_notifier_invalidate_range_free_pages(struct vm_area_struct *vma,
							    unsigned long start,
							    unsigned long end)
{
	if (mm_has_notifiers(vma->vm_mm))
		__mmu_notifier_invalidate_range_free_pages(vma, start, end);
}

static inline void mmu_notifier_invalidate_range_end(struct vm_area_struct *vma,
						     unsigned long start,
						     unsigned long end,
						     enum mmu_event event)
{
	if (mm_has_notifiers(vma->vm_mm))
		__mmu_notifier_invalidate_range_end(vma, start, end, event);
}

static inline void mmu_notifier_invalidate_range(struct mm_struct *mm,
				  unsigned long start, unsigned long end)
{
	if (mm_has_notifiers(mm))
		__mmu_notifier_invalidate_range(mm, start, end);
}

static inline void mmu_notifier_mm_init(struct mm_struct *mm)
{
	mm->mmu_notifier_mm = NULL;
}

static inline void mmu_notifier_mm_destroy(struct mm_struct *mm)
{
	if (mm_has_notifiers(mm))
		__mmu_notifier_mm_destroy(mm);
}

#define ptep_clear_flush_young_notify(__vma, __address, __ptep)		\
({									\
	int __young;							\
	struct vm_area_struct *___vma = __vma;				\
	unsigned long ___address = __address;				\
	__young = ptep_clear_flush_young(___vma, ___address, __ptep);	\
	__young |= mmu_notifier_clear_flush_young(___vma->vm_mm,	\
						  ___address);		\
	__young;							\
})

#define pmdp_clear_flush_young_notify(__vma, __address, __pmdp)		\
({									\
	int __young;							\
	struct vm_area_struct *___vma = __vma;				\
	unsigned long ___address = __address;				\
	__young = pmdp_clear_flush_young(___vma, ___address, __pmdp);	\
	__young |= mmu_notifier_clear_flush_young(___vma->vm_mm,	\
						  ___address);		\
	__young;							\
})

#define	ptep_clear_flush_notify(__vma, __address, __ptep)		\
({									\
	unsigned long ___addr = __address & PAGE_MASK;			\
	struct mm_struct *___mm = (__vma)->vm_mm;			\
	pte_t ___pte;							\
									\
	___pte = ptep_clear_flush(__vma, __address, __ptep);		\
	mmu_notifier_invalidate_range(___mm, ___addr,			\
					___addr + PAGE_SIZE);		\
									\
	___pte;								\
})

#define pmdp_clear_flush_notify(__vma, __haddr, __pmd)			\
({									\
	unsigned long ___haddr = __haddr & HPAGE_PMD_MASK;		\
	struct mm_struct *___mm = (__vma)->vm_mm;			\
	pmd_t ___pmd;							\
									\
	___pmd = pmdp_clear_flush(__vma, __haddr, __pmd);		\
	mmu_notifier_invalidate_range(___mm, ___haddr,			\
				      ___haddr + HPAGE_PMD_SIZE);	\
									\
	___pmd;								\
})

#define pmdp_get_and_clear_notify(__mm, __haddr, __pmd)			\
({									\
	unsigned long ___haddr = __haddr & HPAGE_PMD_MASK;		\
	pmd_t ___pmd;							\
									\
	___pmd = pmdp_get_and_clear(__mm, __haddr, __pmd);		\
	mmu_notifier_invalidate_range(__mm, ___haddr,			\
				      ___haddr + HPAGE_PMD_SIZE);	\
									\
	___pmd;								\
})

/*
 * set_pte_at_notify() sets the pte _after_ running the notifier.
 * This is safe to start by updating the secondary MMUs, because the primary MMU
 * pte invalidate must have already happened with a ptep_clear_flush() before
 * set_pte_at_notify() has been invoked.  Updating the secondary MMUs first is
 * required when we change both the protection of the mapping from read-only to
 * read-write and the pfn (like during copy on write page faults). Otherwise the
 * old page would remain mapped readonly in the secondary MMUs after the new
 * page is already writable by some CPU through the primary MMU.
 */
#define set_pte_at_notify(__mm, __address, __ptep, __pte, __event)	\
({									\
	struct mm_struct *___mm = __mm;					\
	unsigned long ___address = __address;				\
	pte_t ___pte = __pte;						\
									\
	mmu_notifier_change_pte(___mm, ___address, ___pte, __event);	\
	set_pte_at(___mm, ___address, __ptep, ___pte);			\
})

extern void mmu_notifier_call_srcu(struct rcu_head *rcu,
				   void (*func)(struct rcu_head *rcu));

#else /* CONFIG_MMU_NOTIFIER */

static inline void mmu_notifier_release(struct mm_struct *mm)
{
}

static inline int mmu_notifier_clear_flush_young(struct mm_struct *mm,
					  unsigned long address)
{
	return 0;
}

static inline int mmu_notifier_test_young(struct mm_struct *mm,
					  unsigned long address)
{
	return 0;
}

static inline void mmu_notifier_change_pte(struct mm_struct *mm,
					   unsigned long address,
					   pte_t pte,
					   enum mmu_event event)
{
}

static inline void mmu_notifier_invalidate_page(struct vm_area_struct *vma,
						unsigned long address,
						enum mmu_event event)
{
}

static inline void mmu_notifier_invalidate_range_start(struct vm_area_struct *vma,
						       unsigned long start,
						       unsigned long end,
						       enum mmu_event event)
{
}

static inline void mmu_notifier_invalidate_range_free_pages(struct vm_area_struct *vma,
							    unsigned long start,
							    unsigned long end)
{
}

static inline void mmu_notifier_invalidate_range_end(struct vm_area_struct *vma,
						     unsigned long start,
						     unsigned long end,
						     enum mmu_event event)
{
}

static inline void mmu_notifier_mm_init(struct mm_struct *mm)
{
}

static inline void mmu_notifier_mm_destroy(struct mm_struct *mm)
{
}

#define ptep_clear_flush_young_notify ptep_clear_flush_young
#define pmdp_clear_flush_young_notify pmdp_clear_flush_young
#define	ptep_clear_flush_notify ptep_clear_flush
#define pmdp_clear_flush_notify pmdp_clear_flush
#define pmdp_get_and_clear_notify pmdp_get_and_clear
#define set_pte_at_notify(__mm, __address, __ptep, __pte, __event)	\
({									\
	struct mm_struct *___mm = __mm;					\
	unsigned long ___address = __address;				\
	pte_t ___pte = __pte;						\
									\
	set_pte_at(___mm, ___address, __ptep, ___pte);			\
})

#endif /* CONFIG_MMU_NOTIFIER */

#endif /* _LINUX_MMU_NOTIFIER_H */
