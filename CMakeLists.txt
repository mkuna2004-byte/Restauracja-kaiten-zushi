cmake_minimum_required (VERSION 3.10)
project ("KaitenZushi")

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# G£ÓWNY PROGRAM (Kierownik)
add_executable(main_restaurant main.cpp common.h)

# Szukamy biblioteki Threads w systemie
find_package(Threads REQUIRED)

# OSOBNE PROGRAMY (potrzebne do wywo³ania przez exec)
add_executable(obsluga obsluga.cpp common.h)
add_executable(klient klient.cpp common.h)
add_executable(kucharz kucharz.cpp common.h)
# Wszystkie musz¹ byæ po³¹czone z bibliotekami systemowymi Linux
target_link_libraries(main_restaurant rt pthread)
target_link_libraries(obsluga rt pthread)
target_link_libraries(klient rt pthread)
target_link_libraries(kucharz rt pthread)