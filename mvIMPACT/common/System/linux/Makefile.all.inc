#! /bin/make
##############################################
# common make definitions

include $(SYSTEM_PATH)/version.inc
GLOBAL_PACKAGE_VERSION=$(GPV_MAJOR).$(GPV_MINOR).$(GPV_BUILD)

#-------------------------------------------------
# SYSTEM_PATH will be set in the Makefile.inc
# of each library!
#-------------------------------------------------
include $(SYSTEM_PATH)/config.inc

#-------------------------------------------------
# for CYGWIN user
#---------------------------------------------
OTYPEDEF=
TARGET_SYST=$(TARGET)

#---------------------------------------------
GCCPATH=$(GCC_$(HOSTTYPE)X$(TARGET_SYST))
#---------------------------------------------
# when not cross compiling just set these to the standard directories
CROSS_USER_INC_PATH=/usr/include

# This is the default rule but it detects the wrong directory on x86(32bit) systems that happen to have a /usr/lib64 directory.
# However this problem will be corrected for each platform specifically further down the page
CROSS_USER_LIB_PATH=$(shell if [ -h /usr/lib ]; then echo "/usr/lib"; else if [ -e /usr/lib64 ]; then echo "/usr/lib64"; else echo "/usr/lib"; fi; fi)

DISTCC=
CCACHE=
CPUTYPEDEF=
ifeq ($(TARGET),arm)
	# alternative toolchains
	CROSS_USER_INC_PATH=$(GCCPATH)/arm-linux/include
	CROSS_USER_LIB_PATH=$(GCCPATH)/arm-linux/lib
	CROSS_PREFIX=arm-linux-

	# Gentoo toolchain
	# CROSS_USER_INC_PATH=/usr/arm-any-linux-gnu/usr/include/
	# CROSS_USER_LIB_PATH=/usr/arm-any-linux-gnu/usr/lib/
	# CROSS_PREFIX=arm-unknown-linux-gnu-

	CROSS_USR_INC=-I$(CROSS_USER_INC_PATH)
else
	ifeq ($(TARGET),x86)
		ifeq ($(HOSTTYPE),x86_64)
			# building for 32-bit on 64-bit system - use 64-bit compiler with -m32 option
			CROSS_USER_INC_PATH=/usr/include
			CROSS_USER_LIB_PATH=/usr/lib32
		else
			ifneq (,$(findstring 86,$(HOSTTYPE)))
				# building for 32-bit on an x86 32-bit system - just use the standard directories
				CROSS_USER_INC_PATH=/usr/include
				CROSS_USER_LIB_PATH=/usr/lib
			endif
		endif
	else
		ifeq ($(TARGET),armv7a)
			CROSS_PREFIX=arm-angstrom-linux-gnueabi-
			CROSS_USER_INC_PATH=$(GCCPATH)/include
			CROSS_USER_LIB_PATH=$(GCCPATH)/lib
			CPUTYPEDEF=-D__armv7a__
			CROSS_USER_FLAGS=-march=armv7-a -mtune=cortex-a8 -mfpu=neon -mfloat-abi=softfp \
				-Wl,-rpath-link,$(CROSS_USER_LIB_PATH),--hash-style=gnu \
				-mthumb-interwork -mno-thumb -isystem$(CROSS_USER_INC_PATH) \
				-fexpensive-optimizations -frename-registers -fomit-frame-pointer
		else
			ifeq ($(TARGET),armv6zk)
				CROSS_PREFIX=arm-bcm2708hardfp-linux-gnueabi-
				CROSS_USER_INC_PATH=$(SDK_PATH)/arm-bcm2708hardfp-linux-gnueabi/sysroot/usr/include
				CROSS_USER_LIB_PATH=$(SDK_PATH)/arm-bcm2708hardfp-linux-gnueabi/sysroot/usr/lib
				CPUTYPEDEF=-D__armv6zk__
				CROSS_USER_FLAGS=-mfpu=vfp -mfloat-abi=hard -march=armv6zk -mtune=arm1176jzf-s \
				-Wl,-rpath-link,$(CROSS_USER_LIB_PATH),--hash-style=gnu \
				-mthumb-interwork -isystem$(CROSS_USER_INC_PATH) \
				-fexpensive-optimizations -frename-registers -fomit-frame-pointer
			else
				ifeq ($(TARGET),$(filter $(TARGET),armv7ahf armv7axe))
					CROSS_PREFIX=$(TARGET_PREFIX)
					CROSS_USER_INC_PATH=$(PKG_CONFIG_SYSROOT_DIR)/usr/include
					CROSS_USER_LIB_PATH=$(PKG_CONFIG_SYSROOT_DIR)/usr/lib
					CROSS_USER_FLAGS=-march=armv7-a -mtune=cortex-a9 -mfpu=neon -mfloat-abi=hard -ftree-vectorize -ffast-math --sysroot=$(PKG_CONFIG_SYSROOT_DIR)
				else
					ifeq ($(TARGET),armhf)
						CROSS_PREFIX=arm-linux-gnueabihf-
						CROSS_USER_INC_PATH=/usr/arm-linux-gnueabihf/include
						CROSS_USER_LIB_PATH=/usr/arm-linux-gnueabihf/lib
						CROSS_USR_INC=-I$(CROSS_USER_INC_PATH)
					else
						ifeq ($(TARGET),armsf)
							CROSS_PREFIX=arm-linux-gnueabi-
							CROSS_USER_INC_PATH=/usr/arm-linux-gnueabi/include
							CROSS_USER_LIB_PATH=/usr/arm-linux-gnueabi/lib
							CROSS_USR_INC=-I$(CROSS_USER_INC_PATH)
						else
							ifeq ($(TARGET),arm64)
								CROSS_PREFIX=aarch64-linux-gnu-
								CROSS_USER_INC_PATH=/usr/aarch64-linux-gnu/include
								CROSS_USER_LIB_PATH=/usr/aarch64-linux-gnu/lib
								CROSS_USR_INC=-I$(CROSS_USER_INC_PATH)
							endif
						endif
					endif
				endif
			endif
		endif
	endif
