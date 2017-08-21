/* Copyright (c) 2011-2012, Code Aurora Forum. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */
#include <linux/err.h>
#include <linux/msm_ion.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/memory_alloc.h>
#include <linux/fmem.h>
#include <linux/uaccess.h>
#include <linux/dma-mapping.h>
#include <mach/ion.h>
#include <mach/msm_memtypes.h>
#include <asm/cacheflush.h>
#include "../ion_priv.h"
#include "ion_cp_common.h"

static struct ion_device *idev;
static int num_heaps;
static struct ion_heap **heaps;

struct ion_client *msm_ion_client_create(unsigned int heap_mask,
					const char *name)
{
	return ion_client_create(idev, name);
}
EXPORT_SYMBOL(msm_ion_client_create);

int msm_ion_secure_heap(int heap_id)
{
	return ion_secure_heap(idev, heap_id, ION_CP_V1, NULL);
}
EXPORT_SYMBOL(msm_ion_secure_heap);

int msm_ion_unsecure_heap(int heap_id)
{
	return ion_unsecure_heap(idev, heap_id, ION_CP_V1, NULL);
}
EXPORT_SYMBOL(msm_ion_unsecure_heap);

int msm_ion_do_cache_op(struct ion_client *client, struct ion_handle *handle,
			void *vaddr, unsigned long len, unsigned int cmd)
{
	return ion_do_cache_op(client, handle, vaddr, 0, len, cmd);
}
EXPORT_SYMBOL(msm_ion_do_cache_op);

static int ion_no_pages_cache_ops(struct ion_client *client,
			struct ion_handle *handle,
			void *vaddr,
			unsigned int offset, unsigned int length,
			unsigned int cmd)
{
	void (*outer_cache_op)(unsigned long, unsigned long) = NULL;
	unsigned int size_to_vmap, total_size;
	int i, j, ret;
	void *ptr = NULL;
	ion_phys_addr_t buff_phys = 0;
	ion_phys_addr_t buff_phys_start = 0;
	size_t buf_length = 0;

	ret = ion_phys(client, handle, &buff_phys_start, &buf_length);
	if (ret)
		return -EINVAL;

	buff_phys = buff_phys_start;

	if (!vaddr) {
		/*
		 * Split the vmalloc space into smaller regions in
		 * order to clean and/or invalidate the cache.
		 */
		size_to_vmap = ((VMALLOC_END - VMALLOC_START)/8);
		total_size = buf_length;

		for (i = 0; i < total_size; i += size_to_vmap) {
			size_to_vmap = min(size_to_vmap, total_size - i);
			for (j = 0; j < 10 && size_to_vmap; ++j) {
				ptr = ioremap(buff_phys, size_to_vmap);
				if (ptr) {
					switch (cmd) {
					case ION_IOC_CLEAN_CACHES:
						dmac_clean_range(ptr,
							ptr + size_to_vmap);
						outer_cache_op =
							outer_clean_range;
						break;
					case ION_IOC_INV_CACHES:
						dmac_inv_range(ptr,
							ptr + size_to_vmap);
						outer_cache_op =
							outer_inv_range;
						break;
					case ION_IOC_CLEAN_INV_CACHES:
						dmac_flush_range(ptr,
							ptr + size_to_vmap);
						outer_cache_op =
							outer_flush_range;
						break;
					default:
						return -EINVAL;
					}
					buff_phys += size_to_vmap;
					break;
				} else {
					size_to_vmap >>= 1;
				}
			}
			if (!ptr) {
				pr_err("Couldn't io-remap the memory\n");
				return -EINVAL;
			}
			iounmap(ptr);
		}
	} else {
		switch (cmd) {
		case ION_IOC_CLEAN_CACHES:
			dmac_clean_range(vaddr, vaddr + length);
			outer_cache_op = outer_clean_range;
			break;
		case ION_IOC_INV_CACHES:
			dmac_inv_range(vaddr, vaddr + length);
			outer_cache_op = outer_inv_range;
			break;
		case ION_IOC_CLEAN_INV_CACHES:
			dmac_flush_range(vaddr, vaddr + length);
			outer_cache_op = outer_flush_range;
			break;
		default:
			return -EINVAL;
		}
	}

	outer_cache_op(buff_phys_start + offset,
		       buff_phys_start + offset + length);

	return 0;
}

