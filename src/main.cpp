#include <cmath>
#include <string>
#include <iostream>
#include <chrono>
#include <sstream>
#include <iomanip>
#include <memory>
#include <algorithm>
#include <vector>

#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>

#include "imgui.h"
#include "backends/imgui_impl_sdl2.h"
#include "backends/imgui_impl_sdlrenderer2.h"
#include "cat.hpp"

#include <omp.h>

// ===========================
//  CONSTANTS
// ==========================

constexpr float PI = 3.14159265359f;
constexpr float TWO_PI = 2.0f * PI;

// ===========================
//  STRUCTS
// ==========================


struct CLI_Options {
    int width = 0;
    int height = 0;
    int numBoids = 150;
    bool showStats = true;
    bool useParallel = true;
    bool showTrails = false;
    bool useSunset = true;
    bool darkBoids = false;
};

struct RGBA { Uint8 r, g, b, a; };

// ===========================
//  UTILITY FUNCTION
// ==========================
static bool parseInt(const char* s, int& out) {
    if (!s) return false;
    std::istringstream iss(s);
    iss >> out;
    return !iss.fail();
}

static inline Uint8 u8lerp(Uint8 a, Uint8 b, float t) { return (Uint8)(a + (b - a) * t); }

static inline RGBA mix(const RGBA& a, const RGBA& b, float t) {
    return { u8lerp(a.r,b.r,t), u8lerp(a.g,b.g,t), u8lerp(a.b,b.b,t), u8lerp(a.a,b.a,t) };
}

static void drawSunsetGradient(SDL_Renderer* r, int w, int h, float split) {
    const RGBA top    = {255, 136,  0, 255};
    const RGBA mid    = {255,  66, 123, 255};
    const RGBA bottom = { 46,  26,  71, 255};

    const int bandH = 2; 
    for (int y = 0; y < h; y += bandH) {
        float t = (float)y / (float)(h - 1);
        RGBA c = (t < split)
            ? mix(top, mid, t / split)
            : mix(mid, bottom, (t - split) / (1.f - split));
        SDL_SetRenderDrawColor(r, c.r, c.g, c.b, c.a);
        SDL_Rect row{0, y, w, std::min(bandH, h - y)};
        SDL_RenderFillRect(r, &row);
    }
}

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

    Vector2D operator*(float scalar) const {
        return Vector2D(x * scalar, y * scalar);
    }

    Vector2D operator/(float scalar) const {
        return Vector2D(x / scalar, y / scalar);
    }

    void operator+=(const Vector2D& other) {
        x += other.x;
        y += other.y;
    }
    
    void operator-=(const Vector2D& other) {
        x -= other.x;
        y -= other.y;
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

// Retrieve passed on CLI
static CLI_Options parseArgs(int argc, char** argv) {
    CLI_Options opt;

    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        
        // Lambda to consume the next parameters
        auto eat = [&](const std::string& key) {
            if (a == key && i + i < argc) return std::string(argv[++i]);
            if (a.rfind(key + "=", 0) == 0) return a.substr(key.size() + 1);
            return std::string();
        };
        
        // Parses the first argument "n" number of birds
        int num;
        if (parseInt(a.c_str(), num) && num > 0) {
            opt.numBoids = num;
            continue;
        }

        // The rest of the arguments are passed with with flags
        if (auto v = eat("--width");  !v.empty())  parseInt(v.c_str(), opt.width);
        else if (auto v = eat("--height"); !v.empty()) parseInt(v.c_str(), opt.height);
        else if (auto v = eat("--boids"); !v.empty()) parseInt(v.c_str(), opt.numBoids);
        else if (a == "--no-gui") opt.showStats = false;
        else if (a == "--serial") opt.useParallel = false;
        else if (a == "--trails") opt.showTrails = true;
        else if (a == "--sunset")     opt.useSunset = true;
        else if (a == "--no-sunset")  opt.useSunset = false;
        else if (a == "--dark-boids") opt.darkBoids = true;
        // Print help message
        else if (a == "-?" || a == "--help") {
            std::cout << "Uso: flocking [num_boids] [opciones]\n";
            std::cout << "Opciones:\n";
            std::cout << "  --width W       Ancho de ventana\n";
            std::cout << "  --height H      Alto de ventana\n";
            std::cout << "  --boids B       Número de boids\n";
            std::cout << "  --no-gui        Sin overlay GUI\n";
            std::cout << "  --serial        Forzar modo serial (sin OpenMP)\n";
            std::cout << "  --trails        Mostrar estelas\n";
            std::cout << "Ejemplo: flocking 500 --width 1920 --height 1080 --trails\n";
            std::exit(0);
        }
    }

    return opt;
}

