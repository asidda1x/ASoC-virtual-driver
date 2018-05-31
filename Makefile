snd-soc-plat-objs := plat.o
snd-soc-codec-objs := codec.o
snd-soc-mach-objs := mach.o

obj-$(CONFIG_SND_SOC_MY)	= snd-soc-plat.o
obj-$(CONFIG_SND_SOC_MY)	+= snd-soc-codec.o
obj-$(CONFIG_SND_SOC_MY)	+= snd-soc-mach.o
