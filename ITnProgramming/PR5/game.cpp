#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <SDL2/SDL_image.h>
#include <cmath>
#include <vector>
#include <string>
#include <random>
#include <sstream>
#include <algorithm>
#include <memory>

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



// ─── Obstacle (абстрактный базовый класс) ────────────────────────────
class Obstacle {
public:
    float x;
    explicit Obstacle(float sx) : x(sx) {}
    virtual ~Obstacle() = default;

    virtual void     update(float spd, float dt) = 0;
    virtual bool     offScreen() const = 0;
    virtual SDL_Rect bounds()    const = 0;
    virtual void     draw(SDL_Renderer* r) const = 0;
};

// ─── CactusGroup ──────────────────────────────────────────────────────
// Группа из 1–4 кактусов. Параметры генерируются с соблюдением физики:
//   высота ≤ 75% высоты прыжка (растёт со скоростью),
//   суммарная ширина ≤ 90% дальности прыжка,
//   gap ≥ 1px между кактусами, totalW кактуса ≤ его высоте.
struct CactusShape {
    int h;          // высота (px)
    int tw;         // ширина ствола
    int leftExt;    // вылет левой ветки (0 = нет)
    int rightExt;   // вылет правой ветки
    int leftArmY;   // позиция левой ветки от верхушки
    int rightArmY;  // позиция правой ветки от верхушки
    int totalW() const { return leftExt + tw + rightExt; }
};

class CactusGroup : public Obstacle {
    struct Entry { CactusShape shape; int xOff; };
    std::vector<Entry> cacti;
    int groupW = 0;

    static CactusShape genShape(std::mt19937& rng, int maxH) {
        CactusShape s;
        s.h  = std::uniform_int_distribution<int>(28, maxH)(rng);
        s.tw = std::uniform_int_distribution<int>(5, 8)(rng);
        // totalW ≤ h → budget для веток
        int budget = s.h - s.tw;
        // Ветки: хотя бы одна, минимум 5px вылет (иначе незаметно)
        int maxArm = std::min(11, budget / 2);
        if (maxArm >= 5) {
            bool wL = std::uniform_int_distribution<int>(0,3)(rng) > 0; // 75%
            bool wR = std::uniform_int_distribution<int>(0,3)(rng) > 0;
            if (!wL && !wR) wL = true;  // хотя бы одна
            s.leftExt  = wL ? std::uniform_int_distribution<int>(5, maxArm)(rng) : 0;
            int rem    = budget - s.leftExt;
            int maxR   = std::min(11, rem);
            s.rightExt = (wR && maxR >= 5) ? std::uniform_int_distribution<int>(5, maxR)(rng) : 0;
        } else {
            s.leftExt = s.rightExt = 0;
        }
        // Позиция ветки: в диапазоне 1/4 … 3/5 высоты
        int lo = s.h / 4, hi = s.h * 3 / 5;
        if (lo >= hi) { lo = 4; hi = std::max(5, s.h - 5); }
        s.leftArmY  = s.leftExt  > 0 ? std::uniform_int_distribution<int>(lo, hi)(rng) : 0;
        s.rightArmY = s.rightExt > 0 ? std::uniform_int_distribution<int>(lo, hi)(rng) : 0;
        return s;
    }

