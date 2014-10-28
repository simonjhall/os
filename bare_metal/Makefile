SOURCE_A9_CPP = BaseFS.cpp \
	BlockDevice.cpp \
	clib.cpp \
	Configuration.cpp \
	DSS.cpp \
	Ehci.cpp \
	EhciRootHub.cpp \
	elf.cpp \
	EndPoint.cpp \
	Exceptions.cpp \
	FatFS.cpp \
	GenericInterruptController.cpp \
	GPIO.cpp \
	GpTimer.cpp \
	Hcd.cpp \
	Hub.cpp \
	ICMP.cpp \
	Interface.cpp \
	InterruptController.cpp \
	IoSpace.cpp \
	ipv4.cpp \
	M3Control.cpp \
	Mailbox.cpp \
	main.cpp \
	malloc.cpp \
	MBR.cpp \
	memory.cpp \
	MMCi.cpp \
	Modeline.cpp \
	phys_memory.cpp \
	PL011.cpp \
	PL181.cpp \
	PL190.cpp \
	prcm_dump.cpp \
	print.cpp \
	PrintFb.cpp \
	print_uart.cpp \
	Process.cpp \
	Scheduler.cpp \
	SD.cpp \
	SemiHosting.cpp \
	slip.cpp \
	SP804.cpp \
	Stdio.cpp \
	svc.cpp \
	TTY.cpp \
	UART.cpp \
	UDP.cpp \
	UdpPrinter.cpp \
	Usb.cpp \
	UsbDevice.cpp \
	UsbHub.cpp \
	VectorTable.cpp \
	virt_memory.cpp \
	VirtualFS.cpp \
	WrappedFile.cpp
	
SOURCE_M3_CPP = clib.cpp \
	IoSpace.cpp \
	Mailbox.cpp \
	M3Control.cpp \
	main_m3.cpp \
	malloc.cpp \
	print.cpp \
	slip.cpp \
	UART.cpp

SOURCE_A9_S = asm.S \
	cache.S \
	entry.S \
	exceptions.S
	
SOURCE_M3_S = asm.S \
	entry.S
	
OBJ_A9_CPP := $(addprefix a9/obj/,$(notdir $(SOURCE_A9_CPP:.cpp=.o)))
OBJ_A9_S := $(addprefix a9/obj/,$(notdir $(SOURCE_A9_S:.S=.o)))

OBJ_QEMU_CPP := $(addprefix qemu/obj/,$(SOURCE_A9_CPP:.cpp=.o))
OBJ_QEMU_S := $(addprefix qemu/obj/,$(SOURCE_A9_S:.S=.o))

OBJ_M3_CPP := $(addprefix m3/obj/,$(SOURCE_M3_CPP:.cpp=.o))
OBJ_M3_S := $(addprefix m3/obj/,$(SOURCE_M3_S:.S=.o))

DEPS_A9_CPP := $(OBJ_A9_CPP:.o=.d)
DEPS_QEMU_CPP := $(OBJ_QEMU_CPP:.o=.d)
DEPS_M3_CPP := $(OBJ_M3_CPP:.o=.d)

-include $(DEPS_A9_CPP)
-include $(DEPS_QEMU_CPP)
-include $(DEPS_M3_CPP)

PATH_A9 = 
PATH_M3 = ~/os/build/gcc-simon/bin/

PREFIX_A9 = arm-linux-gnueabihf
PREFIX_M3 = arm-none-eabi
	
CC_A9 = $(PATH_A9)$(PREFIX_A9)-gcc
CXX_A9 = $(PATH_A9)$(PREFIX_A9)-g++
AS_A9 = $(PATH_A9)$(PREFIX_A9)-as

CC_M3 = $(PATH_M3)$(PREFIX_M3)-gcc
CXX_M3 = $(PATH_M3)$(PREFIX_M3)-g++
AS_M3 = $(PATH_M3)$(PREFIX_M3)-as

CXXFLAGS_BASE = -g3 -Wall -c -fmessage-length=0 -std=c++11 -fno-exceptions -fno-rtti -fno-stack-protector
ASFLAGS_BASE = -Wa,--gstabs+,

CXXFLAGS_A9 = -marm -mthumb-interwork -mcpu=cortex-a9 -D__ARM_ARCH_7A__ -O0  $(CXXFLAGS_BASE)
ASFLAGS_A9 = $(ASFLAGS_BASE)-mcpu=cortex-a9,-mfpu=neon,-march=armv7a -DM3=0
LDFLAGS_A9 = -nostartfiles -nodefaultlibs -nostdlib -static -Xlinker --entry=entry_maybe_misaligned

CXXFLAGS_M3 = -mthumb -mcpu=cortex-m3 -D__ARM_ARCH_7M__ -O0  $(CXXFLAGS_BASE)
ASFLAGS_M3 = $(ASFLAGS_BASE)-mcpu=cortex-m3,-march=armv7m -DM3=1
LDFLAGS_M3 = -nostartfiles -nodefaultlibs -nostdlib -L/home/simon/build/gcc-simon/lib/gcc/arm-none-eabi/4.9.1/thumb -L/home/simon/build/gcc-simon/arm-none-eabi/lib/thumb -static -mcpu=cortex-m3 -mthumb -Xlinker --entry=M3_Reset -Xlinker -T link_script_4.7_pbes_m3

