# C++23 with Modules
CXX      := g++
CXX_STD  := --std=c++23
MOD_FLAG := -fmodules-ts

MODE      ?= debug
TARGET    ?=
# Default as many wayland users have XWayland
WINDOWING ?= x11

# TODO: set OS target
VALID_TARGETS := inspector reconstructor

ifeq ($(filter clean,$(MAKECMDGOALS)),)
    ifeq ($(filter $(TARGET),$(VALID_TARGETS)),)
        $(error TARGET must be one of: $(VALID_TARGETS))
    endif
endif

# Easier testing command
ifeq ($(MAKECMDGOALS),test)
    MODE := test
endif

BUILD_DIR := build/$(MODE)
LIB_DIR   := lib

INC_DIRS := $(shell find $(TARGET) -type d) $(LIB_DIR)

# Exclude .test.cpp and main.cpp for SRCS
ifeq ($(MODE),test)
    SRCS_CPPM := $(shell find $(TARGET) $(LIB_DIR) -name '*.cppm')
    SRCS_CPP  := $(shell find $(TARGET) $(LIB_DIR) -name '*.cpp' ! -name 'main.cpp')
else
    SRCS_CPPM := $(shell find $(TARGET) $(LIB_DIR) -name '*.cppm')
    SRCS_CPP  := $(shell find $(TARGET) $(LIB_DIR) -name '*.cpp' ! -name '*.test.cpp' ! -name 'main.cpp')
endif

MAIN_SRC := $(TARGET)/main.cpp

OBJS_CPPM := $(SRCS_CPPM:%=$(BUILD_DIR)/%.o)
OBJS_CPP  := $(SRCS_CPP:%=$(BUILD_DIR)/%.o)
OBJS_MAIN := $(BUILD_DIR)/$(MAIN_SRC).o

OBJS := $(OBJS_CPPM) $(OBJS_CPP) $(OBJS_MAIN)
DEPS := $(OBJS:.o=.d)

CXXFLAGS := $(CXX_STD) $(MOD_FLAG)
CXXFLAGS += $(addprefix -I,$(INC_DIRS))
# CXXFLAGS += -MMD -MP

LIBS :=

ifeq ($(WINDOWING), x11)
    CXXFLAGS += -DPLATFORM_LINUX_X11
    LIBS     += -lX11
else ifeq ($(WINDOWING), wayland)
    CXXFLAGS += -DPLATFORM_LINUX_WAYLAND
else
    $(error Unknown WINDOWING option: $(WINDOWING). Choose 'x11' or 'wayland')
endif

WARN_FLAGS    := -Wall -Wextra -Werror
DBG_FLAGS     := -ggdb -O0 -fsanitize=address
RELEASE_FLAGS := -Ofast -flto -march=native -DNDEBUG

LDFLAGS :=

ifeq ($(filter $(MODE),debug test),$(MODE))
    CXXFLAGS += $(WARN_FLAGS) $(DBG_FLAGS)
    LDFLAGS  += -fsanitize=address
else
    CXXFLAGS += $(RELEASE_FLAGS)
endif

BIN := $(BUILD_DIR)/$(TARGET)/$(TARGET)

$(BIN): $(OBJS)
	mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) $(OBJS) -o $@ $(LIBS) $(LDFLAGS)

# Modules need to be built in a specific order to work
$(BUILD_DIR)/inspector/core/inspector.cppm.o: $(BUILD_DIR)/inspector/platform/platform.cppm.o $(BUILD_DIR)/lib/types.cppm.o
$(BUILD_DIR)/inspector/platform/platform.cppm.o: $(BUILD_DIR)/lib/types.cppm.o

$(OBJS_CPP) $(OBJS_MAIN): $(OBJS_CPPM)

$(BUILD_DIR)/%.cppm.o: %.cppm
	mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(BUILD_DIR)/%.cpp.o: %.cpp
	mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -c $< -o $@

# System headers to pre-compile as modules
SYS_HEADERS := cstdint cstdio cassert cstdlib cstring

.PHONY: clean test setup

setup:
	@echo "Pre-compiling system headers..."
	@mkdir -p gcm.cache
	@for hdr in $(SYS_HEADERS); do \
		$(CXX) $(CXXFLAGS) -xc++-system-header $$hdr; \
	done

$(OBJS): | setup

test: $(BIN)
	./$(BIN)

clean:
	rm -rf build gcm.cache

-include $(DEPS)