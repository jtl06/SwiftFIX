# SimdDetection.cmake
#
# Configure-time probe of the build host's ISA support. We record the results
# in SWIFTFIX_HAVE_* cache variables but do NOT bake them into public compile
# flags — ISA selection in libswiftfix happens at *runtime* via
# preparser/src/dispatch.cpp, not configure time.
#
# The cache variables are exposed so tests that exercise a specific ISA path
# can skip themselves on hosts that lack the instructions.

include(CheckCXXSourceCompiles)
include(CMakePushCheckState)

cmake_push_check_state()

# --- AVX2 ------------------------------------------------------------------
set(CMAKE_REQUIRED_FLAGS "-mavx2")
check_cxx_source_compiles("
    #include <immintrin.h>
    int main() {
        __m256i a = _mm256_set1_epi8(1);
        __m256i b = _mm256_set1_epi8(2);
        return _mm256_extract_epi8(_mm256_add_epi8(a, b), 0) == 3 ? 0 : 1;
    }"
    SWIFTFIX_HAVE_AVX2)

# --- AVX-512 (F + BW for byte-level ops) -----------------------------------
set(CMAKE_REQUIRED_FLAGS "-mavx512f -mavx512bw")
check_cxx_source_compiles("
    #include <immintrin.h>
    int main() {
        __m512i a = _mm512_set1_epi8(1);
        return _mm512_cmpeq_epi8_mask(a, a) ? 0 : 1;
    }"
    SWIFTFIX_HAVE_AVX512)

# --- NEON ------------------------------------------------------------------
set(CMAKE_REQUIRED_FLAGS "")
check_cxx_source_compiles("
    #include <arm_neon.h>
    int main() {
        uint8x16_t a = vdupq_n_u8(1);
        return vgetq_lane_u8(a, 0);
    }"
    SWIFTFIX_HAVE_NEON)

cmake_pop_check_state()
