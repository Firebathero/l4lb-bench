# Builds ch_bench against the upstream sources in repos/ unmodified.
# Requires g++ and gcc. Run from harness/run/.
#
#   make            build
#   make run        build and write results to ../results/ch_bench.csv
#   make clean

ROOT     := ../..
REPOS    := $(ROOT)/repos
KATRAN   := $(REPOS)/katran/katran/lib
ANCHOR   := $(REPOS)/anchorhash-cpp
CONHASH  := $(REPOS)/dpvs/src/ipvs/libconhash
BUILD    := build

# shim/ must precede nothing else; it only supplies dpdk.h for libconhash.
CXXFLAGS := -O2 -std=c++17 -msse4.2 -Wall -Wno-sign-compare \
            -I$(REPOS)/katran -I$(ANCHOR) -I$(CONHASH) -Ishim
CFLAGS   := -O2 -msse4.2 -Wall -I$(CONHASH) -Ishim

KATRAN_SRC := $(KATRAN)/CHHelpers.cpp $(KATRAN)/MaglevBase.cpp \
              $(KATRAN)/MaglevHash.cpp $(KATRAN)/MaglevHashV2.cpp \
              $(KATRAN)/MurmurHash3.cpp
ANCHOR_SRC := $(ANCHOR)/AnchorHashQre.cpp
CONHASH_SRC := $(CONHASH)/conhash.c $(CONHASH)/conhash_inter.c \
               $(CONHASH)/conhash_util.c $(CONHASH)/md5.c \
               $(CONHASH)/util_rbtree.c

CXX_OBJS := $(BUILD)/ch_bench.o \
            $(patsubst %,$(BUILD)/%.o,$(notdir $(basename $(KATRAN_SRC) $(ANCHOR_SRC))))
C_OBJS   := $(patsubst %,$(BUILD)/%.o,$(notdir $(basename $(CONHASH_SRC))))

.PHONY: all run clean
all: ch_bench

ch_bench: $(CXX_OBJS) $(C_OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $^

$(BUILD):
	mkdir -p $(BUILD)

$(BUILD)/ch_bench.o: ch_bench.cpp | $(BUILD)
	$(CXX) $(CXXFLAGS) -c $< -o $@

define KATRAN_RULE
$(BUILD)/$(notdir $(basename $(1))).o: $(1) | $(BUILD)
	$$(CXX) $$(CXXFLAGS) -c $$< -o $$@
endef
$(foreach s,$(KATRAN_SRC) $(ANCHOR_SRC),$(eval $(call KATRAN_RULE,$(s))))

define CONHASH_RULE
$(BUILD)/$(notdir $(basename $(1))).o: $(1) | $(BUILD)
	$$(CC) $$(CFLAGS) -c $$< -o $$@
endef
$(foreach s,$(CONHASH_SRC),$(eval $(call CONHASH_RULE,$(s))))

run: ch_bench
	mkdir -p ../results
	./ch_bench 1000000 > ../results/ch_bench.csv
	@echo "wrote ../results/ch_bench.csv"

clean:
	rm -rf $(BUILD) ch_bench
