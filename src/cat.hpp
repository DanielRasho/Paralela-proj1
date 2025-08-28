#pragma once
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <map>
#include <iostream>
#include <cmath>
#include <algorithm>

// Possible states of the cat
enum class CatState { IdleBack, WalkLeft, WalkRight, WalkUp };

// Cat class representing the animated character
class Cat {

// Structure representing a sprite strip
public:
    struct Strip {
        SDL_Texture* tex = nullptr;
        int texW = 0, texH = 0;   // texture size
        int fw = 31, fh = 36;     // frame size
        int frames = -1;          // number of frames (-1 = auto)
        float frameDur = 0.05f;   // seg x frame
        int marginX = 0, marginY = 0; // margin in texture
        int spacingX = 0;         // horizontal spacing between frames
    };

    // Transform
    float x = 40.f, y = 0.f; // position
    float scale = 3.f; // scale factor
    float speed = 140.f; // pixels per second
    float arriveEps = 6.f; // distance to target to consider "arrived"

    // Animation state
    CatState state = CatState::IdleBack; // current state
    int frame = 0; // current frame
    float tAcc = 0.f; // time accumulator for frame switching
    bool moving = false; // is the cat currently moving?
    float tx = 0.f, ty = 0.f; // target position

    // Sprites for each state
    std::map<CatState, Strip> strips;

public:
    Cat() = default; // default constructor
    ~Cat(); // destructor

    // Loads a strip from a sprite sheet image
    bool loadStrip(SDL_Renderer* r, CatState st, const char* path,
                   int fw, int fh, int frames = -1, float frameDur = 0.12f,
                   int marginX = 0, int marginY = 0, int spacingX = 0);

    void placeAtBottom(int W, int H); // places the cat at the bottom of the window
    void goTo(float px, float py); // sets a target position for the cat to walk to
    void pickWalkState(); // picks walking state based on target position

    void update(float dt); // updates the cat's position and animation
    void render(SDL_Renderer* r); // renders the cat at its current position
    void clampToWindow(int W, int H); // clamps the cat's position to stay within the window

private:
    const Strip& cur() const;      // strip of actual state
    int frameCount(const Strip& s) const; // number of frames in a strip
};