    static void drawShape(SDL_Renderer* r, const CactusShape& s, int lx) {
        setCol(r, DARK);
        int top = GROUND_Y - s.h;
        int tx  = lx + s.leftExt;           // левый край ствола
        int sw  = std::max(3, s.tw - 2);    // ширина «подстебля» ветки

        // Основной ствол
        fillRect(r, tx, top, s.tw, s.h);
        // Верхушка — чуть шире
        fillRect(r, tx - 1, top, s.tw + 2, std::max(4, s.h / 6));

        // Левая ветка: подстебель + горизонтальная перемычка
        // Подстебель торчит вверх и немного вниз от точки крепления
        if (s.leftExt >= 5) {
            int jy    = top + s.leftArmY;                     // точка крепления к стволу
            int above = std::max(4, s.leftArmY / 3);          // подстебель поднимается вверх
            int below = std::max(3, (s.h - s.leftArmY) / 5 + 2); // и немного вниз
            fillRect(r, lx,           jy - above, sw, above + below);     // подстебель
            fillRect(r, lx + sw - 1,  jy,          s.leftExt - sw + 2, 4); // горизонт. перемычка
        }

        // Правая ветка — зеркально
        if (s.rightExt >= 5) {
            int jy    = top + s.rightArmY;
            int above = std::max(4, s.rightArmY / 3);
            int below = std::max(3, (s.h - s.rightArmY) / 5 + 2);
            int rx    = tx + s.tw;                             // правый край ствола
            fillRect(r, rx + s.rightExt - sw,  jy - above, sw, above + below); // подстебель
            fillRect(r, rx - 1,                 jy,          s.rightExt - sw + 2, 4); // перемычка
        }
    }

public:
    explicit CactusGroup(float sx) : Obstacle(sx) {}

    // Фабричный метод: генерирует группу с учётом physics-ограничений
    static CactusGroup* generate(std::mt19937& rng, float sx,
                                  float gameSpeed, float jumpDist) {
        float jh   = (JUMP_VEL * JUMP_VEL) / (2.f * GRAVITY);
        int   maxH = (int)(jh * (0.6f + 0.15f * (gameSpeed / INIT_SPEED)));
        maxH = std::max(30, std::min(90, maxH));

        int maxGrpW = std::max(20, (int)(jumpDist * 0.9f));

        int count = std::uniform_int_distribution<int>(1, 4)(rng);
        auto* g   = new CactusGroup(sx);
        int   cur = 0;

        for (int i = 0; i < count; ++i) {
            CactusShape s   = genShape(rng, maxH);
            int gap  = (i == 0) ? 0 : std::uniform_int_distribution<int>(1, 4)(rng);
            int xOff = cur + gap;
            if (xOff + s.totalW() > maxGrpW && i > 0) break;
            g->cacti.push_back({s, xOff});
            cur = xOff + s.totalW();
        }
        g->groupW = cur;
        return g;
    }

    void update(float spd, float dt) override { x -= spd * dt; }

    bool offScreen() const override { return x + groupW < 0; }

    SDL_Rect bounds() const override {
        if (cacti.empty()) return {(int)x, GROUND_Y - 30, 10, 30};
        int minTx = cacti[0].xOff + cacti[0].shape.leftExt;
        int maxRx = 0, maxH = 0;
        for (const auto& e : cacti) {
            int tx = e.xOff + e.shape.leftExt;
            minTx = std::min(minTx, tx);
            maxRx = std::max(maxRx, tx + e.shape.tw);
            maxH  = std::max(maxH, e.shape.h);
        }
        return { (int)x + minTx + 1, GROUND_Y - maxH + 2, maxRx - minTx - 2, maxH - 2 };
    }

    void draw(SDL_Renderer* r) const override {
        for (const auto& e : cacti)
            drawShape(r, e.shape, (int)x + e.xOff);
    }
};

// ─── Pterodactyl ──────────────────────────────────────────────────────
// Летящее препятствие: высота и скорость — непрерывные случайные значения.
//   altitude  — topY (чем меньше, тем выше на экране)
//   ownSpeed  — зафиксирована при спавне (40–72% от скорости игры)
class Pterodactyl : public Obstacle {
    float altitude;       // верхний Y птеродактиля
    float ownSpeed;       // горизонтальная скорость (px/s)
    float wingTimer = 0.f;
    int   wingPhase = 0;  // 0=вверх 1=нейтраль 2=вниз 3=нейтраль

