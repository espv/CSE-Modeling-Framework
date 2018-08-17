cmake_minimum_required(VERSION 3.12)
project(telosb-csw-model)

set(CMAKE_CXX_STANDARD 14)

add_custom_target(
        Build-debug-configuration COMMAND waf-configure-scripts/configure_waf.sh
        WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}/ns-3.19
)
add_custom_target(
        Build-optimized-configuration COMMAND waf-configure-scripts/configure_optimized_waf.sh
        WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}/ns-3.19
)
add_custom_target(
        TelosB COMMAND ./waf
        WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}/ns-3.19
)
