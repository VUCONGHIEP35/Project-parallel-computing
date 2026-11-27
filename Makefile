# Makefile - do an Histogram song song (OpenMP)
#   make          build ban release        -> build/release/
#   make test     build + chay test voi 1, 2, 4, 8 thread
#   make debug    test voi AddressSanitizer/UBSan (chi WSL2/Linux)
#   make tsan     test voi ThreadSanitizer (can clang + libomp; vd make tsan TSAN_CC=clang-18)
#   make flags    in co bien dich (ghi vao bao cao)
#   make clean

CC   ?= gcc
MODE ?= release

ifeq ($(OS),Windows_NT)
EXE := .exe
else
EXE :=
endif

ifeq ($(MODE),release)
OPT := -O3 -march=native
else ifeq ($(MODE),debug)
OPT := -O0 -g -fsanitize=address,undefined -fno-omit-frame-pointer
else ifeq ($(MODE),tsan)
OPT := -O1 -g -fsanitize=thread -fno-omit-frame-pointer
else
$(error MODE phai la release, debug hoac tsan)
endif

CFLAGS  := -std=c11 $(OPT) -fopenmp -Wall -Wextra -Iinclude
LDFLAGS := $(OPT) -fopenmp
LDLIBS  := -lm

BUILD := build/$(MODE)
SRC   := $(shell find src -name '*.c' 2>/dev/null)
OBJ   := $(SRC:%.c=$(BUILD)/%.o)
APPS  := $(patsubst apps/%.c,$(BUILD)/bin/%$(EXE),$(wildcard apps/*.c))
TESTS := $(patsubst tests/%.c,$(BUILD)/tests/%$(EXE),$(wildcard tests/*.c))

.PHONY: all test debug tsan flags clean
all: $(APPS) $(TESTS)

$(BUILD)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -MMD -MP -c $< -o $@

$(BUILD)/bin/%$(EXE): apps/%.c $(OBJ)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $< $(OBJ) -o $@ $(LDFLAGS) $(LDLIBS)

$(BUILD)/tests/%$(EXE): tests/%.c $(OBJ)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $< $(OBJ) -o $@ $(LDFLAGS) $(LDLIBS)

test: $(TESTS)
	@bash tests/run_tests.sh $(BUILD)/tests

debug:
	$(MAKE) MODE=debug test

# TSan khong hieu cac diem dong bo ben trong runtime OpenMP (barrier, reduction) -> bao race gia.
# Archer (libarcher.so, di kem libomp cua LLVM) bao cho TSan cac diem do; tim thay thi nap qua OMPT.
TSAN_CC ?= clang
ARCHER  ?= $(firstword $(wildcard /usr/lib/llvm-*/lib/libarcher.so))
tsan:
	OMP_TOOL_LIBRARIES="$(ARCHER)" TSAN_OPTIONS="halt_on_error=1 second_deadlock_stack=1" \
	    $(MAKE) MODE=tsan CC=$(TSAN_CC) test

flags:
	@echo "CC=$(CC)"; $(CC) --version | head -1; echo "CFLAGS=$(CFLAGS)"

clean:
	rm -rf build

-include $(OBJ:.o=.d)
