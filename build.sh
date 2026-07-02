#!/usr/bin/env bash
set -e

TOP_DIR="$(cd "$(dirname "$0")" && pwd)"
cd "$TOP_DIR"

NPROC=$(nproc 2>/dev/null || echo 4)

# ---------- 全局路径 ----------

BSSL_DIR="$TOP_DIR/third_party/xquic/third_party/boringssl"
BSSL_BUILD="$BSSL_DIR/build"

XQUIC_DIR="$TOP_DIR/third_party/xquic"
XQUIC_BUILD="$XQUIC_DIR/build"

# -------------------------------------------------------------

# Dependency checks
function dependence_check()
{
    for cmd in cmake make cc git; do
        if ! command -v "$cmd" &>/dev/null; then
            echo "ERROR: '$cmd' not found. Please install it."
            exit 1
        fi
    done

    if ! find /usr/include /usr/local/include -name "event.h" -path "*/event2/*" 2>/dev/null | head -1 | grep -q .; then
        echo "ERROR: libevent headers not found. Install: apt install libevent-dev"
        exit 1
    fi
}

# 子模块初始化
function submodule_init()
{
    # xquic
    if [ ! -f "${XQUIC_DIR}/CMakeLists.txt" ]; then
        echo "WARN: submodule xquic need init..."
        git submodule update --init --recursive
    fi

    # 修改boringssl cmake版本要求（文件存在时才执行）
    local bssl_cmake="$BSSL_DIR/CMakeLists.txt"
    if [ -f "$bssl_cmake" ]; then
        sed -i 's/^cmake_minimum_required(VERSION [0-9.]*)/cmake_minimum_required(VERSION 3.16)/' "$bssl_cmake"
    fi
}

# BoringSSL
function boringssl_compile()
{
    # Clone BoringSSL if not present (not a git submodule of xquic)
    if [ ! -f "$BSSL_DIR/CMakeLists.txt" ]; then
        echo "=== Cloning BoringSSL ==="
        git clone https://github.com/google/boringssl.git "$BSSL_DIR"
    fi

    # 再次确保 cmake 版本要求
    sed -i 's/^cmake_minimum_required(VERSION [0-9.]*)/cmake_minimum_required(VERSION 3.16)/' "$BSSL_DIR/CMakeLists.txt"

    echo "=== Building BoringSSL ==="
    mkdir -p "$BSSL_BUILD"
    if [ ! -f "$BSSL_BUILD/CMakeCache.txt" ]; then
        cmake -S "$BSSL_DIR" -B "$BSSL_BUILD" \
            -DBUILD_SHARED_LIBS=0 \
            -DCMAKE_C_FLAGS="-fPIC" \
            -DCMAKE_CXX_FLAGS="-fPIC"
    fi
    make -C "$BSSL_BUILD" -j"$NPROC" ssl crypto
}

# xquic
function xquic_compile()
{
    echo "=== Building xquic ==="
    mkdir -p "$XQUIC_BUILD"
    # Re-configure if cache is missing OR if required flags weren't enabled in a prior
    # configure (older checkouts had this script without FEC/UNLIMITED flags).
    NEED_CONFIGURE=0
    if [ ! -f "$XQUIC_BUILD/CMakeCache.txt" ]; then
        NEED_CONFIGURE=1
    elif ! grep -q "^XQC_ENABLE_FEC:BOOL=ON" "$XQUIC_BUILD/CMakeCache.txt" \
      || ! grep -q "^XQC_ENABLE_XOR:BOOL=ON" "$XQUIC_BUILD/CMakeCache.txt" \
      || ! grep -q "^XQC_ENABLE_UNLIMITED:BOOL=ON" "$XQUIC_BUILD/CMakeCache.txt"; then
        echo "  Existing xquic build lacks required flags — wiping and reconfiguring"
        rm -rf "$XQUIC_BUILD"
        mkdir -p "$XQUIC_BUILD"
        NEED_CONFIGURE=1
    fi
    if [ "$NEED_CONFIGURE" -eq 1 ]; then
        cmake -S "$XQUIC_DIR" -B "$XQUIC_BUILD" \
            -DCMAKE_BUILD_TYPE=Release \
            -DSSL_TYPE=boringssl \
            -DSSL_PATH="$BSSL_DIR" \
            -DXQC_ENABLE_BBR2=ON \
            -DXQC_ENABLE_UNLIMITED=ON \
            -DXQC_ENABLE_FEC=ON \
            -DXQC_ENABLE_XOR=ON
    fi
    make -C "$XQUIC_BUILD" -j"$NPROC"
}

# mqvpn
function mqvpn_compile()
{
    echo "=== Building mqvpn ==="
    mkdir -p "$TOP_DIR/build"
    if [ ! -f "$TOP_DIR/build/CMakeCache.txt" ]; then
        cmake -S "$TOP_DIR" -B "$TOP_DIR/build" \
            -DCMAKE_BUILD_TYPE=Release \
            -DXQUIC_BUILD_DIR="$XQUIC_BUILD"
    fi
    make -C "$TOP_DIR/build" -j"$NPROC"

    echo ""
    echo "Build complete: $(pwd)/build/mqvpn"
}

# clean
function cleanup()
{
    echo "Cleaning all build directories..."
    rm -rf "$TOP_DIR/build"
    rm -rf "$XQUIC_BUILD"
    rm -rf "$BSSL_BUILD"
    echo "Clean complete."
    exit 0
}

# 主入口
function _main()
{
    [ "$1" = "clean" ] && cleanup

    # 检查依赖
    dependence_check

    # 子模块初始化
    submodule_init

    # boringssl
    boringssl_compile

    # xquic
    xquic_compile

    # mqvpn
    mqvpn_compile
}

_main "$@"
