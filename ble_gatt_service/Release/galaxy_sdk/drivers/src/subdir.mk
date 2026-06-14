################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
E:/mwp/202606/VeriHealthi_QEMU_SDK_v3.7_ble_gatt_server_1.0/galaxy_sdk/drivers/src/hal_imu.c \
E:/mwp/202606/VeriHealthi_QEMU_SDK_v3.7_ble_gatt_server_1.0/galaxy_sdk/drivers/src/hal_rtc.c 

C_DEPS += \
./galaxy_sdk/drivers/src/hal_imu.d \
./galaxy_sdk/drivers/src/hal_rtc.d 

OBJS += \
./galaxy_sdk/drivers/src/hal_imu.o \
./galaxy_sdk/drivers/src/hal_rtc.o 


# Each subdirectory must supply rules for building sources it contributes
galaxy_sdk/drivers/src/hal_imu.o: E:/mwp/202606/VeriHealthi_QEMU_SDK_v3.7_ble_gatt_server_1.0/galaxy_sdk/drivers/src/hal_imu.c galaxy_sdk/drivers/src/subdir.mk
	@echo 'Building file: $<'
	@echo 'Invoking: GNU RISC-V Cross C Compiler'
	riscv64-unknown-elf-gcc -march=rv32imafc_xxldsp -mabi=ilp32f -mtune=nuclei-300-series -mcmodel=medlow -mno-save-restore -O2 -ffunction-sections -fdata-sections -fno-common -Werror -Wall -g -I"E:\mwp\202606\VeriHealthi_QEMU_SDK_v3.7_ble_gatt_server_1.0\galaxy_sdk" -I"E:\mwp\202606\VeriHealthi_QEMU_SDK_v3.7_ble_gatt_server_1.0\galaxy_sdk\bsp\include\arch\riscv\n309" -I"E:\mwp\202606\VeriHealthi_QEMU_SDK_v3.7_ble_gatt_server_1.0\galaxy_sdk\bsp\include" -I"E:\mwp\202606\VeriHealthi_QEMU_SDK_v3.7_ble_gatt_server_1.0\galaxy_sdk\config\include" -I"E:\mwp\202606\VeriHealthi_QEMU_SDK_v3.7_ble_gatt_server_1.0\galaxy_sdk\drivers\include" -I"E:\mwp\202606\VeriHealthi_QEMU_SDK_v3.7_ble_gatt_server_1.0\galaxy_sdk\modules\external\riscv_dsp\include" -I"E:\mwp\202606\VeriHealthi_QEMU_SDK_v3.7_ble_gatt_server_1.0\galaxy_sdk\modules\external\riscv_dsp\PrivateInclude" -I"E:\mwp\202606\VeriHealthi_QEMU_SDK_v3.7_ble_gatt_server_1.0\galaxy_sdk\modules\include" -I"E:\mwp\202606\VeriHealthi_QEMU_SDK_v3.7_ble_gatt_server_1.0\galaxy_sdk\os\include" -I"E:\mwp\202606\VeriHealthi_QEMU_SDK_v3.7_ble_gatt_server_1.0\galaxy_sdk\osal\include" -I"E:\mwp\202606\VeriHealthi_QEMU_SDK_v3.7_ble_gatt_server_1.0\galaxy_sdk\prebuilts\bluetooth\health\include" -std=gnu11 -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" -c -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '

galaxy_sdk/drivers/src/hal_rtc.o: E:/mwp/202606/VeriHealthi_QEMU_SDK_v3.7_ble_gatt_server_1.0/galaxy_sdk/drivers/src/hal_rtc.c galaxy_sdk/drivers/src/subdir.mk
	@echo 'Building file: $<'
	@echo 'Invoking: GNU RISC-V Cross C Compiler'
	riscv64-unknown-elf-gcc -march=rv32imafc_xxldsp -mabi=ilp32f -mtune=nuclei-300-series -mcmodel=medlow -mno-save-restore -O2 -ffunction-sections -fdata-sections -fno-common -Werror -Wall -g -I"E:\mwp\202606\VeriHealthi_QEMU_SDK_v3.7_ble_gatt_server_1.0\galaxy_sdk" -I"E:\mwp\202606\VeriHealthi_QEMU_SDK_v3.7_ble_gatt_server_1.0\galaxy_sdk\bsp\include\arch\riscv\n309" -I"E:\mwp\202606\VeriHealthi_QEMU_SDK_v3.7_ble_gatt_server_1.0\galaxy_sdk\bsp\include" -I"E:\mwp\202606\VeriHealthi_QEMU_SDK_v3.7_ble_gatt_server_1.0\galaxy_sdk\config\include" -I"E:\mwp\202606\VeriHealthi_QEMU_SDK_v3.7_ble_gatt_server_1.0\galaxy_sdk\drivers\include" -I"E:\mwp\202606\VeriHealthi_QEMU_SDK_v3.7_ble_gatt_server_1.0\galaxy_sdk\modules\external\riscv_dsp\include" -I"E:\mwp\202606\VeriHealthi_QEMU_SDK_v3.7_ble_gatt_server_1.0\galaxy_sdk\modules\external\riscv_dsp\PrivateInclude" -I"E:\mwp\202606\VeriHealthi_QEMU_SDK_v3.7_ble_gatt_server_1.0\galaxy_sdk\modules\include" -I"E:\mwp\202606\VeriHealthi_QEMU_SDK_v3.7_ble_gatt_server_1.0\galaxy_sdk\os\include" -I"E:\mwp\202606\VeriHealthi_QEMU_SDK_v3.7_ble_gatt_server_1.0\galaxy_sdk\osal\include" -I"E:\mwp\202606\VeriHealthi_QEMU_SDK_v3.7_ble_gatt_server_1.0\galaxy_sdk\prebuilts\bluetooth\health\include" -std=gnu11 -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" -c -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


