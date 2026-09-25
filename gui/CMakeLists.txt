enable_language(CXX)
find_package(Qt6 6.4 REQUIRED COMPONENTS Quick QuickControls2)

qt_standard_project_setup()

qt_add_executable(mescaline-gui main.cpp renderer.cpp renderer.h)
qt_add_qml_module(mescaline-gui
  URI Mescaline
  VERSION 1.0
  RESOURCE_PREFIX /qt/qml
  QML_FILES Main.qml
)
target_link_libraries(mescaline-gui PRIVATE Qt6::Quick Qt6::QuickControls2)
add_dependencies(mescaline-gui mescaline)

if(BUILD_TESTING)
  find_package(Qt6 6.4 REQUIRED COMPONENTS Test)
  qt_add_executable(mescaline_gui_test tests/renderer_test.cpp renderer.cpp renderer.h)
  target_link_libraries(mescaline_gui_test PRIVATE Qt6::Quick Qt6::Test)
  add_test(NAME gui COMMAND mescaline_gui_test "$<TARGET_FILE:mescaline>")
  add_test(NAME gui_smoke COMMAND "$<TARGET_FILE:mescaline-gui>"
                            --cli "$<TARGET_FILE:mescaline>" --smoke-test)
  set_tests_properties(gui gui_smoke PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_QUICK_BACKEND=software")
endif()