############################
.DEFAULT_GOAL = all

all: a9/bin/kernel.elf.bin qemu/bin/kernel.elf.bin

a9/bin/kernel.elf.bin: a9/bin/kernel.elf
	arm-linux-gnueabihf-strip -s $< -o $<.strip
	arm-linux-gnueabihf-objcopy $<.strip -O binary $<.bin -K entry
	mkimage -A arm -C none -O linux -T kernel -d $<.bin -a 82000000 -e 82000040 -n debug uImage
	cp uImage ~/tftpboot

a9/bin/kernel.elf: $(OBJ_A9_CPP) $(OBJ_A9_S) m3/bin/kernel.elf.o
	test -d $(dir $@) || mkdir -p $(dir $@)
	$(CC_A9) $(LDFLAGS_A9) -Xlinker -T link_script_4.7_pbes -o $@ $(OBJ_A9_CPP) $(OBJ_A9_S) m3/bin/kernel.elf.o -lgcc -lstdc++
	
qemu/bin/kernel.elf.bin: qemu/bin/kernel.elf
	arm-linux-gnueabihf-strip -s $< -o $<.strip
	arm-linux-gnueabihf-objcopy $<.strip -O binary $<.bin -K entry
	
qemu/bin/kernel.elf: $(OBJ_QEMU_CPP) $(OBJ_QEMU_S) m3/bin/kernel.elf.o
	test -d $(dir $@) || mkdir -p $(dir $@)
	$(CC_A9) $(LDFLAGS_A9) -Xlinker -T link_script_4.7 -o $@ $(OBJ_QEMU_CPP) $(OBJ_QEMU_S) m3/bin/kernel.elf.o -lgcc -lstdc++
	
m3/bin/kernel.elf: $(OBJ_M3_CPP) $(OBJ_M3_S)
	test -d $(dir $@) || mkdir -p $(dir $@)
	$(CC_M3) $(LDFLAGS_M3) -o $@ $(OBJ_M3_CPP) $(OBJ_M3_S) -lgcc -lstdc++
	
m3/bin/kernel.elf.o: m3/bin/kernel.elf
	arm-linux-gnueabihf-strip -s $< -o $<.strip
	arm-linux-gnueabihf-objcopy $<.strip -O binary $<.bin -K entry
	arm-linux-gnueabihf-objcopy --input binary --output elf32-littlearm --binary-architecture arm $<.bin $<.o

############################
clean:
	$(MAKE) clean_a9
	$(MAKE) clean_m3
	$(MAKE) clean_qemu
	$(MAKE) relink
	
clean_a9:
	rm -f $(OBJ_A9_CPP) $(OBJ_A9_S) $(patsubst %.o,%.d,$(OBJ_A9_CPP))
	
clean_m3:
	rm -f $(OBJ_M3_CPP) $(OBJ_M3_S)  $(patsubst %.o,%.d, $(OBJ_M3_CPP))
	
clean_qemu:
	rm -f $(OBJ_QEMU_CPP) $(OBJ_QEMU_S)  $(patsubst %.o,%.d, $(OBJ_QEMU_CPP))
	
relink:
	rm -f a9/bin/kernel.elf
	rm -f m3/bin/kernel.elf
	rm -f qemu/bin/kernel.elf
	rm -f m3/bin/kernel.elf.o
	rm -f a9/bin/kernel.elf.bin
	rm -f qemu/bin/kernel.elf.bin
	 
############################
a9/obj/%.o: %.cpp
	test -d $(dir $@) || mkdir -p $(dir $@)
	$(CXX_A9) $(CXXFLAGS_A9) -MMD -c -o $@ $< -DPBES 
 
a9/obj/%.o: %.S
	test -d $(dir $@) || mkdir -p $(dir $@)
	$(CC_A9) $(ASFLAGS_A9) -c -o $@ $< -DPBES=1 
	
############################	
m3/obj/%.o: %.cpp
	test -d $(dir $@) || mkdir -p $(dir $@)
	$(CXX_M3) $(CXXFLAGS_M3) -MMD -c -o $@ $< -DPBES 
 
m3/obj/%.o: %.S
	test -d $(dir $@) || mkdir -p $(dir $@)
	$(CC_M3) $(ASFLAGS_M3) -c -o $@ $< -DPBES=1 
	
############################
qemu/obj/%.o: %.cpp
	test -d $(dir $@) || mkdir -p $(dir $@)
	$(CXX_A9) $(CXXFLAGS_A9) -MMD -c -o $@ $<
 
qemu/obj/%.o: %.S
	test -d $(dir $@) || mkdir -p $(dir $@)
	$(CC_A9) $(ASFLAGS_A9) -c -o $@ $< -DPBES=0

	
#/home/simon/os/qemu/arm-softmmu/qemu-system-arm -kernel qemu/bin/kernel.elf.bin -M versatilepb -m 128M -s -cpu cortex-a8 -nographic -sd /home/simon/os/git/os/bare_metal/filesystem -serial stdio -usb
