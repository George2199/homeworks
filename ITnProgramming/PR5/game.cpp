#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <cmath>
#include <vector>
#include <string>
#include <random>
#include <sstream>
#include <algorithm>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

static const int   WIN_W      = 900;
static const int   WIN_H      = 400;
static const int   GROUND_Y   = 368;   // lower ground
static const float GRAVITY    = 2600.f;
static const float JUMP_VEL   = -760.f;
static const float INIT_SPEED = 280.f;
static const float SPEED_INCR = 7.f;

static const Uint8 BG   = 247;
static const Uint8 DARK =  83;
static const Uint8 MED  = 150;
static const Uint8 LITE = 218;   // background dunes

// ─── Helpers ─────────────────────────────────────────────────────────
static void setCol(SDL_Renderer* r, Uint8 v) { SDL_SetRenderDrawColor(r, v, v, v, 255); }
static void fillRect(SDL_Renderer* r, int x, int y, int w, int h) {
    SDL_Rect rc{x,y,w,h}; SDL_RenderFillRect(r,&rc);
}
static void drawRect(SDL_Renderer* r, int x, int y, int w, int h) {
    SDL_Rect rc{x,y,w,h}; SDL_RenderDrawRect(r,&rc);
}

// ─── Sound ───────────────────────────────────────────────────────────
struct Sound { std::vector<Sint16> buf; int pos=0; bool playing=false; };
static Sound g_jumpSnd, g_dieSnd;
static Sound* g_active=nullptr;
static SDL_AudioDeviceID g_audio=0;

static void audioCallback(void*, Uint8* stream, int len) {
    int n=len/2; auto* out=reinterpret_cast<Sint16*>(stream);
    if (!g_active||!g_active->playing) { SDL_memset(stream,0,len); return; }
    for (int i=0;i<n;++i) {
        if (g_active->pos>=(int)g_active->buf.size()) { out[i]=0; g_active->playing=false; }
        else out[i]=g_active->buf[g_active->pos++];
    }
}
static void makeTone(Sound& s, float freq, float dur, float vol=0.25f, bool sweep=false) {
    const int SR=44100; int N=(int)(SR*dur); s.buf.resize(N);
    for (int i=0;i<N;++i) {
        float t=float(i)/SR, f=sweep?freq-(freq-200.f)*(t/dur):freq;
        s.buf[i]=(Sint16)(vol*32767.f*(1.f-t/dur)*std::sin(2.f*float(M_PI)*f*t));
    }
}
static void playSound(Sound& s) {
    SDL_LockAudioDevice(g_audio); s.pos=0; s.playing=true; g_active=&s; SDL_UnlockAudioDevice(g_audio);
}
static bool initAudio() {
    SDL_AudioSpec w{}; w.freq=44100; w.format=AUDIO_S16LSB; w.channels=1; w.samples=512; w.callback=audioCallback;
    g_audio=SDL_OpenAudioDevice(nullptr,0,&w,nullptr,0); if (!g_audio) return false;
    makeTone(g_jumpSnd, 700.f, 0.10f);
    makeTone(g_dieSnd,  500.f, 0.70f, 0.25f, true);
    SDL_PauseAudioDevice(g_audio,0); return true;
}

