#!/bin/bash

set -e

# use out-of-tree build
mkdir -pv _build
cd _build

# enable nightly mode for CI
if [ "${CI_PIPELINE_SOURCE}" = "schedule" ] || [ "${CI_PIPELINE_SOURCE}" = "web" ]; then
	EXTRA_CONFIG_FLAGS="--enable-nightly"
fi

# when running on gitlab-ci, we are not using a production
# build, so we don't want to use NDEBUG
export CPPFLAGS="${CPPFLAGS} -UNDEBUG"

# configure
${SRC_DIR}/configure \
	--disable-doxygen \
	--disable-python \
	--disable-swig-octave \
	--disable-swig-python \
	--enable-help2man \
	--enable-swig-iface \
	--prefix="${PREFIX}" \
	${EXTRA_CONFIG_FLAGS} \
;

# build
make -j ${CPU_COUNT} V=1 VERBOSE=1

# test
make -j ${CPU_COUNT} V=1 VERBOSE=1 check
