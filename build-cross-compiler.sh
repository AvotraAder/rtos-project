#!/bin/bash
set -e

# ==== Configuration ====
export PREFIX="$HOME/opt/cross"
export TARGET=i686-elf
export PATH="$PREFIX/bin:$PATH"

BINUTILS_VERSION=2.42
GCC_VERSION=13.2.0

mkdir -p "$HOME/src"
cd "$HOME/src"

# ==== Binutils ====
if [ ! -f "binutils-${BINUTILS_VERSION}.tar.gz" ]; then
    wget "https://ftp.gnu.org/gnu/binutils/binutils-${BINUTILS_VERSION}.tar.gz"
fi
tar -xzf "binutils-${BINUTILS_VERSION}.tar.gz"

mkdir -p build-binutils
cd build-binutils
../binutils-${BINUTILS_VERSION}/configure \
    --target=$TARGET \
    --prefix="$PREFIX" \
    --with-sysroot \
    --disable-nls \
    --disable-werror
make -j2
make install
cd "$HOME/src"

# ==== GCC ====
if [ ! -f "gcc-${GCC_VERSION}.tar.gz" ]; then
    wget "https://ftp.gnu.org/gnu/gcc/gcc-${GCC_VERSION}/gcc-${GCC_VERSION}.tar.gz"
fi
tar -xzf "gcc-${GCC_VERSION}.tar.gz"

mkdir -p build-gcc
cd build-gcc
../gcc-${GCC_VERSION}/configure \
    --target=$TARGET \
    --prefix="$PREFIX" \
    --disable-nls \
    --enable-languages=c \
    --without-headers
make -j2 all-gcc
make -j2 all-target-libgcc
make install-gcc
make install-target-libgcc

echo ""
echo "=================================================="
echo "Cross-compilateur installé avec succès !"
echo "Ajoute cette ligne à ton ~/.bashrc :"
echo "export PATH=\"\$HOME/opt/cross/bin:\$PATH\""
echo "=================================================="
echo ""
echo "$TARGET-gcc --version" 
$PREFIX/bin/$TARGET-gcc --version