// ─── Wheel sprite ────────────────────────────────────────────────────
class WheelSprite {
    std::vector<SDL_Texture*> frames;
    int   cur=0;
    float timer=0.f, frameDur=0.07f;
public:
    int sz=0;
    void build(SDL_Renderer* r, int rad, int nf) {
        sz=rad*2+2;
        for (int f=0;f<nf;++f) {
            SDL_Texture* tex=SDL_CreateTexture(r,SDL_PIXELFORMAT_RGBA8888,SDL_TEXTUREACCESS_TARGET,sz,sz);
            SDL_SetTextureBlendMode(tex,SDL_BLENDMODE_BLEND);
            SDL_SetRenderTarget(r,tex);
            SDL_SetRenderDrawColor(r,0,0,0,0); SDL_RenderClear(r);
            int cx=sz/2, cy=sz/2;
            SDL_SetRenderDrawColor(r,DARK,DARK,DARK,255);
            for (int dy=-rad;dy<=rad;++dy)
                for (int dx=-rad;dx<=rad;++dx) {
                    int d2=dx*dx+dy*dy, inn=(rad-3)*(rad-3);
                    if (d2>=inn&&d2<=rad*rad) SDL_RenderDrawPoint(r,cx+dx,cy+dy);
                }
            float ba=f*(float)M_PI/(2.f*nf);
            for (int s=0;s<4;++s) {
                float a=ba+s*(float)M_PI/2.f;
                SDL_RenderDrawLine(r,cx,cy,cx+(int)((rad-4)*std::cos(a)),cy+(int)((rad-4)*std::sin(a)));
            }
            fillRect(r,cx-2,cy-2,4,4);
            SDL_SetRenderTarget(r,nullptr);
            frames.push_back(tex);
        }
    }
    void update(float dt, float speed) {
        // Spin rate proportional to speed — significantly faster at higher speeds
        frameDur = std::max(0.012f, 48.f / speed);
        timer += dt;
        if (timer>=frameDur) { timer=0; cur=(cur+1)%(int)frames.size(); }
    }
    void draw(SDL_Renderer* r, int cx, int cy) const {
        if (frames.empty()) return;
        SDL_Rect dst{cx-sz/2, cy-sz/2, sz, sz};
        SDL_RenderCopy(r, frames[cur], nullptr, &dst);
    }
    ~WheelSprite() { for (auto* t:frames) SDL_DestroyTexture(t); }
};

// ─── Obstacle ────────────────────────────────────────────────────────
// type: 0=small cactus  1=tall cactus  2=double cactus
//       3=low bird (jump or duck)  4=high bird (run under)
class Obstacle {
public:
    float x; int type;
    float birdTimer=0.f; int birdWing=0; // 0=up 1=mid 2=down 3=mid

    Obstacle(float sx, int t): x(sx), type(t) {}

    bool flying()  const { return type>=3; }
    int  width()   const { if (type==2) return 36; if (type>=3) return 42; return 18; }
    int  height()  const { if (type==1) return 50; if (type==2) return 50; if (type>=3) return 22; return 35; }
    int  topY()    const {
        if (type==3) return GROUND_Y-75;   // low: jump/duck to avoid
        if (type==4) return GROUND_Y-118;  // high: safe to run under
        return GROUND_Y-height();
    }

    void update(float spd, float dt) {
        x -= spd*dt;
        if (flying()) {
            birdTimer += dt;
            if (birdTimer>0.13f) { birdTimer=0; birdWing=(birdWing+1)%4; }
        }
    }
    bool offScreen() const { return x+width()<0; }

    // Hitbox: body only, not wings
    SDL_Rect bounds() const {
        return { (int)x+4, topY()+6, width()-8, 14 };
    }

    void drawOneCactus(SDL_Renderer* r, int lx, int h) const {
        setCol(r, DARK);
        int top=GROUND_Y-h, tw=7, tx=lx+4;
        fillRect(r, tx,       top+14, tw,   h-14);
        fillRect(r, tx-2,     top,    tw+4, 15);
        fillRect(r, tx-8,     top+7,  8,    5);
        fillRect(r, tx-8,     top+2,  4,    11);
        fillRect(r, tx+tw,    top+17, 8,    5);
        fillRect(r, tx+tw+4,  top+11, 4,    11);
    }

    void draw(SDL_Renderer* r) const {
        int lx=(int)x;
        if (type==0) { drawOneCactus(r,lx,35); }
        else if (type==1) { drawOneCactus(r,lx,50); }
        else if (type==2) { drawOneCactus(r,lx,50); drawOneCactus(r,lx+18,38); }
        else {
            // Pterodactyl — beak faces left, wings animate
            setCol(r, DARK);
            int ty=topY(), cx=lx+width()/2;

            // Animated wing offset
            int wOff = (birdWing==0) ? -7 : (birdWing==2) ? 7 : 0;

            // Left wing
            fillRect(r, cx-8,  ty+8+wOff,  14, 5);
            // Right wing
            fillRect(r, cx+4,  ty+9+wOff,  14, 4);

            // Body (fixed)
            fillRect(r, cx-8,  ty+8,  16, 10);
            // Head
            fillRect(r, cx-16, ty+6,  10,  8);
            // Beak
            fillRect(r, cx-23, ty+8,   7,  3);
            // Eye
            setCol(r, BG);
            fillRect(r, cx-14, ty+7,   2,  2);
        }
    }
};

