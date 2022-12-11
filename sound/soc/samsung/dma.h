/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * ALSA PCM interface for the Samsung SoC
 */

#ifndef _SAMSUNG_DMA_H
#define _SAMSUNG_DMA_H

#include <sound/dmaengine_pcm.h>

<<<<<<< HEAD
/*
 * @tx, @rx arguments can be NULL if the DMA channel names are "tx", "rx",
 * otherwise actual DMA channel names must be passed to this function.
 */
int samsung_asoc_dma_platform_register(struct device *dev, dma_filter_fn filter,
				       const char *tx, const char *rx,
				       struct device *dma_dev);
#endif /* _SAMSUNG_DMA_H */
=======
struct s3c_dma_params {
	void *slave;				/* Channel ID */
	dma_addr_t dma_addr;
	int dma_size;			/* Size of the DMA transfer */
	char *ch_name;
	struct snd_dmaengine_dai_dma_data dma_data;
};

void samsung_asoc_init_dma_data(struct snd_soc_dai *dai,
				struct s3c_dma_params *playback,
				struct s3c_dma_params *capture);
int samsung_asoc_dma_platform_register(struct device *dev);

#endif
>>>>>>> 32d56b82a4422584f661108f5643a509da0184fc
