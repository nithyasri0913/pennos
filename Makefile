######################################################################
# PennOS / PennFAT Makefile - Clean Final Version
######################################################################

# Directories
SRC_DIR := src
BIN_DIR := bin
TESTS_DIR := tests

# Compiler settings
CC := clang-15
CXX := clang++-15
CFLAGS := -g3 -gdwarf-4 -pthread -Wall -Werror -Wno-gnu -O0 --std=gnu2x
CXXFLAGS := -g3 -gdwarf-4 -pthread -Wall -Werror -Wno-gnu -O0 --std=gnu++2b
CPPFLAGS := -I $(SRC_DIR)

# Main programs (files with their own main)
PROGS := pennfat pennos

# Test programs
TEST_PROGS := test_p_errno

# All C sources
ALL_SRCS := $(shell find $(SRC_DIR) -type f -name '*.c')

# Main sources
MAIN_SRCS := $(addprefix $(SRC_DIR)/, $(PROGS:=.c))

# Non-main sources (kernel code, utils, etc.)
NON_MAIN_SRCS := $(filter-out $(MAIN_SRCS), $(ALL_SRCS))

# Add stress test sources
STRESS_SRCS := $(TESTS_DIR)/stress.c
NON_MAIN_SRCS += $(STRESS_SRCS)

NON_MAIN_OBJS := $(NON_MAIN_SRCS:.c=.o)

# All headers
HDRS := $(shell find $(SRC_DIR) -type f -name '*.h')

# Default target: build all main programs
all: $(addprefix $(BIN_DIR)/, $(PROGS))

# Build all test programs
tests: $(addprefix $(BIN_DIR)/, $(TEST_PROGS))



# Phony targets
.PHONY: pennfat tests all clean format info install
pennfat: $(BIN_DIR)/pennfat
	@ln -sf $(BIN_DIR)/pennfat pennfat
	@echo "Created symlink: pennfat -> bin/pennfat"

# Build main programs linking non-main objects
$(BIN_DIR)/%: $(SRC_DIR)/%.c $(NON_MAIN_OBJS)
	mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(CPPFLAGS) -o $@ $^

# Build test programs linking non-main objects
$(BIN_DIR)/%: $(TESTS_DIR)/%.c $(NON_MAIN_OBJS)
	mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(CPPFLAGS) -o $@ $^

# Compile all non-main C files to .o
%.o: %.c $(HDRS)
	$(CC) $(CFLAGS) $(CPPFLAGS) -c -o $@ $<

# Compile C++ files if any
%.o: %.cpp $(HDRS)
	$(CXX) $(CXXFLAGS) $(CPPFLAGS) -c -o $@ $<

# Install pennfat globally
install: $(BIN_DIR)/pennfat
	sudo cp $(BIN_DIR)/pennfat /usr/local/bin/pennfat
	@echo "installed to /usr/local/bin/pennfat"

# Clean build artifacts
clean:
	rm -f $(NON_MAIN_OBJS) $(addprefix $(BIN_DIR)/, $(PROGS)) $(addprefix $(BIN_DIR)/, $(TEST_PROGS))

# Debug info
info:
	$(info PROGS: $(PROGS))
	$(info BIN: $(addprefix $(BIN_DIR)/, $(PROGS)))
	$(info TEST_PROGS: $(TEST_PROGS))
	$(info NON_MAIN_OBJS: $(NON_MAIN_OBJS))

# Format all source files
format:
	clang-format -i --verbose --style=Chromium $(ALL_SRCS) $(HDRS)

# Optional: build scheduler test with its own main
bin/scheduler-test: src/kernel/scheduler.c $(NON_MAIN_OBJS)
	mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(CPPFLAGS) -DSCHEDULER_MAIN -o $@ $^
