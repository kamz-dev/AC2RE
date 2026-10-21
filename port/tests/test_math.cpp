#include "check.h"
#include "scimitar/math.h"

using namespace scimitar;

TEST(yaw_round_trip) {
    for (float yaw : {0.0f, 0.5f, 1.5f, -2.0f, 3.0f}) {
        CHECK_NEAR(yawFromDirection(forwardFromYaw(yaw)), yaw, 1e-5f);
    }
}

TEST(yaw_zero_is_forward_y) {
    Vec3 f = forwardFromYaw(0);
    CHECK_NEAR(f.x, 0.0f, 1e-6f);
    CHECK_NEAR(f.y, 1.0f, 1e-6f);
}

TEST(normalized_zero_stays_zero) {
    CHECK(normalized(Vec3{}) == Vec3{});
    CHECK_NEAR(length(normalized(Vec3{3, 4, 0})), 1.0f, 1e-6f);
}
