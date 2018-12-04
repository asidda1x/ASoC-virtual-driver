snd-soc-my-plat-objs := plat.o
snd-soc-my-codec-objs := codec.o
snd-soc-my-mach-objs := mach.o

obj-$(CONFIG_SND_SOC_MY)	= snd-soc-my-plat.o
obj-$(CONFIG_SND_SOC_MY)	+= snd-soc-my-codec.o
obj-$(CONFIG_SND_SOC_MY)	+= snd-soc-my-mach.o