// ─── Player ──────────────────────────────────────────────────────────
class Player {
public:
    float x=100.f, y;
    float velY=0.f;
    bool  grounded=true;
    bool  ducking=false;
    float legPhase=0.f;
    WheelSprite wheel;

    static const int WR = 14;  // wheel radius
    static const int CH = 64;  // total char height

    float groundedY() const { return GROUND_Y-CH; }

    Player(SDL_Renderer* r): y(GROUND_Y-CH) { wheel.build(r,WR,8); }

    bool jump() {
        if (grounded) { velY=JUMP_VEL; grounded=false; ducking=false; return true; }
        return false;
    }
    // Release jump key early → lower arc
    void cutJump() {
        if (!grounded && velY<0) velY *= 0.42f;
    }

    void setDuck(bool d) { ducking=d; }

    // Returns true the exact frame the player touches ground
    bool update(float dt, float spd) {
        bool wasAir = !grounded;
        if (!grounded) {
            velY += GRAVITY*dt;
            y    += velY*dt;
            if (y>=groundedY()) { y=groundedY(); velY=0; grounded=true; }
        }
        if (grounded) legPhase += spd/120.f*dt*6.f;
        wheel.update(dt, spd);
        return wasAir && grounded;
    }

    SDL_Rect bounds() const {
        // Crouched: upper profile removed (ducks under low birds)
        if (ducking && grounded)
            return { (int)x-10, (int)y+22, 44, 16 };
        return { (int)x-8, (int)y+2, 30, 40 };
    }

    // ── Normal dino (Chrome Dino proportions) ─────────────────────
    void drawNormal(SDL_Renderer* r, int PX, int by) const {
        setCol(r, DARK);

        // Tail — massive, 4 segments tapering left
        fillRect(r, PX-26, by+20, 14, 13);
        fillRect(r, PX-38, by+24, 12,  9);
        fillRect(r, PX-46, by+28,  8,  5);
        fillRect(r, PX-52, by+31,  6,  3);

        // Torso — wide and substantial
        fillRect(r, PX-14, by+16, 30, 20);

        // Neck — thick, short
        fillRect(r, PX+6,  by+10, 12,  8);

        // Head — large block extending forward
        fillRect(r, PX+4,  by,    26, 14);

        // Upper snout extension
        fillRect(r, PX+20, by+9,  14,  6);

        // Lower jaw — long chrome-dino snout
        fillRect(r, PX+8,  by+13, 28,  6);

        // Eye
        setCol(r, BG);   fillRect(r, PX+16, by+2, 6, 6);
        setCol(r, DARK); fillRect(r, PX+17, by+3, 4, 4);

        // Small arm holding handlebar
        SDL_RenderDrawLine(r, PX+10, by+20, PX+38, by+13);
    }

    // ── Crouched dino ─────────────────────────────────────────────
    void drawCrouched(SDL_Renderer* r, int PX, int by) const {
        setCol(r, DARK);

        // Tail (stays roughly level)
        fillRect(r, PX-26, by+22, 14, 10);
        fillRect(r, PX-38, by+26, 12,  6);
        fillRect(r, PX-46, by+29,  8,  4);

        // Torso (same spot, slightly shorter)
        fillRect(r, PX-14, by+18, 32, 16);

        // Neck (nearly flat)
        fillRect(r, PX+8,  by+21, 16,  6);

        // Head — drops down, extends forward
        fillRect(r, PX+12, by+14, 26, 12);

        // Upper snout (still prominent)
        fillRect(r, PX+28, by+20, 14,  5);

        // Lower jaw (long, nearly horizontal)
        fillRect(r, PX+16, by+24, 28,  5);

        // Eye
        setCol(r, BG);   fillRect(r, PX+22, by+16, 6, 5);
        setCol(r, DARK); fillRect(r, PX+23, by+17, 4, 3);

        // Arm
        SDL_RenderDrawLine(r, PX+12, by+22, PX+38, by+13);
    }

