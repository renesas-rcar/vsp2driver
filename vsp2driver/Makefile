CFILES := vsp2_drv.c vsp2_entity.c vsp2_video.c
CFILES += vsp2_rpf.c vsp2_rwpf.c vsp2_wpf.c
CFILES += vsp2_bru.c vsp2_uds.c
CFILES += vsp2_lut.c
CFILES += vsp2_clu.c
CFILES += vsp2_hgo.c
CFILES += vsp2_hgt.c
CFILES += vsp2_vspm.c
CFILES += vsp2_addr.c
CFILES += vsp2_debug.c

obj-m += vsp2.o
vsp2-objs := $(CFILES:.c=.o)

U_INCLUDE := -I$(PWD)
U_INCLUDE += -I$(KERNELSRC)/include
U_INCLUDE += -I$(VSP2_VSPMDIR)/../include
EXTRA_CFLAGS += $(U_INCLUDE)
EXTRA_CFLAGS += -DUSE_BUFFER
#EXTRA_CFLAGS += -DTYPE_GEN2
EXTRA_CFLAGS += -DVSP2_DEBUG
ifndef VSP2_VSPMSYMVERS
VSP2_VSPMSYMVERS = Module.symvers
endif

target:
	make -C $(KERNELDIR) M=$(PWD) KBUILD_EXTRA_SYMBOLS=$(VSP2_VSPMDIR)/$(VSP2_VSPMSYMVERS) modules

all:
	make clean
	make target

#debug:
#	make target -DVSP2_DEBUG

#debug_all:
#	make clean
#	make debug
	
clean:
	make -C $(KERNELDIR) M=$(PWD) clean
