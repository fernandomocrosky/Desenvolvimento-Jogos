#include "RigidBody2D.hpp"

#include <cassert>

void RigidBody2D::integrate(float dt) noexcept {
    // @pre dt > 0. The function is noexcept, so a violation cannot throw:
    // it is caught by assert in debug builds and the step is skipped in
    // release builds (written as !(dt > 0) so NaN is rejected too).
    assert(dt > 0.0f && "RigidBody2D::integrate: dt must be > 0");
    if (!(dt > 0.0f)) {
        return;
    }

    // Semi-implicit (symplectic) Euler: velocity first, then position using
    // the just-updated velocity.
    velocity += acceleration * dt;
    position += velocity * dt;
}
