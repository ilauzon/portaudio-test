# ============================
#   Platform Detection
# ============================
UNAME_S := $(shell uname -s 2>/dev/null)

ifeq ($(UNAME_S),Darwin)
  PLATFORM := macos
else ifeq ($(UNAME_S),Linux)
  PLATFORM := linux
else
  PLATFORM := other
endif

# ============================
#   Compiler
# ============================
# You aliased g++ -> /opt/homebrew/bin/g++-15, so this will use Homebrew GCC on macOS.
CXX ?= g++

# ============================
#   Project Settings
# ============================
EXEC      := audiotest

# Include both root .cpp files and gui/*.cpp files
CPP_FILES := $(wildcard *.cpp gui/*.cpp)
OBJ       := $(CPP_FILES:.cpp=.o)

# PortAudio paths (after install-deps)
PA_DIR  := lib/portaudio
PA_INC  := -I$(PA_DIR)/include
PA_LIB  := $(PA_DIR)/lib/.libs/libportaudio.a

# wxWidgets via wx-config
WX_CONFIG    ?= wx-config
WX_CXXFLAGS  := $(shell $(WX_CONFIG) --cxxflags 2>/dev/null)
WX_LIBS      := $(shell $(WX_CONFIG) --libs 2>/dev/null)

# ============================
#   Platform-specific audio libs
# ============================
ifeq ($(PLATFORM),macos)
  AUDIO_LIBS := $(PA_LIB) \
                -framework CoreAudio \
                -framework AudioToolbox \
                -framework AudioUnit \
                -framework CoreServices \
                -pthread
else ifeq ($(PLATFORM),linux)
  AUDIO_LIBS := $(PA_LIB) \
                -lrt -lasound -ljack -pthread
else
  AUDIO_LIBS := $(PA_LIB)
endif

# ============================
#   Flags
# ============================
CXXFLAGS += -std=c++17 -g $(WX_CXXFLAGS) $(PA_INC)
LDFLAGS  +=
LDLIBS   += $(WX_LIBS) $(AUDIO_LIBS) -lsndfile

# macOS-specific: Homebrew libsndfile
ifeq ($(PLATFORM),macos)
BREW_PREFIX := $(shell brew --prefix 2>/dev/null)
CXXFLAGS    += -I$(BREW_PREFIX)/include
LDFLAGS     += -L$(BREW_PREFIX)/lib
endif

# ============================
#   Build: objects + executable
# ============================

# Compile each .cpp into a .o
%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Link all objects into the final executable
$(EXEC): $(OBJ)
	$(CXX) $(CXXFLAGS) $(LDFLAGS) -o $@ $^ $(LDLIBS)

# Include auto-generated dependency files
-include $(OBJ:.o=.d)

# ============================
#   PortAudio: install-deps
# ============================
.PHONY: install-deps
install-deps:
	mkdir -p lib
	if [ ! -d "$(PA_DIR)" ]; then \
	  echo "Downloading PortAudio..."; \
	  curl -L http://files.portaudio.com/archives/pa_stable_v190700_20210406.tgz | tar -zx -C lib; \
	fi
	cd $(PA_DIR) && \
	  echo "Cleaning old build (if any)..." && \
	  $(MAKE) distclean || true && \
	  echo "Patching PortAudio: removing -Werror..." && \
	  sed -i '' 's/-Werror//g' configure src/common/Makefile.in src/hostapi/coreaudio/Makefile.in 2>/dev/null || true && \
	  echo "Configuring PortAudio..." && \
	  ./configure && \
	  echo "Building PortAudio..." && \
	  $(MAKE) -j

# ============================
#   PortAudio: uninstall-deps
# ============================
.PHONY: uninstall-deps
uninstall-deps:
	if [ -d "$(PA_DIR)" ]; then \
	  cd $(PA_DIR) && $(MAKE) uninstall || true; \
	fi
	rm -rf $(PA_DIR)

# ============================
#   Convenience Targets
# ============================
.PHONY: run
run: $(EXEC)
	./$(EXEC)

.PHONY: clean
clean:
	rm -f $(EXEC) $(OBJ) $(OBJ:.o=.d)
	rm -rf $(EXEC).dSYM