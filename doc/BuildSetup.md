# SBL Build Setup


SBL build environment depends on the following tools being installed:

* [git](https://git-scm.com/downloads) to clone the repo and support development activities
* [uv](https://github.com/astral-sh/uv) for managing python installations
* [direnv](https://direnv.net) for automatically running env on entry to SBL directory
* [what](https://www.gnu.org/software/cssc/manual/what.html) to allow comand line printing of `what` strings (for linux `sudo apt install cssc`, for MacOS already present)
* [nRF Command Line Tools](https://www.nordicsemi.com/Products/Development-tools/nRF-Command-Line-Tools/Download) for `nrfjprog`
* [Arm GCC](https://developer.arm.com/downloads/-/arm-gnu-toolchain-downloads) - version `12.3.Rel1` (version should match `.github/workfows/build.yml`)
* [J-Link](https://www.segger.com/downloads/jlink/) to support `nrfjprog` programming

Then:

```bash
# Clone repo
git clone [SBL repo]
cd sbl

# Optional if arm-none-eabi-gcc not on path
# cat "export ARM_GCC_ROOT=[path-gcc]" > toolpaths

# The following install python and project dependences
# This only needs to be done once
direnv allow

# Install the pre-commit hooks
pre-commmit install
```

---------
__The following has not been tested recently.__

```
# Start with Ubuntu 22.04 LTS - ensure it is up to date
sudo apt update
sudo apt upgrade

# Standard apt tools we need
sudo add-apt-repository ppa:deadsnakes/ppa -y
sudo curl -LsSf https://astral.sh/uv/install.sh | sh
sudo apt install build-essential
sudo apt install libffi-dev
sudo apt install gcc-arm-none-eabi
sudo apt install cssc # To get what(1) utility in /usr/lib/x86_64-linux-gnu/cssc/what
sudo apt install git
sudo apt install direnv
# add eval "$(direnv hook bash)" to ~/.bashrc - see https://direnv.net/docs/hook.html

# Non-standard tools
# Download and install jlink for Ubuntu
# See: https://www.segger.com/downloads/jlink/
# Pick Linux 64bit DEB
# Then install DEB - Using Software Install or apt, e.g.
# sudo apt install ~/Downloads/JLink_Linux_V782d_x86_64.deb

# Download and install nRF Command Line Tools
# See: https://www.nordicsemi.com/Products/Development-tools/nRF-Command-Line-Tools/Download
# Pick Linux x86 64 DEB
# Then install DEB, Using Software Install or apt, e.g.
# sudo apt install ~/Downloads/nrf-command-line-tools_10.18.1_amd64.deb

# git from command line will likely need .ssh configured
# recommend you setup .ssh keys on git hosting service

# Get project and cd to root directory
# All remaining commands expect you to be in the project root
git clone sbl
cd sbl

# One time setup
direnv allow            # Automatically runs .pyenv/bin/activate on dir entry

# OPTIONAL - only needed if making major feature additions to sbl in which case update.sh
# will also need to be updated.
# The needed nRF SDK files are already in src-nordic.
# Download and install rRF SDK 17.1.0 or later in the project directory
# See: https://www.nordicsemi.com/Products/Development-software/nRF5-SDK/Download
(cd ~/Downloads; unzip DeviceDownload.zip)   # To unpack the modules
unzip -d nordic ~/Downloads/nRF5_SDK_17.1.0_ddde560.zip  # To unpack nRF5_SDK
./update.sh # To update src-nordic

# Build the project
make clean
make CONFIG=nrf52840-dk
./test-signing.sh
make erase load-mfi.signed load-sbl-pk

# (In one termainl window) Run the log viewer
make uclog

# (In another terminal window) Try downloading the second app
make fwu-afi.signed
```
