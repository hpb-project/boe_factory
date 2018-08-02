################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
LD_SRCS += \
../src/lscript.ld 

C_SRCS += \
../src/atca_test.c \
../src/axu_connector.c \
../src/community.c \
../src/flashtest.c \
../src/led.c \
../src/main.c \
../src/memory_config_g.c \
../src/memorytest.c \
../src/pl_eth_test.c \
../src/platform.c \
../src/sdtest.c 

OBJS += \
./src/atca_test.o \
./src/axu_connector.o \
./src/community.o \
./src/flashtest.o \
./src/led.o \
./src/main.o \
./src/memory_config_g.o \
./src/memorytest.o \
./src/pl_eth_test.o \
./src/platform.o \
./src/sdtest.o 

C_DEPS += \
./src/atca_test.d \
./src/axu_connector.d \
./src/community.d \
./src/flashtest.d \
./src/led.d \
./src/main.d \
./src/memory_config_g.d \
./src/memorytest.d \
./src/pl_eth_test.d \
./src/platform.d \
./src/sdtest.d 


# Each subdirectory must supply rules for building sources it contributes
src/%.o: ../src/%.c
	@echo 'Building file: $<'
	@echo 'Invoking: ARM v8 gcc compiler'
	aarch64-none-elf-gcc -DATCAPRINTF -Wall -O0 -g3 -I/home/luxq/work/boe_factory/boe_factory_test/src/libatca -c -fmessage-length=0 -MT"$@" -I../../boe_factory_fsbl_bsp/psu_cortexa53_0/include -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@)" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


