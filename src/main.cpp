#include <cmath>


constexpr float PI = 3.14159265359f;
constexpr float TWO_PI = 2.0f * PI;

struct CLI_Options {
    int width = 0;
    int height = 0;
    int numBoids = 150;
    bool showStats = true;
    bool useParallel = true;
    bool showTrails = false;
};

// Representation of a 2D vector
// provides utility function to operate 
struct Vector2D {
    float x, y;

    // Constructors
    Vector2D() : x(0), y(0) {}
    Vector2D(float x_, float y_) : x(x_), y(y_) {}
    
    // Overload the arithmethic operators to 
    // let sintax like Vec1 + Vec2 instead of Vec1.x + Vec2.x & Vec1.y + Vec2.y
    
    Vector2D operator+(const Vector2D & other) const {
        return Vector2D(x + other.x, y + other.y);
    }

    Vector2D operator-(const Vector2D & other) const {
        return Vector2D(x - other.x, y - other.y);
    }

    Vector2D operator*(const Vector2D & other) const {
        return Vector2D(x * other.x, y * other.y);
    }

    Vector2D operator/(const Vector2D & other) const {
        return Vector2D(x / other.x, y / other.y);
    }

    void operator*=(float scalar) {
        x *= scalar;
        y *= scalar;
    }


    void operator/=(float scalar) {
        x /= scalar;
        y /= scalar;
    }
    
    // UTILITY FUNCTION
    
    float magnitude() const {
        return sqrt(x * x + y * y);
    }
    
    /*Scales a vector to have a magnitud of 1*/
    void normalize() {
        float mag = magnitude();
        if (mag > 0) {
            x /= mag;
            y /= mag;
        }
    }
    
    Vector2D normalized() const {
        Vector2D result = *this;
        result.normalized();
        return result;
    }
    
    /*Limits the scale of a vector to have at most maxMav magnitud*/
    void limit(float maxMag) {
        if (magnitude() > maxMag) {
            normalize();
            *this *= maxMag;
        }
    }
    
    float heading() const {
        return atan2(y, x);
    }

    // Return the distance between 2 scalars.
    static float distance(const Vector2D& a, const Vector2D& b) {
        return (a - b).magnitude();
    }
    
    // Returns an unitary vector from 
    static Vector2D fromAngle( float angle ) { 
        return Vector2D(cos(angle), sin(angle));
    }
};


int main(int argc, char** argv) {
    return 0;
}