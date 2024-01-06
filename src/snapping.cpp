#include <iostream>
#include <cmath>
#include <imgui/imgui.h>

const double PI = 3.14159265358979323846;
const int SNAP_ANGLE_DEG = 15;
const double SNAP_ANGLE = SNAP_ANGLE_DEG * PI / 180;  // xp/180

double distance(ImVec2 p1, ImVec2 p2) {
    double dx = p2.x - p1.x;
    double dy = p2.y - p1.y;
    return std::sqrt(dx * dx + dy * dy);
}

double angle(ImVec2 p1, ImVec2 p2) {
    double dx = p2.x - p1.x;
    double dy = p2.y - p1.y;
    return std::atan2(dy, dx);
}

ImVec2 snap(ImVec2 origin, ImVec2 mouse) {
    double dist = distance(origin, mouse);
    double ang = std::round(angle(origin, mouse) / SNAP_ANGLE) * SNAP_ANGLE;
    double x = origin.x + dist * std::cos(ang);
    double y = origin.y + dist * std::sin(ang);
    return { (float) x, (float) y };
}