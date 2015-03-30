/*
 * include/linux/ion.h
 *
 * Copyright (c) 2012, Code Aurora Forum. All rights reserved.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#ifndef _LINUX_MSM_ION_H
#define _LINUX_MSM_ION_H

#include <linux/ion.h>

enum msm_ion_heap_types {
	ION_HEAP_TYPE_MSM_START = ION_HEAP_TYPE_CUSTOM + 1,
	ION_HEAP_TYPE_IOMMU = ION_HEAP_TYPE_MSM_START,
	ION_HEAP_TYPE_DMA,
	ION_HEAP_TYPE_CP,
	ION_HEAP_TYPE_SECURE_DMA,
	ION_HEAP_TYPE_REMOVED,
};

enum cp_mem_usage {
	VIDEO_BITSTREAM = 0x1,
	VIDEO_PIXEL = 0x2,
	VIDEO_NONPIXEL = 0x3,
	MAX_USAGE = 0x4,
	UNKNOWN = 0x7FFFFFFF,
};

#define ION_SET_CACHED(__cache)		(__cache | ION_FLAG_CACHED)
#define ION_SET_UNCACHED(__cache)	(__cache & ~ION_FLAG_CACHED)

#define ION_IS_CACHED(__flags)	((__flags) & ION_FLAG_CACHED)

#ifdef __KERNEL__

/*
 * This flag allows clients when mapping into the IOMMU to specify to
 * defer un-mapping from the IOMMU until the buffer memory is freed.
 */
#define ION_IOMMU_UNMAP_DELAYED	1

/*
 * This flag allows clients to defer unsecuring a buffer until the buffer
 * is actually freed.
 */
#define ION_UNSECURE_DELAYED	1

#ifdef CONFIG_ION

/**
 * ion_unmap_iommu - unmap the handle from an iommu
 *
 * @client - client who allocated the handle
 * @handle - handle to unmap
 * @domain_num - domain to unmap from
 * @partition_num - partition to unmap from
 *
 * Decrement the reference count on the iommu mapping. If the count is
 * 0, the mapping will be removed from the iommu.
 */
void ion_unmap_iommu(struct ion_client *client, struct ion_handle *handle,
			int domain_num, int partition_num);


/**
 * ion_secure_heap - secure a heap
 *
 * @client - a client that has allocated from the heap heap_id
 * @heap_id - heap id to secure.
 * @version - version of content protection
 * @data - extra data needed for protection
 *
 * Secure a heap
 * Returns 0 on success
 */
int ion_secure_heap(struct ion_device *dev, int heap_id, int version,
			void *data);

/**
 * ion_unsecure_heap - un-secure a heap
 *
 * @client - a client that has allocated from the heap heap_id
 * @heap_id - heap id to un-secure.
 * @version - version of content protection
 * @data - extra data needed for protection
 *
 * Un-secure a heap
 * Returns 0 on success
 */
int ion_unsecure_heap(struct ion_device *dev, int heap_id, int version,
			void *data);

/**
 * msm_ion_do_cache_op - do cache operations.
 *
 * @client - pointer to ION client.
 * @handle - pointer to buffer handle.
 * @vaddr -  virtual address to operate on.
 * @len - Length of data to do cache operation on.
 * @cmd - Cache operation to perform:
 *		ION_IOC_CLEAN_CACHES
 *		ION_IOC_INV_CACHES
 *		ION_IOC_CLEAN_INV_CACHES
 *
 * Returns 0 on success
 */
int msm_ion_do_cache_op(struct ion_client *client, struct ion_handle *handle,
			void *vaddr, unsigned long len, unsigned int cmd);

#else

static inline void ion_unmap_iommu(struct ion_client *client,
			struct ion_handle *handle, int domain_num,
			int partition_num)
{
	return;
}

static inline int ion_secure_heap(struct ion_device *dev, int heap_id,
					int version, void *data)
{
	return -ENODEV;

}

static inline int ion_unsecure_heap(struct ion_device *dev, int heap_id,
					int version, void *data)
{
	return -ENODEV;
}

static inline void ion_mark_dangling_buffers_locked(struct ion_device *dev)
{
}

static inline int msm_ion_do_cache_op(struct ion_client *client,
			struct ion_handle *handle, void *vaddr,
			unsigned long len, unsigned int cmd)
{
	return -ENODEV;
}

#endif /* CONFIG_ION */

#endif /* __KERNEL */

#endif
