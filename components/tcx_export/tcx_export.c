#include "tcx_export.h"

#include <stdio.h>

void tcx_write_header(FILE *file, const char *start_time_iso, long *duration_pos, long *distance_pos)
{
    if (!file || !start_time_iso) return;
    fprintf(file,
        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
        "<TrainingCenterDatabase xmlns=\"http://www.garmin.com/xmlschemas/TrainingCenterDatabase/v2\" xmlns:ns3=\"http://www.garmin.com/xmlschemas/ActivityExtension/v2\">\n"
        "  <Activities><Activity Sport=\"Other\"><Id>%s</Id>\n"
        "    <Lap StartTime=\"%s\"><TotalTimeSeconds>", start_time_iso, start_time_iso);
    if (duration_pos) *duration_pos = ftell(file);
    fprintf(file, "000000000.0</TotalTimeSeconds><DistanceMeters>");
    if (distance_pos) *distance_pos = ftell(file);
    fprintf(file, "000000000.0</DistanceMeters><Calories>0</Calories><Intensity>Active</Intensity><TriggerMethod>Manual</TriggerMethod><Track>\n");
}

void tcx_set_summary(FILE *file, long duration_pos, long distance_pos, float distance_m, float duration_s)
{
    if (!file) return;
    fseek(file, duration_pos, SEEK_SET);
    fprintf(file, "%011.1f", duration_s);
    fseek(file, distance_pos, SEEK_SET);
    fprintf(file, "%011.1f", distance_m);
    fseek(file, 0, SEEK_END);
}

void tcx_append_trackpoint(FILE *file, const char *time_iso, double lat,
                           double lon, double altitude, int bpm)
{
    if (!file || !time_iso) return;
    fprintf(file, "      <Trackpoint><Time>%s</Time>", time_iso);
    if ((lat != 0.0 || lon != 0.0) && lat >= -90.0 && lat <= 90.0 && lon >= -180.0 && lon <= 180.0)
        fprintf(file, "<Position><LatitudeDegrees>%.7f</LatitudeDegrees><LongitudeDegrees>%.7f</LongitudeDegrees></Position>", lat, lon);
    fprintf(file, "<AltitudeMeters>%.1f</AltitudeMeters>", altitude);
    if (bpm > 0 && bpm <= 250)
        fprintf(file, "<HeartRateBpm><Value>%d</Value></HeartRateBpm>", bpm);
    fprintf(file, "</Trackpoint>\n");
    fflush(file);
}

void tcx_close_file(FILE *file)
{
    if (!file) return;
    fprintf(file, "    </Track></Lap></Activity></Activities></TrainingCenterDatabase>\n");
    fflush(file);
    fclose(file);
}
