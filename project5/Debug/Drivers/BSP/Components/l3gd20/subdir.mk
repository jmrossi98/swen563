################################################################################
# Automatically-generated file. Do not edit!
# Toolchain: GNU Tools for STM32 (9-2020-q2-update)
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../Drivers/BSP/Components/l3gd20/l3gd20.c 

OBJS += \
./Drivers/BSP/Components/l3gd20/l3gd20.o 

C_DEPS += \
./Drivers/BSP/Components/l3gd20/l3gd20.d 


# Each subdirectory must supply rules for building sources it contributes
Drivers/BSP/Components/l3gd20/l3gd20.o: ../Drivers/BSP/Components/l3gd20/l3gd20.c Drivers/BSP/Components/l3gd20/subdir.mk
	arm-none-eabi-gcc "$<" -mcpu=cortex-m4 -std=gnu11 -g3 -DDEBUG -DUSE_HAL_DRIVER -DSTM32L476xx -c -I../Core/Inc -I../Drivers/STM32L4xx_HAL_Driver/Inc -I../Drivers/STM32L4xx_HAL_Driver/Inc/Legacy -I../Drivers/CMSIS/Device/ST/STM32L4xx/Include -I../Drivers/CMSIS/Include -I"C:/Users/Jake/STM32CubeIDE/workspace_1.5.1/project5/Drivers/BSP/STM32L476G-Discovery" -O0 -ffunction-sections -fdata-sections -Wall -fstack-usage -MMD -MP -MF"Drivers/BSP/Components/l3gd20/l3gd20.d" -MT"$@" --specs=nano.specs -mfpu=fpv4-sp-d16 -mfloat-abi=hard -mthumb -o "$@"