    void draw(SDL_Renderer* r) const {
        int PX=(int)x, by=(int)y;

        // Wheel centers
        int fwx=PX+36, rwx=PX-20;
        int wy=by+CH-WR;

        // Bike frame
        int bbx=PX+8,  bby=wy+3;
        int sx =PX-6,  sy =by+26;
        int hx =PX+26, hy =by+22;

        setCol(r, DARK);
        SDL_RenderDrawLine(r, rwx, wy,  sx,  sy);   // seat stay
        SDL_RenderDrawLine(r, rwx, wy,  bbx, bby);  // chain stay
        SDL_RenderDrawLine(r, bbx, bby, sx,  sy);   // seat tube
        SDL_RenderDrawLine(r, sx,  sy,  hx,  hy);   // top tube
        SDL_RenderDrawLine(r, hx,  hy,  bbx, bby);  // down tube
        SDL_RenderDrawLine(r, hx,  hy,  fwx, wy);   // fork
        SDL_RenderDrawLine(r, hx,  hy,  PX+38, by+13); // handlebar stem
        fillRect(r, PX+36, by+11, 5, 3);   // grip
        fillRect(r, sx-7,  sy-3,  17, 4);  // seat

        wheel.draw(r, fwx, wy);
        wheel.draw(r, rwx, wy);

        // Pedaling legs — 3-pixel thick, compact
        setCol(r, DARK);
        float lp = legPhase;
        int hx0 = PX-2, hy0 = by+34;   // hip = bottom of torso
        int f1x = bbx+(int)(6*std::cos(lp)),             f1y = bby+(int)(5*std::sin(lp));
        int f2x = bbx+(int)(6*std::cos(lp+(float)M_PI)), f2y = bby+(int)(5*std::sin(lp+(float)M_PI));
        for (int t = -1; t <= 1; ++t) {
            SDL_RenderDrawLine(r, hx0+t, hy0,    f1x+t, f1y);       // thigh 1
            SDL_RenderDrawLine(r, f1x+t, f1y,    f1x+3+t, f1y+4);  // shin 1
            SDL_RenderDrawLine(r, hx0+t, hy0,    f2x+t, f2y);       // thigh 2
            SDL_RenderDrawLine(r, f2x+t, f2y,    f2x+3+t, f2y+4);  // shin 2
        }

        // Dino body (choose pose)
        if (ducking && grounded) drawCrouched(r, PX, by);
        else                     drawNormal(r, PX, by);
    }
};

// ─── Game ─────────────────────────────────────────────────────────────
enum class State { INFO, PLAY, OVER };

class Game {
    SDL_Window*   win  = nullptr;
    SDL_Renderer* rnd  = nullptr;
    TTF_Font*     font = nullptr;   // 17pt — all UI blocks
    TTF_Font*     fBig = nullptr;   // 36pt — title / GAME OVER

    State   state = State::INFO;
    Player* player = nullptr;
    std::vector<Obstacle> obs;

    struct Cloud { float x, y; int w; };
    std::vector<Cloud> clouds;

    // Background dunes (decorative, no collision)
    struct Dune { float x; int w, h; };
    std::vector<Dune> dunes;

    // Pre-generated random ground detail (scrolling tiles)
    struct GDot { int rx, ry, w, h; };
    std::vector<GDot> gdots;
    static const int GDOT_REP = 600;

    float groundScroll=0, score=0, highScore=0;
    float speed=INIT_SPEED, obstTimer=0, obstInterval=2.f, speedTimer=0;
    bool  running=true;
    Uint32 prevTick=0;
    int    displayFPS=60;
    bool   keyLeft=false, keyRight=false, keyDown=false;

    std::mt19937 rng{ std::random_device{}() };

    // ── Text ────────────────────────────────────────────────────────
    void drawText(const std::string& s, TTF_Font* f, Uint8 gray,
                  int x, int y, bool center=false) {
        SDL_Color c{gray,gray,gray,255};
        SDL_Surface* sf=TTF_RenderUTF8_Blended(f,s.c_str(),c); if (!sf) return;
        SDL_Texture* tx=SDL_CreateTextureFromSurface(rnd,sf);
        SDL_Rect dst{x,y,sf->w,sf->h};
        if (center) dst.x=(WIN_W-sf->w)/2;
        SDL_FreeSurface(sf);
        SDL_RenderCopy(rnd,tx,nullptr,&dst);
        SDL_DestroyTexture(tx);
    }

