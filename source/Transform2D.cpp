#include "Transform2D.hpp"

#include <cmath>

// --- Factories --------------------------------------------------------------

Transform2D Transform2D::translation(float tx, float ty) noexcept {
    Transform2D t;              // starts as identity
    t.m[2][0] = tx;             // translation lives in the THIRD ROW
    t.m[2][1] = ty;
    return t;
}

Transform2D Transform2D::rotation(float angle_rad) noexcept {
    const float c = std::cos(angle_rad);
    const float s = std::sin(angle_rad);

    Transform2D t;
    // Row-vector convention, positive angle = counter-clockwise.
    t.m[0][0] =  c;  t.m[0][1] = s;
    t.m[1][0] = -s;  t.m[1][1] = c;
    return t;
}

Transform2D Transform2D::scale(float sx, float sy) noexcept {
    Transform2D t;
    t.m[0][0] = sx;
    t.m[1][1] = sy;
    return t;
}

// --- Composition ------------------------------------------------------------

Transform2D Transform2D::operator*(const Transform2D& rhs) const noexcept {
    // Standard matrix product. With row vectors, v * (A * B) == (v * A) * B,
    // so the result applies *this FIRST, then rhs.
    Transform2D result;
    for (int row = 0; row < 3; ++row) {
        for (int col = 0; col < 3; ++col) {
            float sum = 0.0f;
            for (int k = 0; k < 3; ++k) {
                sum += m[row][k] * rhs.m[k][col];
            }
            result.m[row][col] = sum;
        }
    }
    return result;
}

Transform2D& Transform2D::operator*=(const Transform2D& rhs) noexcept {
    // Computed into a temporary first, since the product reads every entry of *this.
    *this = *this * rhs;
    return *this;
}

// --- Application ------------------------------------------------------------

Vector2D Transform2D::transform_point(const Vector2D& point) const noexcept {
    // [x, y, 1] * M: the third row (translation) is added.
    return Vector2D(
        point.x * m[0][0] + point.y * m[1][0] + m[2][0],
        point.x * m[0][1] + point.y * m[1][1] + m[2][1]
    );
}

Vector2D Transform2D::transform_vector(const Vector2D& direction) const noexcept {
    // [x, y, 0] * M: w = 0, so the third row (translation) is ignored.
    return Vector2D(
        direction.x * m[0][0] + direction.y * m[1][0],
        direction.x * m[0][1] + direction.y * m[1][1]
    );
}
