# Builds katran's XDP datapath from repos/ unmodified, plus a userspace driver
# that loads it and times it with BPF_PROG_TEST_RUN.
#
#   make           build both
#   sudo make run  load, populate maps, measure (BPF needs root here)
#   make clean

ROOT    := ../../..
KATRAN  := $(ROOT)/repos/katran
BUILD   := build

CLANG   ?= clang
CC      ?= gcc

# -D__x86_64__ is required: with -target bpf clang does not define it, and
# glibc's gnu/stubs.h then selects the 32-bit variant, which is not installed.
BPF_CFLAGS := -O2 -g -target bpf -D__KERNEL__ -D__x86_64__ \
              -Wno-unused-value -Wno-pointer-sign \
              -Wno-compare-distinct-pointer-types -Wno-address-of-packed-member \
              -I$(KATRAN) -I/usr/include/x86_64-linux-gnu

CFLAGS  := -O2 -Wall -I$(KATRAN)
LDLIBS  := -lbpf

.PHONY: all run clean
all: $(BUILD)/balancer.bpf.o xdp_bench

$(BUILD):
	mkdir -p $(BUILD)

$(BUILD)/balancer.bpf.o: $(KATRAN)/katran/lib/bpf/balancer.bpf.c | $(BUILD)
	$(CLANG) $(BPF_CFLAGS) -c $< -o $@

xdp_bench: xdp_bench.c | $(BUILD)
	$(CC) $(CFLAGS) -o $@ $< $(LDLIBS)

run: all
	./xdp_bench $(BUILD)/balancer.bpf.o 20000

clean:
	rm -rf $(BUILD) xdp_bench
