# .SILENT:
K=kernel
U=user
MEMORY=128

TOOLPREFIX = riscv64-unknown-elf-
FS_IMG = build/fs.img

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
AS = $(TOOLPREFIX)gcc
LD = $(TOOLPREFIX)ld
OBJCOPY = $(TOOLPREFIX)objcopy
OBJDUMP = $(TOOLPREFIX)objdump

CFLAGS = -Wall -Werror -O0 -fno-omit-frame-pointer -ggdb3
CFLAGS += -MD -Wno-infinite-recursion
CFLAGS += -DMEMORY_SIZE_MEGABYTES=$(MEMORY)
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

LDFLAGS = -z max-page-size=4096 --no-warn-rwx-segments 

$(K_OBJ_DIR)/kernel: $(K_OBJS) $K/kernel.ld $(U_OBJ_DIR)/initcode
	@echo "$(ANSI_FG_CYAN)+ LD $(ANSI_NONE)$@"
	@mkdir -p $(dir $@)
	$(LD) $(LDFLAGS) -T $K/kernel.ld -o $(K_OBJ_DIR)/kernel $(K_OBJ_DIR)/entry.o $(filter-out $(K_OBJ_DIR)/entry.o,$(K_OBJS)) 
	$(OBJDUMP) -S $(K_OBJ_DIR)/kernel > $(K_OBJ_DIR)/kernel.asm
	$(OBJDUMP) -t $(K_OBJ_DIR)/kernel | sed '1,/SYMBOL TABLE/d; s/ .* / /; /^$$/d' > $(K_OBJ_DIR)/kernel.sym

$(U_OBJ_DIR)/initcode: $U/initcode.S
	@echo "$(ANSI_FG_GREEN)+ CC $(ANSI_NONE)$@"
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -march=rv64g -nostdinc -I. -Ikernel -c $U/initcode.S -o $(U_OBJ_DIR)/initcode.o
	@echo "$(ANSI_FG_GREEN)+ LD $(ANSI_NONE)$@"
	$(LD) $(LDFLAGS) -N -e start -Ttext 0 -o $(U_OBJ_DIR)/initcode.out $(U_OBJ_DIR)/initcode.o
	$(OBJCOPY) -S -O binary $(U_OBJ_DIR)/initcode.out $(U_OBJ_DIR)/initcode
	$(OBJDUMP) -S $(U_OBJ_DIR)/initcode.o > $(U_OBJ_DIR)/initcode.asm

$(BUILD_DIR)/%.o: %.c
	@echo "$(ANSI_FG_GREEN)+ CC $(ANSI_NONE)$@"
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c -o $@ $<

$(BUILD_DIR)/%.o: %.S
	@echo "$(ANSI_FG_GREEN)+ AS $(ANSI_NONE)$@"
	@mkdir -p $(dir $@)
	$(CC) -c -o $@ $<

_%: %.o $(U_LIB_OBJS)
	@echo "$(ANSI_FG_GREEN)+ LD $(ANSI_NONE)$@"
	@mkdir -p $(dir $@)
	$(LD) $(LDFLAGS) -N -e main -o $@ $^
	$(OBJDUMP) -S $@ > $*.asm
	$(OBJDUMP) -t $@ | sed '1,/SYMBOL TABLE/d; s/ .* / /; /^$$/d' > $*.sym

$(U_OBJ_DIR)/usys.o: $U/usys.S
	@echo "$(ANSI_FG_GREEN)+ AS $(ANSI_NONE)$@"
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c -o $@ $<

$U/_forktest: $U/forktest.o $(U_LIB_OBJS)
	# forktest has less library code linked in - needs to be small
	# in order to be able to max out the proc table.
	@echo "$(ANSI_FG_GREEN)+ LD $(ANSI_NONE)$@"
	$(LD) $(LDFLAGS) -N -e main -o $U/_forktest $U/forktest.o $U/ulib.o $U/usys.o
	$(OBJDUMP) -S $U/_forktest > $U/forktest.asm

build/mkfs: mkfs/mkfs.c include/kernel/fs.h include/kernel/param.h
	@echo "$(ANSI_FG_GREEN)+ CC $(ANSI_NONE)$@"
	gcc -Werror -Wall -Iinclude -o $@ mkfs/mkfs.c

$(FS_IMG): build/mkfs README $(U_PROGS)
	@echo "$(ANSI_FG_GREEN)+ $@ $(ANSI_NONE)"
	build/mkfs $@ README $(U_PROGS)

-include $(K_OBJ_DIR)/*.d $(U_OBJ_DIR)/*.d

# try to generate a unique GDB port
GDBPORT = $(shell expr `id -u` % 5000 + 25000)
# QEMU's gdb stub command line changed in 0.11
QEMUGDB = $(shell if $(QEMU) -help | grep -q '^-gdb'; \
	then echo "-gdb tcp::$(GDBPORT)"; \
	else echo "-s -p $(GDBPORT)"; fi)
ifndef CPUS
CPUS := 3
endif

QEMUOPTS = -machine virt -bios none -kernel $(K_OBJ_DIR)/kernel -m $(MEMORY)M -smp $(CPUS) -nographic
QEMUOPTS += -drive file=$(FS_IMG),if=none,format=raw,id=x0
QEMUOPTS += -device virtio-blk-device,drive=x0,bus=virtio-mmio-bus.0

qemu: $(K_OBJ_DIR)/kernel $(FS_IMG)
	$(QEMU) $(QEMUOPTS)

qemu-gdb: $(K_OBJ_DIR)/kernel $(FS_IMG)
	@echo "Now run 'gdb' in another window." 1>&2
	$(QEMU) $(QEMUOPTS) -S $(QEMUGDB)

all: $(K_OBJ_DIR)/kernel $(FS_IMG)
.DEFAULT_GOAL := all

clean: 
	rm -rf mkfs/mkfs build

tags: $(K_OBJS) _init
	etags *.S *.c

ANSI_FG_BLACK   = \033[1;30m
ANSI_FG_RED     = \033[1;31m
ANSI_FG_GREEN   = \033[1;32m
ANSI_FG_YELLOW  = \033[1;33m
ANSI_FG_BLUE    = \033[1;34m
ANSI_FG_MAGENTA = \033[1;35m
ANSI_FG_CYAN    = \033[1;36m
ANSI_FG_WHITE   = \033[1;37m
ANSI_BG_BLACK   = \033[1;40m
ANSI_BG_RED     = \033[1;41m
ANSI_BG_GREEN   = \033[1;42m
ANSI_BG_YELLOW  = \033[1;43m
ANSI_BG_BLUE    = \033[1;44m
ANSI_BG_MAGENTA = \033[1;35m
ANSI_BG_CYAN    = \033[1;46m
ANSI_BG_WHITE   = \033[1;47m
ANSI_NONE       = \033[0m