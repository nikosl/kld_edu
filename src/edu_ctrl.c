/*
 * Copyright 2024 Nikos Leivadaris <nikosleiv@gmail.com>.
 * SPDX-License-Identifier: MIT
 */

#include "edu_ctrl.h"
#include "sys/mutex.h"

uint32_t
edu_liveness(const struct edu_softc *sc, uint32_t c)
{
	edu_mmio_liveness_w(sc, c);
	edu_mmio_rw_barier_4(sc);
	return (edu_mmio_liveness_r(sc));
}

struct edu_version
edu_get_hwversion(const struct edu_softc *sc)
{
	uint32_t id = edu_mmio_ident_r(sc);
	return (edu_init_version(id));
}

bool
edu_is_computing(const struct edu_softc *sc)
{
	return edu_mmio_status_r(sc) & EDU_STATUS_COMPUTING;
}

bool
edu_is_xfer_active(const struct edu_softc *sc)
{
	return edu_mmio_dma_cmd_r(sc) & EDU_DMA_CMD_START_XFER;
}

void
edu_raise_intr(const struct edu_softc *sc, uint32_t i)
{
	edu_mmio_ack_intr_w(sc, i);
}

uint32_t
edu_factorial(struct edu_softc *sc, uint32_t i)
{
	edu_mmio_factorial_w(sc, i);

	printf("computing for interrupt");
	edu_mmio_rw_barier_4(sc);
	edu_mmio_status_w(sc, EDU_STATUS_RAISE_IRQ);
	printf("wait for interrupt");

	mtx_lock(&sc->fact_lock);
	cv_wait(&sc->fact_cv, &sc->fact_lock);
	mtx_unlock(&sc->fact_lock);
	return (edu_mmio_factorial_r(sc));
}

uint32_t
edu_intr_status(const struct edu_softc *sc)
{
	return (edu_mmio_intr_status_r(sc));
}

void
edu_intr_ack(const struct edu_softc *sc, uint32_t ack)
{
	edu_mmio_ack_intr_w(sc, ack);
}
