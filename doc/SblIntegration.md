# SBL Commands/Responses

SBL has the following commands/responses (see `sbl.h`).   All commands are initiated using `sbl_run(...)` which will force a SW reset to start SBL.  After completing the operation, SBL will run the application, which can then retrive the response using `sbl_rsp()`.

* `SBL_CMD_INSTALL_APP` - this requests that SBL attempt to install the FW in slot 1.
* `SBL_CMD_RUN_APP` - this requests that SBL restart the existing FW.  This can be used to force updating the SBL state machine when accepting/rejecting a newly installed FW image.


A running FW image can determine the actions of SBL on the most recent reset by calling `sbl_rsp()`:

* `SBL_RSP_NORMAL` - normal restart of existing FW image.
* `SBL_RSP_NEW_FW_FIRST_RUN` - this indicates that the new FW in slot 1 was sucessufully installed and is now running.
* `SBL_RSP_RESTORE_FIRST_RUN` - occurs if an accept of a newly installed FW image was not completed before the reset.  SBL has restored the existing image, which is now running.
* `SBL_RSP_INSTALL_ERROR` - this indicates that the new FW in slot 1 was invalid and not installed.  The existing FW is still running.
* `SBL_RSP_INTERNAL_ERROR` - the internal state of the SBL state machine was corrupted.  The exisiting FW is still running.


# Typical FW update procedure

1. Erase FLASH areas `Application Slot 1 Part 1` and `Application Slot 1 Part 2`.
2. Write FLASH area `Application Slot 1 Part 2` with the binary image of the new FW.
3. Ensure that there is sufficent power reserves (enough for 10min of operation).
4. Call `sbl_run(SBL_CMD_INSTALL_APP)`.
5. ... wait for device to complete a reset cycle ...
6. Call `sbl_rsp()` and verify that it is `SBL_RSP_NEW_FW_FIRST_RUN`.
7. Validate the install (device initialization, file system integrity, ....)
    * If ok, then __ACCEPT__ the new FW image by erasing FLASH area `Application Slot 1 Part 1`.
    * Otherwise, __REJECT__ the FW image by calling `sbl_run(SBL_CMD_RUN_APP)`.


> NOTES:
> * In step 7, after __ACCEPTING__ the image applications may want to also eraseFLASH area `Application Slot 1 Part 2`.  Ideally this is done by a low prioirty process to minimuze the impact to the user.  This speeds up the next FW update, and the erase of both  `Application Slot 1 Part 1` and  `Application Slot 1 Part 2` will be completed.  These FLASH erase operations can take significant time, which can impact user experience if left to when the user request's a FW update.
> * The time between steps 4 and 5 can take up to 60s.
> * In step 7, after __REJECTING__ the image and calling `sbl_run(SBL_CMD_RUN_APP)`, it can take up to 60s before the reset completes and the existing FW in restarted.

# SBL HW Resources

## nRF52840 Target

* `NRF_RNG` - used for fault injeciton mitigation.  Before running the FW, its state corresponds to the reset state.
* `NRF_WDT` - initialized to 5s with all 8 reload registers enabled.  The FW needs to ensure that at least every 5s it write to all 8 reload registers.  For release SBL, WDT is configured to run in `SLEEP` and `HALT` modes.  For developer SBL, WDT is configured to run in `SLEEP` mode only.
* `NRF_POWER->RESETREA` - in the case of `RESETPIN` bit being set, the mitigation described by errata 136 ([nRF52840 Errata](https://docs-be.nordicsemi.com/bundle/errata_nRF52840_Rev3/attach/nRF52840_Rev_3_Errata_v1.3.pdf)) is implemented.
* `NRF_APPROTECT` and `NRF_UICR->APPPROTECT` - in the release version of SBL these are set to prevent access to the chip via SWD interface, in the developer version of SBL these are set to enable access to the chip via the SWD interface.
* `NRF_UICR->DEBUGCTRL` - in the release verison of SBL ITM/ETM and FLASH Patch and Breakpoint (FBP) units are disabled.
* `NRF_ACL->ACL` - is configured to disabled writes/erases to the `Boot Loader`, `Boot Loader State`, `Manufacturing Data` (only if FW type is `AFI`), and `Application Slot 0` areas in FLASH.
* `NRF_POWER->GPREGRET` and `NRF_POWER->GPREGRET2` - are used to communicate commands and responses between SBL and the FW.
* `NRF_RTC0` - is only configured if SBL enters shutdown due to a corrupt FW image in `Application Slot 0`, otherwise it is not configured.
* `NRF_UART0` - is only configured for deveroper SBL for specific targets (e.g. nrf52480-DK) to facilitate the debug and testing of SBL itself.  It is only configured when SBL does something than just run the main FW (e.g. FW update, or roll-back, ...).
