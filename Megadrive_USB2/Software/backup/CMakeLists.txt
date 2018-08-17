######################################################################################
# CMake directives
######################################################################################

# Nous voulons un cmake "récent" pour utiliser les dernières fonctionnalités
cmake_minimum_required(VERSION 3.0)

######################################################################################
# Project declaration and options
######################################################################################

# Notre projet est étiqueté Sega_Dumper
project(Sega_Dumper)

# Crée des variables avec les fichiers à compiler
set(SRCS
    main.c
    )

######################################################################################
# CMake
######################################################################################
    
# Notre exécutable
add_executable(main ${SRCS})


######################################################################################
# Dependencies and Definitions
######################################################################################

# Find packages needed to build library

find_package(Libusb1 QUIET)
if (LIBUSB1_FOUND)
  # Une fois la dépendance trouvée, nous l'incluons au projet
message("libusb found")
set(_includes ${LIBUSB1_INCLUDE_DIRS})
set(_libs ${LIBUSB1_LIBRARIES})
target_include_directories(main PUBLIC ${LIBUSB1_INCLUDE_DIRS}})
target_link_libraries (main ${LIBUSB1_LIBRARIES})
include_directories(main ${LIBUSB1_INCLUDE_DIRS})
else ()
  # Sinon, nous affichons un message
  message(FATAL_ERROR "libusb not found")
endif ()