    static const int W = 42;
    static const int H = 22;

public:
    Pterodactyl(float sx, float gameSpeed, std::mt19937& rng)
        : Obstacle(sx)
    {
        altitude = float(std::uniform_int_distribution<int>(
            GROUND_Y - 125, GROUND_Y - 48)(rng));
        // Птеродактиль летит примерно со скоростью игры — не медленнее пола
        float f  = std::uniform_real_distribution<float>(1.0f, 1.28f)(rng);
        ownSpeed = f * gameSpeed;
    }

    void update(float /*spd*/, float dt) override {
        x -= ownSpeed * dt;
        wingTimer += dt;
        if (wingTimer > 0.13f) { wingTimer = 0.f; wingPhase = (wingPhase + 1) % 4; }
    }

    bool offScreen() const override { return x + W < 0; }

    // Хитбокс — только тело, без крыльев
    SDL_Rect bounds() const override {
        return { (int)x + 5, (int)altitude + 6, W - 10, 12 };
    }

    void draw(SDL_Renderer* r) const override {
        setCol(r, DARK);
        int ty = (int)altitude, cx = (int)x + W / 2;
        int wOff = (wingPhase == 0) ? -7 : (wingPhase == 2) ? 7 : 0;
        // Крылья
        fillRect(r, cx - 8,  ty + 8 + wOff, 14, 5);
        fillRect(r, cx + 4,  ty + 9 + wOff, 14, 4);
        // Тело
        fillRect(r, cx - 8,  ty + 8, 16, 10);
        // Голова
        fillRect(r, cx - 16, ty + 6, 10,  8);
        // Клюв
        fillRect(r, cx - 23, ty + 8,  7,  3);
        // Глаз
        setCol(r, BG);
        fillRect(r, cx - 14, ty + 7,  2,  2);
    }
};

// ─── Player ──────────────────────────────────────────────────────────
class Player {
    // ── Sprites ──────────────────────────────────────────────────────
    // texRun[0] = правая нога, texRun[1] = левая нога
    SDL_Texture* texRun[2]  = {nullptr, nullptr};
    // texDuck[0] = правая нога, texDuck[1] = левая нога
    SDL_Texture* texDuck[2] = {nullptr, nullptr};
    SDL_Texture* texDead    = nullptr;
    // Размеры каждого спрайта
    int runW=0,  runH=0;
    int duckW=0, duckH=0;
    int deadW=0, deadH=0;
    bool hasSpr=false;

    // Горизонтальный сдвиг спрайта относительно PX (подобрать по виду)
    static const int SPR_OX = -32;

    static SDL_Texture* loadTex(SDL_Renderer* r, const char* path) {
        SDL_Surface* s = IMG_Load(path);
        if (!s) { SDL_Log("Sprite load failed [%s]: %s", path, IMG_GetError()); return nullptr; }
        SDL_Texture* t = SDL_CreateTextureFromSurface(r, s);
        SDL_FreeSurface(s);
        return t;
    }

public:
    float x=100.f, y;
    float velY=0.f;
    bool  grounded=true, ducking=false, shocked=false;
    // Анимация: 0 = правая нога (старт), 1 = левая нога; 10 кадров/сек
    int   animFrame=0;
    float animTimer=0.f;

    static const int WR = 14;
    static const int CH = 64;

    float groundedY() const { return GROUND_Y-CH; }

    Player(SDL_Renderer* r): y(GROUND_Y-CH) {

        texRun[0]  = loadTex(r, "sprites/Chrome_Bycicle_T-Rex_Right_Run.png");
        texRun[1]  = loadTex(r, "sprites/Chrome_Bycicle_T-Rex_Left_Run.png");
        texDuck[0] = loadTex(r, "sprites/Chrome_T-Rex_Right_Duck.png");
        texDuck[1] = loadTex(r, "sprites/Chrome_T-Rex_Left_Duck.png");
        texDead    = loadTex(r, "sprites/Dead_Chrome_T-Rex.webp");

        if (texRun[0]) {
            SDL_QueryTexture(texRun[0], nullptr, nullptr, &runW, &runH);
            hasSpr = true;
        }
        if (texDuck[0])
            SDL_QueryTexture(texDuck[0], nullptr, nullptr, &duckW, &duckH);
        if (texDead)
            SDL_QueryTexture(texDead, nullptr, nullptr, &deadW, &deadH);
    }

