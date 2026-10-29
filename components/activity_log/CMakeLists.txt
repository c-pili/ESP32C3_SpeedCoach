idf_component_register(
    SRCS "activity_log.c"
    INCLUDE_DIRS "include"
    REQUIRES activity sensor_hub storage_paths tcx_export
)
