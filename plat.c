#include <linux/module.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>
#include <linux/dma-mapping.h>
#include <linux/jiffies.h>
#include <linux/time.h>

#define DEBUG_MACRO "Sample Platform driver:"
#define DEBUG_PRINT printk("%s In %s\n",DEBUG_MACRO,__func__)
#define PDEV_NAME "s-plt-dev"
snd_pcm_uframes_t position;
struct s_plat_dev
{
	struct platform_device *mach_dev;		
};

struct timer_list timer;
//ktime_t kt;
uint32_t Bps; //Bytes per second
uint32_t per_cnt; //Period count
uint32_t pos; //Hardware pointer position
unsigned long timer_cal; //time taken for each period 

/*AUDIO FUNDAS
 * 1 Sample = Number of bits can be transferred in single channel at a time
 * 1 Frame = Total number of Samples(in bytes) can be transferred in all the channels at a time.
 * 1 Period = Number of frames, hardware can process at a time. So after processing of each period, hardware generates an interrupt.
 * Rate is called as a sample rate can be defined as amount of data can be transferred per second.
 * Based on rate BYtes per second is calculated.
 * Then number of Bytes per second(Bps) = Number_of_channels * Sample size in bytes * Rate
 * Eg: Rate: 19200, Channels: 2, Format: 16 bit
 *	Then number of bytes per second(Bps) = Number_of_channels * Sample size in bytes * Rate
 */
static struct snd_pcm_hardware asoc_uspace_pcm_hw = {
	.info =		(SNDRV_PCM_INFO_MMAP |
			SNDRV_PCM_INFO_INTERLEAVED |
			SNDRV_PCM_INFO_MMAP_VALID |
			SNDRV_PCM_INFO_PAUSE |
			SNDRV_PCM_INFO_RESUME |
			SNDRV_PCM_INFO_SYNC_START |
			SNDRV_PCM_INFO_NO_PERIOD_WAKEUP),
	.formats =	(SNDRV_PCM_FMTBIT_S16_LE |
			SNDRV_PCM_FMTBIT_S32_LE |
			SNDRV_PCM_FMTBIT_S24_LE |
			SNDRV_PCM_FMTBIT_FLOAT_LE),
	.rates =	SNDRV_PCM_RATE_8000_192000 | SNDRV_PCM_RATE_KNOT,
	.rate_min =		8000,
	.rate_max =		192000,
	.channels_min =		1,
	.channels_max =		8,
	.buffer_bytes_max =	2 * 1024 * 1024,
	.period_bytes_min =	64,
	.period_bytes_max =	(1024 * 1024),
	.periods_min =		2,
	.periods_max =		32,
	.fifo_size =		0,
};

void timer_callback(unsigned long a)
{
	struct snd_pcm_substream *substream = (struct snd_pcm_substream *)a;
	per_cnt++;
	mod_timer(&timer,jiffies+ (1000/timer_cal));
	snd_pcm_period_elapsed(substream);
	printk("In %s per_cnt %u \n", __func__, per_cnt);
}

static int asoc_platform_open(struct snd_pcm_substream *substream)
{
	DEBUG_PRINT;
	setup_timer(&timer, timer_callback, (unsigned long)substream);
	snd_soc_set_runtime_hwparams(substream, &asoc_uspace_pcm_hw);
	return 0;
}

static int asoc_platform_close(struct snd_pcm_substream *substream)
{
	DEBUG_PRINT;
	return 0;
}

