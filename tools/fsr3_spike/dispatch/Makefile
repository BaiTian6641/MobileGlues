CXX ?= g++
# -ffp-contract=off keeps the CPU reference model portable and deterministic.
CXXFLAGS ?= -O2 -std=c++17 -Wall -Wextra -ffp-contract=off -ffloat-store \
            -DGL_GLEXT_PROTOTYPES=1 -DEGL_EGLEXT_PROTOTYPES=1
LIBS ?= -lEGL -lGLESv2

fsr3_dispatch: fsr3_dispatch.cpp
	$(CXX) $(CXXFLAGS) -o $@ $< $(LIBS) -lm

clean:
	rm -f fsr3_dispatch fsr3_dispatch_arm64

.PHONY: clean
