################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
E:/mwp/mmy/galaxy_sdk/main.c 

C_DEPS += \
./galaxy_sdk/main.d 

OBJS += \
./galaxy_sdk/main.o 


# Each subdirectory must supply rules for building sources it contributes
galaxy_sdk/main.o: E:/mwp/mmy/galaxy_sdk/main.c galaxy_sdk/subdir.mk
	@echo 'Building file: $<'
	@echo 'Invoking: GNU RISC-V Cross C Compiler'
	riscv64-unknown-elf-gcc -march=rv32imafc_xxldsp -mabi=ilp32f -mtune=nuclei-300-series -mcmodel=medlow -mno-save-restore -O2 -ffunction-sections -fdata-sections -fno-common -Werror -Wall -g -I"E:\mwp\mmy\galaxy_sdk" -I"E:\mwp\mmy\galaxy_sdk\bsp\include\arch\riscv\n309" -I"E:\mwp\mmy\galaxy_sdk\bsp\include" -I"E:\mwp\mmy\galaxy_sdk\config\include" -I"E:\mwp\mmy\galaxy_sdk\drivers\include" -I"E:\mwp\mmy\galaxy_sdk\modules\external\riscv_dsp\include" -I"E:\mwp\mmy\galaxy_sdk\modules\external\riscv_dsp\PrivateInclude" -I"E:\mwp\mmy\galaxy_sdk\modules\include" -I"E:\mwp\mmy\galaxy_sdk\os\include" -I"E:\mwp\mmy\galaxy_sdk\osal\include" -std=gnu11 -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" -c -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


