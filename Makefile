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

# Compiler that produced the original executable: only GCC 2.8.1 reproduces
# ns.c (it splits symbol addresses into registers; 2.7.2 does not).
COMPILER      ?= gcc-2.8.1-psx
ASPSX_VERSION ?= 2.56
# The game code reads small variables through $gp (-G8); the PSY-Q libraries do not.
GP_SIZE       ?= 8

TOOLCHAIN := tools/toolchain
PYTHON    := $(if $(wildcard .venv/bin/python3),.venv/bin/python3,python3)
CROSS     := mipsel-linux-gnu-
AS        := $(CROSS)as
LD        := $(CROSS)ld
OBJCOPY   := $(CROSS)objcopy
CPP       := cpp
CC1        = $(TOOLCHAIN)/$(COMPILER)/cc1
MASPSX    := $(PYTHON) $(TOOLCHAIN)/maspsx/maspsx.py
SPLAT     := $(PYTHON) -m splat split
M2C       := $(PYTHON) $(TOOLCHAIN)/m2c/m2c.py
DIFF      := $(PYTHON) $(TOOLCHAIN)/asm-differ/diff.py

VERSION_UPPER := $(shell echo $(VERSION) | tr a-z A-Z)
CPPFLAGS  := -undef -nostdinc -Iinclude -DVERSION_$(VERSION_UPPER) -DTARGET_PSX
# NM_FILES="camera gfx" limits NON_MATCHING=1 to those source files (to find
# which one breaks the game).
NM_FILES ?=
ifneq ($(NON_MATCHING),0)
ifeq ($(NM_FILES),)
CPPFLAGS  += -DNON_MATCHING
endif
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

# NON_MATCHING builds keep every function at its original address, since the
# overlays on the disc call them there (see tools/nm_place.py).
ifneq ($(NON_MATCHING),0)
NM_PLACE = | $(PYTHON) tools/nm_place.py $(notdir $*) config/$(VERSION)/nm_too_long.txt
endif

# NON_MATCHING builds link with every segment at its original address (see
# tools/pad_ld.py), so that the game still runs.
ifneq ($(NON_MATCHING),0)
LINK_SCRIPT := $(BUILD)/main_nm.ld
else
LINK_SCRIPT := $(LD_SCRIPT)
endif

LDFLAGS := -EL -nostdlib --no-check-sections -Map $(BUILD)/main.map \
           -T $(LINK_SCRIPT) \
           -T $(ASM_DIR)/undefined_syms_auto.txt \
           -T $(ASM_DIR)/undefined_funcs_auto.txt \
           -T config/$(VERSION)/undefined_syms.txt

# The linker script written by splat is the list of everything that gets linked.
OBJECTS := $(if $(wildcard $(LD_SCRIPT)),$(shell grep -oE '$(BUILD)/[^ ()]+\.o' $(LD_SCRIPT) | sort -u))

# ---- Build -------------------------------------------------------------------

.PHONY: all build check setup init extract clean distclean progress expected ctx decompile diff test pc

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

$(ELF): $(OBJECTS) $(LINK_SCRIPT) config/$(VERSION)/undefined_syms.txt
	$(LD) $(LDFLAGS) -o $@

$(BUILD)/main_nm.ld: $(LD_SCRIPT) $(CONFIG) tools/pad_ld.py
	@mkdir -p $(BUILD)
	$(PYTHON) tools/pad_ld.py $(CONFIG) $(LD_SCRIPT) $@

$(LD_SCRIPT):
	@echo "$(LD_SCRIPT) не найден: сначала выполните 'make extract'"
	@exit 1

$(BUILD)/%.c.o: %.c
	@mkdir -p $(@D)
	$(CPP) $(CPPFLAGS) -MMD -MP -MT $@ -MF $@.d $< | $(CC1) $(CC1FLAGS) | $(PYTHON) tools/comm_globl.py | $(MASPSX) $(MASPSXFLAGS) $(NM_PLACE) | $(AS) $(ASFLAGS) -o $@

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
	@# The entry point and $$gp follow the code and data, so that a build whose
	@# code differs in size (NON_MATCHING=1) still starts.
	sed -i -E 's/^(\s*_gp = )0x[0-9A-Fa-f]+;/\1main_SDATA_START;/' $(LD_SCRIPT)
	sed -i -E 's/^\.word 0x[0-9A-F]+( +\/\* Initial PC \*\/)/.word __SN_ENTRY_POINT\1/' asm/us/header.s
	@# The heap starts right after .bss (the startup code clears .bss up to it).
	sed -i -E 's/^D_8006EA78 = 0x8006EA78;/D_8006EA78 = main_BSS_END;/' $(ASM_DIR)/undefined_syms_auto.txt
	@# The C objects include the asm of their functions (INCLUDE_ASM), which the
	@# dependency files do not track: rebuild them from the new asm.
	rm -rf $(BUILD)/src

$(CONFIG):
	@echo "$(CONFIG) не найден: положите $(EXE_NAME) в disc/ и выполните 'make init'"
	@exit 1

clean:
	rm -rf $(BUILD)

distclean:
	rm -rf build asm assets expected ctx.c

# ---- PC port -------------------------------------------------------------------

# The PC port (32-bit x86, needs gcc-multilib): build/pc/crash3. It takes the
# addresses of the variables from the PlayStation build, so that comes first.
pc: build
	python3 tools/pc/build.py

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
	@file=$$(find asm/$(VERSION)/nonmatchings asm/$(VERSION)/matchings -name '$(FUNC).s' | head -n 1); \
	if [ -z "$$file" ]; then \
		file=$$(grep -l "^glabel $(FUNC)$$" asm/$(VERSION)/*.s | head -n 1); \
	fi; \
	test -n "$$file" || { echo "$(FUNC) не найдена в asm/$(VERSION)"; exit 1; }; \
	$(M2C) --target mipsel-gcc-c --context ctx.c -f $(FUNC) $$file

# Compare a function with the original: make diff FUNC=func_80012345
diff:
	@test -n "$(FUNC)" || (echo "укажите функцию: make diff FUNC=func_80012345"; exit 1)
	VERSION=$(VERSION) $(DIFF) -mwo $(FUNC)

test:
	$(PYTHON) -m unittest discover -s tests

ifneq ($(NON_MATCHING),0)
$(foreach f,$(NM_FILES),$(eval $(BUILD)/src/$(f).c.o: CPPFLAGS += -DNON_MATCHING))
endif
