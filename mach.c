#include <linux/module.h>
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/soc.h>
#include <sound/pcm_params.h>

static int machine_dai_init(struct snd_soc_pcm_runtime *rtd)
{
	pr_info("In machine_dai_init\n");
	return 0;
}

static struct snd_soc_dai_link machine_dai[] = {
 {
                .name = "MY Audio Port",
                .stream_name = "Audio",
                .cpu_dai_name = "snd-soc-myplat",
		.platform_name = "snd-soc-myplat",
                .nonatomic = 1,
                //.dynamic = 1, 
                .codec_name = "snd-soc-mycodec",
                .codec_dai_name = "codec-dai1",
                .trigger = {
                        SND_SOC_DPCM_TRIGGER_POST,
                        SND_SOC_DPCM_TRIGGER_POST
                },
		.dpcm_playback = 1,
        },

};

static struct snd_soc_card machine = {
	.name = "s-virtual",
	.owner = THIS_MODULE,
	.dai_link = machine_dai,
	.num_links = ARRAY_SIZE(machine_dai),
	.controls = NULL,
	.num_controls = 0,
};

static int s_mach_dev_probe(struct platform_device *pdev)
{
	int ret;

	printk("In Registering machine driver\n\n");
	machine.dev = &pdev->dev;
	ret = devm_snd_soc_register_card(&pdev->dev, &machine);
	if (ret) {
		dev_err(&pdev->dev,"Failed in devm_snd_soc_register_card err: %d", ret);
	}

	return ret;
}

static int s_mach_dev_remove(struct platform_device *pdev)
{
	pr_info("In %s \n", __func__);

	return 0;
}

static struct platform_driver s_machine = {
	.driver = {
		.name = "sample-machine",
	},
	.probe = s_mach_dev_probe,
	.remove = s_mach_dev_remove,
};

module_platform_driver(s_machine);

MODULE_AUTHOR("JAIKRISHNA");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:sample-machine");