#ifdef CONFIG_OUTER_CACHE
static void ion_pages_outer_cache_op(void (*op)(unsigned long, unsigned long),
				struct sg_table *table)
{
	unsigned long pstart;
	struct scatterlist *sg;
	int i;
	for_each_sg(table->sgl, sg, table->nents, i) {
		struct page *page = sg_page(sg);
		pstart = page_to_phys(page);
		/*
		 * If page -> phys is returning NULL, something
		 * has really gone wrong...
		 */
		if (!pstart) {
			WARN(1, "Could not translate virtual address to physical address\n");
			return;
		}
		op(pstart, pstart + PAGE_SIZE);
	}
}
#else
static void ion_pages_outer_cache_op(void (*op)(unsigned long, unsigned long),
					struct sg_table *table)
{

}
#endif

static int ion_pages_cache_ops(struct ion_client *client,
			struct ion_handle *handle,
			void *vaddr, unsigned int offset, unsigned int length,
			unsigned int cmd)
{
	void (*outer_cache_op)(unsigned long, unsigned long);
	struct sg_table *table = NULL;

	table = ion_sg_table(client, handle);
	if (IS_ERR_OR_NULL(table))
		return PTR_ERR(table);

	switch (cmd) {
	case ION_IOC_CLEAN_CACHES:
		if (!vaddr)
			dma_sync_sg_for_device(NULL, table->sgl,
			table->nents, DMA_TO_DEVICE);
		else
			dmac_clean_range(vaddr, vaddr + length);
			outer_cache_op = outer_clean_range;
		break;
	case ION_IOC_INV_CACHES:
		if (!vaddr)
			dma_sync_sg_for_cpu(NULL, table->sgl,
			table->nents, DMA_FROM_DEVICE);
		else
			dmac_inv_range(vaddr, vaddr + length);
			outer_cache_op = outer_inv_range;
		break;
	case ION_IOC_CLEAN_INV_CACHES:
		if (!vaddr) {
			dma_sync_sg_for_device(NULL, table->sgl,
				table->nents, DMA_TO_DEVICE);
			dma_sync_sg_for_cpu(NULL, table->sgl,
				table->nents, DMA_FROM_DEVICE);
		} else {
			dmac_flush_range(vaddr, vaddr + length);
		}
		outer_cache_op = outer_flush_range;
		break;
	default:
		return -EINVAL;
	}

	ion_pages_outer_cache_op(outer_cache_op, table);

	return 0;
}

int ion_do_cache_op(struct ion_client *client, struct ion_handle *handle,
			void *uaddr, unsigned long offset, unsigned long len,
			unsigned int cmd)
{
	int ret = -EINVAL;
	unsigned long flags;
	struct sg_table *table;
	struct page *page;

	ret = ion_handle_get_flags(client, handle, &flags);
	if (ret)
		return -EINVAL;

	if (!ION_IS_CACHED(flags))
		return 0;

	table = ion_sg_table(client, handle);

	if (IS_ERR_OR_NULL(table))
		return PTR_ERR(table);

	page = sg_page(table->sgl);

	if (page)
		ret = ion_pages_cache_ops(client, handle, uaddr,
					offset, len, cmd);
	else
		ret = ion_no_pages_cache_ops(client, handle, uaddr,
					offset, len, cmd);

	return ret;

}

static unsigned long msm_ion_get_base(unsigned long size, int memory_type,
				    unsigned int align)
{
	switch (memory_type) {
	case ION_EBI_TYPE:
		return allocate_contiguous_ebi_nomap(size, align);
		break;
	case ION_SMI_TYPE:
		return allocate_contiguous_memory_nomap(size, MEMTYPE_SMI,
							align);
		break;
	default:
		pr_err("%s: Unknown memory type %d\n", __func__, memory_type);
		return 0;
	}
}

static struct ion_platform_heap *find_heap(const struct ion_platform_heap
					   heap_data[],
					   unsigned int nr_heaps,
					   int heap_id)
{
	unsigned int i;
	for (i = 0; i < nr_heaps; ++i) {
		const struct ion_platform_heap *heap = &heap_data[i];
		if (heap->id == heap_id)
			return (struct ion_platform_heap *) heap;
	}
	return 0;
}

static void ion_set_base_address(struct ion_platform_heap *heap,
			    struct ion_platform_heap *shared_heap,
			    struct ion_co_heap_pdata *co_heap_data,
			    struct ion_cp_heap_pdata *cp_data)
{
	if (cp_data->reusable) {
		const struct fmem_data *fmem_info = fmem_get_info();

		if (!fmem_info) {
			pr_err("fmem info pointer NULL!\n");
			BUG();
		}

		heap->base = fmem_info->phys - fmem_info->reserved_size_low;
		cp_data->virt_addr = fmem_info->virt;
		pr_info("ION heap %s using FMEM\n", shared_heap->name);
	} else {
		heap->base = msm_ion_get_base(heap->size + shared_heap->size,
						shared_heap->memory_type,
						co_heap_data->align);
	}
	if (heap->base) {
		shared_heap->base = heap->base + heap->size;
		cp_data->secure_base = heap->base;
		cp_data->secure_size = heap->size + shared_heap->size;
	} else {
		pr_err("%s: could not get memory for heap %s (id %x)\n",
			__func__, heap->name, heap->id);
	}
}

