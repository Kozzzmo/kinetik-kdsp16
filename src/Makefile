#!/usr/bin/make -f
# Kinetik KDSP-16 — build with DPF (https://github.com/DISTRHO/DPF)
NAME = KinetikKDSP16
FILES_DSP = KinetikPlugin.cpp third_party/monocypher/monocypher.c third_party/monocypher/monocypher-ed25519.c
FILES_UI  = KinetikUI.cpp FileDrop.cpp Updater.cpp
ifeq ($(shell uname -s),Darwin)
FILES_UI += FileDropMac.mm
endif
DPF_PATH ?= ../dpf
DPF_TARGET_DIR = ../bin
DPF_BUILD_DIR = ../build
include $(DPF_PATH)/Makefile.plugins.mk
BUILD_CXX_FLAGS += -std=gnu++17 -DKN_VERSION='"$(shell cat ../VERSION)"'
ifeq ($(WINDOWS),true)
LINK_FLAGS += -lole32 -lshell32 -luuid
endif
TARGETS = vst3 clap
ifeq ($(MACOS),true)
TARGETS += au
endif
ifeq ($(LINUX),true)
TARGETS += jack
endif
all: $(TARGETS)
