LIB_SRC  := src/elf.cpp src/rv64gc.cpp
TEST_SRC := tests/failures.cpp tests/simulate.cpp
LIB_OBJS  := $(LIB_SRC:%.cpp=obj_dir/%.o)
TEST_OBJS := $(TEST_SRC:%.cpp=obj_dir/%.o)

RISCV_PREFIX ?= riscv64-elf-

CXXFLAGS += -MMD -MP -Wall -Wextra -std=c++20 -Isrc -g

obj_dir/%.o: %.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -c -o $@ $<

obj_dir/tests/%.o: tests/%.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -c -o $@ $<

.PHONY: all check tests clean lint sim
all: obj_dir/failures
-include $(LIB_OBJS:.o=.d) $(TEST_OBJS:.o=.d)

# going to add RTL testbenches here
tests: obj_dir/failures obj_dir/simulate riscv-tests
	./obj_dir/failures
	./obj_dir/simulate

# Options go through SIM_ARGS, since make claims anything starting with a dash
# for itself: make sim SIM_ARGS=--trace riscv-tests/isa/rv64ui-p-add
sim: obj_dir/simulate riscv-tests
	./obj_dir/simulate $(SIM_ARGS) $(filter-out $@,$(MAKECMDGOALS))

obj_dir/%: $(LIB_OBJS) obj_dir/tests/%.o
	@mkdir -p obj_dir
	clang++ $(CXXFLAGS) $^ -o $@

clean:
	rm -rf obj_dir

lint:
	@echo "No RTL yet - nothing to lint."

.PHONY: riscv-tests
riscv-tests:
	make -k -C riscv-tests/isa XLEN=64 RISCV_PREFIX=$(RISCV_PREFIX)

RVCC      := riscv64-elf-gcc
RVOBJCOPY := riscv64-elf-objcopy
RVFLAGS   := -march=rv64gc -mabi=lp64d
