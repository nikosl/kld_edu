/*
 * Copyright 2024 Nikos Leivadaris <nikosleiv@gmail.com>.
 * SPDX-License-Identifier: MIT
 */

#include <sys/types.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/buf.h>
#include <sys/bus.h>
#include <sys/condvar.h>
#include <sys/conf.h> /* cdevsw struct */
#include <sys/errno.h>
#include <sys/kassert.h>
#include <sys/kernel.h> /* types used in module initialization */
#include <sys/libkern.h>
#include <sys/malloc.h>
#include <sys/module.h>
#include <sys/mutex.h>
#include <sys/rman.h>
#include <sys/sysctl.h>
#include <sys/uio.h>

#include <machine/bus.h>
#include <machine/endian.h>
#include <machine/resource.h>

#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>

#include "edu_ctrl.h"
#include "edu_ioctl.h"
#include "edu_priv.h"

#define EDU_DEV_NAME	    "edu"

#define EDU_PCI_VENDOR_ID   0x1234
#define EDU_PCI_DEVICE_ID   0x11e8
#define EDU_PCI_DEVICE_DESC "Edu device"

int edu_debug_lvl = 0;

static void
edu_intr_handler(void *ctrl)
{
	struct edu_softc *sc = ctrl;
	edu_printf(sc, "Intrx \n");

	uint32_t ir = edu_intr_status(sc);
	switch (ir) {
	case EDU_INTR_DMA:
		edu_debug("DMA Ready");

		break;
	case EDU_INTR_FACTORIAL:
		edu_debug("Factorial result");

		mtx_lock(&sc->fact_lock);
		cv_signal(&sc->fact_cv);
		mtx_unlock(&sc->fact_lock);
		break;
	}

	edu_intr_ack(sc, ir);
}

static d_open_t edu_open;
static d_close_t edu_close;
static d_read_t edu_read;
static d_write_t edu_write;
static d_ioctl_t edu_ioctl;

static struct cdevsw edu_cdevsw = {
	.d_version = D_VERSION,
	.d_open = edu_open,
	.d_close = edu_close,
	.d_read = edu_read,
	.d_write = edu_write,
	.d_name = EDU_DEV_NAME,
};

int
edu_open(struct cdev *dev, int oflags, int devtype, struct thread *td)
{
	struct edu_softc *sc;

	/* Look up our softc. */
	sc = dev->si_drv1;
	device_printf(sc->dev, "Opened successfully.\n");
	return (0);
}

int
edu_close(struct cdev *dev, int fflag, int devtype, struct thread *td)
{
	struct edu_softc *sc;

	/* Look up our softc. */
	sc = dev->si_drv1;
	device_printf(sc->dev, "Closed.\n");
	return (0);
}

int
edu_ioctl(struct cdev *dev, u_long cmd, caddr_t addr, int flags,
    struct thread *td)
{
	switch (cmd) {
	case EDU_IOCTL_HWVERSION:

		break;
	}

	return (EINVAL);
}

int
edu_read(struct cdev *dev, struct uio *uio, int ioflag)
{
	struct edu_softc *sc;
	char buf[8] = { 0 };
	int rv = 0;
	size_t amt = 0;

	/* Look up our softc. */
	sc = dev->si_drv1;
	device_printf(sc->dev, "Asked to read %zd bytes.\n", uio->uio_resid);
	int i = edu_factorial(sc, 5);
	// printf("res %d", i);
	size_t s = sizeof(buf);
	snprintf(buf, s, "%d\n", i);
	size_t l = strnlen(buf, s) + 1;
	amt = MIN(uio->uio_resid,
	    uio->uio_offset >= l ? 0 : l - uio->uio_offset);
	KASSERT(amnt == 3, " ==2");
	rv = uiomove(buf, amt, uio);
	if (rv)
		printf("uio failed");
	return (rv);
}