// ask for a number from std in, if not provided set fallback valu
static int askInt(const std::string& prompt, int minVal, int fallback) {
    while (true) {
        std::cout << prompt << " (min " << minVal << ", enter para " << fallback << "): ";
        std::string line; std::getline(std::cin, line);
        if (line.empty()) return fallback;
        std::istringstream iss(line);
        int v = 0; iss >> v;
        if (!iss.fail() && v >= minVal) return v;
        std::cout << "Valor inválido, intenta de nuevo.\n";
    }
}

// ==========================
// FLOCK SYSTEM
// ==========================

class Bird {

public:
    Vector2D position;
    Vector2D velocity;
    Vector2D acceleration;
    
    float r;            // Size
    float maxSpeed;     // Maximum speed
    float maxForce;     // Maximum steering force
    
    // Flocking parameters
    float separationRadius;
    float alignmentRadius;
    float cohesionRadius;
    
    // Visual properties
    Uint8 red, green, blue, alpha;

public:
        Bird(float x, float y) {
            position = Vector2D(x, y);
            
            // Random initial velocity
            float angle = static_cast<float>(rand()) / RAND_MAX * TWO_PI;
            velocity = Vector2D::fromAngle(angle) * 2.0f;

            acceleration = Vector2D(0, 0);
            
            r = 4.0f;
            maxSpeed = 2.0f;
            maxForce = 0.03f;

            // Flock parameter initalization
            // Same for all birds
            separationRadius = 25.0f;
            alignmentRadius = 50.0f;
            cohesionRadius = 50.0f;
            
            // Random color with bird-like hues
            red = 150 + rand() % 105;    // 150-255
            green = 100 + rand() % 100;  // 100-200
            blue = 50 + rand() % 100;    // 50-150
            alpha = 255;
        }

        // Sets the Bird new position based on current accelaration, velocity and current coordinates.
        void update() {
            // Update velocity
            velocity += acceleration;
            velocity.limit(maxSpeed);
            position += velocity;
            
            // Reset acceleration
            acceleration *= 0;
        }

        // Update Bird current fields based on flock model ecuations.
        void flock(const std::vector<Bird>& birds, int windowWidth, int windowHeight) {
            Vector2D sep = separate(birds);
            Vector2D ali = align(birds);
            Vector2D coh = cohesion(birds);
            Vector2D bias = environmentalBias(windowWidth, windowHeight);
            
            // Weight the forces
            sep *= 1.5f;   // Avoid collisions (highest priority)
            ali *= 1.0f;   // Match neighbors
            coh *= 1.0f;   // Stay together
            bias *= 0.8f;  // Environmental preference
            
            // Apply forces
            applyForce(sep);
            applyForce(ali);
            applyForce(coh);
            applyForce(bias);
        }

        void applyForce(const Vector2D& force) {
            acceleration += force;
        }

        // A method that calculates and applies a steering force towards a target
        // STEER = DESIRED MINUS VELOCITY
        Vector2D seek(const Vector2D& target) {
            Vector2D desired = target - position;
            desired.normalize();
            desired *= maxSpeed;
            
            Vector2D steer = desired - velocity;
            steer.limit(maxForce);
            return steer;
        }


        // A given unit attempts to move away from neighbors who are too close.
        Vector2D separate(const std::vector<Bird>& birds) {
            Vector2D steer(0, 0);
            int count = 0;
            
            for (const auto& other : birds) {
                float d = Vector2D::distance(position, other.position);
                if (d > 0 && d < separationRadius) {
                    Vector2D diff = position - other.position;
                    diff.normalize();
                    diff /= d; // Weight by distance
                    steer += diff;
                    count++;
                }
            }
            
            if (count > 0) {
                steer /= static_cast<float>(count);
                
                if (steer.magnitude() > 0) {
                    steer.normalize();
                    steer *= maxSpeed;
                    steer -= velocity;
                    steer.limit(maxForce);
                }
            }
            
            return steer;
        }