    // ── Reset ───────────────────────────────────────────────────────
    void resetRound() {
        delete player; player=new Player(rnd);
        obs.clear(); score=0; speed=INIT_SPEED;
        obstTimer=0; obstInterval=2.f; speedTimer=0; groundScroll=0;
        keyLeft=false; keyRight=false; keyDown=false;

        clouds.clear();
        for (int i=0;i<5;++i)
            clouds.push_back({
                float(i*180 + std::uniform_int_distribution<int>(0,60)(rng)),
                float(std::uniform_int_distribution<int>(25,100)(rng)),
                std::uniform_int_distribution<int>(55,120)(rng)
            });

        // Dunes — wide, low humps across the whole screen at start
        dunes.clear();
        for (int i=0;i<8;++i)
            dunes.push_back({
                float(i*120 + std::uniform_int_distribution<int>(0,60)(rng)),
                std::uniform_int_distribution<int>(65,200)(rng),
                std::uniform_int_distribution<int>(5,20)(rng)
            });

        // Ground dots — random pattern tiling every GDOT_REP pixels
        gdots.clear();
        for (int i=0;i<48;++i)          // small single-pixel dots
            gdots.push_back({
                std::uniform_int_distribution<int>(0,GDOT_REP-1)(rng),
                std::uniform_int_distribution<int>(3,16)(rng), 2, 1 });
        for (int i=0;i<16;++i)          // medium pebbles
            gdots.push_back({
                std::uniform_int_distribution<int>(0,GDOT_REP-1)(rng),
                std::uniform_int_distribution<int>(3,10)(rng),
                std::uniform_int_distribution<int>(4,9)(rng),
                std::uniform_int_distribution<int>(2,3)(rng) });
    }

    // ── Obstacle spawning ───────────────────────────────────────────
    void spawnObstacle() {
        int roll=std::uniform_int_distribution<int>(0,9)(rng);
        int type = (roll<4)?0 : (roll<6)?1 : (roll<7)?2 : (roll<9)?3 : 4;
        obs.emplace_back((float)WIN_W+10, type);

        // 35% chance: second obstacle close behind
        if (std::uniform_int_distribution<int>(0,9)(rng)<4) {
            int gap=std::uniform_int_distribution<int>(55,130)(rng);
            obs.emplace_back((float)WIN_W+10+gap,
                             std::uniform_int_distribution<int>(0,1)(rng));
        }
    }

    // ── Info screen ─────────────────────────────────────────────────
    void renderInfo() {
        setCol(rnd, BG); SDL_RenderClear(rnd);

        // Ground
        setCol(rnd, DARK); fillRect(rnd, 0, GROUND_Y, WIN_W, 2);
        setCol(rnd, MED);  fillRect(rnd, 0, GROUND_Y+2, WIN_W, WIN_H-GROUND_Y-2);
        if (player) player->draw(rnd);

        // Title bar
        setCol(rnd, DARK); fillRect(rnd, WIN_W/2-158, 12, 316, 50);
        drawText("DINO BIKE", fBig, BG, 0, 19, true);

        // All boxes: identical style — MED fill, DARK border, DARK text, font=17pt
        const int BX=WIN_W/2-285, BW=570;
        auto box = [&](int y, int h) {
            setCol(rnd, MED); fillRect(rnd, BX, y, BW, h);
            setCol(rnd, DARK); drawRect(rnd, BX, y, BW, h);
        };

        // Box 1 — author
        box(74, 38);
        drawText("Автор: Kayle  |  ИТИП  |  Вариант 10  |  C++ / SDL2",
                 font, DARK, 0, 84, true);

        // Box 2 — goal
        box(122, 38);
        drawText("Цель: уехать как можно дальше, перепрыгивая препятствия!",
                 font, DARK, 0, 132, true);

        // Box 3 — controls (4 lines)
        box(170, 96);
        drawText("Управление:",                                          font, DARK, 0, 178, true);
        drawText("ПРОБЕЛ / ВВЕРХ — прыжок  (держать = выше)",           font, DARK, 0, 198, true);
        drawText("ВНИЗ — пригнуться",                                    font, DARK, 0, 218, true);
        drawText("ВЛЕВО / ВПРАВО — движение          ESC — выход",       font, DARK, 0, 238, true);

        if ((SDL_GetTicks()/500)%2==0)
            drawText("Нажмите ПРОБЕЛ для начала", font, DARK, 0, 280, true);

        SDL_RenderPresent(rnd);
    }

