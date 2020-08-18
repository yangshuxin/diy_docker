CXX_SRCS := $(wildcard src/utils/*.cpp) $(wildcard src/*.cpp) \
    $(wildcard src/image/*.cpp) $(wildcard src/container/*.cpp)

CXX_SRC_DIRS := $(sort $(dir $(CXX_SRCS)))
INC_DIRS := $(patsubst %, -I%, $(CXX_SRC_DIRS))

BUILD_DIR := build
CXX_OBJS := $(patsubst %.cpp, $(BUILD_DIR)/%.o, $(CXX_SRCS))
CXX_DEPS := $(patsubst %.cpp, $(BUILD_DIR)/%.d, $(CXX_SRCS))
CXX_OBJ_DIRS := $(sort $(dir $(CXX_OBJS)))

DEP_FILES := autodep.txt

CXX_FLAGS := -std=c++11 -MMD -O0 -g -Wall #-Werror
CXX_LDFLAGS :=

.PHONY = default build prep

PROGRAM := diy_docker
default: $(PROGRAM)

-include $(DEP_FILES)

$(PROGRAM): $(CXX_OBJS)
	$(CXX) $+ $(CXX_LDFLAGS) -o $@
	@-rm -f $(DEP_FILES)
	$(foreach d, $(CXX_DEPS), cat $d >> $(DEP_FILES);)


$(CXX_OBJS): $(BUILD_DIR)/%.o: %.cpp | prep
	$(CXX) $< $(CXX_FLAGS) -I. -I$(dir $<) $(INC_DIRS) -Ithird_party/install/include -c -o $@

prep:
	@if [ ! -f third_party/json/README.md ]; then \
		echo "please update submodule by 'git submodule update --init --recursive'"; \
		exit 1; \
	fi
	@mkdir -p $(CXX_OBJ_DIRS)

clean:
	rm -rf $(BUILD_DIR) $(DEP_FILES) $(PROGRAM)
