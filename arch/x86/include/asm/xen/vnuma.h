#ifndef _ASM_X86_VNUMA_H
#define _ASM_X86_VNUMA_H

#define XEN_NUMNODES	16

#ifdef CONFIG_XEN
int xen_numa_init(void);
#else
static inline int xen_numa_init(void) { return -1; };
#endif

/* Physical/static numa information */
extern int *xen_numa_distance;
extern unsigned long *xen_numa_memranges;
extern int xen_numa_num_nodes;
extern int nodelists[][XEN_NUMNODES];

/* Virtual/dynamic numa information */
extern int xen_vnuma_num_nodes;
extern int *xen_memnode_map;
extern int *xen_vnode_to_pnode;
/* extern int *xen_vcpu_to_pnode; */
extern atomic64_t topology_version;

void xen_update_vcpu_to_pnode(void);

static inline int xen_pnode_to_vnode(int pnode)
{
	int vnode;
	for (vnode = 0; vnode < xen_vnuma_num_nodes; vnode++)
		if (xen_vnode_to_pnode[vnode] == pnode)
			return vnode;
	return 0;
}

#endif /* _ASM_X86_VNUMA_H */
