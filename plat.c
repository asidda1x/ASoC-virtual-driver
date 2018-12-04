#include <linux/module.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>
#include <linux/dma-mapping.h>
#include <linux/jiffies.h>
#include <linux/time.h>

#include <linux/init.h>
#include <linux/err.h>
#include <linux/platform_device.h>
#include <linux/jiffies.h>
#include <linux/slab.h>
#include <linux/time.h>
#include <linux/wait.h>
#include <linux/hrtimer.h>
#include <linux/math64.h>
#include <linux/module.h>
#include <sound/core.h>
#include <sound/control.h>
#include <sound/tlv.h>
#include <sound/pcm.h>
#include <sound/rawmidi.h>
#include <sound/info.h>
#include <sound/initval.h>

#define DEBUG_MACRO "Sample Platform driver:"
#define DEBUG_PRINT printk("%s In %s\n",DEBUG_MACRO,__func__)
#define PDEV_NAME "s-plt-dev"

#define get_dummy_ops(substream) \
	((struct buffer_manipulation_tools *)(substream->runtime->private_data))->timer_ops

snd_pcm_uframes_t position;
struct s_plat_dev
{
	struct platform_device *mach_dev;		
};

struct dummy_timer_ops {
	int (*create)(struct snd_pcm_substream *);
	void (*free)(struct snd_pcm_substream *);
	int (*prepare)(struct snd_pcm_substream *);
	int (*start)(struct snd_pcm_substream *);
	int (*stop)(struct snd_pcm_substream *);
	snd_pcm_uframes_t (*pointer)(struct snd_pcm_substream *);
};

struct buffer_manipulation_tools {
	const struct dummy_timer_ops *timer_ops;
	spinlock_t lock;
	struct timer_list timer;
	unsigned long base_time;
	unsigned int frac_pos;	/* fractional sample position (based HZ) */
	unsigned int frac_period_rest;
	unsigned int frac_buffer_size;	/* buffer_size * HZ */
	unsigned int frac_period_size;	/* period_size * HZ */
	unsigned int rate;
	int elapsed;
	struct snd_pcm_substream *substream;
};

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

static void dummy_systimer_rearm(struct buffer_manipulation_tools *dpcm)
{
	mod_timer(&dpcm->timer, jiffies +
		(dpcm->frac_period_rest + dpcm->rate - 1) / dpcm->rate);
}

static void dummy_systimer_update(struct buffer_manipulation_tools *dpcm)
{
	unsigned long delta;

	delta = jiffies - dpcm->base_time;
	if (!delta)
		return;
	dpcm->base_time += delta;
	delta *= dpcm->rate;
	dpcm->frac_pos += delta;
	while (dpcm->frac_pos >= dpcm->frac_buffer_size)
		dpcm->frac_pos -= dpcm->frac_buffer_size;
	while (dpcm->frac_period_rest <= delta) {
		dpcm->elapsed++;
		dpcm->frac_period_rest += dpcm->frac_period_size;
	}
	dpcm->frac_period_rest -= delta;
}

static int dummy_systimer_start(struct snd_pcm_substream *substream)
{
	struct buffer_manipulation_tools *dpcm = substream->runtime->private_data;
	spin_lock(&dpcm->lock);
	dpcm->base_time = jiffies;
	dummy_systimer_rearm(dpcm);
	spin_unlock(&dpcm->lock);
	return 0;
}

static int dummy_systimer_stop(struct snd_pcm_substream *substream)
{
	struct buffer_manipulation_tools *dpcm = substream->runtime->private_data;
	spin_lock(&dpcm->lock);
	del_timer(&dpcm->timer);
	spin_unlock(&dpcm->lock);
	return 0;
}

static int dummy_systimer_prepare(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct buffer_manipulation_tools *dpcm = runtime->private_data;

	dpcm->frac_pos = 0;
	dpcm->rate = runtime->rate;
	dpcm->frac_buffer_size = runtime->buffer_size * HZ;
	dpcm->frac_period_size = runtime->period_size * HZ;
	dpcm->frac_period_rest = dpcm->frac_period_size;
	dpcm->elapsed = 0;

	return 0;
}