static void allocate_co_memory(struct ion_platform_heap *heap,
			       struct ion_platform_heap heap_data[],
			       unsigned int nr_heaps)
{
	struct ion_co_heap_pdata *co_heap_data =
		(struct ion_co_heap_pdata *) heap->extra_data;

	if (co_heap_data->adjacent_mem_id != INVALID_HEAP_ID) {
		struct ion_platform_heap *shared_heap =
			find_heap(heap_data, nr_heaps,
				  co_heap_data->adjacent_mem_id);
		if (shared_heap) {
			struct ion_cp_heap_pdata *cp_data =
			   (struct ion_cp_heap_pdata *) shared_heap->extra_data;
			if (cp_data->fixed_position == FIXED_MIDDLE) {
				const struct fmem_data *fmem_info =
					fmem_get_info();

				if (!fmem_info) {
					pr_err("fmem info pointer NULL!\n");
					BUG();
				}

				cp_data->virt_addr = fmem_info->virt;
				cp_data->secure_base = heap->base;
				cp_data->secure_size =
						heap->size + shared_heap->size;
			} else if (!heap->base) {
				ion_set_base_address(heap, shared_heap,
					co_heap_data, cp_data);
			}
		}
	}
}

/* Fixup heaps in board file to support two heaps being adjacent to each other.
 * A flag (adjacent_mem_id) in the platform data tells us that the heap phy
 * memory location must be adjacent to the specified heap. We do this by
 * carving out memory for both heaps and then splitting up the memory to the
 * two heaps. The heap specifying the "adjacent_mem_id" get the base of the
 * memory while heap specified in "adjacent_mem_id" get base+size as its
 * base address.
 * Note: Modifies platform data and allocates memory.
 */
static void msm_ion_heap_fixup(struct ion_platform_heap heap_data[],
			       unsigned int nr_heaps)
{
	unsigned int i;

	for (i = 0; i < nr_heaps; i++) {
		struct ion_platform_heap *heap = &heap_data[i];
		if (heap->type == ION_HEAP_TYPE_CARVEOUT) {
			if (heap->extra_data)
				allocate_co_memory(heap, heap_data, nr_heaps);
		}
	}
}

static void msm_ion_allocate(struct ion_platform_heap *heap)
{

	if (!heap->base && heap->extra_data) {
		unsigned int align = 0;
		switch ((int) heap->type) {
		case ION_HEAP_TYPE_CARVEOUT:
			align =
			((struct ion_co_heap_pdata *) heap->extra_data)->align;
			break;
		case ION_HEAP_TYPE_CP:
		{
			struct ion_cp_heap_pdata *data =
				(struct ion_cp_heap_pdata *)
				heap->extra_data;
			if (data->reusable) {
				const struct fmem_data *fmem_info =
					fmem_get_info();
				heap->base = fmem_info->phys;
				data->virt_addr = fmem_info->virt;
				pr_info("ION heap %s using FMEM\n", heap->name);
			} else if (data->mem_is_fmem) {
				const struct fmem_data *fmem_info =
					fmem_get_info();
				heap->base = fmem_info->phys + fmem_info->size;
			}
			align = data->align;
			break;
		}
		default:
			break;
		}
		if (align && !heap->base) {
			heap->base = msm_ion_get_base(heap->size,
						      heap->memory_type,
						      align);
			if (!heap->base)
				pr_err("%s: could not get memory for heap %s "
				   "(id %x)\n", __func__, heap->name, heap->id);
		}
	}
}

static int check_vaddr_bounds(unsigned long start, unsigned long end)
{
	struct mm_struct *mm = current->active_mm;
	struct vm_area_struct *vma;
	int ret = 1;

	if (end < start)
		goto out;

	vma = find_vma(mm, start);
	if (vma && vma->vm_start < end) {
		if (start < vma->vm_start)
			goto out;
		if (end > vma->vm_end)
			goto out;
		ret = 0;
	}

out:
	return ret;
}

