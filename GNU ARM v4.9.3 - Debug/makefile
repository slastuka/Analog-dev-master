################################################################################
# Automatically-generated file. Do not edit!
################################################################################

-include ../makefile.init

RM := rm -rf

# All of the sources participating in the build are defined here
-include sources.mk
-include src/subdir.mk
-include emlib/subdir.mk
-include CMSIS/EFM32WG/subdir.mk
-include subdir.mk
-include objects.mk

ifneq ($(MAKECMDGOALS),clean)
ifneq ($(strip $(C_DEPS)),)
-include $(C_DEPS)
endif
endif

-include ../makefile.defs

# Add inputs and outputs from these tool invocations to the build variables 

# All Target
all: Analog-dev-master.axf

# Tool invocations
Analog-dev-master.axf: $(OBJS) $(USER_OBJS)
	@echo 'Building target: $@'
	@echo 'Invoking: GNU ARM C Linker'
	arm-none-eabi-gcc -g -gdwarf-2 -mcpu=cortex-m4 -mthumb -T "Analog-dev-master.ld" -Xlinker --gc-sections -Xlinker -Map="Analog-dev-master.map" -mfpu=fpv4-sp-d16 -mfloat-abi=softfp --specs=nano.specs -o Analog-dev-master.axf "./CMSIS/EFM32WG/startup_efm32wg.o" "./CMSIS/EFM32WG/system_efm32wg.o" "./emlib/em_cmu.o" "./emlib/em_emu.o" "./emlib/em_gpio.o" "./emlib/em_system.o" "./emlib/em_timer.o" "./emlib/em_usart.o" "./src/epsilometer_analog.o" "./src/epsilometer_coms.o" "./src/epsilometer_main.o" -Wl,--start-group -lgcc -lc -lnosys -Wl,--end-group
	@echo 'Finished building target: $@'
	@echo ' '

	@echo 'Building hex file: Analog-dev-master.hex'
	arm-none-eabi-objcopy -O ihex "Analog-dev-master.axf" "Analog-dev-master.hex"
	@echo ' '

	@echo 'Building bin file: Analog-dev-master.bin'
	arm-none-eabi-objcopy -O binary "Analog-dev-master.axf" "Analog-dev-master.bin"
	@echo ' '

	@echo 'Building s37 file: Analog-dev-master.s37'
	arm-none-eabi-objcopy -O srec "Analog-dev-master.axf" "Analog-dev-master.s37"
	@echo ' '

	@echo 'Running size tool'
	arm-none-eabi-size "Analog-dev-master.axf"
	@echo ' '

# Other Targets
clean:
	-$(RM) $(EXECUTABLES)$(OBJS)$(C_DEPS) Analog-dev-master.axf
	-@echo ' '

.PHONY: all clean dependents
.SECONDARY:

-include ../makefile.targets
