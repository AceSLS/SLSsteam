#Thanks to https://stackoverflow.com/questions/52034997/how-to-make-makefile-recompile-when-a-header-file-is-changed for the -MMD & -MP flags
#Without them headers wouldn't trigger recompilation

#Force g++ cause clang crashes on some hooks
CXX := g++
CXXFLAGS += -std=c++20

libs := $(wildcard lib/*.a)
srcs := $(shell find src/ -type f -iname "*.cpp")
objs := $(srcs:src/%.cpp=obj/%.o)
deps := $(objs:%.o=%.d)

CXXFLAGS := -O3 -flto=auto -fPIC -m32 -std=c++20 -Wall -Wextra -Wpedantic -Wno-error=format-security

LDFLAGS := -shared
LDFLAGS += $(shell pkg-config --libs "openssl")

DATE := $(shell date "+%Y%m%d%H%M%S")

ifeq ($(shell echo $$NATIVE),1)
	CXXFLAGS += -march=native
endif

#Speed up compilation if additional dependencies are found
ifeq ($(shell type ccache &> /dev/null && echo "found"),found)
	export PATH := /usr/lib/ccache/bin:$(PATH)
endif
ifeq ($(shell type mold &> /dev/null && echo "found"),found)
	LDFLAGS += -fuse-ld=mold
endif

bin/SLSsteam.so: $(objs) $(libs)
	@mkdir -p bin
	$(CXX) $(CXXFLAGS) $^ -o bin/SLSsteam.so $(LDFLAGS)

-include $(deps)
obj/%.o : src/%.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -Iinclude -MMD -MP -c $< -o $@

clean:
	rm -rvf "obj/" "bin/" "zips/"

install:
	sh setup.sh

zips: rebuild
	@mkdir -p zips
	7z a -mx9 -m9=lzma2 "zips/SLSsteam $(DATE).7z" "bin/SLSsteam.so" "setup.sh"
	#Maybe should be somewhere else, but who cares. Does anyone even use this besides me?
	7z a -mx9 -m9=lzma2 "zips/SLSsteam - SLSConfig $(DATE).7z" "$(HOME)/.config/SLSsteam/config.yaml"

	#Compatibility for Github issues
	7z a -mx9 -m9=lzma "zips/SLSsteam $(DATE).zip" "bin/SLSsteam.so" "setup.sh"
	7z a -mx9 -m9=lzma "zips/SLSsteam - SLSConfig $(DATE).zip" "$(HOME)/.config/SLSsteam/config.yaml"

build: bin/SLSsteam.so
rebuild: clean build
all: clean build zips

.PHONY: all build clean rebuild zips