    ~Player() {
        for (auto* t : texRun)  if (t) SDL_DestroyTexture(t);
        for (auto* t : texDuck) if (t) SDL_DestroyTexture(t);
        if (texDead) SDL_DestroyTexture(texDead);
    }

    bool jump() {
        if (grounded) { velY=JUMP_VEL; grounded=false; ducking=false; return true; }
        return false;
    }
    void cutJump() { if (!grounded && velY<0) velY *= 0.42f; }
    void setDuck(bool d) { ducking=d; }

    bool update(float dt, float spd) {
        bool wasAir = !grounded;
        if (!grounded) {
            velY += GRAVITY*dt;
            y    += velY*dt;
            if (y>=groundedY()) { y=groundedY(); velY=0; grounded=true; }
        }

        // Смена кадра: только на земле и живой, 10 кадров/сек
        if (grounded && !shocked) {
            animTimer += dt;
            if (animTimer >= 0.1f) {
                animTimer = 0.f;
                animFrame = 1 - animFrame;  // правая → левая → правая…
            }
        }

        return wasAir && grounded;
    }

    SDL_Rect bounds() const {
        // Хитбоксы подогнаны под размеры спрайтов (с полями ~8px с боков)
        // Спрайт бега (80×93) нарисован от (x-32, y-29)
        // Спрайт приседания (110×75) нарисован от (x-32, y-11)
        if (ducking && grounded)
            return { (int)x - 24, (int)y - 8, 94, 72 };
        return { (int)x - 24, (int)y - 24, 64, 88 };
    }

