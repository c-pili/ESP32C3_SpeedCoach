#ifndef TCX_EXPORT_H
#define TCX_EXPORT_H

#include <stdio.h>

void tcx_write_header(FILE* file, const char* start_time_iso, long *duration_pos, long *distance_pos);
void tcx_append_trackpoint(FILE* file, const char* current_time_iso, double lat, double lon, double altitude, int bpm);
void tcx_set_summary(FILE *file, long duration_pos, long distance_pos, float distance_m, float duration_s);
void tcx_close_file(FILE* file);

#endif
