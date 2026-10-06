#include "Vector2D.hpp"

#include <cmath>
#include <stdexcept>

// --- Length -----------------------------------------------------------------

Vector2D Vector2D::normalized() const {
    const float len = length();
    // @pre length() > EPSILON: the zero vector has no defined direction.
    if (!(len > EPSILON)) {
        throw std::domain_error("Vector2D::normalized: vector length must be greater than EPSILON");
    }
    return Vector2D(x / len, y / len);
}

void Vector2D::normalize() {
    // Reuses normalized() so the precondition is checked in a single place.
    *this = normalized();
}

bool Vector2D::equals(const Vector2D& rhs, float tolerance) const noexcept {
    return std::abs(x - rhs.x) <= tolerance && std::abs(y - rhs.y) <= tolerance;
}

// --- Operators --------------------------------------------------------------

Vector2D Vector2D::operator+(const Vector2D& rhs) const noexcept {
    return Vector2D(x + rhs.x, y + rhs.y);
}

Vector2D Vector2D::operator-(const Vector2D& rhs) const noexcept {
    return Vector2D(x - rhs.x, y - rhs.y);
}

Vector2D Vector2D::operator*(float scalar) const noexcept {
    return Vector2D(x * scalar, y * scalar);
}

Vector2D Vector2D::operator/(float scalar) const {
    // @pre std::abs(scalar) > EPSILON.
    if (!(std::abs(scalar) > EPSILON)) {
        throw std::domain_error("Vector2D::operator/: |scalar| must be greater than EPSILON");
    }
    return Vector2D(x / scalar, y / scalar);
}

Vector2D& Vector2D::operator+=(const Vector2D& rhs) noexcept {
    x += rhs.x;
    y += rhs.y;
    return *this;
}

Vector2D& Vector2D::operator-=(const Vector2D& rhs) noexcept {
    x -= rhs.x;
    y -= rhs.y;
    return *this;
}

Vector2D& Vector2D::operator*=(float scalar) noexcept {
    x *= scalar;
    y *= scalar;
    return *this;
}

Vector2D& Vector2D::operator/=(float scalar) {
    // operator/ checks the precondition; on failure *this is left unchanged.
    *this = *this / scalar;
    return *this;
}

/// Left-scalar multiplication (scalar * vector).
Vector2D operator*(float scalar, const Vector2D& vec) noexcept {
    return vec * scalar;
}
