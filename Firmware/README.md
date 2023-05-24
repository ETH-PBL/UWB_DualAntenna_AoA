# Firmware

STM32 firmware used for single and double antenna modules. Sources for each
device and usage can be found in the `Core/Src/apps/` folder.

To use, the appropriate application has to be selected (uncommented) in
`Core/Src/apps/applications.h`. For the double antenna module this will be
`APPLICATION_TWR_PDOA_TAG`, for the single antenna module
`APPLICATION_TWR_ANCHOR`. Other applications are for testing purposes.

Additionally, the Qorvo driver package version 04.00.00 has to be added to the
`Drivers/dwt_uwb_driver` folder (required files: `deca_device_api.h`,
`deca_device.c`, `deca_regs.h`, `deca_types.h`, `deca_vals.h`,
`deca_version.h`).
