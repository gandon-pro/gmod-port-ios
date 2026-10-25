#pragma once
// touch_addbutton reconstructs height as width * screenAspect * this value.
inline float GMP_TouchAspect(float x1, float y1, float x2, float y2, int width, int height)
{
    if (x2 <= x1 || y2 <= y1 || width <= 0 || height <= 0) return 1.f;
    return ((y2 - y1) * float(height)) / ((x2 - x1) * float(width));
}
