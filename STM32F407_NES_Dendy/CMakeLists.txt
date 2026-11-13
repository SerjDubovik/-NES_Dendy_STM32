cmake_minimum_required(VERSION 3.22)

option(LCD_PLAYBACK_DIAGNOSTICS "Compare LCD updates from RAM and SD" OFF)

#
# User is free to modify the file as much as necessary
#

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(gcc-arm-none-eabi)
message("Build CMAKE_MODULE_PATH: " ${CMAKE_MODULE_PATH})

# Core project settings
set(CMAKE_PROJECT_NAME STM32F407_NES_Dendy)
project(${CMAKE_PROJECT_NAME})
enable_language(C CXX ASM)
message("Build type: " ${CMAKE_BUILD_TYPE})
 
# Setup compiler settings
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)
set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS ON)


# Core MCU flags, CPU type, instruction set and FPU setup
set(cpu_PARAMS 
    -mthumb

    # Other parameters
    # -mcpu, -mfloat, -mfloat-abi, ...
    -mcpu=cortex-m4
    -mfpu=fpv4-sp-d16
    -mfloat-abi=hard
)
    
file(GLOB_RECURSE sources_SRCS CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_LIST_DIR}/Core/Src/*.c
    ${CMAKE_CURRENT_LIST_DIR}/FATFS/App/*.c
    ${CMAKE_CURRENT_LIST_DIR}/FATFS/Target/*.c
    ${CMAKE_CURRENT_LIST_DIR}/Middlewares/Third_Party/FatFs/src/*.c
    ${CMAKE_CURRENT_LIST_DIR}/USB_DEVICE/App/*.c
    ${CMAKE_CURRENT_LIST_DIR}/USB_DEVICE/Target/*.c
    ${CMAKE_CURRENT_LIST_DIR}/Middlewares/ST/STM32_USB_Device_Library/Core/Src/*.c
    ${CMAKE_CURRENT_LIST_DIR}/Middlewares/ST/STM32_USB_Device_Library/Class/CDC/Src/*.c
	${CMAKE_CURRENT_SOURCE_DIR}/startup/startup_stm32f407zetx.s
	${CMAKE_CURRENT_SOURCE_DIR}/startup/system_stm32f4xx.c
)

# Include directories for all compilers
set(include_DIRS
${CMAKE_CURRENT_LIST_DIR}/Core/Inc
${CMAKE_CURRENT_LIST_DIR}/FATFS/App
${CMAKE_CURRENT_LIST_DIR}/FATFS/Target
${CMAKE_CURRENT_LIST_DIR}/Middlewares/Third_Party/FatFs/src
${CMAKE_CURRENT_LIST_DIR}/USB_DEVICE/App
${CMAKE_CURRENT_LIST_DIR}/USB_DEVICE/Target
${CMAKE_CURRENT_LIST_DIR}/Middlewares/ST/STM32_USB_Device_Library/Core/Inc
${CMAKE_CURRENT_LIST_DIR}/Middlewares/ST/STM32_USB_Device_Library/Class/CDC/Inc

)
# Include directories for each compiler
set(include_c_DIRS

)
set(include_cxx_DIRS
)
set(include_asm_DIRS)
    
# Symbols definition for all compilers
set(symbols_SYMB
STM32F407xx
)

# Symbols definition for each compiler
set(symbols_c_SYMB)
set(symbols_cxx_SYMB)
set(symbols_asm_SYMB)

# Link directories and names of libraries
set(link_DIRS)
set(link_LIBS
# ll_stm32f4xx
cmsis_stm32f4xx
hal_stm32f4xx
)

add_subdirectory(drivers/STM32F4xx_HAL_Driver)
add_subdirectory(drivers/CMSIS)

# Linker script
set(linker_script_SRC ${linker_script_SRC}
    ${CMAKE_CURRENT_SOURCE_DIR}/STM32F407VGTX_FLASH.ld
)

set(compiler_OPTS)
# Compiler options

# Linker options
set(linker_OPTS)


# Link directories setup
# Must be before executable is added
link_directories(${CMAKE_PROJECT_NAME} ${link_DIRS})

# Create an executable object type
add_executable(${CMAKE_PROJECT_NAME})

# Add sources to executable
target_sources(${CMAKE_PROJECT_NAME} PUBLIC ${sources_SRCS})

# Add include paths
target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE
    ${include_DIRS}
    $<$<COMPILE_LANGUAGE:C>: ${include_c_DIRS}>
    $<$<COMPILE_LANGUAGE:CXX>: ${include_cxx_DIRS}>
    $<$<COMPILE_LANGUAGE:ASM>: ${include_asm_DIRS}>
)

# Add project symbols (macros)
target_compile_definitions(${CMAKE_PROJECT_NAME} PRIVATE
    ${symbols_SYMB}
    $<$<BOOL:${LCD_PLAYBACK_DIAGNOSTICS}>:LCD_PLAYBACK_DIAGNOSTICS>
    $<$<COMPILE_LANGUAGE:C>: ${symbols_c_SYMB}>
    $<$<COMPILE_LANGUAGE:CXX>: ${symbols_cxx_SYMB}>
    $<$<COMPILE_LANGUAGE:ASM>: ${symbols_asm_SYMB}>

    # Configuration specific
    $<$<CONFIG:Debug>:DEBUG>
    $<$<CONFIG:Release>: >
)



# Add linked libraries
target_link_libraries(${CMAKE_PROJECT_NAME} ${link_LIBS})

# Compiler options
target_compile_options(${CMAKE_PROJECT_NAME} PRIVATE
    ${cpu_PARAMS}
    ${compiler_OPTS}
    # -save-temps
    -Wall
    -Wextra
    -Wpedantic
    -Wno-unused-parameter
    -Wdouble-promotion
    -u _printf_float
    $<$<COMPILE_LANGUAGE:C>: >
    $<$<COMPILE_LANGUAGE:CXX>:

    # -Wno-volatile
    # -Wold-style-cast
    # -Wuseless-cast
    # -Wsuggest-override
    >
    $<$<COMPILE_LANGUAGE:ASM>:-x assembler-with-cpp -MMD -MP>
    $<$<CONFIG:Debug>:-Og -g3 -ggdb>
    $<$<CONFIG:Release>:-Og -g0>
)

# Linker options
if(NOT LCD_PLAYBACK_DIAGNOSTICS)
    target_link_options(${CMAKE_PROJECT_NAME} PRIVATE -Wl,--defsym=__nes_overlay_enabled=1)
endif()
set_property(TARGET ${CMAKE_PROJECT_NAME} APPEND PROPERTY LINK_DEPENDS ${linker_script_SRC})
target_link_options(${CMAKE_PROJECT_NAME} PRIVATE
    -T${linker_script_SRC}
    ${cpu_PARAMS}
    ${linker_OPTS}
    -Wl,-Map=${CMAKE_PROJECT_NAME}.map
    -u _printf_float # STDIO float formatting support (remove if not used)
    --specs=nosys.specs
    -Wl,--start-group
    -lc
    -lm
    -lstdc++
    -lsupc++
    -Wl,--end-group
    -Wl,-z,max-page-size=8 # Allow good software remapping across address space (with proper GCC section making)
    -Wl,--print-memory-usage
)

if(NOT LCD_PLAYBACK_DIAGNOSTICS)
    set(nes_module_dir "${CMAKE_CURRENT_SOURCE_DIR}/Middlewares/Third_Party/InfoNES")
    set(nes_module_linker "${CMAKE_CURRENT_SOURCE_DIR}/cmake/nes_module.ld")
    set(nes_module_packer "${CMAKE_CURRENT_SOURCE_DIR}/cmake/pack_nes_module.ps1")
    set(nes_module_image "${CMAKE_CURRENT_BINARY_DIR}/NES/InfoNES.bin")

    add_executable(InfoNES_module
        "${nes_module_dir}/InfoNES.cpp"
        "${nes_module_dir}/InfoNES_Mapper.cpp"
        "${nes_module_dir}/InfoNES_STM32.cpp"
        "${nes_module_dir}/K6502.cpp"
    )
    target_include_directories(InfoNES_module PRIVATE Core/Inc)
    target_compile_options(InfoNES_module PRIVATE
        ${cpu_PARAMS}
        -O2 -ffreestanding -fno-unwind-tables -fno-asynchronous-unwind-tables
    )
    target_link_options(InfoNES_module PRIVATE
        ${cpu_PARAMS}
        -T${nes_module_linker}
        -nostartfiles -nodefaultlibs
        -Wl,-Map=${CMAKE_CURRENT_BINARY_DIR}/InfoNES_module.map
    )
    target_link_libraries(InfoNES_module PRIVATE c gcc)
    set_property(TARGET InfoNES_module APPEND PROPERTY LINK_DEPENDS "${nes_module_linker}")

    find_program(nes_powershell NAMES powershell pwsh REQUIRED)
    add_custom_command(OUTPUT "${nes_module_image}"
        COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_CURRENT_BINARY_DIR}/NES"
        COMMAND ${CMAKE_OBJCOPY} -O binary --gap-fill=0 $<TARGET_FILE:InfoNES_module>
            "${CMAKE_CURRENT_BINARY_DIR}/InfoNES.raw.bin"
        COMMAND "${nes_powershell}" -NoProfile -NonInteractive -ExecutionPolicy Bypass
            -File "${nes_module_packer}" "${CMAKE_CURRENT_BINARY_DIR}/InfoNES.raw.bin" "${nes_module_image}"
        DEPENDS InfoNES_module "${nes_module_packer}"
        VERBATIM
    )
    add_custom_target(NES_Module_Image DEPENDS "${nes_module_image}")
    add_dependencies(${CMAKE_PROJECT_NAME} NES_Module_Image)
endif()

# Execute post-build to print size, generate hex and bin
add_custom_command(TARGET ${CMAKE_PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_SIZE} $<TARGET_FILE:${CMAKE_PROJECT_NAME}>
    COMMAND ${CMAKE_OBJCOPY} -O ihex $<TARGET_FILE:${CMAKE_PROJECT_NAME}> ${CMAKE_PROJECT_NAME}.hex
    # COMMAND ${CMAKE_OBJCOPY} -O binary $<TARGET_FILE:${CMAKE_PROJECT_NAME}> ${CMAKE_PROJECT_NAME}.bin
)
