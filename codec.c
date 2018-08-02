#include <linux/device.h>
#include <linux/module.h>
#include <linux/init.h>
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>
#include <sound/soc-dapm.h>
#include <sound/initval.h>
#include <sound/tlv.h>

#define CODEC_STEREO_RATES (SNDRV_PCM_RATE_44100 | SNDRV_PCM_RATE_48000)
#define CODEC_FORMATS (SNDRV_PCM_FMTBIT_S16_LE | SNDRV_PCM_FMTBIT_S20_3LE | \
			SNDRV_PCM_FMTBIT_S24_LE | SNDRV_PCM_FMTBIT_S8)


static int codec_probe(struct snd_soc_codec *codec)
{
pr_info("In codec probe\n");
return 0;
}

static int codec_remove(struct snd_soc_codec *codec)
{
pr_info("In codec_remove\n");
return 0;
}

static int codec_suspend(struct snd_soc_codec *codec)
{
pr_info("In codec_suspend\n");
return 0;
}

static int codec_resume(struct snd_soc_codec *codec)
{
pr_info("In codec_resume\n");
return 0;
}

/*Dai driver ops*/
static int codec_set_dai_fmt(struct snd_soc_dai *dai, unsigned int fmt)
{
	pr_info("In codec_set_dai_fmt\n");
	return 0;
}

static int codec_set_dai_sysclk(struct snd_soc_dai *dai,
				int clk_id, unsigned int freq, int dir)
{
	pr_info("In codec_set_dai_sysclk\n");
	return 0;
}

static int codec_set_bclk_ratio(struct snd_soc_dai *dai, unsigned int ratio)
{
	pr_info("In codec_set_bclk_ratio\n");
	return 0;
}

static int codec_set_bias_level(struct snd_soc_codec *codec,
				 enum snd_soc_bias_level level)
{
	pr_info("In codec_set_bias_level\n");
	return 0;
}

static int codec_hw_params(struct snd_pcm_substream *substream,
			    struct snd_pcm_hw_params *params,
			    struct snd_soc_dai *dai)
{
	printk("In codec_hw_params\n");
	return 0;
}

static int codec_pcm_startup(struct snd_pcm_substream *substream,
		struct snd_soc_dai *dai)
{
	printk("In %s\n", __func__);
	return 0;
}

static void codec_pcm_shutdown(struct snd_pcm_substream *substream,
		struct snd_soc_dai *dai)
{
	printk("In %s\n", __func__);
}

static int codec_pcm_prepare(struct snd_pcm_substream *substream,
		struct snd_soc_dai *dai)
{
	printk("In %s\n", __func__);
	return 0;
}

static const struct snd_soc_dai_ops codec_dai_ops = {
	.startup = codec_pcm_startup,
	.shutdown = codec_pcm_shutdown,
	.prepare = codec_pcm_prepare,
	.hw_params = codec_hw_params,
	.set_fmt = codec_set_dai_fmt,
	.set_sysclk = codec_set_dai_sysclk,
	.set_bclk_ratio = codec_set_bclk_ratio,
};

static struct snd_soc_dai_driver codec_dai[] = {

	{
		.name = "codec-dai1",
		.id = 0,
		.playback = {
			.stream_name = "dai1 Playback",
			.channels_min = 1,
			.channels_max = 2,
			.rates = CODEC_STEREO_RATES,
			.formats = CODEC_FORMATS,
		},
		.capture = {
			.stream_name = "dai1 Capture",
			.channels_min = 1,
			.channels_max = 2,
			.rates = CODEC_STEREO_RATES,
			.formats = CODEC_FORMATS,
		},
		.ops = &codec_dai_ops,
		.symmetric_rates = 1,
	}
};

static int rt298_spk_event(struct snd_soc_dapm_widget *w,
			    struct snd_kcontrol *kcontrol, int event)
{
	pr_info("In rt298_spk_event\n");
	return 0;
}

static struct snd_soc_codec_driver codec_driver = {
	/*driver ops*/
	.probe = codec_probe,
	.remove = codec_remove,
	.suspend = codec_suspend,
	.resume = codec_resume,
	.set_bias_level = codec_set_bias_level,
	.idle_bias_off = true,

	.component_driver = {
		.controls = NULL,
		.num_controls = 0,
	},
};

int snd_codec_probe(struct platform_device *pdev)
{
	int ret = 0;
	printk("Hi I am Codec Driver\n\n");
	pr_info("Now I am doing all codec configuration\n");
	ret = snd_soc_register_codec(&pdev->dev, &codec_driver, codec_dai, ARRAY_SIZE(codec_dai));
	printk("ret %d\n",ret);
	return 0;
}

static int snd_codec_remove(struct platform_device *pdev)
{
	snd_soc_unregister_platform(&pdev->dev);
	snd_soc_unregister_codec(&pdev->dev);

	return 0;
}

static struct platform_driver soc_dummy_driver = {
	.driver = {
		.name = "snd-soc-mycodec",
	},
	.probe = snd_codec_probe,
	.remove = snd_codec_remove,
};

static struct platform_device *soc_dummy_dev;

int __init codec_init(void)
{
	int ret;

	soc_dummy_dev =
		platform_device_register_simple("snd-soc-mycodec", -1, NULL, 0);
	if (IS_ERR(soc_dummy_dev))
		return PTR_ERR(soc_dummy_dev);

	ret = platform_driver_register(&soc_dummy_driver);
	if (ret != 0)
		platform_device_unregister(soc_dummy_dev);

	return ret;
}

void __exit codec_exit(void)
{
	platform_device_unregister(soc_dummy_dev);
	platform_driver_unregister(&soc_dummy_driver);
}

module_init(codec_init);
module_exit(codec_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("JAIKRISHNA");
MODULE_DESCRIPTION("My first codec driver");

