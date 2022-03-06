DESCRIPTION = "hcsr04 gpio driver"

LICENSE = "GPLv3"

LIC_FILES_CHKSUM = "file://${COMMON_LICENSE_DIR}/MIT;md5=0835ade698e0bcf8506ecda2f7b4f302"

inherit module

PR = "r0"

# BPN: Base Name of the recipe (i.e. the name of this recipe file without the version number)
# SRC_URI = "file://Makefile file://${BPN}.c file://test_hcsr04.c"
SRC_URI = "file://Makefile file://${BPN}.c"

S = "${WORKDIR}"