        // A given unit attempts to move to the center of mass of its neighbors.
        Vector2D cohesion(const std::vector<Bird>& birds) {
            Vector2D sum(0, 0);
            int count = 0;
            
            for (const auto& other : birds) {
                float d = Vector2D::distance(position, other.position);
                if (d > 0 && d < cohesionRadius) {
                    sum += other.position;
                    count++;
                }
            }
            
            if (count > 0) {
                sum /= static_cast<float>(count);
                return seek(sum);
            }
            
            return Vector2D(0, 0);
        }
    

        // A given unit attempts to face the same direction as its neighbors.
        Vector2D align(const std::vector<Bird>& birds) {
            Vector2D sum(0, 0);
            int count = 0;
            
            for (const auto& other : birds) {
                float d = Vector2D::distance(position, other.position);
                if (d > 0 && d < alignmentRadius) {
                    sum += other.velocity;
                    count++;
                }
            }
            
            if (count > 0) {
                sum /= static_cast<float>(count);
                sum.normalize();
                sum *= maxSpeed;
                
                Vector2D steer = sum - velocity;
                steer.limit(maxForce);
                return steer;
            }
            
            return Vector2D(0, 0);
        }

        // Environmental bias - encourages rightward flight in upper half
        Vector2D environmentalBias(int windowWidth, int windowHeight) {
            Vector2D bias(0, 0);
            
            // Encourage rightward movement (like migrating birds)
            bias.x = 0.5f;
            
            // Encourage staying in upper half of screen
            float upperHalf = windowHeight * 0.3f;
            if (position.y > upperHalf) {
                // If in lower half, add upward bias
                float distanceFromTop = (position.y - upperHalf) / upperHalf;
                bias.y = -distanceFromTop * 0.8f;  // Stronger bias the lower you are
            } else {
                // If in upper half, slight downward bias to prevent clustering at top
                bias.y = 0.15f;
            }
            
            // Scale bias based on distance from ideal "flight corridor"
            float idealY = windowHeight * 0.2f;  // Prefer flying at 30% from top
            float distanceFromIdeal = abs(position.y - idealY) / (windowHeight * 0.5f);
            
            Vector2D steer = bias;
            steer.normalize();
            steer *= maxSpeed * (0.3f + distanceFromIdeal * 0.5f);  // Stronger when far from ideal
            steer -= velocity;
            steer.limit(maxForce * 0.5f);  // Gentler than other forces
            
            return steer;
        }

        // Behaviour when birds touch the borders: just draw on the opposite section of the window.
        void borders(int width, int height) {
            if (position.x < -r) position.x = width + r;
            if (position.y < -r) position.y = height + r;
            if (position.x > width + r) position.x = -r;
            if (position.y > height + r) position.y = -r;
        }

        // Draws the birds onto window , based on current fields.
        void render(SDL_Renderer* renderer, bool dark) const {
            // Draw boid as triangle pointing in direction of velocity
            float theta = velocity.heading() + PI / 2;
            
            Vector2D v1(0, -r * 2);
            Vector2D v2(-r, r * 2);
            Vector2D v3(r, r * 2);
            
            // Rotate vertices
            float cosTheta = cos(theta);
            float sinTheta = sin(theta);
            
            auto rotate = [cosTheta, sinTheta](Vector2D& v) {
                float newX = v.x * cosTheta - v.y * sinTheta;
                float newY = v.x * sinTheta + v.y * cosTheta;
                v.x = newX;
                v.y = newY;
            };
            
            rotate(v1);
            rotate(v2);
            rotate(v3);
            
            // Translate to position
            v1 += position;
            v2 += position;
            v3 += position;

            Uint8 R = red, G = green, B = blue, A = alpha;
            if (dark) {
                R = (Uint8)(R * 0.35f);
                G = (Uint8)(G * 0.35f);
                B = (Uint8)(B * 0.45f);
                SDL_SetRenderDrawColor(renderer, R, G, B, A);
            } else {
                // Draw filled triangle (approximate with lines)
                SDL_SetRenderDrawColor(renderer, red, green, blue, alpha);
            }
            
            // Draw triangle outline
            SDL_RenderDrawLine(renderer, static_cast<int>(v1.x), static_cast<int>(v1.y),
                              static_cast<int>(v2.x), static_cast<int>(v2.y));
            SDL_RenderDrawLine(renderer, static_cast<int>(v2.x), static_cast<int>(v2.y),
                              static_cast<int>(v3.x), static_cast<int>(v3.y));
            SDL_RenderDrawLine(renderer, static_cast<int>(v3.x), static_cast<int>(v3.y),
                              static_cast<int>(v1.x), static_cast<int>(v1.y));
            
            // Fill triangle with additional lines
            for (int i = 1; i <= 3; i++) {
                Vector2D p1 = v1 + (v2 - v1) * (i / 4.0f);
                Vector2D p2 = v1 + (v3 - v1) * (i / 4.0f);
                SDL_RenderDrawLine(renderer, static_cast<int>(p1.x), static_cast<int>(p1.y),
                                  static_cast<int>(p2.x), static_cast<int>(p2.y));
            }
        }
};

