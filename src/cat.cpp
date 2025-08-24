#include "cat.hpp"

Cat::~Cat() {
    for (auto& [_, s] : strips) {
        if (s.tex) SDL_DestroyTexture(s.tex);
        s.tex = nullptr;
    }
}

// Loads a strip from a sprite sheet image
bool Cat::loadStrip(SDL_Renderer* r, CatState st, const char* path,
                    int fw, int fh, int frames, float frameDur,
                    int marginX, int marginY, int spacingX) {
    SDL_Surface* s = IMG_Load(path);
    if (!s) { std::cerr << "IMG_Load(" << path << "): " << IMG_GetError() << "\n"; return false; }
    SDL_Texture* tex = SDL_CreateTextureFromSurface(r, s);
    if (!tex) { std::cerr << "SDL_CreateTextureFromSurface: " << SDL_GetError() << "\n"; SDL_FreeSurface(s); return false; }
    Strip stp;
    stp.tex = tex;
    stp.texW = s->w; stp.texH = s->h;
    stp.fw = fw; stp.fh = fh;
    stp.frames = frames;
    stp.frameDur = frameDur;
    stp.marginX = marginX; stp.marginY = marginY;
    stp.spacingX = spacingX;
    SDL_FreeSurface(s);
    strips[st] = stp;
    return true;
}

// Returns the strip of the current state, or IdleBack if not found
const Cat::Strip& Cat::cur() const {
    auto it = strips.find(state);
    if (it != strips.end() && it->second.tex) return it->second;

    auto itIdle = strips.find(CatState::IdleBack);
    if (itIdle != strips.end()) return itIdle->second;

    static Strip dummy;
    return dummy;
}

// Returns the number of frames in a strip
int Cat::frameCount(const Strip& s) const {
    if (s.frames > 0) return s.frames;

    int usableW = s.texW - s.marginX;
    int perRow  = std::max(0, usableW + s.spacingX) / (s.fw + s.spacingX);
    return std::max(1, perRow);
}

// Places the cat at the bottom of the window
void Cat::placeAtBottom(int /*W*/, int H) {
    const Strip& s = cur();
    y = H - s.fh * scale - 20.f;
}

// Sets a target position for the cat to walk to
void Cat::goTo(float px, float /*py*/) {
    tx = px;

    const Strip& s = cur();
    ty = y + s.fh * scale * 0.5f;
    moving = true;
    pickWalkState();
    frame = 0; tAcc = 0.f;
}

// Picks walking state based on target position
void Cat::pickWalkState() {
    const Strip& s = cur();
    float cx = x + s.fw * scale * 0.5f;
    float cy = y + s.fh * scale * 0.5f;
    float dx = tx - cx, dy = ty - cy;
    if (std::fabs(dx) > std::fabs(dy)) {
        state = (dx < 0) ? CatState::WalkLeft : CatState::WalkRight;
    } else {
        state = CatState::WalkUp;
    }
}

// Updates the cat's position and animation
void Cat::update(float dt) {
    const Strip& s = cur();
    if (moving) {
        float cx = x + s.fw * scale * 0.5f;
        float cy = y + s.fh * scale * 0.5f;
        float dx = tx - cx, dy = ty - cy;
        float d  = std::sqrt(dx*dx + dy*dy);

        if (d <= arriveEps) {
            moving = false;
            state = CatState::IdleBack;
            frame = 0; tAcc = 0.f;
        } else if (d > 0.0001f) {
            float vx = dx / d * speed, vy = dy / d * speed;
            x += vx * dt; y += vy * dt;
        }
    } else {
        state = CatState::IdleBack;
    }

    const Strip& s2 = cur();
    int fcount = frameCount(s2);
    tAcc += dt;
    while (tAcc >= s2.frameDur) {
        tAcc -= s2.frameDur;
        frame = (frame + 1) % fcount;
    }
}

// Renders the cat at its current position
void Cat::render(SDL_Renderer* r) {
    const Strip& s = cur();
    if (!s.tex) return;
    int fcount = frameCount(s);
    int col = frame % fcount;

    int sx = s.marginX + col * (s.fw + s.spacingX);
    int sy = s.marginY; 
    SDL_Rect src{ sx, sy, s.fw, s.fh };

    int dw = int(s.fw * scale), dh = int(s.fh * scale);
    SDL_Rect dst{ int(x), int(y), dw, dh };

    SDL_RenderCopy(r, s.tex, &src, &dst);
}

// Clamps the cat's position to stay within the window
void Cat::clampToWindow(int W, int H) {
    const Strip& s = cur();
    float dw = s.fw * scale, dh = s.fh * scale;
    if (x < 0) x = 0;
    if (y < 0) y = 0;
    if (x + dw > W) x = W - dw;
    if (y + dh > H) y = H - dh;
}