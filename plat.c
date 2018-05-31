#include <linux/module.h>
#include <linux/pci.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>
#include <sound/hda_register.h>

#define DEBUG_MACRO "Sample Platform driver:"
#define DEBUG_PRINT printk("%s In %s\n",DEBUG_MACRO,__func__)
#define PDEV_NAME "s-plt-dev"

struct s_plat_dev
{
	struct platform_device *mach_dev;		
};

static struct snd_pcm_hardware asoc_uspace_pcm_hw = {
	.info =			(SNDRV_PCM_INFO_MMAP |
			SNDRV_PCM_INFO_INTERLEAVED |
			SNDRV_PCM_INFO_BLOCK_TRANSFER |
			SNDRV_PCM_INFO_MMAP_VALID |
			SNDRV_PCM_INFO_PAUSE |
			SNDRV_PCM_INFO_RESUME |
			SNDRV_PCM_INFO_SYNC_START |
			SNDRV_PCM_INFO_NO_PERIOD_WAKEUP),
	.formats =		SNDRV_PCM_FMTBIT_S16_LE |
		SNDRV_PCM_FMTBIT_S32_LE |
		SNDRV_PCM_FMTBIT_S24_LE |
		SNDRV_PCM_FMTBIT_FLOAT_LE,
	.rates =		SNDRV_PCM_RATE_8000_192000 | SNDRV_PCM_RATE_KNOT,
	.rate_min =		8000,
	.rate_max =		192000,
	.channels_min =		1,
	.channels_max =		8,
	.buffer_bytes_max =	(1024 * 1024 * 1024),
	.period_bytes_min =	128,
	.period_bytes_max =	(1024 * 1024 * 1024) / 2,
	.periods_min =		2,
	.periods_max =		AZX_MAX_FRAG,
	.fifo_size =		0,
};

static int asoc_platform_open(struct snd_pcm_substream *substream)
{
	DEBUG_PRINT;
	snd_soc_set_runtime_hwparams(substream, &asoc_uspace_pcm_hw);
	return 0;
}

static int asoc_platform_close(struct snd_pcm_substream *substream)
{
	DEBUG_PRINT;
	return 0;
}
int asoc_platform_copy(struct snd_pcm_substream *substream, int channel, snd_pcm_uframes_t pos, void __user *buf, snd_pcm_uframes_t count)
{
	DEBUG_PRINT;
	return 0;
}
static int asoc_platform_pcm_trigger(struct snd_pcm_substream *substream,
		int cmd)
{
	struct snd_pcm_runtime *rtd = substream->runtime;

	DEBUG_PRINT;
	pr_debug("sample-pcm:buffer_size = %ld,"
			"dma_area = %p, dma_bytes = %zu\n",
			rtd->buffer_size, rtd->dma_area, rtd->dma_bytes);

	return 0;
}

	static snd_pcm_uframes_t asoc_platform_pcm_pointer
(struct snd_pcm_substream *substream)
{
	unsigned int pos = 2024;
	DEBUG_PRINT;
	pr_info("In asocplatform_pcm_pointer\n");
	return bytes_to_frames(substream->runtime, pos);
}

static const struct snd_pcm_ops platform_ops =
{
	.open = asoc_platform_open,
	.close = asoc_platform_close,
	.ioctl = snd_pcm_lib_ioctl,
	.trigger = asoc_platform_pcm_trigger,
	.pointer = asoc_platform_pcm_pointer,
	.mmap = snd_pcm_lib_default_mmap,
	.page = snd_pcm_sgbuf_ops_page,
	.copy = asoc_platform_copy
};

#define MAX_PREALLOC_SIZE	(32* 1024)

static int platform_pcm_new(struct snd_soc_pcm_runtime *rtd)
{
	struct snd_soc_dai *dai = rtd->cpu_dai;
	struct snd_card *card = rtd->card->snd_card;
	struct snd_pcm *pcm = rtd->pcm;
	unsigned int size;
	int retval = 0;

	DEBUG_PRINT;
	if (dai->driver->playback.channels_min ||
			dai->driver->capture.channels_min) {
		/* buffer pre-allocation */
		size = CONFIG_SND_HDA_PREALLOC_SIZE * 1024;
		if (size > MAX_PREALLOC_SIZE)
			size = MAX_PREALLOC_SIZE;
		retval = snd_pcm_lib_preallocate_pages_for_all(pcm,
				SNDRV_DMA_TYPE_DEV_SG,
				card->dev,
				size, MAX_PREALLOC_SIZE);
		if (retval) {
			printk("dma buffer allocation fail\n");
			return retval;
		}
	}

	return retval;
}

static void platform_pcm_free(struct snd_pcm *pcm)
{
	snd_pcm_lib_preallocate_free_for_all(pcm);
}

static int platform_soc_probe(struct snd_soc_platform *platform)
{
	DEBUG_PRINT;

	return 0;
}

static struct snd_soc_platform_driver platform_drv  = {
	.probe		= platform_soc_probe,
	.ops		= &platform_ops,
	.pcm_new	= platform_pcm_new,
	.pcm_free	= platform_pcm_free,
};

static const struct snd_soc_component_driver platform_component = {
	.name           = "snd-soc-plat",
};

static int dai_pcm_open(struct snd_pcm_substream *substream,
		struct snd_soc_dai *dai)
{
	DEBUG_PRINT;
	return 0;
}

static void dai_pcm_close(struct snd_pcm_substream *substream,
		struct snd_soc_dai *dai)
{
	DEBUG_PRINT;
}

