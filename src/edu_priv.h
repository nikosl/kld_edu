#ifndef __EDU_PRIV_H__
#define __EDU_PRIV_H__

#include <sys/types.h>
#include <sys/condvar.h>
#include <sys/mutex.h>

#include "sys/systm.h"

#define edu_debug(fmt, args...)                                           \
	do {                                                              \
		if ((edu_debug_lvl > 2))                                  \
			printf("DEBUG: %s: " fmt "\n", __func__, ##args); \
	} while (0)

#define edu_error(fmt, args...) \
	printf("Error: %s: " fmt "\n", __func__, ##args);

#define edu_printf(ctrlr, fmt, args...) \
	device_printf(ctrlr->dev, fmt "\n", ##args)

#define EDU_DMA_MASK   28
#define EDU_DMA_SIZE   4096
#define EDU_DMA_OFFSET 0x40000

struct edu_regs {
	uint32_t ident;
	uint32_t liveness;
	uint32_t factorial;
	uint8_t reserved1[20];
	uint32_t status;
	uint32_t irq_status;
	uint8_t reserved2[56];
	uint32_t irq_raise;
	uint32_t irq_ack;
	uint8_t reserved3[24];
	uint64_t dma_src;
	uint64_t dma_dst;
	uint64_t dma_cnt;
	uint64_t dma_cmd;
};

#define EDU_ID_MAJOR(n)		(((n)&0xff000000) >> 24)
#define EDU_ID_MINOR(n)		(((n)&0x00ff0000) >> 16)

#define EDU_BUF_VERSION_MAX_LEN 8
#define EDU_BUF_NUM_MAX_LEN	16
#define EDU_BUF_INPUT_MAX_LEN	5

struct edu_version {
	uint8_t major;
	uint8_t minor;
};

static inline struct edu_version
edu_init_version(uint32_t id)
{
	struct edu_version ver = {
		.major = EDU_ID_MAJOR(id),
		.minor = EDU_ID_MINOR(id),
	};

	printf("id sc is %d, %d %d", EDU_ID_MAJOR(id), EDU_ID_MINOR(id), id);
	return ver;
}

struct edu_softc {
	device_t dev;
	struct cdev *cdev;

	int bar_id;
	struct resource *bar;

	// shared intr
	int irq_id;
	struct resource *irq_res;
	void *cookie;

	struct mtx fact_lock;
	struct cv fact_cv;
};

#endif // !__EDU_PRIV_H__