endif

#---------------------------------------------
ifeq ($(CCACHE),)
	# ccache not found, just use distcc if found
	SPEED_PREFIX = $(DISTCC)

else
	# ccache found, use it
	ifneq ($(DISTCC),)
		# use distcc with ccache if found
		export CCACHE_PREFIX = distcc
	endif

	SPEED_PREFIX = ccache

endif

#---------------------------------------------
#=============================================
# PARALLEL COMPILING ON MANY PROCESSOR CORES, IF ENOUGH FREE MEMORY(512MB PER COMPILATION THREAD)
NUM_OF_THREADS := $(shell if [ "x$$(which getconf )" != "x" ] && [ "x$$(which free )" != "x" ]  									\
	&& [ "x$$(which grep )" != "x" ] && [ "x$$(which sed )" != "x" ] && [ $$(getconf _NPROCESSORS_CONF ) -gt 0 ] ; then			\
		RECOMMENDED_NUM_OF_THREADS_BASED_ON_FREE_MEM=$$(( ( $$(export LC_MESSAGES=C && free | grep Mem | sed -e "s/[[:space:]]\+/ /g" | cut -d ' ' -f4 ) +	\
								$$( export LC_MESSAGES=C && free | grep Swap | sed -e "s/[[:space:]]\+/ /g" | cut -d ' ' -f4 ) ) / 524288 ));				\
		if [ $$RECOMMENDED_NUM_OF_THREADS_BASED_ON_FREE_MEM -gt $$(getconf _NPROCESSORS_CONF ) ] ; then								\
			echo -n $$(getconf _NPROCESSORS_CONF );																					\
		else																														\
			if [ $$RECOMMENDED_NUM_OF_THREADS_BASED_ON_FREE_MEM -gt 0 ] ; then														\
				echo -n $$RECOMMENDED_NUM_OF_THREADS_BASED_ON_FREE_MEM;																\
			else																													\
				echo -n 1;																											\
			fi																														\
		fi																															\
	else																															\
		echo -n 1;																													\
	fi )
ifneq ($(ALLOW_CMDLINE_MAKEFLAGS),YES)
	MAKEFLAGS+=" -j$(NUM_OF_THREADS) "
endif
#=============================================
#===========================================
# OPT
# ---
# To get debug-info set OPT=-g; with OPT=-O2
# you get the sources optimized; if OPT=-pg
# -a there will be generated a profiling
# file (bb.out).
#
#===========================================
#---------------------------------------------
ifeq ($(OPT),)
	OPT= -O2
#	OPT= -ggdb
	USE_NDEBUG=-DNDEBUG
	ifeq ($(TARGET),armv7a)
		OPT=-O3
	endif
endif

#---------------------------------------------
#===========================================
# compile level options
# -fPIC	 needed for SO
#===========================================
COMP_OPTIONS=						\
	-Wall -W -fPIC					\
	-D_REENTRANT					\
	-D_GNU_SOURCE					\
	-D__STDC_WANT_LIB_EXT1__=1		\
	-D_MAJOR_VERSION=$(GPV_MAJOR)	\
	-D_MINOR_VERSION=$(GPV_MINOR)	\
	-D_BUILD_VERSION=$(GPV_BUILD)	\
	-D_BUILD2_VERSION=$(GPV_BUILD2)
#---------------------------------------------
CPPFLAGS_2=				\
	$(OTYPEDEF)			\
	$(CPUTYPEDEF)		\
	$(INCFILES)			\
	$(CROSS_USR_INC)	\
	$(OPT)				\
	$(COMP_OPTIONS)		\
	$(CPPFLAGS_1)		\
	$(CROSS_USER_FLAGS)	\
	$(USE_NDEBUG)