int
edu_write(struct cdev *dev, struct uio *uio, int ioflag)
{
	struct edu_softc *sc;

	sc = dev->si_drv1;
	device_printf(sc->dev, "Asked to write %zd bytes.\n", uio->uio_resid);

	size_t amt = 0;
	int rc = 0;

	if (uio->uio_offset != 0)
		return (EINVAL);

	char buf[3] = { 0 };
	size_t s = 3;

	amt = MIN(uio->uio_resid, s);
	return (rc);
}

/* PCI Support Functions */

SYSCTL_NODE(_hw, OID_AUTO, edu, CTLFLAG_RD, 0, "Edu PCI");
SYSCTL_INT(_hw_edu, OID_AUTO, debug, CTLFLAG_RW, &edu_debug_lvl, 0,
    "enable debug logs");

static int
edu_probe(device_t dev)
{
	if (pci_get_vendor(dev) == EDU_PCI_VENDOR_ID &&
	    pci_get_device(dev) == EDU_PCI_DEVICE_ID) {
		device_printf(dev,
		    "Edu Probe\nVendor ID : 0x%x\nDevice ID : 0x%x\n",
		    pci_get_vendor(dev), pci_get_device(dev));

		device_set_desc(dev, EDU_PCI_DEVICE_DESC);

		return (BUS_PROBE_DEFAULT);
	}
	return (ENXIO);
}

static int
edu_allocate_bar(struct edu_softc *ctrlr)
{
	ctrlr->bar_id = PCIR_BAR(0);
	ctrlr->bar = bus_alloc_resource_any(ctrlr->dev, SYS_RES_MEMORY,
	    &ctrlr->bar_id, RF_ACTIVE);
	if (ctrlr->bar == NULL) {
		return (ENOMEM);
	}

	return (0);
}

static void
edu_release_bar(struct edu_softc *sc)
{
	if (sc->bar != NULL)
		bus_release_resource(sc->dev, SYS_RES_MEMORY, sc->bar_id,
		    sc->bar);
}

static int
edu_setup_intr(struct edu_softc *sc)
{
	int rc = 0;

	int msi_count = pci_msi_count(sc->dev);
	msi_count = msi_count > 0 ? 1 : 0;
	if (msi_count == 0)
		return (EINVAL);

	rc = pci_alloc_msi(sc->dev, &msi_count);
	if (rc != 0) {
		edu_printf(sc, "unable to allocate MSI\n");
		msi_count = 0;
		return (ENOMEM);
	}

	sc->irq_id = msi_count;
	sc->irq_res = bus_alloc_resource_any(sc->dev, SYS_RES_IRQ, &sc->irq_id,
	    RF_SHAREABLE | RF_ACTIVE);
	if (sc->irq_res == NULL) {
		edu_printf(sc, "unable to allocate shared interrupt\n");
		rc = ENOMEM;
		goto release_msi;
	}

	rc = bus_setup_intr(sc->dev, sc->irq_res, INTR_TYPE_MISC | INTR_MPSAFE,
	    NULL, edu_intr_handler, sc, &sc->cookie);
	if (rc != 0) {
		edu_printf(sc, "unable to setup shared interrupt\n");
		goto release_res;
	}

	return (rc);

release_res:
	bus_release_resource(sc->dev, SYS_RES_IRQ, sc->irq_id, sc->irq_res);
release_msi:
	pci_release_msi(sc->dev);
	return (rc);
}

static void
edu_release_intr(struct edu_softc *sc)
{
	if (sc->irq_res != NULL) {
		bus_teardown_intr(sc->dev, sc->irq_res, sc->cookie);
		bus_release_resource(sc->dev, SYS_RES_IRQ, sc->irq_id,
		    sc->irq_res);
	}

	if (sc->irq_id != 0)
		pci_release_msi(sc->dev);
}

static int
edu_sysctl_version_str(SYSCTL_HANDLER_ARGS)
{
	struct edu_softc *sc = arg1;
	struct edu_version id = edu_get_hwversion(sc);
	edu_printf(sc, "sysid %d %d", id.major, id.minor);
	char vers[8];

	int rc = 0;
	snprintf(vers, sizeof(vers), "%d:%d", id.major, id.minor);

	rc = sysctl_handle_string(oidp, &vers, sizeof(vers), req);
	if (rc) {
		printf("failed sysctl n %d", rc);
		return (rc);
	}

	return (rc);
}

