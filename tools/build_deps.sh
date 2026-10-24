#!/bin/bash
set -euo pipefail
root="$(cd "$(dirname "$0")/.." && pwd)"
prefix="$root/build-deps/install"
sdk="$(xcrun --sdk iphoneos --show-sdk-path)"
common=(-DCMAKE_SYSTEM_NAME=iOS -DCMAKE_OSX_SYSROOT="$sdk" -DCMAKE_OSX_ARCHITECTURES=arm64
        -DCMAKE_OSX_DEPLOYMENT_TARGET=15.0 -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX="$prefix"
        -DCMAKE_POLICY_VERSION_MINIMUM=3.5 -DCMAKE_POSITION_INDEPENDENT_CODE=ON
        -DBUILD_SHARED_LIBS=OFF -DCMAKE_XCODE_ATTRIBUTE_CODE_SIGNING_ALLOWED=NO)
build() {
    local name="$1" source="$2"
    shift 2
    cmake -S "$source" -B "$root/build-deps/$name" -G Ninja "${common[@]}" "$@"
    cmake --build "$root/build-deps/$name" --parallel 3
    cmake --install "$root/build-deps/$name"
}
third="$root/deps/engine/thirdparty"
python3 "$root/tools/patch_thirdparty.py" "$third"
build sdl "$root/deps/sdl" -DSDL_SHARED=OFF -DSDL_STATIC=ON -DSDL_TEST=OFF
build jpeg "$third/libjpeg" -DENABLE_SHARED=OFF -DENABLE_STATIC=ON -DWITH_SIMD=OFF -DWITH_TURBOJPEG=OFF
build png "$third/libpng" -DPNG_SHARED=OFF -DPNG_STATIC=ON -DPNG_TESTS=OFF
build freetype "$third/freetype" -DFT_DISABLE_HARFBUZZ=ON -DFT_DISABLE_BZIP2=ON -DFT_DISABLE_BROTLI=ON -DFT_DISABLE_PNG=ON
build curl "$third/curl" -DBUILD_CURL_EXE=OFF -DBUILD_TESTING=OFF -DCMAKE_USE_OPENSSL=OFF -DCMAKE_USE_SECTRANSP=ON -DCURL_DISABLE_LDAP=ON -DCURL_DISABLE_LDAPS=ON -DHAVE_POLL_FINE=0 -DHAVE_POLL_FINE_EXITCODE=1 -DCURL_CA_BUNDLE=none -DCURL_CA_PATH=none
build openal "$third/openal-soft" -DLIBTYPE=STATIC -DALSOFT_UTILS=OFF -DALSOFT_EXAMPLES=OFF -DALSOFT_TESTS=OFF -DALSOFT_REQUIRE_COREAUDIO=ON
# Fontconfig is still used by the fork's FreeType font backend. Cross-compile
# the library; no macOS/Homebrew library is linked into the iOS application.
font="$third/fontconfig"
export PATH="$(brew --prefix gettext)/bin:$PATH"
export ACLOCAL_PATH="$(brew --prefix gettext)/share/aclocal:$(brew --prefix)/share/aclocal"
export LIBTOOLIZE=glibtoolize
(
    cd "$font"
    autoreconf -fi
)
mkdir -p "$root/build-deps/fontconfig"
(
    cd "$root/build-deps/fontconfig"
    export CC="$(xcrun --sdk iphoneos --find clang)"
    export CFLAGS="-arch arm64 -isysroot $sdk -miphoneos-version-min=15.0"
    # Autoconf's CPP recipes do not inherit CFLAGS. Hash-table generation must
    # preprocess with the same target and SDK as the library compilation.
    export CPP="$CC $CFLAGS -E"
    export CPPFLAGS="-I$prefix/include -I$prefix/include/freetype2"
    export LDFLAGS="-arch arm64 -isysroot $sdk -L$prefix/lib"
    export PKG_CONFIG_LIBDIR="$prefix/lib/pkgconfig"
    # iPhoneOS provides zlib/expat but not their pkg-config .pc files. Do not
    # let pkg-config fall back to a Homebrew macOS archive to satisfy them.
    export FREETYPE_CFLAGS="-I$prefix/include/freetype2"
    export FREETYPE_LIBS="-L$prefix/lib -lfreetype -lz"
    export EXPAT_CFLAGS="-isysroot $sdk"
    export EXPAT_LIBS="-lexpat"
    export CC_FOR_BUILD=clang
    export CFLAGS_FOR_BUILD=-O2
    "$font/configure" --host=aarch64-apple-darwin --prefix="$prefix" --disable-shared --enable-static --disable-docs --disable-nls
    # Only the library and its headers belong in the app. Top-level `make`
    # also builds CLI utilities and tests using system(), unavailable on iOS.
    make -C src -j3
    make -C src install-libLTLIBRARIES
    make -C fontconfig install-fontconfigincludeHEADERS
)
find "$prefix/lib" -name '*.a' -maxdepth 2 -print
