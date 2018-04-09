#include <linux/err.h>
#include <linux/memblock.h>
#include <xen/interface/xen.h>
#include <xen/interface/memory.h>
#include <asm/xen/interface.h>
#include <asm/xen/hypercall.h>
#include <asm/xen/vnuma.h>

int *xen_numa_distance;
unsigned long *xen_numa_memranges;
int xen_numa_num_nodes;
int nodelists[XEN_NUMNODES][XEN_NUMNODES];

int xen_vnuma_num_nodes;
int *xen_memnode_map;
int *xen_vnode_to_pnode;
atomic64_t topology_version;

void xen_update_vcpu_to_pnode(void)
{
	int i, cpu;
	int pnode, found_pnode;
	int dist, min_dist;
	struct shared_info *sh;

	sh = HYPERVISOR_shared_info;

	cpu = smp_processor_id();
	pnode = sh->vcpu_to_pnode[cpu];

	min_dist = INT_MAX;
	found_pnode = pnode;
	if (pnode < xen_numa_num_nodes && !xen_memnode_map[pnode]) {
		/* Find the closest mem node */
		for (i = 0; i < xen_numa_num_nodes; i++) {
			if (xen_memnode_map[i]) {
				dist = xen_numa_distance[pnode*XEN_NUMNODES+i];
				if (min_dist > dist) {
					min_dist = dist;
					found_pnode = i;
				}
			}
		}
		sh->vcpu_to_pnode[cpu] = found_pnode;
	}
}

/*
 * Called from numa_init if numa_off = 0.
 */
int __init xen_numa_init(void)
{
	int i, j, vnode;
	unsigned long mem_size, dist_size,
		map_size, vnode_to_pnode_size;
	u64 physm, physd, physp, physv;

	struct xen_numa_topology_info numa_topo;
	physm = physd = physp = physv = 0;

	mem_size = 2 * XEN_NUMNODES * sizeof(*numa_topo.memranges);
	dist_size = XEN_NUMNODES * XEN_NUMNODES * sizeof(*numa_topo.distance);
	map_size = XEN_NUMNODES * sizeof(*numa_topo.memnode_map);
	vnode_to_pnode_size = XEN_NUMNODES * sizeof(int);

	physm = memblock_alloc(mem_size, PAGE_SIZE);
	physd = memblock_alloc(dist_size, PAGE_SIZE);
	physp = memblock_alloc(map_size, PAGE_SIZE);
	physv = memblock_alloc(vnode_to_pnode_size, PAGE_SIZE);

	if (!physm || !physd || !physp || !physv)
		goto out;

	xen_numa_memranges = __va(physm);
	xen_numa_distance = __va(physd);
	xen_memnode_map = __va(physp);
	xen_vnode_to_pnode = __va(physv);

	set_xen_guest_handle(numa_topo.memranges, xen_numa_memranges);
	set_xen_guest_handle(numa_topo.distance, xen_numa_distance);
	set_xen_guest_handle(numa_topo.memnode_map, xen_memnode_map);

	if (HYPERVISOR_memory_op(XENMEM_get_numainfo, &numa_topo) < 0)
		goto out;

	xen_numa_num_nodes = numa_topo.nr_nodes;

	for (i = 0; i < xen_numa_num_nodes; i++) {
		printk(KERN_INFO "Node %d's memory range: start %lu num %lu\n",
			i, xen_numa_memranges[2*i], xen_numa_memranges[2*i+1]);

		for (j = 0; j < xen_numa_num_nodes; j++)
			printk(KERN_INFO "Distance(%d, %d) = %d\n",
				i, j, xen_numa_distance[i*XEN_NUMNODES+j]);
	}

	for (i = vnode = 0; i < xen_numa_num_nodes; i++) {
		if (xen_memnode_map[i]) {
			printk(KERN_INFO "vNUMA: vnode %d -> pnode %d\n", vnode, i);
			xen_vnode_to_pnode[vnode++] = i;
		}
	}

	xen_vnuma_num_nodes = vnode;
	atomic64_set(&topology_version, 0);

	/*
	 * We have enough information to properly build the nodelists
	 * but I'm just too lazy at this point. So they are hardcoded.
	 * It's not very important anyways.
	 */
	nodelists[0][0] = 1;
	nodelists[0][1] = 0;
	nodelists[1][0] = 0;
	nodelists[1][1] = 1;
out:
	node_set(0, numa_nodes_parsed);
	numa_add_memblk(0, 0, PFN_PHYS(max_pfn));

	return 0;
}