static int asoc_platform_pcm_trigger(struct snd_pcm_substream *substream,
								int cmd)
{
	struct snd_pcm_runtime *rtd = substream->runtime;
	struct snd_pcm_runtime *runtime = substream->runtime;

	DEBUG_PRINT;
	printk("sample-pcm:buffer_size = %ld,"
			"dma_area = %p, dma_bytes = %zu\n",
			rtd->buffer_size, rtd->dma_area, rtd->dma_bytes);
	printk("cmd: %x\n", cmd);
	switch (cmd) {
		case SNDRV_PCM_TRIGGER_START:
	printk("runtime->access: %02x\n", runtime->access);
	printk("runtime->format: %02x\n", runtime->format);
	printk("runtime->frame_bits: %ux\n", runtime->frame_bits);
	printk("runtime->rate: %u\n", runtime->rate);
	printk("runtime->channels: %02x\n", runtime->channels);
	printk("runtime->period_size: %lu\n", runtime->period_size);
	printk("runtime->period_size: %u\n", frames_to_bytes(runtime, runtime->period_size));
	printk("runtime->periods: %02x\n", runtime->periods);
	printk("runtime->buffer_size: %lu\n", runtime->buffer_size);
	printk("runtime->buffer_size: %u\n", frames_to_bytes(runtime, runtime->buffer_size));

		Bps = (runtime->channels * (runtime->frame_bits / 8) * runtime->rate);
		printk("Bps: %u\n",Bps);
		timer_cal = Bps/frames_to_bytes(runtime, runtime->period_size);
		printk("timer_cal: %02x\n", timer_cal);
		per_cnt = 0;

		mod_timer(&timer,jiffies+ (1000/timer_cal));
		case SNDRV_PCM_TRIGGER_RESUME:
			pr_info("SNDRV_PCM_TRIGGER_RESUME\n");
			return 0;//get_dummy_ops(substream)->start(substream);
		case SNDRV_PCM_TRIGGER_STOP:
			pr_info("SNDRV_PCM_TRIGGER_STOP\n");
			per_cnt = 0;
			del_timer(&timer);
		case SNDRV_PCM_TRIGGER_SUSPEND:
			pr_info("SNDRV_PCM_TRIGGER_SUSPEND\n");
			return 0;// get_dummy_ops(substream)->stop(substream);
	}

	return -EINVAL;
}

static snd_pcm_uframes_t asoc_platform_pcm_pointer(struct snd_pcm_substream *substream)
{
	unsigned int pos = 2024;
	snd_pcm_uframes_t tmp;
	DEBUG_PRINT;
	pr_info("In asoc_platform_pcm_pointer per_cnt %u\n", per_cnt);
	tmp = (substream->runtime->period_size * per_cnt);//bytes_to_frames(substream->runtime, position);
	pr_info("In asoc_platform_pcm_pointer tmp %u\n", tmp);
	if(tmp >= substream->runtime->buffer_size)
		return tmp -1;
	return tmp;
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
};

#define MAX_PREALLOC_SIZE	(32* 1024)

static int platform_pcm_new(struct snd_soc_pcm_runtime *rtd)
{
	struct snd_soc_dai *dai = rtd->cpu_dai;
	struct snd_card *card = rtd->card->snd_card;
	struct snd_pcm *pcm = rtd->pcm;
	size_t size;
	int retval = 0;

	DEBUG_PRINT;
	printk("%s: retval: %d", __func__, retval);
	
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

/*
 * Call backs specific to dai
 */
static int dai_pcm_startup(struct snd_pcm_substream *substream,
		struct snd_soc_dai *dai)
{
	DEBUG_PRINT;
	return 0;
}

static void dai_pcm_shutdown(struct snd_pcm_substream *substream,
		struct snd_soc_dai *dai)
{
	DEBUG_PRINT;
}

static int dai_pcm_prepare(struct snd_pcm_substream *substream,
		struct snd_soc_dai *dai)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	DEBUG_PRINT;
	printk("runtime->access: %02x\n", runtime->access);
	printk("runtime->format: %02x\n", runtime->format);
	printk("runtime->frame_bits: %ux\n", runtime->frame_bits);
	printk("runtime->rate: %u\n", runtime->rate);
	printk("runtime->channels: %02x\n", runtime->channels);
	printk("runtime->period_size: %02lx\n", runtime->period_size);
	printk("runtime->period_size: %02x\n", frames_to_bytes(runtime, runtime->period_size));
	printk("runtime->periods: %02x\n", runtime->periods);
	printk("runtime->buffer_size: %02lx\n", runtime->buffer_size);
	printk("runtime->buffer_size: %02x\n", frames_to_bytes(runtime, runtime->buffer_size));
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
	.startup = dai_pcm_startup,
	.shutdown = dai_pcm_shutdown,
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
