CFLAGS = -std=c11 -O2 -Wall
LDFLAGS = -lglfw -lvulkan -ldl -lpthread -lX11 -lXxf86vm -lXrandr -lXi

vulkan_triangle: src/*.c
	mkdir -p build
	gcc $(CFLAGS) -o build/vulkan_triangle src/*.c $(LDFLAGS)

	mkdir -p res/shaders
	glslc -o res/shaders/vert.spv src/shaders/shader.vert
	glslc -o res/shaders/frag.spv src/shaders/shader.frag

.PHONY: test clean

test: build/vulkan_triangle
	cd res && ../build/vulkan_triangle

clean:
	rm -rf build res/shaders