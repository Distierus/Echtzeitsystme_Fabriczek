##############################################################################
# general declaration and symbols
##############################################################################
VERSION      =	1_00
EXT          =
CCSCA        =	clang
GCCSCA       =	gcc-11
EXECUTABLE   =	x86_64
CSTD	     =	gnu17
CFLAGS       =	-g3 -ffunction-sections -fdata-sections -Wall -Wpedantic -fstrict-aliasing \
				-Wextra -Wshift-count-negative -Wshift-count-overflow -Wuninitialized\
				-Wmissing-braces  -Wignored-qualifiers -Wnonnull -Wunused-variable
GCCFLAGS     =	-fanalyzer -Werror
SCAFLAGS     =	-DSCA -D__SCA__X86 -c
MEMSANFLAGS  =	-fsanitize=memory -fno-omit-frame-pointer -fno-optimize-sibling-calls
ADDRSANFLAGS =	-fsanitize=address -fno-omit-frame-pointer -fno-optimize-sibling-calls
TARGETDEVICE =	STM32F746xx
SYMBOLS      =	-DVERSION=$(VERSION) -DUSE_HAL_DRIVER -D$(TARGETDEVICE) -DWIN32 -D_CRT_SECURE_NO_WARNINGS
LDFLAGS      =	-lm -lpthread
SCATITLE     =	PM-SCA-REPORT
INCLUDES     =	-I../../libs/LibSpindle/inc \
				-I../../libs/LibL6474/inc \
				-I../../libs/LibRTOSConsole/inc \
				-I../../libs/LibHALMockup/inc \
				-I../../libs/FreeRTOS-LTS/FreeRTOS/FreeRTOS-Kernel/include \
				-I../../libs/FreeRTOS-LTS/FreeRTOS/FreeRTOS-Kernel/portable/Clang-Win32 \
				-I../../simulation/FreeRTOSWin32/include/sys \
				-I../../simulation/FreeRTOSWin32/include \
				-I../Core/Inc/Spindle \
				-I../Core/Inc/Stepper \
				-I../Core/Inc/Console \
				-I../Core/Inc 

SRCS         = 	$(wildcard ../../libs/LibSpindle/src/*.c) \
				$(wildcard ../../libs/LibRTOSConsole/src/*.c) \
				$(wildcard ../../libs/LibL6474/src/*.c) \
				$(wildcard ../Core/Src/Stepper/*.c)

##############################################################################
# checking os to select correct extension
##############################################################################
ifeq ($(OS),Windows_NT)
    EXT=.exe
	RMC=rm
	RMF=-r -f
else
	RMC=rm
	RMF=-r -f
#    UNAME_S := $(shell uname -s)
#	ifeq ($(UNAME_S),Linux)
#       can do other stuff if its linux
#    endif
#    ifeq ($(UNAME_S),Darwin)
#	    can do other stuff if its linux 
#	endif
endif

##############################################################################
# static code ananlysis
##############################################################################
.PHONY: sca
sca: rem-report mk-report clean sca-do tidy
	@echo 'sca done'

rem-report:
	@$(RMC) $(RMF) ./report

mk-report:
	@mkdir -p ./report

sca-do:
	@scan-build -o ./report  --html-title $(SCATITLE) $(CCSCA) $(CFLAGS) $(SRCS) -std=$(CSTD) $(SCAFLAGS) $(SYMBOLS) $(INCLUDES)
	
##############################################################################
# Cleaning targets
##############################################################################
.PHONY: clean
clean:
	@$(RMC) $(RMF) $(OBJ) $(EXECUTABLE)$(EXT) $(EXECUTABLE)_test$(EXT) $(EXECUTABLE)_mem$(EXT) $(EXECUTABLE)_addr$(EXT)
	@echo 'cleaned up'

.PHONY: tidy
tidy:
	@$(RMC) $(RMF) ./*.o
	@echo 'tidied up'

##############################################################################
# Help Information
##############################################################################
.PHONY: setup
setup:
	@echo 'only supported for linux and linux subsystem'
	@sudo apt-get update
	@sudo apt-get install clang clang-tools gcc-multilib g++-multilib build-essential binutils-arm-none-eabi -y
	
##############################################################################
# Help Information
##############################################################################
.PHONY: help
help :
	@echo 'Summary of Makefile targets'
	@echo '  Build targets:'
	@echo '    sca              - static code analysis'
	@echo '  Clean targets:'
	@echo '    clean            - All files and executables'
	@echo '    tidy             - All *.o files)'
	@echo '  Setup targets:'
	@echo '    setup            - installs clang sca dependencies'
	@echo '                       (linux or ubuntu subsystem in windows only)'
