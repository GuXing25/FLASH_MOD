CXX ?= g++
CXXFLAGS ?= -std=c++17 -O2 -Wall -Wextra -pedantic
CPPFLAGS ?= -Iinclude

# NOR-only 命令行仿真器入口。
TARGET := flash_mod_nor_sim

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

.PHONY: all clean run run-w25q

# 默认目标：只构建仿真器，不自动运行测试。
all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) $(OBJS) -o $@

%.o: %.cpp
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c $< -o $@

run: $(TARGET)
	./$(TARGET) configs/demo_nor.conf --self-test

# SPI-NAND demo，覆盖 page_read/cache/program_execute 路径。
# 典型 Winbond SPI-NOR 配置。
run-w25q: $(TARGET)
	./$(TARGET) configs/w25q32jv.conf --self-test

# 清理构建产物和自测生成的 storage 镜像。
clean:
	rm -f $(TARGET) $(OBJS) *.bin configs/*.bin
