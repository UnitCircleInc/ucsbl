# Fault Injection

Fault injection techniques require physical access to the MCU, with some technique requiring direct access to the silicon, which involves removing MCU packing.  Fault injection techniques attempt to introduct instruction fetch/execution or data read/write errors into in the running MCU.  The goal of fault injection techniques is to allow the attacker access to internal details of the MCU (e.g. keys, code stored in internal memory).  Fault injection allows bypassing of integrity checks allowing the running of unathorized code (e.g. bypass FW signature checks or running data as code) that can then read out the keys or code).   Example techniques include:

* Pulsing the voltage on the power pins on the MCU to create "brown out" or "over voltage" conditions that disrupt the logic circuits within the MCU
* Pulsing large voltages, or high currents near the MCU to induce voltages/currents that disrupt the logic circuits within the MCU.
* Pulsing high intensity light (or radiation) on the the MCU silicon elements to induce charge/currents that disrupt the logic circuits within the MCU.
* Modifying the external clocks of the MCU such that it disrupts the logic circuits within the MCU.
* Pulsing voltages into the back side of the MCU silicon (substrate) such that it disrupts the logic circuits within the MCU.

In general each of these attacks requires that the pulse be precisly timed so that only the desired data read/write or instruction fetch/execution is modified.  This way the system behaves normally except for the desired "modification".  To precisely time other pulses, an FPGA with a high speed clock is used to drive the system reset of the MCU and drive the timing/intensity of the pulsing source.  This way repeated attemps can be quickly made an evalauted .  Often system systems include monitoring of MCU behaviour (current draw, radiated emssisions, ...) to allow fast searching of the correct timing/intensity of the pulsing source to achive the desired fault injection that bypasses all the security checks.

# Mitigation Strategies

* Use of random delays to dissrupt the precise timing of the attacks.
* Use of data oriented programming so that integrity checks rely on correct/complete data/executation path to disrupt the attacks that attempt to modify data, or instructions that do not result in a significant change in the PC.
* Ensure representations for success/failture return values have large hamming distance (i.e do not use an encoding like 1 = success, 0 = failure instead success should be 1's complement of failture) to disrupt attacks on return values and testing.
* Force attacker to spend a "long time" between each attempt by delaying key tests during booting.

# Challenges

Modification to instructions that result in significant changes to the PC.  This can be on instructions that were intended to modify the PC (blx, ldm, ...) but end up using an incorrect value to perform the update.  Alternatively it can be modifying fetching/decoding of an instruction converting it to a PC modifying instruction.  In either case, this can allow running of unauthoized code.  Fortunately this is not as easy as it sounds.

# Other Physical Access Attacks

* Modifacation of the silicon to remove HW like protections
* Direclty reading the contents of an embeded memory to gain access to keys or code.


# References

See: [The Hardware Hacking Handbook](https://nostarch.com/hardwarehacking)
