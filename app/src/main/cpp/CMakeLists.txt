cmake_minimum_required(VERSION 3.22.1)

# 16KB 页大小支持（Android 15+ 新设备兼容，源自 marcomorosi06/DroidWine LGPL-2.1）
add_link_options("-Wl,-z,max-page-size=16384" "-Wl,-z,common-page-size=16384")

add_subdirectory(winlator)
add_subdirectory(vortekrenderer)
add_subdirectory(virglrenderer)
add_subdirectory(midihandler)
add_subdirectory(libadrenotools)
add_subdirectory(gladiorenderer)
