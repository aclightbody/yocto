obj-m := hcsr04.o

SRC := $(shell pwd)

# "MAKE" is recursive, "make" is not.
# CC: C compiler.
# -C: compile .c files into .o files.
# all: make all top-level targets (source files) the Makefile knows about.
# KERNEL_SRC: location of the kernel source files,
# M: variable. The Makefile script can read and use the contents of the variable. Allows make to return to current working directory after processing Makefile.
all:
		$(MAKE) -C $(KERNEL_SRC) M=$(SRC)

# modules_install: make modules command will just compile the modules, leaving the compiled binaries in the build directory. make modules_install will make sure that there are compiled binaries (and compile the modules, if not) and install the binaries into your kernel's modules directory.
# Have to excute make before make modules_install. You can't execute modules_install if modules weren't built with the make modules or just make. There is only one difference between make and make modules: the make modules compiles only modules or source code which is set as obj-CONFIG_OPTION_NAME=m in the Makefile. If you want just to build Linux kernel, just use make, the make modules already included there.
modules_install:
		$(MAKE) INSTALL_MOD_DIR=kernel/drivers/my_mod -C $(KERNEL_SRC) M=$(SRC) modules_install

# test_hcsr04:
# 		$(MAKE) test_hcsr04.c -o M=$(SRC) test_hcsr04

# Remove files (rm: remove -f:file -r:recursive)
clean:
		rm -f *.o *~core .depend .*.cmd *.ko *.mod.c
		rm -f Module.markers Module.symvers modules.order
		rm -rf .tmp_versions Modules.symvers