    void draw(SDL_Renderer* r) const {
        int PX=(int)x, by=(int)y;
        bool useDuck = ducking && grounded;

        if (hasSpr) {
            SDL_Texture* tex = nullptr;
            int sprW=0, sprH=0;

            if (shocked) {
                // Смерть: если есть спрайт — он; иначе замёрзший беговой кадр
                if (texDead) {
                    tex=texDead; sprW=deadW; sprH=deadH;
                } else {
                    tex=texRun[animFrame]; sprW=runW; sprH=runH;
                }
            } else if (!useDuck) {
                // Бег / прыжок
                tex=texRun[animFrame]; sprW=runW; sprH=runH;
            } else {
                // Пригибание — оба кадра одинакового размера
                tex = texDuck[animFrame];
                if (!tex) tex = texDuck[1 - animFrame];  // запасной кадр
                sprW = duckW; sprH = duckH;
            }

            if (tex) {
                // Y: спрайт снизу касается земли (GROUND_Y), формула: by + (CH - sprH)
                SDL_Rect dst = { PX + SPR_OX, by + (CH - sprH), sprW, sprH };
                SDL_RenderCopy(r, tex, nullptr, &dst);
                return;
            }
        }

        // ── Программный запасной вариант (если спрайты не загрузились) ──
        int fwx=PX+36, rwx=PX-20, wy=by+CH-WR;
        int bbx=PX+8, bby=wy+3, sx=PX-6, sy=by+26, hx=PX+26, hy=by+22;
        setCol(r, DARK);
        SDL_RenderDrawLine(r, rwx,wy,  sx, sy);
        SDL_RenderDrawLine(r, rwx,wy,  bbx,bby);
        SDL_RenderDrawLine(r, bbx,bby, sx, sy);
        SDL_RenderDrawLine(r, sx, sy,  hx, hy);
        SDL_RenderDrawLine(r, hx, hy,  bbx,bby);
        SDL_RenderDrawLine(r, hx, hy,  fwx,wy);
        SDL_RenderDrawLine(r, hx, hy,  PX+38,by+13);
        fillRect(r, PX+36,by+11,5,3);
        fillRect(r, sx-7, sy-3,17,4);
        float lp=0.f;
        int hx0=PX-2, hy0=by+34;
        int f1x=bbx+(int)(6*std::cos(lp)),            f1y=bby+(int)(5*std::sin(lp));
        int f2x=bbx+(int)(6*std::cos(lp+(float)M_PI)),f2y=bby+(int)(5*std::sin(lp+(float)M_PI));
        for (int k=-1;k<=1;++k) {
            SDL_RenderDrawLine(r,hx0+k,hy0, f1x+k,f1y);
            SDL_RenderDrawLine(r,f1x+k,f1y, f1x+3+k,f1y+4);
            SDL_RenderDrawLine(r,hx0+k,hy0, f2x+k,f2y);
            SDL_RenderDrawLine(r,f2x+k,f2y, f2x+3+k,f2y+4);
        }
        if (useDuck) {
            setCol(r,DARK);
            fillRect(r,PX-14,by+18,32,16); fillRect(r,PX+8,by+21,16,6);
            fillRect(r,PX+12,by+14,26,12); fillRect(r,PX+28,by+20,14,5);
            fillRect(r,PX+16,by+24,28,5);
            setCol(r,BG); fillRect(r,PX+22,by+16,6,5);
        } else {
            setCol(r,DARK);
            fillRect(r,PX-14,by+16,30,20); fillRect(r,PX+6,by+10,12,8);
            fillRect(r,PX+4, by,   26,14); fillRect(r,PX+20,by+9,14,6);
            fillRect(r,PX+8, by+13,28, 6);
            setCol(r,BG); fillRect(r,PX+16,by+2,6,6);
        }
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
    std::vector<std::unique_ptr<Obstacle>> obs;

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
    bool   keyLeft=false, keyRight=false, keyDown=false, keyJump=false;

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
        keyLeft=false; keyRight=false; keyDown=false; keyJump=false;

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
        float airTime  = 2.f * (-JUMP_VEL) / GRAVITY;   // время полёта при прыжке
        float jumpDist = speed * airTime;                 // дальность прыжка (px)

        if (std::uniform_int_distribution<int>(0, 9)(rng) < 2) {
            // 20% вероятность — птеродактиль
            obs.push_back(std::make_unique<Pterodactyl>(
                (float)WIN_W + 10, speed, rng));
        } else {
            // 70% — группа кактусов (1–4 шт.)
            obs.emplace_back(CactusGroup::generate(
                rng, (float)WIN_W + 10, speed, jumpDist));
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
        drawText("Автор: Денисов Георгий  |  ИТИП  |  Вариант 10  |  C++ / SDL2",
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

        for (const auto& o : obs) o->draw(rnd);
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

        bool justLanded = player->update(dt, speed);
        if (justLanded && keyJump)
            if (player->jump()) playSound(g_jumpSnd);

        obstTimer += dt;
        if (obstTimer>=obstInterval) {
            obstTimer=0;
            obstInterval = std::uniform_real_distribution<float>(1.2f,2.8f)(rng)
                           * (INIT_SPEED/speed);
            spawnObstacle();
        }

        for (auto& o : obs) o->update(speed, dt);
        obs.erase(std::remove_if(obs.begin(), obs.end(),
            [](const std::unique_ptr<Obstacle>& o){ return o->offScreen(); }),
            obs.end());

        // Collision
        SDL_Rect pb=player->bounds();
        for (const auto& o : obs) {
            SDL_Rect ob=o->bounds(), ix;
            if (SDL_IntersectRect(&pb,&ob,&ix)) {
                if (score>highScore) highScore=score;
                player->shocked = true;
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
                    else if (state==State::PLAY) { keyJump=true; if (player->jump()) playSound(g_jumpSnd); }
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
                    keyJump=false;
                    if (player) player->cutJump();
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
        IMG_Quit(); TTF_Quit(); SDL_Quit();
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

        IMG_Init(IMG_INIT_PNG);  // нужно для загрузки PNG-спрайтов
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