static int dai_pcm_prepare(struct snd_pcm_substream *substream,
		struct snd_soc_dai *dai)
{
	DEBUG_PRINT;
	return 0;
}

static int dai_pcm_hw_params(struct snd_pcm_substream *substream,
		struct snd_pcm_hw_params *params,
		struct snd_soc_dai *dai)
{
	DEBUG_PRINT;
	return snd_pcm_lib_alloc_vmalloc_buffer(substream, params_buffer_bytes(params));
}

static int dai_pcm_hw_free(struct snd_pcm_substream *substream,
		struct snd_soc_dai *dai)
{
	DEBUG_PRINT;
	return snd_pcm_lib_free_vmalloc_buffer(substream);
	return 0;
}

static int dai_pcm_trigger(struct snd_pcm_substream *substream, int cmd,
		struct snd_soc_dai *dai)
{
	DEBUG_PRINT;
	return 0;
}

static struct snd_soc_dai_ops platform_pcm_dai_ops = {
	.startup = dai_pcm_open,
	.shutdown = dai_pcm_close,
	.prepare = dai_pcm_prepare,
	.hw_params = dai_pcm_hw_params,
	.hw_free = dai_pcm_hw_free,
	.trigger = dai_pcm_trigger,
};

#define STUB_RATES	SNDRV_PCM_RATE_8000_192000
#define STUB_FORMATS	(SNDRV_PCM_FMTBIT_S8 | \
		SNDRV_PCM_FMTBIT_U8 | \
		SNDRV_PCM_FMTBIT_S16_LE | \
		SNDRV_PCM_FMTBIT_U16_LE | \
		SNDRV_PCM_FMTBIT_S24_LE | \
		SNDRV_PCM_FMTBIT_U24_LE | \
		SNDRV_PCM_FMTBIT_S32_LE | \
		SNDRV_PCM_FMTBIT_U32_LE | \
		SNDRV_PCM_FMTBIT_IEC958_SUBFRAME_LE)

static struct snd_soc_dai_driver platform_dai[] = {{
	.name = "plat-dai",
		.id = 0,
		.ops = &platform_pcm_dai_ops,
		.playback = {
			.stream_name	= "Playback",
			.channels_min	= 1,
			.channels_max	= 384,
			.rates		= STUB_RATES,
			.formats	= STUB_FORMATS,
		},
		.capture = {
			.stream_name	= "Capture",
			.channels_min	= 1,
			.channels_max	= 384,
			.rates = STUB_RATES,
			.formats = STUB_FORMATS,
		},},
};

static int destroy_machine_device(struct s_plat_dev *spd)
{
	platform_device_unregister(spd->mach_dev);
	return 0;
}

int create_machine_device(struct s_plat_dev *spd)
{
	struct platform_device *pdev;
	int ret = 0;

	pdev = platform_device_alloc("sample-machine", -1);
	if (!pdev) {
		pr_err("In %s failed to allocate machine device\n", __func__);
		return -ENOMEM;
	}

	ret = platform_device_add(pdev);
	if (ret) {
		pr_err("In %s failed to add machine device: %d\n", __func__, ret);
		platform_device_put(pdev);
		return ret;
	}

	spd->mach_dev = pdev;
	dev_set_drvdata(&pdev->dev, spd);

	return ret;
}

static int s_pdev_probe(struct platform_device *pdev)
{
	int ret;
	struct s_plat_dev *spd;

	printk("Register plaform driver for asoc\n");

	spd = devm_kzalloc(&pdev->dev, sizeof(*spd), GFP_KERNEL);
	if (!spd) { 
		pr_err("In %s, Memory not available\n", __func__);
		return -ENOMEM;
	}
	platform_set_drvdata(pdev, spd);

	ret = snd_soc_register_component(&pdev->dev, &platform_component,
			platform_dai,1);

	if (ret) {
		dev_err(&pdev->dev, "soc component registration failed %d\n", ret);
		return ret;
	}

	ret = snd_soc_register_platform(&pdev->dev, &platform_drv);
	if (ret) {
		dev_err(&pdev->dev, "soc platform registration failed %d\n", ret);
		return ret;
	}

	ret = create_machine_device(spd);
	if (ret) {
		dev_err(&pdev->dev, "Machine devie creation failed %d\n", ret);
		return ret;
	}


	return 0;
}

static int s_pdev_remove(struct platform_device *pdev)
{
	struct s_plat_dev *spd;

	spd = platform_get_drvdata(pdev);
	if (spd)
		destroy_machine_device(spd);

	return 0;
}

static struct platform_driver soc_plat_driver = {
	.driver = {
		.name = "snd-soc-myplat",
	},
	.probe = s_pdev_probe,
	.remove = s_pdev_remove,
};

static struct platform_device *soc_plat_dev;
static int __init pdevinit(void)
{
	int ret;

	pr_info("Init platform device driver\n");

	soc_plat_dev =
		platform_device_register_simple("snd-soc-myplat", -1, NULL, 0);
	if (IS_ERR(soc_plat_dev))
		return PTR_ERR(soc_plat_dev);

	ret = platform_driver_register(&soc_plat_driver);
	if (ret != 0) {
		platform_device_unregister(soc_plat_dev);
		return ret;
	}
	return 0;	
}

static void __exit  pdevexit(void)
{
	pr_info("Exit platform device driver\n");
	platform_driver_unregister(&soc_plat_driver);
	platform_device_unregister(soc_plat_dev);
}

module_init(pdevinit);
module_exit(pdevexit);
MODULE_DESCRIPTION("Sample ASoC Platform driver");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("platform:snd-soc-plat");
