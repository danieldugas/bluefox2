#! /bin/make
# -*- sh -*-

SUB_DIRS=$(shell ls)

#-------------------------------------------------------------------------
.PHONY:	all clean strip new build x86 x86_64 arm cygwin info native armv7a armv6zk armv7ahf armv7axe armhf armsf arm64

#=========================================================================
all clean strip new build x86 x86_64 arm cygwin info native armv7a armv6zk armv7ahf armv7axe armhf armsf arm64:
	@for sdir in $(SUB_DIRS) ;							\
	do													\
		echo $$sdir; 									\
		(												\
			if [ -d $$sdir ];							\
			then										\
				if [ -e $$sdir/Makefile ];				\
				then									\
					echo "make in :"$$sdir;				\
					$(MAKE) -C $$sdir $@ || exit $$?;	\
				fi;										\
			fi											\
		)												\
		done

#-------------------------------------------------------------------------
