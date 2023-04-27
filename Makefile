.SILENT:
K=kernel
U=user

# riscv64-unknown-elf- or riscv64-linux-gnu-
# perhaps in /opt/riscv/bin
#TOOLPREFIX = 

# Try to infer the correct TOOLPREFIX if not set
# ifndef TOOLPREFIX
# TOOLPREFIX := $(shell if riscv64-unknown-elf-objdump -i 2>&1 | grep 'elf64-big' >/dev/null 2>&1; \
# 	then echo 'riscv64-unknown-elf-'; \
# 	elif riscv64-linux-gnu-objdump -i 2>&1 | grep 'elf64-big' >/dev/null 2>&1; \
# 	then echo 'riscv64-linux-gnu-'; \
# 	elif riscv64-unknown-linux-gnu-objdump -i 2>&1 | grep 'elf64-big' >/dev/null 2>&1; \
# 	then echo 'riscv64-unknown-linux-gnu-'; \
# 	else echo "***" 1>&2; \
# 	echo "*** Error: Couldn't find a riscv64 version of GCC/binutils." 1>&2; \
# 	echo "*** To turn off this error, run 'gmake TOOLPREFIX= ...'." 1>&2; \
# 	echo "***" 1>&2; exit 1; fi)
# endif

TOOLPREFIX = riscv64-linux-gnu-

QEMU = qemu-system-riscv64
BUILD_DIR = build
U_OBJ_DIR = $(BUILD_DIR)/$U
K_OBJ_DIR = $(BUILD_DIR)/$K

U_SRCS = $(filter-out $U/forktest.c $(shell find $U/lib -name "*"), $(shell find $U -name "*.[c]"))
U_LIB_OBJS = $(patsubst %.c, $(BUILD_DIR)/%.o, $(shell find $U/lib -name "*.[c]")) $(U_OBJ_DIR)/usys.o
U_SRC_OBJS = $(patsubst %.c, $(BUILD_DIR)/%.o, $(U_SRCS))
U_PROGS = $(addprefix $(U_OBJ_DIR)/_, $(basename $(notdir $(U_SRCS))))

K_OBJS = $(patsubst %.c, $(BUILD_DIR)/%.o, $(patsubst %.S, $(BUILD_DIR)/%.o, $(shell find $K -name "*.[cS]")))

CC = $(TOOLPREFIX)gcc
AS = $(TOOLPREFIX)gas
LD = $(TOOLPREFIX)ld
OBJCOPY = $(TOOLPREFIX)objcopy
OBJDUMP = $(TOOLPREFIX)objdump

CFLAGS = -Wall -Werror -O -fno-omit-frame-pointer -ggdb
CFLAGS += -MD 
CFLAGS += -mcmodel=medany
CFLAGS += -ffreestanding -fno-common -nostdlib -mno-relax
CFLAGS += -I$(XV6_HOME)/include
CFLAGS += $(shell $(CC) -fno-stack-protector -E -x c /dev/null >/dev/null 2>&1 && echo -fno-stack-protector)

# Disable PIE when possible (for Ubuntu 16.10 toolchain)
ifneq ($(shell $(CC) -dumpspecs 2>/dev/null | grep -e '[^f]no-pie'),)
CFLAGS += -fno-pie -no-pie
endif
ifneq ($(shell $(CC) -dumpspecs 2>/dev/null | grep -e '[^f]nopie'),)
CFLAGS += -fno-pie -nopie
endif

LDFLAGS = -z max-page-size=4096

$(K_OBJ_DIR)/kernel: $(K_OBJS) $K/kernel.ld $(U_OBJ_DIR)/initcode
	@echo "+ LD $@"
	@mkdir -p $(dir $@)
	$(LD) $(LDFLAGS) -T $K/kernel.ld -o $(K_OBJ_DIR)/kernel $(K_OBJ_DIR)/entry.o $(filter-out $(K_OBJ_DIR)/entry.o,$(K_OBJS)) 
	$(OBJDUMP) -S $(K_OBJ_DIR)/kernel > $(K_OBJ_DIR)/kernel.asm
	$(OBJDUMP) -t $(K_OBJ_DIR)/kernel | sed '1,/SYMBOL TABLE/d; s/ .* / /; /^$$/d' > $(K_OBJ_DIR)/kernel.sym

