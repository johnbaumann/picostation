#pragma once

#include <math.h>
#include <stdint.h>

// For calculating sector at a position in the spiral track/groove
inline int trackToSector(int track) { return (int)(((int64_t)track * 642889 + ((int64_t)track * (track - 1) * 21))>>16); }

inline int sectorsPerTrack(const int track) { return round(track * 0.000616397 + 9); }