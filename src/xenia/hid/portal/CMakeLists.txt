add_library(xenia-hid-portal STATIC
  ${CMAKE_CURRENT_SOURCE_DIR}/portal.h
  ${CMAKE_CURRENT_SOURCE_DIR}/portal.cc
  ${CMAKE_CURRENT_SOURCE_DIR}/portal_flags.cc
  ${CMAKE_CURRENT_SOURCE_DIR}/portal_manager.cc
  ${CMAKE_CURRENT_SOURCE_DIR}/virtual_portal.cc
  ${CMAKE_CURRENT_SOURCE_DIR}/portal_slot_state.cc
  ${CMAKE_CURRENT_SOURCE_DIR}/xbox360_portal_protocol.cc
  ${CMAKE_CURRENT_SOURCE_DIR}/figure_image.cc
  ${CMAKE_CURRENT_SOURCE_DIR}/figure_store.cc
  ${CMAKE_CURRENT_SOURCE_DIR}/portal_session_store.cc
  ${CMAKE_CURRENT_SOURCE_DIR}/figure_crypto.cc
  ${CMAKE_CURRENT_SOURCE_DIR}/atomic_file_writer.cc
)

if(WIN32)
  target_sources(xenia-hid-portal PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/atomic_file_writer_win.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/hardware_portal.h
    ${CMAKE_CURRENT_SOURCE_DIR}/hardware_portal.cc
  )
  target_link_libraries(xenia-hid-portal PUBLIC libusb)
else()
  target_sources(xenia-hid-portal PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/atomic_file_writer_posix.cc
  )
endif()
xe_target_defaults(xenia-hid-portal)
target_link_libraries(xenia-hid-portal PUBLIC tomlplusplus)

if(XENIA_BUILD_TESTS)
  add_subdirectory(testing)
endif()
