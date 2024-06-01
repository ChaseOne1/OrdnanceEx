cmake_minimum_required(VERSION 3.5.0)
project(OrdnanceEx VERSION 1.0.0 LANGUAGES C CXX)


add_library(OrdnanceEx SHARED   src/OrdnanceEx.cpp
                                src/COrdnanceSystem.cpp)

set_target_properties(OrdnanceEx PROPERTIES SUFFIX .asi RUNTIME_OUTPUT_DIRECTORY_RELEASE $ENV{GTA_SA_DIR}\\scripts)

target_include_directories(OrdnanceEx PRIVATE   $ENV{PLUGIN_SDK_DIR}\\plugin_sa
                                                $ENV{PLUGIN_SDK_DIR}\\plugin_sa\\game_sa
                                                $ENV{PLUGIN_SDK_DIR}\\shared
                                                $ENV{PLUGIN_SDK_DIR}\\shared\\game)

target_compile_features(OrdnanceEx PRIVATE cxx_std_17)
target_compile_options(OrdnanceEx PRIVATE /MT)
target_compile_definitions(OrdnanceEx PRIVATE   GTASA
                                                PLUGIN_SGV_10US
                                                _CRT_SECURE_NO_WARNINGS
                                                _SILENCE_CXX17_CODECVT_HEADER_DEPRECATION_WARNING
                                                _USE_MATH_DEFINES)

target_link_directories(OrdnanceEx PRIVATE $ENV{PLUGIN_SDK_DIR}\\output\\lib)
target_link_libraries(OrdnanceEx PRIVATE plugin)
target_link_options(OrdnanceEx PRIVATE /LTCG)