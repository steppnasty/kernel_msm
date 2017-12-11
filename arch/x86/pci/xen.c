/*
 * Xen PCI Frontend Stub - puts some "dummy" functions in to the Linux
 *			   x86 PCI core to support the Xen PCI Frontend
 *
 *   Author: Ryan Wilson <hap9@epoch.ncsc.mil>
 */
#include <linux/module.h>
#include <linux/init.h>
#include <linux/pci.h>
#include <linux/acpi.h>

#include <linux/io.h>
#include <asm/pci_x86.h>

#include <asm/xen/hypervisor.h>

#include <xen/features.h>
#include <xen/events.h>
#include <asm/xen/pci.h>

#ifdef CONFIG_ACPI
static int xen_hvm_register_pirq(u32 gsi, int triggering)
{
	int rc, irq;
	struct physdev_map_pirq map_irq;
	int shareable = 0;
	char *name;

	if (!xen_hvm_domain())
		return -1;

	map_irq.domid = DOMID_SELF;
	map_irq.type = MAP_PIRQ_TYPE_GSI;
	map_irq.index = gsi;
	map_irq.pirq = -1;

	rc = HYPERVISOR_physdev_op(PHYSDEVOP_map_pirq, &map_irq);
	if (rc) {
		printk(KERN_WARNING "xen map irq failed %d\n", rc);
		return -1;
	}

	if (triggering == ACPI_EDGE_SENSITIVE) {
		shareable = 0;
		name = "ioapic-edge";
	} else {
		shareable = 1;
		name = "ioapic-level";
	}

	irq = xen_map_pirq_gsi(map_irq.pirq, gsi, shareable, name);

	printk(KERN_DEBUG "xen: --> irq=%d, pirq=%d\n", irq, map_irq.pirq);

	return irq;
}

static int acpi_register_gsi_xen_hvm(struct device *dev, u32 gsi,
				 int trigger, int polarity)
{
	return xen_hvm_register_pirq(gsi, trigger);
}
#endif

#if defined(CONFIG_PCI_MSI)
#include <linux/msi.h>

struct xen_pci_frontend_ops *xen_pci_frontend;
EXPORT_SYMBOL_GPL(xen_pci_frontend);

/*
 * For MSI interrupts we have to use drivers/xen/event.s functions to
 * allocate an irq_desc and setup the right */


static int xen_setup_msi_irqs(struct pci_dev *dev, int nvec, int type)
{
	int irq, ret, i;
	struct msi_desc *msidesc;
	int *v;

	v = kzalloc(sizeof(int) * max(1, nvec), GFP_KERNEL);
	if (!v)
		return -ENOMEM;

	if (!xen_initial_domain()) {
		if (type == PCI_CAP_ID_MSIX)
			ret = xen_pci_frontend_enable_msix(dev, &v, nvec);
		else
			ret = xen_pci_frontend_enable_msi(dev, &v);
		if (ret)
			goto error;
	}
	i = 0;
	list_for_each_entry(msidesc, &dev->msi_list, list) {
		irq = xen_allocate_pirq(v[i], 0, /* not sharable */
			(type == PCI_CAP_ID_MSIX) ?
			"pcifront-msi-x" : "pcifront-msi");
		if (irq < 0)
			return -1;

		ret = set_irq_msi(irq, msidesc);
		if (ret)
			goto error_while;
		i++;
	}
	kfree(v);
	return 0;

error_while:
	unbind_from_irqhandler(irq, NULL);
error:
	if (ret == -ENODEV)
		dev_err(&dev->dev, "Xen PCI frontend has not registered" \
			" MSI/MSI-X support!\n");

	kfree(v);
	return ret;
}

static void xen_teardown_msi_irqs(struct pci_dev *dev)
{
	/* Only do this when were are in non-privileged mode.*/
	if (!xen_initial_domain()) {
		struct msi_desc *msidesc;

		msidesc = list_entry(dev->msi_list.next, struct msi_desc, list);
		if (msidesc->msi_attrib.is_msix)
			xen_pci_frontend_disable_msix(dev);
		else
			xen_pci_frontend_disable_msi(dev);
	}

}

static void xen_teardown_msi_irq(unsigned int irq)
{
	xen_destroy_irq(irq);
}
#endif

static int xen_pcifront_enable_irq(struct pci_dev *dev)
{
	int rc;
	int share = 1;

	dev_info(&dev->dev, "Xen PCI enabling IRQ: %d\n", dev->irq);

	if (dev->irq < 0)
		return -EINVAL;

	if (dev->irq < NR_IRQS_LEGACY)
		share = 0;

	rc = xen_allocate_pirq(dev->irq, share, "pcifront");
	if (rc < 0) {
		dev_warn(&dev->dev, "Xen PCI IRQ: %d, failed to register:%d\n",
			 dev->irq, rc);
		return rc;
	}
	return 0;
}

int __init pci_xen_init(void)
{
	if (!xen_pv_domain() || xen_initial_domain())
		return -ENODEV;

	printk(KERN_INFO "PCI: setting up Xen PCI frontend stub\n");

	pcibios_set_cache_line_size();

	pcibios_enable_irq = xen_pcifront_enable_irq;
	pcibios_disable_irq = NULL;

#ifdef CONFIG_ACPI
	/* Keep ACPI out of the picture */
	acpi_noirq = 1;
#endif

#ifdef CONFIG_PCI_MSI
	x86_msi.setup_msi_irqs = xen_setup_msi_irqs;
	x86_msi.teardown_msi_irq = xen_teardown_msi_irq;
	x86_msi.teardown_msi_irqs = xen_teardown_msi_irqs;
#endif
	return 0;
}

int __init pci_xen_hvm_init(void)
{
	if (!xen_feature(XENFEAT_hvm_pirqs))
		return 0;

#ifdef CONFIG_ACPI
	/*
	 * We don't want to change the actual ACPI delivery model,
	 * just how GSIs get registered.
	 */
	__acpi_register_gsi = acpi_register_gsi_xen_hvm;
#endif
	return 0;
}
