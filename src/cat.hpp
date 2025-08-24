#pragma once
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <map>
#include <iostream>
#include <cmath>
#include <algorithm>

enum class CatState { IdleBack, WalkLeft, WalkRight, WalkUp };

class Cat {
public:
    struct Strip {
        SDL_Texture* tex = nullptr;
        int texW = 0, texH = 0;   // texture size
        int fw = 31, fh = 36;     // frame size
        int frames = -1;          // -1 = auto 
        float frameDur = 0.05f;   // seg x frame
        int marginX = 0, marginY = 0; // margin in texture
        int spacingX = 0;         // horizontal spacing between frames
    };

    // Transform
    float x = 40.f, y = 0.f;
    float scale = 3.f;
    float speed = 140.f;
    float arriveEps = 6.f;

    // Anim
    CatState state = CatState::IdleBack;
    int frame = 0;
    float tAcc = 0.f;
    bool moving = false;
    float tx = 0.f, ty = 0.f;

    // Sprites for each state
    std::map<CatState, Strip> strips;

public:
    Cat() = default;
    ~Cat();

    // Loads a strip from a sprite sheet image
    bool loadStrip(SDL_Renderer* r, CatState st, const char* path,
                   int fw, int fh, int frames = -1, float frameDur = 0.12f,
                   int marginX = 0, int marginY = 0, int spacingX = 0);

    void placeAtBottom(int W, int H);
    void goTo(float px, float py);
    void pickWalkState();

    void update(float dt);
    void render(SDL_Renderer* r);
    void clampToWindow(int W, int H);

private:
    const Strip& cur() const;      // strip of actual state
    int frameCount(const Strip& s) const;
};