#-------------------------------------------------
#-------------------------------------------------
ifneq ($(GCCPATH),)
	ifeq ($(TARGET),armv7ahf)
		CC_TOOL_PATH=$(GCCPATH)/bin/arm-angstrom-linux-gnueabi/$(CROSS_PREFIX)
	else
		ifeq ($(TARGET),armv7axe)
			CC_TOOL_PATH=$(GCCPATH)/bin/arm-poky-linux-gnueabi/$(CROSS_PREFIX)
		else
			CC_TOOL_PATH=$(GCCPATH)/bin/$(CROSS_PREFIX)
		endif
	endif
	TOOLS= $(SPEED_PREFIX) $(CC_TOOL_PATH)
endif

#---------------------------------------------
#===========================================
# modifier for built-in rules
#===========================================
ifeq ($(TARGET),$(filter $(TARGET),armv7ahf armv7axe))
	CC  = $(TOOLS)gcc --sysroot=$(PKG_CONFIG_SYSROOT_DIR)
	CXX = $(TOOLS)g++ --sysroot=$(PKG_CONFIG_SYSROOT_DIR)
else
	CC  = $(TOOLS)gcc
	CXX = $(TOOLS)g++
endif
#AR	= $(CC_TOOL_PATH)ar
AR	= ar

#---------------------------------------------
HAVE_GCC := $(shell if [ -x $(CC_TOOL_PATH)gcc ]; then echo "yes"; else echo "no"; fi )

ifeq ($(HAVE_GCC),yes)
	GCC_VERSION        := $(shell $(CC) -dumpversion 2>&1)
	GCC_MAJOR          := $(shell echo $(GCC_VERSION) | cut -d'.' -f1)
	GCC_MINOR          := $(shell echo $(GCC_VERSION) | cut -d'.' -f2)
	# C++ symbol visibility support since version 4.0
	CPP_SYM_VISIBILITY := $(shell if [ $(GCC_MAJOR) -ge 4 ] ; then 						\
								echo "-fvisibility=hidden";			\
							else							\
								echo "";					\
							fi )
endif

ifeq ($(CPP_SYM_VISIBLE),y)
else
	CPPFLAGS_2 += $(CPP_SYM_VISIBILITY)
endif
# uncomment following line to re-enable OpenMP
# CPPFLAGS_2 += $(OPENMP)

#---------------------------------------------
#---------------------------------------------
.PHONY: new default info build $(TARGET)
#===========================================
default:	all

#-------------------------------------------
build: clean_mod all

#-------------------------------------------
new: clean_mod strip

#-------------------------------------------
$(TARGET_SYST):
#	mkdir -p $(TARGET)

#-------------------------------------------
$(TARGET_SYST)/%.o:	%.c
	$(CC) -c $(CPPFLAGS)	-o $@ $<

#-------------------------------------------
$(TARGET_SYST)/%.o:	%.cpp
	$(CXX)	-c $(CPPFLAGS) -o $@ $<

#-------------------------------------------
info:
	@echo "==================================================="
	@echo -n "PWD         =" ; pwd
	@echo "OTSYPE         =" $(OSTYPE)
	@echo "HOSTTYPE       =" $(HOSTTYPE)
	@echo "TARGET         =" $(TARGET)
	@echo "ROOT_PATH      =" $(ROOT_PATH)
	@echo "SYSTEM_PATH    =" $(SYSTEM_PATH)
	@echo "IPP_REPLACEMENT_OBJ    =" $(IPP_REPLACEMENT_OBJ)
	@echo "INTEL_IPP_LIBS =" $(INTEL_IPP_LIBS)
	@echo "TOOLS          =" $(TOOLS)
	@echo "INCFILES       =" $(INCFILES)
	@echo "CC             =" $(CC)
	@echo "CXX            =" $(CXX)
	@echo "CPPFLAGS       =" $(CPPFLAGS)
	@echo "LDOPT          =" $(LDOPT)
	@echo "LDFLAGS        =" $(LDFLAGS)
	@echo "TARGET_SYST    =" $(TARGET_SYST)
	@echo "GCCPATH        =" $(GCCPATH)
	@echo "CROSS_USER_LIB_PATH" = $(CROSS_USER_LIB_PATH)
	@echo "CROSS_USER_INC_PATH" = $(CROSS_USER_INC_PATH)
	@echo "USER_AR_LIB_LIST" = $(USER_AR_LIB_LIST)
	@echo "DISTCC         =" $(DISTCC)
	@echo "CCACHE         =" $(CCACHE)
	@echo "GCC_VERSION    =" $(GCC_VERSION)
	@echo "OPENMP         =" $(OPENMP)
	@echo "CCACHE_PREFIX  =" $(CCACHE_PREFIX)
	@echo "CC_TOOL_PATH   =" $(CC_TOOL_PATH)
	@echo "WX_CONF_PATH   =" $(WX_CONF_PATH)
	@echo "WX_CONF_TOOL   =" $(WX_CONF_TOOL)
	@echo "==================================================="

#-------------------------------------------------
#------------------------------ vim: filetype=make