static void dummy_systimer_callback(unsigned long data)
{
	struct buffer_manipulation_tools *dpcm = (struct buffer_manipulation_tools *)data;
	unsigned long flags;
	int elapsed = 0;

	spin_lock_irqsave(&dpcm->lock, flags);
	dummy_systimer_update(dpcm);
	dummy_systimer_rearm(dpcm);
	elapsed = dpcm->elapsed;
	dpcm->elapsed = 0;
	spin_unlock_irqrestore(&dpcm->lock, flags);
	if (elapsed) {
		pr_info("snd_my: snd_pcm_period_elapsed\n");
		snd_pcm_period_elapsed(dpcm->substream);
	}
}

static snd_pcm_uframes_t
dummy_systimer_pointer(struct snd_pcm_substream *substream)
{
	struct buffer_manipulation_tools *dpcm = substream->runtime->private_data;
	snd_pcm_uframes_t pos;

	spin_lock(&dpcm->lock);
	dummy_systimer_update(dpcm);
	pos = dpcm->frac_pos / HZ;
	spin_unlock(&dpcm->lock);
	return pos;
}

static int dummy_systimer_create(struct snd_pcm_substream *substream)
{
	struct buffer_manipulation_tools *dpcm;

	dpcm = kzalloc(sizeof(*dpcm), GFP_KERNEL);
	if (!dpcm)
		return -ENOMEM;
	substream->runtime->private_data = dpcm;
	setup_timer(&dpcm->timer, dummy_systimer_callback,
			(unsigned long) dpcm);
	spin_lock_init(&dpcm->lock);
	dpcm->substream = substream;
	return 0;
}

static void dummy_systimer_free(struct snd_pcm_substream *substream)
{
	kfree(substream->runtime->private_data);
}

static const struct dummy_timer_ops dummy_systimer_ops = {
	.create =	dummy_systimer_create,
	.free =		dummy_systimer_free,
	.prepare =	dummy_systimer_prepare,
	.start =	dummy_systimer_start,
	.stop =		dummy_systimer_stop,
	.pointer =	dummy_systimer_pointer,
};

struct buffer_manipulation_tools *get_bmt(struct snd_pcm_substream *ss)
{
	return ss->runtime->private_data;
}

static int asoc_platform_open(struct snd_pcm_substream *substream)
{
	int err;
	struct snd_pcm_runtime *runtime = substream->runtime;
	const struct dummy_timer_ops *ops;

	DEBUG_PRINT;

	ops = &dummy_systimer_ops;

	err = ops->create(substream);
	if (err < 0)
		return err;

	get_dummy_ops(substream) = ops;

	snd_soc_set_runtime_hwparams(substream, &asoc_uspace_pcm_hw);

	if (err < 0) {
		get_dummy_ops(substream)->free(substream);
		return err;
	}

	return 0;
}

static int asoc_platform_close(struct snd_pcm_substream *substream)
{
	DEBUG_PRINT;
	get_dummy_ops(substream)->free(substream);

	return 0;
}

static snd_pcm_uframes_t asoc_platform_pcm_pointer(struct snd_pcm_substream *substream)
{
	snd_pcm_uframes_t rbuf_pos;
	struct buffer_manipulation_tools *bmt;

	DEBUG_PRINT;
	bmt = get_bmt(substream);
	rbuf_pos = bmt->timer_ops->pointer(substream);

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

	bmt->timer_ops->prepare(substream);

	return 0;
}

static int dai_pcm_hw_params(struct snd_pcm_substream *substream,
		struct snd_pcm_hw_params *params,
		struct snd_soc_dai *dai)
{
	struct buffer_manipulation_tools *bmt;

	DEBUG_PRINT;
	printk("substream->private_data %x\n", substream->private_data);
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
	case SNDRV_PCM_TRIGGER_RESUME:
		return get_dummy_ops(substream)->start(substream);
	case SNDRV_PCM_TRIGGER_STOP:
	case SNDRV_PCM_TRIGGER_SUSPEND:
		return get_dummy_ops(substream)->stop(substream);
	}
	return -EINVAL;

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
MODULE_AUTHOR("Aravind Siddappaji");
MODULE_DESCRIPTION("Sample ASoC Platform driver");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("platform:snd-soc-plat");
