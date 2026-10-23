# The recreation. src/scimitar/ mirrors the game's own classes (names, namespaces and structure from the symbols);
# src/port/ is the port's own scaffolding (the sim harness, test worlds, finding the install).
# A renderer / viewer comes later as its own target on top of ac2_movement.

add_library(ac2_sdk INTERFACE)
target_include_directories(ac2_sdk INTERFACE ../sdk/include)

add_library(ac2_movement STATIC
  src/scimitar/ai_actor.cpp
  src/scimitar/guidance.cpp
  src/scimitar/ac2/assassin_task.cpp
  src/scimitar/ac2/edge_clamping.cpp
  src/scimitar/ac2/human.cpp
  src/scimitar/ac2/human_ground.cpp
  src/scimitar/ac2/human_in_air.cpp
  src/scimitar/ac2/human_ledge.cpp
  src/port/game_dir.cpp
  src/port/sim.cpp
)
target_include_directories(ac2_movement PUBLIC src)
target_link_libraries(ac2_movement PUBLIC ac2_sdk)

add_executable(ac2sim apps/ac2sim/main.cpp)
target_link_libraries(ac2sim PRIVATE ac2_movement)

add_executable(ac2_tests
  tests/main.cpp
  tests/test_math.cpp
  tests/test_assassin_task.cpp
  tests/test_ground.cpp
  tests/test_ground_turning.cpp
  tests/test_ground_landing.cpp
  tests/test_ground_crouch.cpp
  tests/test_edge_clamping.cpp
  tests/test_guidance.cpp
  tests/test_ledge.cpp
  tests/test_context_switch.cpp
  tests/test_movement.cpp
  tests/test_sim.cpp
  tests/test_sdk.cpp
)
target_link_libraries(ac2_tests PRIVATE ac2_movement)
add_test(NAME ac2_tests COMMAND ac2_tests)
