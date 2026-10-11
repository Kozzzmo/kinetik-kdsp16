#!/usr/bin/make -f
# Kinetik KDSP-16 — build with DPF (https://github.com/DISTRHO/DPF)
NAME = KinetikKDSP16
FILES_DSP = KinetikPlugin.cpp
FILES_UI  = KinetikUI.cpp
DPF_PATH ?= ../dpf
DPF_TARGET_DIR = ../bin
DPF_BUILD_DIR = ../build
include $(DPF_PATH)/Makefile.plugins.mk
BUILD_CXX_FLAGS += -std=gnu++17
TARGETS = vst3 clap
ifeq ($(MACOS),true)
TARGETS += au
endif
ifeq ($(LINUX),true)
TARGETS += jack
endif
all: $(TARGETS)
