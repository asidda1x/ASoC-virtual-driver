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

struct buffer_manipulation_tools {
	struct timer_list timer;
	uint32_t Bps; //Bytes per second
	unsigned long per_size; //Period size in bytes
	uint32_t pos; //Hardware pointer position in ring buffer
	unsigned long timer_cal; //time taken for each period
	unsigned stop_timer; 
};

/*AUDIO FUNDAS
 * 1 Sample = Number of bits can be transferred in single channel at a time
 * 1 Frame = Total number of Samples(in bytes) can be transferred in all the channels at a time.
 * 1 Period = Number of frames, hardware can process at a time. So after processing of each period, hardware generates an interrupt.
 * Rate is called as a sample rate can be defined as amount of data can be transferred per second.
 * ALSA calculates buffer interms of frames. So in practical rate is number of frames per second.
 * Based on rate Bytes per second is calculated.
 * Then number of Bytes per second(Bps) = (Number_of_channels * Sample size in bytes) * Rate
 * Eg: Rate: 19200, Channels: 2, Format: 16 bit
 *	Then number of bytes per second(Bps) = 2 * 2 * 19200 = 76800
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

struct buffer_manipulation_tools *get_bmt(struct snd_pcm_substream *ss)
{
	return ss->runtime->private_data;
}

void start_timer(struct buffer_manipulation_tools *bmt)
{
	unsigned long jif;
	if(!bmt->stop_timer) {
		jif = bmt->timer_cal;
		jif = (jif + bmt->Bps - 1)/bmt->Bps;
		printk("addl jiffies: %lu\n", jif);
		mod_timer(&bmt->timer, jiffies + jif);
	}
}

void stop_timer(struct buffer_manipulation_tools *bmt)
{
	bmt->stop_timer = 1;
	del_timer(&bmt->timer);
	bmt->timer.expires = 0;
}

void update_pos(struct buffer_manipulation_tools *bmt)
{
	bmt->pos += bmt->per_size;
}

void timer_callback(unsigned long pvt_data)
{
	struct snd_pcm_substream *substream = (struct snd_pcm_substream *)pvt_data;
	struct buffer_manipulation_tools *bmt = get_bmt(substream);
	update_pos(bmt);
	snd_pcm_period_elapsed(substream);
	start_timer(bmt);
}

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

static snd_pcm_uframes_t asoc_platform_pcm_pointer(struct snd_pcm_substream *substream)
{
	snd_pcm_uframes_t rbuf_pos;
	struct buffer_manipulation_tools *bmt;

	DEBUG_PRINT;
	bmt = get_bmt(substream);
	rbuf_pos = bytes_to_frames(substream->runtime, bmt->pos);
	if( rbuf_pos >= substream->runtime->buffer_size)
		rbuf_pos %= substream->runtime->buffer_size;
	
	pr_info("In asoc_platform_pcm_pointer tmp %lu\n", rbuf_pos);
	return rbuf_pos;
}

static const struct snd_pcm_ops platform_ops =
{
	.open = asoc_platform_open,
	.close = asoc_platform_close,
	.ioctl = snd_pcm_lib_ioctl,
	.pointer = asoc_platform_pcm_pointer,
	.mmap = snd_pcm_lib_default_mmap,
};

static int platform_pcm_new(struct snd_soc_pcm_runtime *rtd)
{
	DEBUG_PRINT;

	return 0;
}

static void platform_pcm_free(struct snd_pcm *pcm)
{
	DEBUG_PRINT;
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
	struct buffer_manipulation_tools *bmt = get_bmt(substream);
	
	DEBUG_PRINT;
	printk("runtime->access: %u\n", runtime->access);
	printk("runtime->format: %u\n", runtime->format);
	printk("runtime->frame_bits: %u\n", runtime->frame_bits);
	printk("runtime->rate: %u\n", runtime->rate);
	printk("runtime->channels: %u\n", runtime->channels);
	printk("runtime->period_size: %lu\n", runtime->period_size);
	printk("runtime->period_size: %u\n", frames_to_bytes(runtime, runtime->period_size));
	printk("runtime->periods: %u\n", runtime->periods);
	printk("runtime->buffer_size: %lu\n", runtime->buffer_size);
	printk("runtime->buffer_size: %u\n", frames_to_bytes(runtime, runtime->buffer_size));
	bmt->Bps = ((runtime->frame_bits / 8) * runtime->rate);
	printk("Bps: %u\n",bmt->Bps);
	printk("sample-pcm:buffer_size = %ld,"
			"dma_area = %p, dma_bytes = %zu\n",
			runtime->buffer_size, runtime->dma_area, runtime->dma_bytes);
	bmt->per_size = frames_to_bytes(runtime, runtime->period_size);
	printk("HZ: %u\n", HZ);
	bmt->timer_cal = bmt->per_size * HZ;
	bmt->pos = 0;
	return 0;
}

static int dai_pcm_hw_params(struct snd_pcm_substream *substream,
		struct snd_pcm_hw_params *params,
		struct snd_soc_dai *dai)
{
	struct buffer_manipulation_tools *bmt;

	DEBUG_PRINT;
	bmt = kmalloc(sizeof(struct buffer_manipulation_tools), GFP_KERNEL);
	printk("substream->private_data %x\n", substream->private_data);
	substream->runtime->private_data = bmt;
	if(!bmt) {
		printk("Not enough memory\n");
		return -ENOMEM;
	}
	setup_timer(&bmt->timer, timer_callback, (unsigned long)substream);
	return snd_pcm_lib_alloc_vmalloc_buffer(substream, params_buffer_bytes(params));
}

static int dai_pcm_hw_free(struct snd_pcm_substream *substream,
		struct snd_soc_dai *dai)
{
	DEBUG_PRINT;
	if(substream->runtime->private_data)
		kfree(substream->runtime->private_data);
	return snd_pcm_lib_free_vmalloc_buffer(substream);
	return 0;
}

static int dai_pcm_trigger(struct snd_pcm_substream *substream, int cmd,
		struct snd_soc_dai *dai)
{
	struct buffer_manipulation_tools *bmt = get_bmt(substream);
	DEBUG_PRINT;
	printk("cmd: %x\n", cmd);
	switch (cmd) {
		case SNDRV_PCM_TRIGGER_START:
			printk("timer_cal: %02lx\n", bmt->timer_cal);
			bmt->stop_timer = 0;
			start_timer(bmt);
		case SNDRV_PCM_TRIGGER_RESUME:
			pr_info("SNDRV_PCM_TRIGGER_RESUME\n");
			return 0;
		case SNDRV_PCM_TRIGGER_STOP:
			pr_info("SNDRV_PCM_TRIGGER_STOP\n");
			stop_timer(bmt);
			return 0;
		case SNDRV_PCM_TRIGGER_SUSPEND:
			pr_info("SNDRV_PCM_TRIGGER_SUSPEND\n");
			return 0;
	}

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

	ret = snd_soc_register_platform(&pdev->dev, &platform_drv);
	if (ret) {
		dev_err(&pdev->dev, "soc platform registration failed %d\n", ret);
		return ret;
	}

	ret = snd_soc_register_component(&pdev->dev, &platform_component,
			platform_dai,1);
	if (ret) {
		dev_err(&pdev->dev, "soc component registration failed %d\n", ret);
		snd_soc_unregister_platform(&pdev->dev);
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

	snd_soc_unregister_component(&pdev->dev); 
	snd_soc_unregister_platform(&pdev->dev);
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
