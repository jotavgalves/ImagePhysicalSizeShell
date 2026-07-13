cmake_minimum_required(VERSION 3.24)

project(ImagePhysicalSizeShell
  VERSION 0.1.0
  DESCRIPTION "Windows Shell property extension for physical image sizes"
  LANGUAGES CXX)

if(NOT WIN32)
  message(FATAL_ERROR "ImagePhysicalSizeShell is a Windows-only project.")
endif()

if(NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
  message(FATAL_ERROR "Only x64 builds are supported in this version.")
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_compile_definitions(
  UNICODE
  _UNICODE
  NOMINMAX
  WIN32_LEAN_AND_MEAN
)

if(MSVC)
  add_compile_options(/W4 /permissive- /EHsc /utf-8)
endif()

add_library(ips_common STATIC
  src/common/JsonWriter.cpp
  src/common/RegistryAudit.cpp
  src/common/Settings.cpp
  src/common/Utf.cpp
  src/imaging/ImageInspection.cpp
  src/imaging/MetadataParser.cpp
  src/imaging/PhysicalSize.cpp
)

target_include_directories(ips_common PUBLIC src)
target_link_libraries(ips_common PUBLIC windowscodecs ole32 advapi32)

add_library(ImagePhysicalSizeShell SHARED
  src/handler/ImagePhysicalSizeShell.def
  src/handler/ClassFactory.cpp
  src/handler/DllExports.cpp
  src/handler/Module.cpp
  src/handler/OriginalHandler.cpp
  src/handler/PhysicalSizePropertyHandler.cpp
  src/handler/PropVariantHelpers.cpp
  src/imaging/StreamImageInspection.cpp
)

target_include_directories(ImagePhysicalSizeShell PRIVATE src)
target_link_libraries(ImagePhysicalSizeShell PRIVATE ips_common ole32 propsys shlwapi shell32 windowscodecs)

add_executable(ipsdiag
  src/diagnostic/main.cpp
)

target_link_libraries(ipsdiag PRIVATE ips_common advapi32 ole32 propsys shlwapi shell32 windowscodecs)

add_executable(RegisterSchema
  src/property_schema/RegisterSchema.cpp
)

target_link_libraries(RegisterSchema PRIVATE propsys ole32 shlwapi xmllite)

add_executable(ipsconfigui WIN32
  src/configui/main.cpp
)

target_include_directories(ipsconfigui PRIVATE src)
target_link_libraries(ipsconfigui PRIVATE ips_common comctl32 comdlg32 user32 gdi32 ole32 windowscodecs)

enable_testing()

add_executable(ips_unit_tests
  tests/unit/test_physical_size.cpp
)

target_link_libraries(ips_unit_tests PRIVATE ips_common)
add_test(NAME ips_unit_tests COMMAND ips_unit_tests)

install(TARGETS ipsdiag RUNTIME DESTINATION .)
install(TARGETS ipsconfigui RUNTIME DESTINATION .)
install(TARGETS RegisterSchema RUNTIME DESTINATION .)
install(TARGETS ImagePhysicalSizeShell RUNTIME DESTINATION . LIBRARY DESTINATION .)
install(FILES schema/ImagePhysicalSize.propdesc DESTINATION schema)
