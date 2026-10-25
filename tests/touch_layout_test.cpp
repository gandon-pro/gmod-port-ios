#include "gmp_touch_layout.h"
#include <cassert>
#include <cmath>
int main() {
    const int screens[][2] = {{2338,1080},{2532,1170},{1920,1080},{1170,2532}};
    const float rects[][4] = {{.86f,.7f,.98f,.9f},{0,.1f,.48f,1},{.04f,.02f,.14f,.13f}};
    for (const auto& screen:screens) for (const auto& r:rects) {
        float a=GMP_TouchAspect(r[0],r[1],r[2],r[3],screen[0],screen[1]);
        assert(std::isfinite(a) && a>0);
        float restored=r[1]+(r[2]-r[0])*float(screen[0])/float(screen[1])*a;
        assert(std::fabs(restored-r[3])<.00001f);
    }
    assert(GMP_TouchAspect(0,0,0,1,2338,1080)==1.f);
}
