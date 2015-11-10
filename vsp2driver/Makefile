CFILES := vsp2_drv.c vsp2_entity.c vsp2_pipe.c
CFILES += vsp2_video.c
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
EXTRA_CFLAGS += $(U_INCLUDE)
EXTRA_CFLAGS += -DUSE_BUFFER

target:
	make -C $(KERNELDIR) M=$(PWD) KBUILD_EXTRA_SYMBOLS=$(KERNELSRC)/include/vspm.symvers modules

all:
	make clean
	make target
	
clean:
	make -C $(KERNELDIR) M=$(PWD) clean
