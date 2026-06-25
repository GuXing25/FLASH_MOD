CXX ?= g++
CXXFLAGS ?= -std=c++17 -O2 -Wall -Wextra -pedantic
CPPFLAGS ?= -Iinclude

TARGET := flash_modle_sim

SRCS := src/main.cpp \
	src/config.cpp \
	src/config_loader.cpp \
	src/validator.cpp \
	src/address_mapper.cpp \
	src/register_engine.cpp \
	src/timing_engine.cpp \
	src/capability_module.cpp \
	src/device_policy.cpp \
	src/storage_backend.cpp \
	src/flash_model.cpp \
	src/model_builder.cpp

OBJS := $(SRCS:.cpp=.o)
ADDRESS_MAPPER_TEST := tests/address_mapper_test
REGISTER_ENGINE_TEST := tests/register_engine_test
TIMING_ENGINE_TEST := tests/timing_engine_test
ADDRESS_MAPPER_TEST_SRCS := tests/address_mapper_test.cpp \
	src/config.cpp \
	src/config_loader.cpp \
	src/address_mapper.cpp
REGISTER_ENGINE_TEST_SRCS := tests/register_engine_test.cpp \
	src/config.cpp \
	src/config_loader.cpp \
	src/register_engine.cpp
TIMING_ENGINE_TEST_SRCS := tests/timing_engine_test.cpp \
	src/timing_engine.cpp

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

$(ADDRESS_MAPPER_TEST): $(ADDRESS_MAPPER_TEST_SRCS)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(ADDRESS_MAPPER_TEST_SRCS) -o $@

$(REGISTER_ENGINE_TEST): $(REGISTER_ENGINE_TEST_SRCS)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(REGISTER_ENGINE_TEST_SRCS) -o $@

$(TIMING_ENGINE_TEST): $(TIMING_ENGINE_TEST_SRCS)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(TIMING_ENGINE_TEST_SRCS) -o $@

test: $(TARGET) $(ADDRESS_MAPPER_TEST) $(REGISTER_ENGINE_TEST) $(TIMING_ENGINE_TEST)
	./$(ADDRESS_MAPPER_TEST)
	./$(REGISTER_ENGINE_TEST)
	./$(TIMING_ENGINE_TEST)
	sh tests/smoke.sh

clean:
	rm -f $(TARGET) $(OBJS) $(ADDRESS_MAPPER_TEST) $(REGISTER_ENGINE_TEST) $(TIMING_ENGINE_TEST)
