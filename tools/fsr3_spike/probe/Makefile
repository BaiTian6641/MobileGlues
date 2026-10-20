CXX ?= g++
CXXFLAGS ?= -O2 -std=c++17 -Wall -Wextra -DGL_GLEXT_PROTOTYPES=1 -DEGL_EGLEXT_PROTOTYPES=1
LIBS ?= -lEGL -lGLESv2

fsr3_gles_probe: probe.cpp
	$(CXX) $(CXXFLAGS) -o $@ $< $(LIBS) -lm

clean:
	rm -f fsr3_gles_probe

.PHONY: clean
