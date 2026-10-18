CXX      = clang++
CXXFLAGS = -std=c++17 -I./include -I./include/visualizer/imgui -I/usr/include -Wall -Wextra -fexceptions \
           $(shell llvm-config --cxxflags | sed 's/-fno-exceptions//g') \
           $(shell pkg-config --cflags libdw 2>/dev/null) \
           $(shell pkg-config --cflags vulkan glfw3 2>/dev/null)
LDFLAGS  = $(shell llvm-config --ldflags) -lclang \
           $(shell pkg-config --libs libdw 2>/dev/null || echo "-ldw -lelf") \
           -lcppdap -lpthread \
           -lvulkan -lglfw -limgui

SLANGC   = /opt/slang/bin/slangc

SRC = src/main.cpp \
      src/static_analyzer.cpp \
      src/ast_visitor.cpp \
      src/report.cpp \
      src/runtime_analyze.cpp \
      src/compiler_helper.cpp \
      src/trace_model.cpp \
      src/visualizer/vk_context.cpp \
      src/visualizer/vk_pipeline.cpp \
      src/visualizer/scene_renderer.cpp \
      src/visualizer/visualizer_app.cpp \
      src/visualizer/ui_overlay.cpp \
      src/visualizer/imgui/imgui_impl_glfw.cpp \
      src/visualizer/imgui/imgui_impl_vulkan.cpp

OBJ = $(SRC:.cpp=.o)
OUT = crix

SHADERS_SLANG = shaders/bar_visualizer.slang
SHADERS_SPV = shaders/bar_visualizer.vert.spv shaders/bar_visualizer.frag.spv

all: shaders $(OUT)

shaders: $(SHADERS_SPV)

shaders/bar_visualizer.vert.spv: shaders/bar_visualizer.slang
	$(SLANGC) $< -target spirv -entry vertexMain -stage vertex -o $@

shaders/bar_visualizer.frag.spv: shaders/bar_visualizer.slang
	$(SLANGC) $< -target spirv -entry fragmentMain -stage fragment -o $@

$(OUT): $(OBJ)
	$(CXX) $(OBJ) -o $@ $(LDFLAGS)

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -f $(OBJ) $(OUT) shaders/*.spv

.PHONY: all clean shaders