int ion_heap_allow_secure_allocation(enum ion_heap_type type)
{
	return type == ((enum ion_heap_type) ION_HEAP_TYPE_CP) ||
		type == ((enum ion_heap_type) ION_HEAP_TYPE_SECURE_DMA);
}

int ion_heap_allow_handle_secure(enum ion_heap_type type)
{
	return type == ((enum ion_heap_type) ION_HEAP_TYPE_CP) ||
		type == ((enum ion_heap_type) ION_HEAP_TYPE_SECURE_DMA);
}

int ion_heap_allow_heap_secure(enum ion_heap_type type)
{
	return type == ((enum ion_heap_type) ION_HEAP_TYPE_CP);
}

static long msm_ion_custom_ioctl(struct ion_client *client,
				unsigned int cmd,
				unsigned long arg)
{
	switch (cmd) {
	case ION_IOC_CLEAN_CACHES:
	case ION_IOC_INV_CACHES:
	case ION_IOC_CLEAN_INV_CACHES:
	{
		struct ion_flush_data data;
		unsigned long start, end;
		struct ion_handle *handle = NULL;
		int ret;
		struct mm_struct *mm = current->active_mm;

		if (copy_from_user(&data, (void __user *)arg,
				sizeof(struct ion_flush_data)))
			return -EFAULT;

		if (!data.handle) {
			handle = ion_import_dma_buf(client, data.fd);
			if (IS_ERR(handle)) {
				pr_info("%s: Could not import handle: %d\n",
					__func__, (int)handle);
				return -EINVAL;
			}
		}

		down_read(&mm->mmap_sem);

		start = (unsigned long) data.vaddr;
		end = (unsigned long) data.vaddr + data.length;

		if (start && check_vaddr_bounds(start, end)) {
			up_read(&mm->mmap_sem);
			pr_err("%s: virtual address %p is out of bounds\n",
				__func__, data.vaddr);
			if (!data.handle)
				ion_free(client, handle);
			return -EINVAL;
		}

		ret = ion_do_cache_op(client,
				data.handle ? data.handle : handle,
				data.vaddr, data.offset, data.length,
				cmd);

		up_read(&mm->mmap_sem);

		if (!data.handle)
			ion_free(client, handle);

		if (ret < 0)
			return ret;
		break;

	}
	default:
		return -ENOTTY;
	}
	return 0;
}

static int msm_ion_probe(struct platform_device *pdev)
{
	struct ion_platform_data *pdata = pdev->dev.platform_data;
	int err;
	int i;

	num_heaps = pdata->nr;

	heaps = kcalloc(pdata->nr, sizeof(struct ion_heap *), GFP_KERNEL);

	if (!heaps) {
		err = -ENOMEM;
		goto out;
	}

	idev = ion_device_create(msm_ion_custom_ioctl);
	if (IS_ERR_OR_NULL(idev)) {
		err = PTR_ERR(idev);
		goto freeheaps;
	}

	msm_ion_heap_fixup(pdata->heaps, num_heaps);

	/* create the heaps as specified in the board file */
	for (i = 0; i < num_heaps; i++) {
		struct ion_platform_heap *heap_data = &pdata->heaps[i];
		msm_ion_allocate(heap_data);

		heaps[i] = ion_heap_create(heap_data);
		if (IS_ERR_OR_NULL(heaps[i])) {
			heaps[i] = 0;
			continue;
		} else {
			if (heap_data->size)
				pr_info("ION heap %s created at %lx "
					"with size %x\n", heap_data->name,
							  heap_data->base,
							  heap_data->size);
			else
				pr_info("ION heap %s created\n",
							  heap_data->name);
		}

		ion_device_add_heap(idev, heaps[i]);
	}
	platform_set_drvdata(pdev, idev);
	return 0;

freeheaps:
	kfree(heaps);
out:
	return err;
}

static int msm_ion_remove(struct platform_device *pdev)
{
	struct ion_device *idev = platform_get_drvdata(pdev);
	int i;

	for (i = 0; i < num_heaps; i++)
		ion_heap_destroy(heaps[i]);

	ion_device_destroy(idev);
	kfree(heaps);
	return 0;
}

static struct platform_driver msm_ion_driver = {
	.probe = msm_ion_probe,
	.remove = msm_ion_remove,
	.driver = { .name = "ion-msm" }
};

static int __init msm_ion_init(void)
{
	return platform_driver_register(&msm_ion_driver);
}

static void __exit msm_ion_exit(void)
{
	platform_driver_unregister(&msm_ion_driver);
}

subsys_initcall(msm_ion_init);
module_exit(msm_ion_exit);

