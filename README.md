# yocto projects

# Clone whole repository:

  git clone `<repo URL>`
  
  cd yocto

  git submodule init

  git submodule update

# To build the project:

  cd `<yocto path>`
  
  source poky/oe-init-build-env `<project name>`

  e.g. source poky/oe-init-build-env build-rpi-lab3

  bitbake -c clean rpi-test-image

  bitbake rpi-test-image
  