static int
edu_attach(device_t dev)
{
	struct edu_softc *sc;
	int rc;

	struct sysctl_ctx_list *ctx;
	struct sysctl_oid *tree_node;
	struct sysctl_oid_list *tree;

	printf("Edu Attach for : deviceID : 0x%x\n", pci_get_devid(dev));

	sc = device_get_softc(dev);
	sc->dev = dev;

	ctx = device_get_sysctl_ctx(dev);
	tree_node = device_get_sysctl_tree(dev);
	tree = SYSCTL_CHILDREN(tree_node);

	rc = edu_allocate_bar(sc);
	if (rc != 0) {
		edu_printf(sc, "unable to allocate pci resource\n");
		return (rc);
	}
	pci_enable_busmaster(dev);

	rc = edu_setup_intr(sc);
	if (rc != 0)
		goto cleanup;

	mtx_init(&sc->fact_lock, "fact_lock", NULL, MTX_DEF);
	cv_init(&sc->fact_cv, "fact_cv");

	sc->cdev = make_dev(&edu_cdevsw, device_get_unit(dev), UID_ROOT,
	    GID_WHEEL, 0600, "%s%u", EDU_DEV_NAME, device_get_unit(dev));
	sc->cdev->si_drv1 = sc;

	// SYSCTL_NODE(_hw, OID_AUTO, edu, CTLFLAG_RD | CTLFLAG_MPSAFE, 0, "Edu
	// sysctl");
	// struct sysctl_oid *ver_tree = SYSCTL_ADD_NODE(ctx, tree, OID_AUTO,
	//     "edu", CTLFLAG_RD | CTLFLAG_MPSAFE, NULL, "HW major:minor
	//     version");

	// struct sysctl_oid_list *ver_list = SYSCTL_CHILDREN(ver_tree);
	SYSCTL_ADD_UINT(ctx, tree, OID_AUTO, "devid",
	    CTLFLAG_RD | CTLFLAG_MPSAFE, &sc->irq_id, 0, "test");
	SYSCTL_ADD_PROC(ctx, tree, OID_AUTO, "edunode",
	    CTLTYPE_STRING | CTLFLAG_RD | CTLFLAG_MPSAFE, sc, 0,
	    edu_sysctl_version_str, "A", "HW major:minor version");

	return (rc);

cleanup:
	edu_release_intr(sc);
	pci_disable_busmaster(dev);
	edu_release_bar(sc);
	return (rc);
}

static int
edu_detach(device_t dev)
{
	struct edu_softc *sc = device_get_softc(dev);

	destroy_dev(sc->cdev);
	edu_release_intr(sc);
	pci_disable_busmaster(dev);
	edu_release_bar(sc);
	printf("Edu detach!\n");

	return (0);
}

/* Called during system shutdown after sync. */

static int
edu_shutdown(device_t dev)
{
	printf("Edu shutdown!\n");
	return (0);
}

/*
 * Device suspend routine.
 */
static int
edu_suspend(device_t dev)
{
	printf("Edu suspend!\n");
	return (0);
}

/*
 * Device resume routine.
 */
static int
edu_resume(device_t dev)
{
	printf("Edu resume!\n");
	return (0);
}

static device_method_t edu_pci_methods[] = { DEVMETHOD(device_probe, edu_probe),
	DEVMETHOD(device_attach, edu_attach),
	DEVMETHOD(device_detach, edu_detach),
	DEVMETHOD(device_shutdown, edu_shutdown),
	DEVMETHOD(device_suspend, edu_suspend),
	DEVMETHOD(device_resume, edu_resume), DEVMETHOD_END };

static driver_t edu_pci_driver = {
	EDU_DEV_NAME,
	edu_pci_methods,
	sizeof(struct edu_softc),
};
// static devclass_t edu_devclass;

// DEFINE_CLASS_0(Edu, edu_driver, edu_methods, sizeof(struct
// edu_softc));
DRIVER_MODULE(edu, pci, edu_pci_driver, NULL, NULL);
