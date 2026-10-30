# Crash Bandicoot 3: Warped — decompilation.
#
#   make setup       install the toolchain (tools/setup.sh)
#   make init        create config/$(VERSION)/splat.yaml from disc/<exe> (once)
#   make extract     split the executable into asm/ and src/ stubs
#   make             build and check that the result is byte-identical
#   make progress    show how much of the code is decompiled
#
# See docs/decompiling.md for the full workflow.

VERSION ?= us
NON_MATCHING ?= 0

SHELL := /bin/bash
.SHELLFLAGS := -o pipefail -c
# Never keep a half-written object after a failed compiler pipeline.
.DELETE_ON_ERROR:

# ---- Toolchain ---------------------------------------------------------------

# Compiler and assembler that produced the original executable. To be confirmed
# by matching the first functions; switch with e.g. `make COMPILER=gcc-2.8.1-psx`.
COMPILER      ?= gcc-2.7.2-psx
ASPSX_VERSION ?= 2.56
# -G value (small data accessed through $gp); 0 until the executable says otherwise.
GP_SIZE       ?= 0

TOOLCHAIN := tools/toolchain
PYTHON    := $(if $(wildcard .venv/bin/python3),.venv/bin/python3,python3)
CROSS     := mipsel-linux-gnu-
AS        := $(CROSS)as
LD        := $(CROSS)ld
OBJCOPY   := $(CROSS)objcopy
CPP       := cpp
CC1       := $(TOOLCHAIN)/$(COMPILER)/cc1
MASPSX    := $(PYTHON) $(TOOLCHAIN)/maspsx/maspsx.py
SPLAT     := $(PYTHON) -m splat split
M2C       := $(PYTHON) $(TOOLCHAIN)/m2c/m2c.py
DIFF      := $(PYTHON) $(TOOLCHAIN)/asm-differ/diff.py

VERSION_UPPER := $(shell echo $(VERSION) | tr a-z A-Z)
CPPFLAGS  := -undef -nostdinc -Iinclude -DVERSION_$(VERSION_UPPER) -DTARGET_PSX
ifneq ($(NON_MATCHING),0)
CPPFLAGS  += -DNON_MATCHING
endif
# -funsigned-char follows the PSY-Q convention used by other PS1 decompilations.
CC1FLAGS  := -quiet -O2 -G$(GP_SIZE) -g -mips1 -mcpu=3000 -mgas -msoft-float \
             -mno-abicalls -fgnu-linker -funsigned-char
MASPSXFLAGS := --aspsx-version=$(ASPSX_VERSION) --expand-div \
               $(if $(filter-out 0,$(GP_SIZE)),-G$(GP_SIZE))
ASFLAGS   := -EL -Iinclude -march=r3000 -mtune=r3000 -no-pad-sections -G0 -msoft-float

# ---- Paths -------------------------------------------------------------------

EXE_NAME  := $(shell $(PYTHON) tools/versions.py $(VERSION) exe)
CONFIG    := config/$(VERSION)/splat.yaml
BUILD     := build/$(VERSION)
ASM_DIR   := asm/$(VERSION)
LD_SCRIPT := $(ASM_DIR)/main.ld
ELF       := $(BUILD)/main.elf
TARGET    := $(BUILD)/$(EXE_NAME)

LDFLAGS := -EL -nostdlib --no-check-sections -Map $(BUILD)/main.map \
           -T $(LD_SCRIPT) \
           -T $(ASM_DIR)/undefined_syms_auto.txt \
           -T $(ASM_DIR)/undefined_funcs_auto.txt \
           -T config/$(VERSION)/undefined_syms.txt

# The linker script written by splat is the list of everything that gets linked.
OBJECTS := $(if $(wildcard $(LD_SCRIPT)),$(shell grep -oE '$(BUILD)/[^ ()]+\.o' $(LD_SCRIPT) | sort -u))

# ---- Build -------------------------------------------------------------------

.PHONY: all build check setup init extract clean distclean progress expected ctx decompile diff test

all: check

build: $(TARGET)

check: $(TARGET)
	@sha1sum --quiet -c config/$(VERSION)/checksum.sha1 \
		&& echo "$(TARGET): OK, совпадает с оригиналом" \
		|| (echo "$(TARGET): НЕ совпадает с оригиналом"; exit 1)

# PS-EXE files are padded with zeros to a multiple of 0x800 bytes.
$(TARGET): $(ELF)
	$(OBJCOPY) -O binary $< $@
	truncate -s %2048 $@

$(ELF): $(OBJECTS) $(LD_SCRIPT) config/$(VERSION)/undefined_syms.txt
	$(LD) $(LDFLAGS) -o $@

$(LD_SCRIPT):
	@echo "$(LD_SCRIPT) не найден: сначала выполните 'make extract'"
	@exit 1

$(BUILD)/%.c.o: %.c
	@mkdir -p $(@D)
	$(CPP) $(CPPFLAGS) -MMD -MP -MT $@ -MF $@.d $< | $(CC1) $(CC1FLAGS) | $(MASPSX) $(MASPSXFLAGS) | $(AS) $(ASFLAGS) -o $@

$(BUILD)/%.s.o: %.s
	@mkdir -p $(@D)
	$(AS) $(ASFLAGS) -o $@ $<

$(BUILD)/%.bin.o: %.bin
	@mkdir -p $(@D)
	$(LD) -EL -r -b binary -o $@ $<

-include $(OBJECTS:.o=.o.d)

# ---- Project setup -----------------------------------------------------------

setup:
	tools/setup.sh

init:
	$(PYTHON) tools/gen_config.py --version $(VERSION)

extract: $(CONFIG)
	$(SPLAT) $(CONFIG)
	@# GNU as gives .bss a 16-byte alignment, so ld would move the NOLOAD bss
	@# section; the PSY-Q linker put it right after the data.
	sed -i -E 's/^(\s*\.\w+_bss) \(NOLOAD\) :/\1 . (NOLOAD) :/' $(LD_SCRIPT)

$(CONFIG):
	@echo "$(CONFIG) не найден: положите $(EXE_NAME) в disc/ и выполните 'make init'"
	@exit 1

clean:
	rm -rf $(BUILD)

distclean:
	rm -rf build asm assets expected ctx.c

# ---- Decompilation helpers ---------------------------------------------------

progress:
	@$(PYTHON) tools/progress.py --version $(VERSION)

# Snapshot of a matching build, used by asm-differ as the reference.
expected: check
	rm -rf expected/$(BUILD)
	mkdir -p expected/$(BUILD)
	cp -r $(BUILD)/. expected/$(BUILD)/

# Context file for m2c and decomp.me: all project headers, preprocessed.
ctx:
	$(PYTHON) tools/m2ctx.py

# First draft of a function in C: make decompile FUNC=func_80012345
decompile: ctx
	@test -n "$(FUNC)" || (echo "укажите функцию: make decompile FUNC=func_80012345"; exit 1)
	$(M2C) --target mipsel-gcc-c --context ctx.c \
		$$(find asm/$(VERSION)/nonmatchings -name '$(FUNC).s' | head -n 1)

# Compare a function with the original: make diff FUNC=func_80012345
diff:
	@test -n "$(FUNC)" || (echo "укажите функцию: make diff FUNC=func_80012345"; exit 1)
	VERSION=$(VERSION) $(DIFF) -mwo $(FUNC)

test:
	$(PYTHON) -m unittest discover -s tests