// Entity responsable for managing a group of birds
class FlockingSystem {
private:
    std::vector<Bird> birds;
    int windowWidth, windowHeight;
    
public:
    FlockingSystem(int width, int height) : windowWidth(width), windowHeight(height) {}
    
    void addBoid(float x, float y) {
        birds.emplace_back(x, y);
    }
    
    void initializeBirds(int numBirds) {
        birds.clear();
        birds.reserve(numBirds);
        
        for (int i = 0; i < numBirds; i++) {
            float x = static_cast<float>(rand()) / RAND_MAX * windowWidth;
            float y = static_cast<float>(rand()) / RAND_MAX * windowHeight;
            addBoid(x, y);
        }
    }
    
    // Serial version - each boid processes neighbors sequentially
    void updateSerial() {
        for (auto& bird : birds) {
            bird.flock(birds, windowWidth, windowHeight);
        }
        
        for (auto& boid : birds) {
            boid.update();
            boid.borders(windowWidth, windowHeight);
        }
    }
    
    // Serial version - each boid processes neighbors 
    void updateParallel() {
        const size_t n = birds.size();
        if (n == 0) return;

        // SoA snapshot to improve locality and avoid false sharing
        std::vector<float> px(n), py(n), vx(n), vy(n);
        #pragma omp parallel for schedule(static)
        for (size_t i = 0; i < n; ++i) {
            px[i] = birds[i].position.x;
            py[i] = birds[i].position.y;
            vx[i] = birds[i].velocity.x;
            vy[i] = birds[i].velocity.y;
        }

        // Buffer for accelerations
        std::vector<float> ax(n, 0.0f), ay(n, 0.0f);

        // Si todos los boids comparten radios (como en tu ctor), cachea radios^2
        // If all of the boids share the same radiius, caches squared radius
        const float sepR2 = birds[0].separationRadius * birds[0].separationRadius;
        const float aliR2 = birds[0].alignmentRadius  * birds[0].alignmentRadius;
        const float cohR2 = birds[0].cohesionRadius   * birds[0].cohesionRadius;

        // Calculate forces in parallel with SoA access
        #pragma omp parallel for schedule(static)
        for (size_t i = 0; i < n; ++i) {
            const float pix = px[i], piy = py[i];
            const float vix = vx[i], viy = vy[i];
            float sep_x = 0.f, sep_y = 0.f; int sep_c = 0;
            float ali_x = 0.f, ali_y = 0.f; int ali_c = 0;
            float coh_x = 0.f, coh_y = 0.f; int coh_c = 0;

            // Neighboors: vectorizable with simd
            #pragma omp simd reduction(+:sep_x, sep_y, sep_c, ali_x, ali_y, ali_c, coh_x, coh_y, coh_c)
            for (size_t j = 0; j < n; ++j) {
                const float dx = pix - px[j];
                const float dy = piy - py[j];
                const float d2 = dx*dx + dy*dy;

                if (d2 > 0.f) {
                    if (d2 < sepR2) {
                        // diff.normalize(); diff/=d  -> invsqrt * inv (equals a /d)
                        const float invd = 1.0f / std::sqrt(d2);
                        sep_x += dx * invd * invd;
                        sep_y += dy * invd * invd;
                        sep_c++;
                    }
                    if (d2 < aliR2) {
                        ali_x += vx[j];
                        ali_y += vy[j];
                        ali_c++;
                    }
                    if (d2 < cohR2) {
                        coh_x += px[j];
                        coh_y += py[j];
                        coh_c++;
                    }
                }
            }

            // Combine into a local "acc" using fast limit version
            auto fast_limit = [](float& x, float& y, float maxMag) {
                const float s2 = x*x + y*y;
                const float m2 = maxMag * maxMag;
                if (s2 > m2 && s2 > 0.f) {
                    const float inv = maxMag / std::sqrt(s2);
                    x *= inv; y *= inv;
                }
            };

            float acc_x = 0.f, acc_y = 0.f;

            // Separation (weight 1.5)
            if (sep_c > 0) {
                float sx = sep_x / sep_c, sy = sep_y / sep_c;
                const float s2 = sx*sx + sy*sy;
                if (s2 > 0.f) {
                    const float inv = 1.0f / std::sqrt(s2);
                    sx *= inv; sy *= inv;
                    sx *= birds[i].maxSpeed; sy *= birds[i].maxSpeed;
                    sx -= vix; sy -= viy;
                    fast_limit(sx, sy, birds[i].maxForce);
                    acc_x += 1.5f * sx;
                    acc_y += 1.5f * sy;
                }
            }

            // Alignment
            if (ali_c > 0) {
                float axm = ali_x / ali_c, aym = ali_y / ali_c;
                const float s2 = axm*axm + aym*aym;
                if (s2 > 0.f) {
                    const float inv = 1.0f / std::sqrt(s2);
                    axm *= inv; aym *= inv;
                    axm *= birds[i].maxSpeed; aym *= birds[i].maxSpeed;
                    axm -= vix; aym -= viy;
                    fast_limit(axm, aym, birds[i].maxForce);
                    acc_x += axm;
                    acc_y += aym;
                }
            }

            // Cohesion
            if (coh_c > 0) {
                const float tx = coh_x / coh_c, ty = coh_y / coh_c;
                float dx = tx - pix, dy = ty - piy;
                const float s2 = dx*dx + dy*dy;
                if (s2 > 0.f) {
                    const float inv = 1.0f / std::sqrt(s2);
                    dx *= inv; dy *= inv;
                    dx *= birds[i].maxSpeed; dy *= birds[i].maxSpeed;
                    dx -= vix; dy -= viy;
                    fast_limit(dx, dy, birds[i].maxForce);
                    acc_x += dx;
                    acc_y += dy;
                }
            }

            // Environmental bias
            {
                float bx = 0.5f, by;
                const float upperHalf = windowHeight * 0.3f;
                if (piy > upperHalf) {
                    const float distanceFromTop = (piy - upperHalf) / upperHalf;
                    by = -distanceFromTop * 0.8f;
                } else {
                    by = 0.15f;
                }
                const float idealY = windowHeight * 0.2f;
                const float distanceFromIdeal = std::abs(piy - idealY) / (windowHeight * 0.5f);

                // steer = norm(bias) * maxSpeed*(0.3 + d*0.5) - v
                float bsx = bx, bsy = by;
                const float b2 = bsx*bsx + bsy*bsy;
                if (b2 > 0.f) {
                    const float inv = 1.0f / std::sqrt(b2);
                    bsx *= inv; bsy *= inv;
                    const float speed = birds[i].maxSpeed * (0.3f + distanceFromIdeal * 0.5f);
                    bsx *= speed; bsy *= speed;
                    bsx -= vix;   bsy -= viy;
                    fast_limit(bsx, bsy, birds[i].maxForce * 0.5f);
                    acc_x += 0.8f * bsx;
                    acc_y += 0.8f * bsy;
                }
            }

            ax[i] = acc_x;
            ay[i] = acc_y;
        }

        // Parallel integration (only one operation per bird)
        #pragma omp parallel for schedule(static)
        for (size_t i = 0; i < n; ++i) {
            birds[i].acceleration.x += ax[i];
            birds[i].acceleration.y += ay[i];
            birds[i].update();
            birds[i].borders(windowWidth, windowHeight);
        }
    }

    
    void render(SDL_Renderer* renderer, bool darkBoids) {
        for (const auto& bird : birds) {
            bird.render(renderer, darkBoids);
        }
    }
    
