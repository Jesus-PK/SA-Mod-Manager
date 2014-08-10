# Win32-specific CFLAGS/CXXFLAGS.
# For MinGW compilers.

# Basic platform flags:
# - wchar_t is short.
# - Enable strict type checking in the Windows headers.
# - Set minimum Windows version to Windows 2000. (Windows NT 5.0)
SET(MCRECOVER_C_FLAGS_WIN32 "-fshort-wchar -DSTRICT -D_WIN32_WINNT=0x0500")

# Test for static libgcc/libstdc++.
SET(MODLOADER_EXE_LINKER_FLAGS_WIN32 "")
FOREACH(FLAG_TEST "-static-libgcc" "-static-libstdc++" "-Wl,--large-address-aware" "-Wl,--nxcompat" "-Wl,--dynamicbase" "-Wl,--tsaware")
	# CMake doesn't like "+" characters in variable names.
	STRING(REPLACE "+" "_" FLAG_TEST_VARNAME "${FLAG_TEST}")

	CHECK_C_COMPILER_FLAG("${FLAG_TEST}" LDFLAG_${FLAG_TEST_VARNAME})
	IF(LDFLAG_${FLAG_TEST_VARNAME})
		SET(MODLOADER_EXE_LINKER_FLAGS_WIN32 "${MODLOADER_EXE_LINKER_FLAGS_WIN32} ${FLAG_TEST}")
	ENDIF(LDFLAG_${FLAG_TEST_VARNAME})
	UNSET(LDFLAG_${FLAG_TEST_VARNAME})
	UNSET(FLAG_TEST_VARNAME)
ENDFOREACH()

# Enable windres support on MinGW.
# http://www.cmake.org/Bug/view.php?id=4068
IF(MINGW)
	SET(CMAKE_RC_COMPILER_INIT windres)
	ENABLE_LANGUAGE(RC)
	
	# NOTE: Setting CMAKE_RC_OUTPUT_EXTENSION doesn't seem to work.
	# Force windres to output COFF, even though it'll use the .res extension.
	SET(CMAKE_RC_OUTPUT_EXTENSION .obj)
	SET(CMAKE_RC_COMPILE_OBJECT
		"<CMAKE_RC_COMPILER> --output-format=coff <FLAGS> <DEFINES> -o <OBJECT> <SOURCE>")
ENDIF(MINGW)

# Append the CFLAGS and LDFLAGS.
SET(MODLOADER_C_FLAGS_COMMON "${MODLOADER_C_FLAGS_COMMON} ${MODLOADER_C_FLAGS_WIN32}")
SET(MODLOADER_CXX_FLAGS_COMMON "${MODLOADER_CXX_FLAGS_COMMON} ${MODLOADER_C_FLAGS_WIN32}")
SET(MODLOADER_EXE_LINKER_FLAGS_COMMON "${MODLOADER_EXE_LINKER_FLAGS_COMMON} ${MODLOADER_EXE_LINKER_FLAGS_WIN32}")
UNSET(MODLOADER_C_FLAGS_WIN32)
UNSET(MODLOADER_EXE_LINKER_FLAGS_WIN32)
