#ifndef SNAPPING_H
#define SNAPPING_H

#include <cmath>
#include <imgui/imgui.h>

const double PI = 3.14159265358979323846;
const int SNAP_ANGLE_DEG = 45;
const double SNAP_ANGLE = SNAP_ANGLE_DEG * PI / 180;

double distance(ImVec2 p1, ImVec2 p2);
double angle(ImVec2 p1, ImVec2 p2);
ImVec2 snap(ImVec2 origin, ImVec2 mouse);

#endif // SNAPPING_H