    // handle window resize
    void resize(int width, int height) {
        windowWidth = width;
        windowHeight = height;
    }
    
    size_t getBoidCount() const { return birds.size(); }
    
    void addBoids(int count) {
        for (int i = 0; i < count; i++) {
            float x = static_cast<float>(rand()) / RAND_MAX * windowWidth;
            float y = static_cast<float>(rand()) / RAND_MAX * windowHeight;
            addBoid(x, y);
        }
    }
    
    void removeBoids(int count) {
        if (count >= static_cast<int>(birds.size())) {
            birds.clear();
        } else {
            birds.erase(birds.end() - count, birds.end());
        }
    }
    
    // Calculate average velocity magnitude for stats
    float getAverageSpeed() const {
        if (birds.empty()) return 0.0f;
        double total = 0.0;
        #pragma omp parallel for reduction(+:total) schedule(static)
        for (size_t i = 0; i < birds.size(); ++i)
            total += birds[i].velocity.magnitude();
        return static_cast<float>(total / birds.size());
    }
    
    // Calculate flock coherence (how tightly grouped they are)
    float getCoherence() const {
        const size_t n = birds.size();
        if (n < 2) return 0.0f;

        // Centro
        // Center
        double cx = 0.0, cy = 0.0;
        #pragma omp parallel for reduction(+:cx,cy) schedule(static)
        for (size_t i = 0; i < n; ++i) {
            cx += birds[i].position.x;
            cy += birds[i].position.y;
        }
        cx /= n; cy /= n;

        // Average distance to center
        double totalDist = 0.0;
        #pragma omp parallel for reduction(+:totalDist) schedule(static)
        for (size_t i = 0; i < n; ++i) {
            const float dx = birds[i].position.x - static_cast<float>(cx);
            const float dy = birds[i].position.y - static_cast<float>(cy);
            totalDist += std::sqrt(dx*dx + dy*dy);
        }
        return static_cast<float>(totalDist / n);
    }
};

