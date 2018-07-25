################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
LD_SRCS += \
../src/lscript.ld 

C_SRCS += \
../src/axu_connector.c \
../src/common_functions.c \
../src/community.c \
../src/flashtest.c \
../src/led.c \
../src/main.c \
../src/memtest.c \
../src/pl_eth_test.c \
../src/platform.c \
../src/sdtest.c 

OBJS += \
./src/axu_connector.o \
./src/common_functions.o \
./src/community.o \
./src/flashtest.o \
./src/led.o \
./src/main.o \
./src/memtest.o \
./src/pl_eth_test.o \
./src/platform.o \
./src/sdtest.o 

C_DEPS += \
./src/axu_connector.d \
./src/common_functions.d \
./src/community.d \
./src/flashtest.d \
./src/led.d \
./src/main.d \
./src/memtest.d \
./src/pl_eth_test.d \
./src/platform.d \
./src/sdtest.d 


# Each subdirectory must supply rules for building sources it contributes
src/%.o: ../src/%.c
	@echo 'Building file: $<'
	@echo 'Invoking: ARM v8 gcc compiler'
	aarch64-none-elf-gcc -Wall -O0 -g3 -c -fmessage-length=0 -MT"$@" -I../../boe_factory_fsbl_bsp/psu_cortexa53_0/include -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@)" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


