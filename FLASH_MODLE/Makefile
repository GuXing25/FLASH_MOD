CXX ?= g++
CXXFLAGS ?= -std=c++17 -O2 -Wall -Wextra -pedantic
CPPFLAGS ?= -Iinclude

# 对外可执行文件使用正确的 flash_model 拼写。
# 早期生成过的 flash_modle_sim 只在 clean 中清理，不再作为正式入口。
TARGET := flash_model_sim
LEGACY_TARGET := flash_modle_sim

# 框架库源码：不包含 CLI，表示可被命令行工具、测试或未来其他前端复用的核心层。
LIB_SRCS := src/config/config.cpp \
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

# 应用源码：CLI 是框架的一层外壳，真正模型逻辑仍放在 LIB_SRCS。
APP_SRCS := src/cli/main.cpp $(LIB_SRCS)
APP_OBJS := $(APP_SRCS:.cpp=.o)

ADDR_TEST := tests/address_test
REG_TEST := tests/registers_test
TIME_TEST := tests/timing_test
STORAGE_TEST := tests/storage_test
CAP_TEST := tests/capability_test
EVIDENCE_TEST := tests/evidence_test
INTERFACE_TEST := tests/interface_test
COMMAND_TEST := tests/command_test
MODEL_TEST := tests/model_feature_test

# 小型单元测试只链接自己需要的层，编译更快，也能暴露层间依赖是否过重。
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
STORAGE_TEST_SRCS := tests/storage_test.cpp \
	src/core/storage.cpp
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

# 集成型模型测试链接完整框架库，覆盖 builder -> model -> core/capability/policy 的组合路径。
MODEL_TEST_SRCS := tests/model_feature_test.cpp $(LIB_SRCS)

.PHONY: all clean run-nor run-nor-protect run-nand test

all: $(TARGET)

$(TARGET): $(APP_OBJS)
	$(CXX) $(CXXFLAGS) $(APP_OBJS) -o $@

%.o: %.cpp
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c $< -o $@

run-nor: $(TARGET)
	./$(TARGET) configs/demo_nor.yaml --self-test

run-nor-protect: $(TARGET)
	./$(TARGET) configs/demo_nor_bp.yaml --self-test

run-nand: $(TARGET)
	./$(TARGET) configs/demo_nand.yaml --self-test

$(ADDR_TEST): $(ADDR_TEST_SRCS)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(ADDR_TEST_SRCS) -o $@

$(REG_TEST): $(REG_TEST_SRCS)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(REG_TEST_SRCS) -o $@

$(TIME_TEST): $(TIME_TEST_SRCS)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(TIME_TEST_SRCS) -o $@

$(STORAGE_TEST): $(STORAGE_TEST_SRCS)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(STORAGE_TEST_SRCS) -o $@

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

test: $(TARGET) $(ADDR_TEST) $(REG_TEST) $(TIME_TEST) $(STORAGE_TEST) $(CAP_TEST) $(EVIDENCE_TEST) $(INTERFACE_TEST) $(COMMAND_TEST) $(MODEL_TEST)
	./$(ADDR_TEST)
	./$(REG_TEST)
	./$(TIME_TEST)
	./$(STORAGE_TEST)
	./$(CAP_TEST)
	./$(EVIDENCE_TEST)
	./$(INTERFACE_TEST)
	./$(COMMAND_TEST)
	./$(MODEL_TEST)
	sh tests/smoke.sh

clean:
	rm -f $(TARGET) $(LEGACY_TARGET) $(APP_OBJS) $(ADDR_TEST) $(REG_TEST) $(TIME_TEST) $(STORAGE_TEST) $(CAP_TEST) $(EVIDENCE_TEST) $(INTERFACE_TEST) $(COMMAND_TEST) $(MODEL_TEST)
