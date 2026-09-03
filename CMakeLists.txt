cmake_minimum_required(VERSION 3.21)

project(depixelator
  VERSION 0.1.0
  DESCRIPTION "Header-only contour tracing for black and white images"
  LANGUAGES CXX)

add_library(depixelator INTERFACE)
add_library(depixelator::depixelator ALIAS depixelator)

target_compile_features(depixelator INTERFACE cxx_std_17)
target_include_directories(depixelator INTERFACE
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:include>)

install(TARGETS depixelator EXPORT depixelatorTargets)

install(DIRECTORY include/ DESTINATION include)

# With no dependencies to resolve, the exported targets file is a complete config package.
install(EXPORT depixelatorTargets
  FILE depixelatorConfig.cmake
  NAMESPACE depixelator::
  DESTINATION share/depixelator)
