CXX ?= g++
CXXFLAGS ?= -std=c++17 -O2 -Wall -Wextra -pedantic
CPPFLAGS ?= -Iinclude

TARGET := flash_modle_sim

SRCS := src/cli/main.cpp \
	src/config/config.cpp \
	src/config/loader.cpp \
	src/config/validator.cpp \
	src/core/address.cpp \
	src/core/command.cpp \
	src/core/interface.cpp \
	src/core/registers.cpp \
	src/core/timing.cpp \
	src/core/storage.cpp \
	src/capabilities/capability.cpp \
	src/policies/policy.cpp \
	src/model/model.cpp \
	src/builder/builder.cpp

OBJS := $(SRCS:.cpp=.o)
ADDR_TEST := tests/address_test
REG_TEST := tests/registers_test
TIME_TEST := tests/timing_test
CAP_TEST := tests/capability_test
EVIDENCE_TEST := tests/evidence_test
INTERFACE_TEST := tests/interface_test
COMMAND_TEST := tests/command_test
MODEL_TEST := tests/model_feature_test
ADDR_TEST_SRCS := tests/address_test.cpp \
	src/config/config.cpp \
	src/config/loader.cpp \
	src/core/address.cpp
REG_TEST_SRCS := tests/registers_test.cpp \
	src/config/config.cpp \
	src/config/loader.cpp \
	src/core/registers.cpp
TIME_TEST_SRCS := tests/timing_test.cpp \
	src/core/timing.cpp
CAP_TEST_SRCS := tests/capability_test.cpp \
	src/config/config.cpp \
	src/config/loader.cpp \
	src/capabilities/capability.cpp \
	src/core/address.cpp \
	src/core/command.cpp \
	src/config/validator.cpp
EVIDENCE_TEST_SRCS := tests/evidence_test.cpp \
	src/config/config.cpp \
	src/config/loader.cpp \
	src/core/command.cpp \
	src/config/validator.cpp
INTERFACE_TEST_SRCS := tests/interface_test.cpp \
	src/config/config.cpp \
	src/core/interface.cpp
COMMAND_TEST_SRCS := tests/command_test.cpp \
	src/config/config.cpp \
	src/config/loader.cpp \
	src/core/command.cpp
MODEL_TEST_SRCS := tests/model_feature_test.cpp \
	src/config/config.cpp \
	src/config/loader.cpp \
	src/config/validator.cpp \
	src/core/address.cpp \
	src/core/command.cpp \
	src/core/interface.cpp \
	src/core/registers.cpp \
	src/core/timing.cpp \
	src/core/storage.cpp \
	src/capabilities/capability.cpp \
	src/policies/policy.cpp \
	src/model/model.cpp \
	src/builder/builder.cpp

.PHONY: all clean run-nor run-nor-protect run-nand test

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) $(OBJS) -o $@

%.o: %.cpp
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c $< -o $@

run-nor: $(TARGET)
	./$(TARGET) configs/demo_nor.yaml --self-test

run-nor-protect: $(TARGET)
	./$(TARGET) configs/demo_nor_block_protect.yaml --self-test

run-nand: $(TARGET)
	./$(TARGET) configs/demo_spinand.yaml --self-test

$(ADDR_TEST): $(ADDR_TEST_SRCS)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(ADDR_TEST_SRCS) -o $@

$(REG_TEST): $(REG_TEST_SRCS)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(REG_TEST_SRCS) -o $@

$(TIME_TEST): $(TIME_TEST_SRCS)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(TIME_TEST_SRCS) -o $@

$(CAP_TEST): $(CAP_TEST_SRCS)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(CAP_TEST_SRCS) -o $@

$(EVIDENCE_TEST): $(EVIDENCE_TEST_SRCS)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(EVIDENCE_TEST_SRCS) -o $@

$(INTERFACE_TEST): $(INTERFACE_TEST_SRCS)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(INTERFACE_TEST_SRCS) -o $@

$(COMMAND_TEST): $(COMMAND_TEST_SRCS)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(COMMAND_TEST_SRCS) -o $@

$(MODEL_TEST): $(MODEL_TEST_SRCS)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(MODEL_TEST_SRCS) -o $@

test: $(TARGET) $(ADDR_TEST) $(REG_TEST) $(TIME_TEST) $(CAP_TEST) $(EVIDENCE_TEST) $(INTERFACE_TEST) $(COMMAND_TEST) $(MODEL_TEST)
	./$(ADDR_TEST)
	./$(REG_TEST)
	./$(TIME_TEST)
	./$(CAP_TEST)
	./$(EVIDENCE_TEST)
	./$(INTERFACE_TEST)
	./$(COMMAND_TEST)
	./$(MODEL_TEST)
	sh tests/smoke.sh

clean:
	rm -f $(TARGET) $(OBJS) $(ADDR_TEST) $(REG_TEST) $(TIME_TEST) $(CAP_TEST) $(EVIDENCE_TEST) $(INTERFACE_TEST) $(COMMAND_TEST) $(MODEL_TEST)
