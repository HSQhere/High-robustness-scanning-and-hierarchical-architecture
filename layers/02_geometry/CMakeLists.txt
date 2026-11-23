add_library(qrscan_geometry STATIC src/crop.cpp)
target_include_directories(qrscan_geometry PUBLIC include ${OpenCV_INCLUDE_DIRS})
target_link_libraries(qrscan_geometry PUBLIC qrscan_common ${OpenCV_LIBS})

add_executable(test_roi_geometry tests/test_crop.cpp)
target_link_libraries(test_roi_geometry PRIVATE qrscan_geometry)
add_test(NAME geometry_crop_contract COMMAND test_roi_geometry)
