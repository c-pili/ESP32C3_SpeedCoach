idf_component_register(SRCS "flash_storage.c" INCLUDE_DIRS "include" REQUIRES storage_paths spiffs)
spiffs_create_partition_image(storage "${CMAKE_CURRENT_LIST_DIR}/../../storage_image")