    // ── Game Over ───────────────────────────────────────────────────
    void renderGameOver() {
        drawText("GAME  OVER", fBig, DARK, 0, 50, true);

        std::ostringstream ss; ss << "Счёт:    " << (int)score;
        drawText(ss.str(), font, DARK, 0, 105, true);

        if (highScore>0) {
            std::ostringstream hs; hs << "Рекорд:  " << (int)highScore;
            drawText(hs.str(), font, DARK, 0, 132, true);
        }

        if ((SDL_GetTicks()/500)%2==0)
            drawText("ПРОБЕЛ — заново          ESC — выход",
                     font, DARK, 0, 175, true);
    }

    // ── World render ────────────────────────────────────────────────
    void renderWorld() {
        setCol(rnd, BG); SDL_RenderClear(rnd);

        // Clouds
        setCol(rnd, MED);
        for (const auto& c : clouds) {
            int cx=(int)c.x;
            fillRect(rnd, cx,          (int)c.y+6,  c.w/2, 10);
            fillRect(rnd, cx+c.w/4,    (int)c.y,    c.w/2, 16);
            fillRect(rnd, cx+c.w/2,    (int)c.y+4,  c.w/2, 12);
        }

        // Background dunes — very light, before the ground line
        setCol(rnd, LITE);
        for (const auto& d : dunes) {
            int dx=(int)d.x;
            fillRect(rnd, dx,         GROUND_Y-d.h,   d.w,     d.h);
            if (d.w>50)
                fillRect(rnd, dx+d.w/5, GROUND_Y-d.h-3, d.w*3/5, 3);
        }

        // Ground line
        setCol(rnd, DARK);
        fillRect(rnd, 0, GROUND_Y, WIN_W, 2);

        // Random ground detail (pebbles/dots)
        setCol(rnd, DARK);
        int gsc=(int)groundScroll;
        for (const auto& dot : gdots) {
            int base=((dot.rx - gsc%GDOT_REP)%GDOT_REP + GDOT_REP)%GDOT_REP;
            for (int rep=-1; rep<=1; ++rep) {
                int dx=base+rep*GDOT_REP;
                if (dx>=-10 && dx<WIN_W+10)
                    fillRect(rnd, dx, GROUND_Y+dot.ry, dot.w, dot.h);
            }
        }

        for (const auto& o : obs) o.draw(rnd);
        player->draw(rnd);

        // HUD
        std::ostringstream ss; ss << "Счёт: " << (int)score;
        drawText(ss.str(), font, DARK, 20, 12);
        if (highScore>0) {
            std::ostringstream hs; hs << "Рекорд: " << (int)highScore;
            drawText(hs.str(), font, DARK, WIN_W-178, 12);
        }
    }

    // ── Update ──────────────────────────────────────────────────────
    void update(float dt) {
        score        += speed * dt * 0.018f;   // slower score growth
        groundScroll += speed * dt;

        // Horizontal movement
        const float MS=160.f;
        if (keyLeft  && player->x>55.f)  player->x -= MS*dt;
        if (keyRight && player->x<290.f) player->x += MS*dt;

        // Duck
        player->setDuck(keyDown);

        speedTimer += dt;
        if (speedTimer>=1.f) { speed+=SPEED_INCR; speedTimer=0; }

        // Clouds
        for (auto& c : clouds) {
            c.x -= 50.f*dt;
            if (c.x+c.w<0) {
                c.x = WIN_W+10.f;
                c.y = float(std::uniform_int_distribution<int>(25,100)(rng));
                c.w = std::uniform_int_distribution<int>(55,120)(rng);
            }
        }

        // Dunes — slower parallax scroll
        for (auto& d : dunes) {
            d.x -= speed*0.22f*dt;
            if (d.x+d.w<0) {
                d.x = float(WIN_W + std::uniform_int_distribution<int>(0,80)(rng));
                d.w = std::uniform_int_distribution<int>(65,200)(rng);
                d.h = std::uniform_int_distribution<int>(5,20)(rng);
            }
        }

        player->update(dt, speed);

        obstTimer += dt;
        if (obstTimer>=obstInterval) {
            obstTimer=0;
            obstInterval = std::uniform_real_distribution<float>(1.2f,2.8f)(rng)
                           * (INIT_SPEED/speed);
            spawnObstacle();
        }

        for (auto& o : obs) o.update(speed, dt);
        obs.erase(std::remove_if(obs.begin(),obs.end(),
            [](const Obstacle& o){return o.offScreen();}), obs.end());

        // Collision
        SDL_Rect pb=player->bounds();
        for (const auto& o : obs) {
            SDL_Rect ob=o.bounds(), ix;
            if (SDL_IntersectRect(&pb,&ob,&ix)) {
                if (score>highScore) highScore=score;
                state=State::OVER;
                playSound(g_dieSnd);
                return;
            }
        }
    }