// ==========================
// MAIN
// ==========================
int main(int argc, char** argv) {

    CLI_Options opt = parseArgs(argc, argv);

    if (opt.width <= 0)  opt.width  = askInt("Ancho de la ventana", 640, 1280);
    if (opt.height <= 0) opt.height = askInt("Alto de la ventana",  480, 720);

    if (opt.width  < 640) opt.width  = 640;
    if (opt.height < 480) opt.height = 480;

    std::cout << "Iniciando simulación de flocking con " << opt.numBoids << " boids...\n";
    std::cout << "Modo: " << (opt.useParallel ? "Paralelo (OpenMP)" : "Serial") << "\n";
    if (opt.showTrails) std::cout << "Estelas activadas\n";

    // Initialize SDL
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) != 0) {
        std::cerr << "[Error] SDL_Init: " << SDL_GetError() << std::endl;
        return 1;
    }

    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "0");

    SDL_Window* window = SDL_CreateWindow("Flocking Birds Simulation",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        opt.width, opt.height, SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE);
    
    if (!window) {
        std::cerr << "[Error] SDL_CreateWindow: " << SDL_GetError() << std::endl;
        SDL_Quit();
        return 1;
    }

    // Instantiation of the renderer, controller that let us interact with sdl window.
    SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, 
        SDL_RENDERER_SOFTWARE);

    if (!renderer) {
        std::cerr << "[Error] SDL_CreateRenderer: " << SDL_GetError() << std::endl;
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    if ((IMG_Init(IMG_INIT_PNG) & IMG_INIT_PNG) == 0) {
        std::cerr << "[Warn] IMG_Init PNG: " << IMG_GetError() << "\n";
    }

    Cat cat;
    cat.scale = 3.f;
    cat.speed = 140.f;

    // Idle: 4 frames
    cat.loadStrip(renderer, CatState::IdleBack,
                "assets/cat_idle.png", 31, 36, /*frames=*/1, /*frameDur=*/0.35f);

    // Walk left: 4 frames
    cat.loadStrip(renderer, CatState::WalkLeft,
        "assets/cat_walk_left.png", 31, 36, /*frames=*/4, /*frameDur=*/0.10f,
        0, 0, 0);

    // Walk right: 4 frames
    cat.loadStrip(renderer, CatState::WalkRight,
        "assets/cat_walk_right.png", 31, 36, /*frames=*/4, /*frameDur=*/0.10f,
        0, 0, 0);

    cat.strips[CatState::WalkUp] = cat.strips[CatState::IdleBack];

    cat.placeAtBottom(opt.width, opt.height);

    // Initialize ImGui if GUI is enabled
    if (opt.showStats) {
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO(); (void)io;
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
        
        ImGui::StyleColorsDark();
        
        ImGui_ImplSDL2_InitForSDLRenderer(window, renderer);
        ImGui_ImplSDLRenderer2_Init(renderer);
    }

    // Initialize flocking system
    FlockingSystem flock(opt.width, opt.height);
    flock.initializeBirds(opt.numBoids);
    
    // Performance tracking
    auto lastTime = std::chrono::high_resolution_clock::now();
    auto lastFlockingTime = std::chrono::microseconds(0);
    auto lastUpdateTime = std::chrono::microseconds(0);
    auto lastRenderTime = std::chrono::microseconds(0);
    
    float fps = 0.0f;
    int frameCount = 0;
    auto lastStatsTime = lastTime;

    bool running = true;
    bool showDetailedStats = false;
    bool paused = false;
    
    // RENDER LOOP
    while (running) {

        auto frameStartTime = std::chrono::high_resolution_clock::now();

        // If there are unprocessed events, procces one at a time.
        SDL_Event event;
        while (SDL_PollEvent(&event)) {

            // if show stats == true, render im gui stats window
            if (opt.showStats) {
                ImGui_ImplSDL2_ProcessEvent(&event);
            }
            
            if (event.type == SDL_QUIT){
                running = false;
            }
            
            else if (event.type == SDL_KEYDOWN) {
                switch (event.key.keysym.sym) {
                    case SDLK_ESCAPE:
                        running = false;
                        break;
                    case SDLK_SPACE:
                        paused = !paused;
                        std::cout << (paused ? "Pausado" : "Reanudado") << "\n";
                        break;
                    case SDLK_p:
                        opt.useParallel = !opt.useParallel;
                        std::cout << "Modo cambiado a: " << (opt.useParallel ? "Paralelo" : "Serial") << "\n";
                        break;
                    case SDLK_t:
                        opt.showTrails = !opt.showTrails;
                        std::cout << "Estelas: " << (opt.showTrails ? "ON" : "OFF") << "\n";
                        break;
                    case SDLK_s:
                        showDetailedStats = !showDetailedStats;
                        break;
                    case SDLK_PLUS:
                    case SDLK_KP_PLUS:
                        flock.addBoids(50);
                        break;
                    case SDLK_MINUS:
                    case SDLK_KP_MINUS:
                        flock.removeBoids(50);
                        break;
                    case SDLK_b:
                        opt.useSunset = !opt.useSunset;
                        std::cout << "Fondo: " << (opt.useSunset ? "Sunset" : "Plano") << "\n";
                        break;
                    case SDLK_c:
                        opt.darkBoids = !opt.darkBoids;
                        std::cout << "Boids: " << (opt.darkBoids ? "Oscuros" : "Originales") << "\n";
                        break;
                }
            }

            // Add boid at mouse position
            else if (event.type == SDL_MOUSEBUTTONDOWN && event.button.button == SDL_BUTTON_LEFT) {
                // Move cat to mouse position
                cat.goTo((float)event.button.x, (float)event.button.y);
                // Add boid at mouse position
                flock.addBoid(static_cast<float>(event.button.x), static_cast<float>(event.button.y));
            }

            // Handle window events gracefully
            else if (event.type == SDL_WINDOWEVENT) {
                if (event.window.event == SDL_WINDOWEVENT_RESIZED) {
                    opt.width = event.window.data1;
                    opt.height = event.window.data2;
                    flock.resize(opt.width, opt.height);
                    cat.placeAtBottom(opt.width, opt.height);
                }
            }
        }

        if (!paused) {
            // Update flocking with timing
            auto flockingStart = std::chrono::high_resolution_clock::now();
            
            if (opt.useParallel) {
                flock.updateParallel();
            } else {
                flock.updateSerial();
            }
            
            auto flockingEnd = std::chrono::high_resolution_clock::now();
            lastFlockingTime = std::chrono::duration_cast<std::chrono::microseconds>(flockingEnd - flockingStart);

            static auto lastT = std::chrono::high_resolution_clock::now();
            auto nowT = std::chrono::high_resolution_clock::now();
            float dt = std::chrono::duration<float>(nowT - lastT).count();
            lastT = nowT;

            cat.update(dt);
            cat.clampToWindow(opt.width, opt.height);
        }

        // Render with timing
        auto lastTime = std::chrono::high_resolution_clock::now();
        const auto startTime = lastTime;
        auto renderStart = std::chrono::high_resolution_clock::now();
    
        if (opt.useSunset) {
            float tsec  = std::chrono::duration<float>(renderStart - startTime).count();
            float split = 0.45f + 0.1f * std::sin(tsec * 0.2f);
            drawSunsetGradient(renderer, opt.width, opt.height, split);
        } else {
            SDL_SetRenderDrawColor(renderer, 20, 25, 40, 255);
            SDL_RenderClear(renderer);
        }

        cat.render(renderer);

        flock.render(renderer, opt.darkBoids);

        // Render ImGui overlay
        if (opt.showStats) {
            ImGui_ImplSDLRenderer2_NewFrame();
            ImGui_ImplSDL2_NewFrame();
            ImGui::NewFrame();

            if (showDetailedStats) {
                ImGui::Begin("Flocking Analysis", &showDetailedStats);
                ImGui::Text("Boids: %zu", flock.getBoidCount());
                ImGui::Text("FPS: %.1f", fps);
                ImGui::Text("Flocking: %ld μs", lastFlockingTime.count());
                ImGui::Text("Render: %ld μs", lastRenderTime.count());
                ImGui::Text("Mode: %s", opt.useParallel ? "Parallel" : "Serial");
                ImGui::Text("Avg Speed: %.2f", flock.getAverageSpeed());
                ImGui::Text("Coherence: %.1f", flock.getCoherence());
                ImGui::Text("Status: %s", paused ? "PAUSED" : "Running");
                ImGui::Text("Fondo: %s | Boids: %s",
                            opt.useSunset ? "Sunset" : "Plano",
                            opt.darkBoids ? "Oscuros" : "Originales");
                ImGui::Text("B: fondo | C: color boids");
                ImGui::Separator();
                ImGui::Text("Controls:");
                ImGui::Text("  SPACE: Pause/Resume");
                ImGui::Text("  P: Toggle parallel mode");
                ImGui::Text("  Click: Add boid");
                ImGui::Text("  +/-: Add/remove 50 boids");
                ImGui::End();
            } else {
                ImGui::SetNextWindowPos(ImVec2(10, 10));
                ImGui::SetNextWindowBgAlpha(0.8f);
                ImGui::Begin("Flocking", nullptr, 
                    ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | 
                    ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove);
                ImGui::Text("Boids: %zu | FPS: %.0f | %s", 
                           flock.getBoidCount(), fps, 
                           opt.useParallel ? "PAR" : "SER");
                ImGui::Text("SPACE: pause | P: mode | S: stats");
                if (paused) ImGui::TextColored(ImVec4(1,1,0,1), "PAUSED");
                ImGui::End();
            }

            ImGui::Render();
            // Flush im gui data to framebuffer.
            ImGui_ImplSDLRenderer2_RenderDrawData(ImGui::GetDrawData(), renderer);
        }

        // FINALLY, SHOW CURRENT FRAMEBUFFER ON WINDOW!
        SDL_RenderPresent(renderer);
        
        auto renderEnd = std::chrono::high_resolution_clock::now();
        lastRenderTime = std::chrono::duration_cast<std::chrono::microseconds>(renderEnd - renderStart);

        // Calculate FPS
        frameCount++;
        auto currentTime = std::chrono::high_resolution_clock::now();
        auto statsElapsed = std::chrono::duration<float>(currentTime - lastStatsTime).count();

        if (statsElapsed >= 1.0f) {
            fps = frameCount / statsElapsed;
            frameCount = 0;
            lastStatsTime = currentTime;
            
            // if (!opt.showStats) {
            //     std::cout << "\rBoids: " << flock.getBoidCount() 
            //              << " | FPS: " << static_cast<int>(fps)
            //              << " | Flocking: " << lastFlockingTime.count() << "μs"
            //              << " | " << (opt.useParallel ? "PAR" : "SER")
            //              << " | " << (paused ? "PAUSED" : "Running")
            //              << std::flush;
            // }
        }
        
        lastTime = frameStartTime;

    }

    // Resources cleanup
    if (opt.showStats) {
        ImGui_ImplSDLRenderer2_Shutdown();
        ImGui_ImplSDL2_Shutdown();
        ImGui::DestroyContext();
    }

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    IMG_Quit();
    SDL_Quit();

    if (!opt.showStats) std::cout << "\n";
    std::cout << "Simulación terminada.\n";


    return 0;
}