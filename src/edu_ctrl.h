/*
 * Copyright 2024 Nikos Leivadaris <nikosleiv@gmail.com>.
 * SPDX-License-Identifier: MIT
 */

#ifndef __EDU_CTRL_H__
#define __EDU_CTRL_H__

#include <sys/types.h>
#include <sys/bus.h>
#include <sys/rman.h>

#include <machine/bus.h>

#include "edu_priv.h"

#define EDU_DMA_CMD_START_XFER 1
#define EDU_DMA_CMD_RAM_TO_DEV 0
#define EDU_DMA_CMD_DEV_TO_RAM 2
#define EDU_DMA_CMD_RAISE_IRQ  4

#define EDU_DMA_CMD_XFER_TO_DEV                            \
	(EDU_DMA_CMD_START_XFER | EDU_DMA_CMD_RAM_TO_DEV | \
	    EDU_DMA_CMD_RAISE_IRQ)

#define EDU_DMA_CMD_XFER_TO_RAM                            \
	(EDU_DMA_CMD_START_XFER | EDU_DMA_CMD_DEV_TO_RAM | \
	    EDU_DMA_CMD_RAISE_IRQ)

enum edu_status { EDU_STATUS_COMPUTING = 0x01, EDU_STATUS_RAISE_IRQ = 0x80 };
enum edu_intr_signal { EDU_INTR_FACTORIAL = 0x01, EDU_INTR_DMA = 0x100 };

#define EDU_ADDR_IDENT		 0x0
#define EDU_ADDR_LIVENESS	 0x04
#define EDU_ADDR_FACTORIAL	 0x08
#define EDU_ADDR_STATUS		 0x20
#define EDU_ADDR_IRQ_STATUS	 0x24
#define EDU_ADDR_IRQ_RAISE	 0x60
#define EDU_ADDR_IRQ_ACK	 0x64
#define EDU_ADDR_DMA_SRC	 0x80
#define EDU_ADDR_DMA_DST	 0x88
#define EDU_ADDR_DMA_COUNT	 0x90
#define EDU_ADDR_DMA_CMD	 0x98

#define edu_mmio_offsetof(reg)	 offsetof(struct edu_regs, reg)
#define edu_mmio_read_4(sc, reg) bus_read_4((sc)->bar, edu_mmio_offsetof(reg))
#define edu_mmio_read_8(sc, reg) bus_read_8((sc)->bar, edu_mmio_offsetof(reg))
#define edu_mmio_write_4(sc, reg, v) \
	bus_write_4((sc)->bar, edu_mmio_offsetof(reg), (v))
#define edu_mmio_write_8(sc, reg, v) \
	bus_write_8((sc)->bar, edu_mmio_offsetof(reg), (v))

#define edu_mmio_rw_barier_4(sc)     \
	bus_barrier((sc)->bar, 0, 4, \
	    BUS_SPACE_BARRIER_READ | BUS_SPACE_BARRIER_WRITE);

static __inline uint32_t
edu_mmio_intr_status_r(const struct edu_softc *sc)
{
	return edu_mmio_read_4(sc, irq_status);
}

static __inline void
edu_mmio_raise_intr_w(const struct edu_softc *sc, uint32_t i)
{
	edu_mmio_write_4(sc, irq_raise, i);
}

static __inline void
edu_mmio_ack_intr_w(const struct edu_softc *sc, uint32_t i)
{
	edu_mmio_write_4(sc, irq_ack, i);
}

static __inline uint32_t
edu_mmio_liveness_r(const struct edu_softc *sc)
{
	return edu_mmio_read_4(sc, liveness);
}

static __inline void
edu_mmio_liveness_w(const struct edu_softc *sc, uint32_t c)
{
	edu_mmio_write_4(sc, liveness, c);
}

static __inline uint32_t
edu_mmio_factorial_r(const struct edu_softc *sc)
{
	return edu_mmio_read_4(sc, factorial);
}

static __inline void
edu_mmio_factorial_w(const struct edu_softc *sc, uint32_t i)
{
	edu_mmio_write_4(sc, factorial, i);
}

static __inline uint32_t
edu_mmio_ident_r(const struct edu_softc *sc)
{
	return edu_mmio_read_4(sc, ident);
}

static __inline uint32_t
edu_mmio_status_r(const struct edu_softc *sc)
{
	return edu_mmio_read_4(sc, status);
}

static __inline void
edu_mmio_status_w(const struct edu_softc *sc, uint32_t i)
{
	edu_mmio_write_4(sc, status, i);
}

static __inline uint32_t
edu_mmio_dma_cmd_r(const struct edu_softc *sc)
{
	return edu_mmio_read_4(sc, dma_cmd);
}

bool edu_is_computing(const struct edu_softc *);
bool edu_is_xfer_active(const struct edu_softc *);
struct edu_version edu_get_hwversion(const struct edu_softc *);
uint32_t edu_liveness(const struct edu_softc *, uint32_t);
void edu_raise_intr(const struct edu_softc *, uint32_t);
uint32_t edu_intr_status(const struct edu_softc *);
void edu_intr_ack(const struct edu_softc *, uint32_t);
uint32_t edu_factorial(struct edu_softc *, uint32_t i);

#endif // !__EDU_CTRL_H__