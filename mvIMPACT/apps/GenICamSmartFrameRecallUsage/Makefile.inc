#! /bin/make
##############################################
# Makefile for the 'GenICamSmartFrameRecallUsage' sample
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
BINARY_NAME=GenICamSmartFrameRecallUsage
MODULE_NAME=GenICamSmartFrameRecallUsage.legacy

#-------------------------------------------
OOBJ =										\

#-------------------------------------------
vpath %.cpp ..

#-------------------------------------------
USR_INCLS =					\

#-------------------------------------------
USR_LIBS =					\

#		-lefence

#-------------------------------------------
CPPFLAGS_1 = 				\
	-pedantic -Wno-long-long

#-------------------------------------------
include $(ROOT_PATH)/Makefile.samp.inc