    // ── Events ──────────────────────────────────────────────────────
    void handleEvents() {
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            if (e.type==SDL_QUIT) { running=false; return; }

            if (e.type==SDL_KEYDOWN && !e.key.repeat) {
                switch (e.key.keysym.sym) {
                case SDLK_ESCAPE: running=false; return;
                case SDLK_SPACE: case SDLK_UP:
                    if      (state==State::INFO) { state=State::PLAY; }
                    else if (state==State::PLAY) { if (player->jump()) playSound(g_jumpSnd); }
                    else if (state==State::OVER) { resetRound(); state=State::PLAY; }
                    break;
                case SDLK_DOWN:  keyDown=true;  break;
                case SDLK_LEFT:  keyLeft=true;  break;
                case SDLK_RIGHT: keyRight=true; break;
                default: break;
                }
            }
            if (e.type==SDL_KEYUP) {
                switch (e.key.keysym.sym) {
                case SDLK_SPACE: case SDLK_UP:
                    if (player) player->cutJump();  // variable jump height
                    break;
                case SDLK_DOWN:  keyDown=false;  break;
                case SDLK_LEFT:  keyLeft=false;  break;
                case SDLK_RIGHT: keyRight=false; break;
                default: break;
                }
            }
        }
    }

public:
    ~Game() {
        delete player;
        if (fBig)    TTF_CloseFont(fBig);
        if (font)    TTF_CloseFont(font);
        if (rnd)     SDL_DestroyRenderer(rnd);
        if (win)     SDL_DestroyWindow(win);
        if (g_audio) SDL_CloseAudioDevice(g_audio);
        TTF_Quit(); SDL_Quit();
    }

    bool init() {
        if (SDL_Init(SDL_INIT_VIDEO|SDL_INIT_AUDIO)<0) return false;
        if (TTF_Init()<0) return false;
        win=SDL_CreateWindow("Dino Bike",
            SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
            WIN_W, WIN_H, SDL_WINDOW_SHOWN);
        if (!win) return false;
        rnd=SDL_CreateRenderer(win,-1,
            SDL_RENDERER_ACCELERATED|SDL_RENDERER_PRESENTVSYNC);
        if (!rnd) return false;

        SDL_DisplayMode mode;
        if (SDL_GetCurrentDisplayMode(0,&mode)==0 && mode.refresh_rate>0)
            displayFPS=mode.refresh_rate;

        const char* fp="C:/Windows/Fonts/arial.ttf";
        font=TTF_OpenFont(fp, 17);
        fBig=TTF_OpenFont(fp, 36);
        if (!font||!fBig) return false;

        initAudio();
        resetRound();
        prevTick=SDL_GetTicks();
        return true;
    }

    void run() {
        while (running) {
            Uint32 now=SDL_GetTicks();
            float  dt=std::min((now-prevTick)/1000.f, 0.05f);
            prevTick=now;

            handleEvents();
            if (!running) break;

            if (state==State::INFO) {
                renderInfo();
            } else {
                if (state==State::PLAY) update(dt);
                renderWorld();
                if (state==State::OVER) renderGameOver();
                SDL_RenderPresent(rnd);
            }

            Uint32 elapsed=SDL_GetTicks()-now;
            Uint32 frame  =1000u/(Uint32)displayFPS;
            if (elapsed<frame) SDL_Delay(frame-elapsed);
        }
    }
};

// ─────────────────────────────────────────────────────────────────────
int main(int, char**) {
    Game game;
    if (!game.init()) {
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR,"Init error",SDL_GetError(),nullptr);
        return 1;
    }
    game.run();
    return 0;
}
