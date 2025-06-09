#pragma once

#include <math.h>
#include <stdint.h>

// For calculating sector at a position in the spiral track/groove
//inline int trackToSector(const int track) { return pow(track, 2) * 0.00031499 + track * 9.0; }

//int sector_length = 16;
//int inner_diameter = 25;
//float track_pitch = 0.0016;
//(2*pi)/ sector length = (2 * 3.1415926) / 16) = 0.392699075
//inline int trackToSector(int track) {
//    return 
//    ((track * 9.8175) + (0.00031416 * pow(track, 2) - 0.00031416 * track));
//}
inline int trackToSector(int track) {
    // Fixed-point 16.16 
   // const int32_t A = 642889;  // 9.8175 * 65536
   // const int32_t B = 21;      // 0.00031416 * 65536

    //int64_t part1 = (int64_t)track * 642889;
    //int64_t part2 = (int64_t)track * (track - 1) * 21;

    return (int)(((int64_t)track * 642889 + ((int64_t)track * (track - 1) * 21))>>16);  // 16.16 to int
}

//inline int sectorsPerTrack(const int track) { return round(track * 0.000616397 + 9); }
//inline int sectorsPerTrack(const int track) { return floor(2 * 3.1415926 * (25 + (0.0016 * track))); }
inline int sectorsPerTrack(const int track) { return floor( track/100 + 157); }