$(U_OBJ_DIR)/initcode: $U/initcode.S
	@echo "+ CC $@"
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -march=rv64g -nostdinc -I. -Ikernel -c $U/initcode.S -o $(U_OBJ_DIR)/initcode.o
	@echo "+ LD $@"
	$(LD) $(LDFLAGS) -N -e start -Ttext 0 -o $(U_OBJ_DIR)/initcode.out $(U_OBJ_DIR)/initcode.o
	$(OBJCOPY) -S -O binary $(U_OBJ_DIR)/initcode.out $(U_OBJ_DIR)/initcode
	$(OBJDUMP) -S $(U_OBJ_DIR)/initcode.o > $(U_OBJ_DIR)/initcode.asm

tags: $(K_OBJS) _init
	etags *.S *.c

$(BUILD_DIR)/%.o: %.c
	@echo "+ CC $@"
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c -o $@ $<

$(BUILD_DIR)/%.o: %.S
	@echo "+ AS $@"
	@mkdir -p $(dir $@)
	$(CC) -c -o $@ $<

_%: %.o $(U_LIB_OBJS)
	@echo "+ LD $@"
	@mkdir -p $(dir $@)
	$(LD) $(LDFLAGS) -N -e main -Ttext 0 -o $@ $^
	$(OBJDUMP) -S $@ > $*.asm
	$(OBJDUMP) -t $@ | sed '1,/SYMBOL TABLE/d; s/ .* / /; /^$$/d' > $*.sym

$U/usys.S : $U/lib/usys.pl
	perl $U/lib/usys.pl > $U/usys.S

$(U_OBJ_DIR)/usys.o: $U/usys.S
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c -o $@ $<

$U/_forktest: $U/forktest.o $(U_LIB_OBJS)
	# forktest has less library code linked in - needs to be small
	# in order to be able to max out the proc table.
	@echo "+ LD $@"
	$(LD) $(LDFLAGS) -N -e main -Ttext 0 -o $U/_forktest $U/forktest.o $U/ulib.o $U/usys.o
	$(OBJDUMP) -S $U/_forktest > $U/forktest.asm

mkfs/mkfs: mkfs/mkfs.c include/kernel/fs.h include/kernel/param.h
	@echo "+ CC $@"
	gcc -Werror -Wall -Iinclude -o mkfs/mkfs mkfs/mkfs.c --verbose

# Prevent deletion of intermediate files, e.g. cat.o, after first build, so
# that disk image changes after first build are persistent until clean.  More
# details:
# http://www.gnu.org/software/make/manual/html_node/Chained-Rules.html
.PRECIOUS: %.o

fs.img: mkfs/mkfs README $(U_PROGS)
	@echo "+ fs.img"
	mkfs/mkfs fs.img README $(U_PROGS)

-include $(K_OBJ_DIR)/*.d $(U_OBJ_DIR)/*.d

clean: 
	rm -rf \
	$U/usys.S \
	mkfs/mkfs .gdbinit build \

# try to generate a unique GDB port
GDBPORT = $(shell expr `id -u` % 5000 + 25000)
# QEMU's gdb stub command line changed in 0.11
QEMUGDB = $(shell if $(QEMU) -help | grep -q '^-gdb'; \
	then echo "-gdb tcp::$(GDBPORT)"; \
	else echo "-s -p $(GDBPORT)"; fi)
ifndef CPUS
CPUS := 3
endif

QEMUOPTS = -machine virt -bios none -kernel $(K_OBJ_DIR)/kernel -m 128M -smp $(CPUS) -nographic
QEMUOPTS += -drive file=fs.img,if=none,format=raw,id=x0
QEMUOPTS += -device virtio-blk-device,drive=x0,bus=virtio-mmio-bus.0

qemu: $(K_OBJ_DIR)/kernel fs.img
	$(QEMU) $(QEMUOPTS)

.gdbinit: .gdbinit.tmpl-riscv
	sed "s/:1234/:$(GDBPORT)/" < $^ > $@

qemu-gdb: $(K_OBJ_DIR)/kernel .gdbinit fs.img
	@echo "Now run 'gdb' in another window." 1>&2
	$(QEMU) $(QEMUOPTS) -S $(QEMUGDB)

gdb: 
	riscv64-unknown-linux-gnu-gdb
