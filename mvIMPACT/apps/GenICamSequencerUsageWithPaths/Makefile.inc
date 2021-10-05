#! /bin/make
##############################################
# Makefile for the 'GenICamSequencerUsageWithPaths' sample
##############################################
#-------------------------------------------
ifndef MVIMPACT_ACQUIRE_SOURCE_DIR
    ifndef MVIMPACT_ACQUIRE_DIR
        ROOT_PATH=../..
    else
        ROOT_PATH=$(MVIMPACT_ACQUIRE_DIR)/apps
    endif
else
    ROOT_PATH=$(MVIMPACT_ACQUIRE_SOURCE_DIR)/apps
endif

#-------------------------------------------
BINARY_NAME=GenICamSequencerUsageWithPaths
MODULE_NAME=GenICamSequencerUsageWithPaths.legacy

#-------------------------------------------
OOBJ =										\

#-------------------------------------------
vpath %.cpp ..

#-------------------------------------------
USR_INCLS =					\

#-------------------------------------------
USR_LIBS =					\
        -lrt

#		-lefence

#-------------------------------------------
CPPFLAGS_1 = 				\
	-pedantic -Wno-long-long

#-------------------------------------------
include $(ROOT_PATH)/Makefile.samp.inc
