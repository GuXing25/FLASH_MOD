CXX ?= g++
CXXFLAGS ?= -std=c++17 -O2 -Wall -Wextra -pedantic
CPPFLAGS ?= -Iinclude

# NAND-only 命令行仿真器入口。
TARGET := flash_mod_nand_sim

# 源文件顺序大致对应架构链路：
# config_parser -> hardware/event/core -> ftl -> flash_test -> main。
SRCS := src/main.cpp \
	src/config_parser.cpp \
	src/addr_mapping.cpp \
	src/storage_backend.cpp \
	src/flash_hardware.cpp \
	src/flash_event.cpp \
	src/flash_core.cpp \
	src/ftl.cpp \
	src/flash_test.cpp
OBJS := $(SRCS:.cpp=.o)

.PHONY: all clean run run-w25n

# 默认目标：只构建仿真器，不自动运行测试。
all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) $(OBJS) -o $@

%.o: %.cpp
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c $< -o $@

run: $(TARGET)
	./$(TARGET) configs/demo_spinand.conf --self-test

# 典型 Winbond SPI-NAND 配置。
run-w25n: $(TARGET)
	./$(TARGET) configs/w25n01gv.conf --self-test

# 清理构建产物和自测生成的 storage 镜像。
clean:
	rm -f $(TARGET) $(OBJS) *.bin configs/*.bin
