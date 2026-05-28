#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <cmath>
#include <cstdio>
#include <vector>
#include <string>
#include <random>
#include <algorithm>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// ── Window / physics ────────────────────────────────────────────────────
static const int   WIN_W      = 900;
static const int   WIN_H      = 290;        // −100 sky, −10 ground vs original
static const int   GROUND_Y   = 268;
static const float GRAVITY    = 2600.f;
static const float JUMP_VEL   = -760.f;
static const float INIT_SPEED = 420.f;
static const float SPEED_INCR = 16.f;
// Bird base speed as fraction of INIT_SPEED (slow, atmospheric)
static const float BIRD_SPEED_FRAC = 0.55f;

// ── Palette ──────────────────────────────────────────────────────────────
static const Uint8 BG   = 247;
static const Uint8 DARK =  83;
static const Uint8 MED  = 150;
static const Uint8 LITE = 218;

// ── Pixel font (3×5 bitmap, drawn with fillRect at GLYPH_SC px/pixel) ───
//    Indices: 0-9 = digits, 10 = H, 11 = I
static const uint8_t GLYPHS[12][5] = {
    {0b111,0b101,0b101,0b101,0b111}, // 0
    {0b110,0b010,0b010,0b010,0b111}, // 1
    {0b111,0b001,0b111,0b100,0b111}, // 2
    {0b111,0b001,0b111,0b001,0b111}, // 3
    {0b101,0b101,0b111,0b001,0b001}, // 4
    {0b111,0b100,0b111,0b001,0b111}, // 5
    {0b111,0b100,0b111,0b101,0b111}, // 6
    {0b111,0b001,0b011,0b001,0b001}, // 7
    {0b111,0b101,0b111,0b101,0b111}, // 8
    {0b111,0b101,0b111,0b001,0b111}, // 9
    {0b101,0b101,0b111,0b101,0b101}, // H
    {0b111,0b010,0b010,0b010,0b111}, // I
};
static const int GLYPH_SC = 3;                   // pixels per dot
static const int GLYPH_CW = 3*GLYPH_SC+GLYPH_SC; // char advance = 12
static const int GLYPH_CH = 5*GLYPH_SC;          // char height  = 15

// ── Draw helpers ────────────────────────────────────────────────────────
static void setCol(SDL_Renderer* r, Uint8 v)
    { SDL_SetRenderDrawColor(r,v,v,v,255); }
static void fillRect(SDL_Renderer* r, int x, int y, int w, int h)
    { SDL_Rect rc{x,y,w,h}; SDL_RenderFillRect(r,&rc); }
static void drawRect(SDL_Renderer* r, int x, int y, int w, int h)
    { SDL_Rect rc{x,y,w,h}; SDL_RenderDrawRect(r,&rc); }

static int drawGlyph(SDL_Renderer* r, int idx, int x, int y) {
    // returns x after the glyph (with gap)
    if (idx < 0) { return x + GLYPH_SC*2; } // space
    for (int row=0;row<5;++row) {
        uint8_t bits=GLYPHS[idx][row];
        for (int col=0;col<3;++col)
            if (bits&(4>>col))
                fillRect(r, x+col*GLYPH_SC, y+row*GLYPH_SC, GLYPH_SC, GLYPH_SC);
    }
    return x+GLYPH_CW;
}
static void drawGlyphStr(SDL_Renderer* r, const std::string& s, int x, int y) {
    int cx=x;
    for (char c:s) {
        int idx=-1;
        if (c>='0'&&c<='9') idx=c-'0';
        else if (c=='H') idx=10;
        else if (c=='I') idx=11;
        // else space
        cx=drawGlyph(r,idx,cx,y);
    }
}
static int glyphStrW(const std::string& s) {
    int w=0;
    for (char c:s)
        w += (c==' ') ? GLYPH_SC*2 : GLYPH_CW;
    return w > 0 ? w-GLYPH_SC : 0; // remove trailing gap
}
static std::string fmtScore(int v) {
    char buf[8]; std::snprintf(buf,sizeof(buf),"%05d",std::min(v,99999));
    return buf;
}

// ── Sound ────────────────────────────────────────────────────────────────
struct Sound { std::vector<Sint16> buf; int pos=0; bool playing=false; };
static Sound g_jumpSnd, g_dieSnd;
static Sound* g_active=nullptr;
static SDL_AudioDeviceID g_audio=0;

static void audioCallback(void*, Uint8* stream, int len) {
    int n=len/2; auto* out=reinterpret_cast<Sint16*>(stream);
    if (!g_active||!g_active->playing){SDL_memset(stream,0,len);return;}
    for (int i=0;i<n;++i) {
        if (g_active->pos>=(int)g_active->buf.size()){out[i]=0;g_active->playing=false;}
        else out[i]=g_active->buf[g_active->pos++];
    }
}
static void makeTone(Sound& s,float freq,float dur,float vol=0.25f,bool sweep=false) {
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
    makeTone(g_jumpSnd,700.f,0.10f);
    makeTone(g_dieSnd, 500.f,0.70f,0.25f,true);
    SDL_PauseAudioDevice(g_audio,0); return true;
}

// ── Wheel sprite ─────────────────────────────────────────────────────────
class WheelSprite {
    std::vector<SDL_Texture*> frames;
    int cur=0; float timer=0.f;
public:
    int sz=0;
    void build(SDL_Renderer* r, int rad, int nf) {
        sz=rad*2+2;
        for (int f=0;f<nf;++f) {
            SDL_Texture* tex=SDL_CreateTexture(r,SDL_PIXELFORMAT_RGBA8888,SDL_TEXTUREACCESS_TARGET,sz,sz);
            SDL_SetTextureBlendMode(tex,SDL_BLENDMODE_BLEND);
            SDL_SetRenderTarget(r,tex);
            SDL_SetRenderDrawColor(r,0,0,0,0); SDL_RenderClear(r);
            int cx=sz/2,cy=sz/2;
            SDL_SetRenderDrawColor(r,DARK,DARK,DARK,255);
            for (int dy=-rad;dy<=rad;++dy)
                for (int dx=-rad;dx<=rad;++dx) {
                    int d2=dx*dx+dy*dy,inn=(rad-3)*(rad-3);
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
    void update(float dt) {
        const float DUR=0.022f; timer+=dt;
        if (timer>=DUR){timer=0;cur=(cur+1)%(int)frames.size();}
    }
    void draw(SDL_Renderer* r, int cx, int cy) const {
        if (frames.empty()) return;
        SDL_Rect dst{cx-sz/2,cy-sz/2,sz,sz};
        SDL_RenderCopy(r,frames[cur],nullptr,&dst);
    }
    ~WheelSprite(){for(auto* t:frames) SDL_DestroyTexture(t);}
};

// ── Cactus (random, scalable) ─────────────────────────────────────────────
struct CactusParams {
    int h;          // total pixel height (already scaled)
    int tw;         // trunk width (scaled)
    int leftArm;    // left horizontal arm length (0 = none)
    int rightArm;
    int leftArmY;   // y offset from top where arm attaches
    int rightArmY;
    int totalW() const { return leftArm + tw + rightArm; }
};

static CactusParams genCactus(std::mt19937& rng, float maxH) {
    auto uid=[&](int a,int b)->int{ return std::uniform_int_distribution<int>(a,b)(rng); };
    auto ufd=[&](float a,float b)->float{ return std::uniform_real_distribution<float>(a,b)(rng); };

    float scale = ufd(0.75f,1.35f);
    int baseH = uid(22, std::max(22,(int)(maxH/scale)));
    CactusParams c;
    c.h   = std::max(10,(int)(baseH*scale));
    c.tw  = std::max(3,(int)(uid(4,7)*scale));

    // Each arm ≤ (h - tw) / 2  so total width (leftArm+tw+rightArm) ≤ h
    int maxArm = std::max(2, (c.h - c.tw) / 2);

    c.leftArm  = (uid(0,1)==0) ? uid(3,maxArm) : 0;
    c.rightArm = (uid(0,1)==0) ? uid(3,maxArm) : 0;
    if (c.leftArm==0&&c.rightArm==0) c.rightArm=uid(3,maxArm); // at least one

    // Clamp so leftArm+tw+rightArm ≤ h
    int excess = c.leftArm + c.tw + c.rightArm - c.h;
    if (excess>0) { c.rightArm=std::max(0,c.rightArm-excess); }

    int armZoneH = std::max(c.tw+2, c.h/2);
    c.leftArmY  = uid(c.tw+2, armZoneH);
    c.rightArmY = uid(c.tw+2, armZoneH);
    return c;
}

static void drawCactus(SDL_Renderer* r, const CactusParams& c, int lx) {
    setCol(r,DARK);
    int top   = GROUND_Y - c.h;
    int trunkX = lx + c.leftArm;   // trunk starts after left arm
    int armH  = std::max(2, c.tw*2/3);

    // Trunk
    fillRect(r, trunkX, top, c.tw, c.h);

    // Left arm: horizontal stub + small cap going up
    if (c.leftArm>0) {
        fillRect(r, trunkX-c.leftArm,  top+c.leftArmY,          c.leftArm+c.tw/2, armH);
        fillRect(r, trunkX-c.leftArm,  top+c.leftArmY-c.leftArm, c.tw/2,           c.leftArm);
    }
    // Right arm
    if (c.rightArm>0) {
        fillRect(r, trunkX+c.tw/2,             top+c.rightArmY,            c.rightArm+c.tw/2, armH);
        fillRect(r, trunkX+c.tw+c.rightArm-c.tw/2, top+c.rightArmY-c.rightArm, c.tw/2,        c.rightArm);
    }
}

// ── Obstacle ──────────────────────────────────────────────────────────────
class Obstacle {
public:
    float x;
    bool  isBird=false;
    int   birdAlt=0;            // 0=low, 1=high
    float birdTimer=0.f; int birdWing=0;

    // Cactus cluster
    std::vector<CactusParams> cacti;
    std::vector<int>          cOffsets; // x offset of each cactus in cluster
    int                       clusterW=0;

    // Factory helpers
    static Obstacle makeBird(float sx, int alt)
        { Obstacle o; o.x=sx; o.isBird=true; o.birdAlt=alt; return o; }
    static Obstacle makeCluster(float sx,
                                std::vector<CactusParams> cv,
                                std::vector<int> ov, int tw)
        { Obstacle o; o.x=sx; o.cacti=std::move(cv); o.cOffsets=std::move(ov); o.clusterW=tw; return o; }

    int birdTopY() const { return birdAlt==0 ? GROUND_Y-58 : GROUND_Y-92; }
    static const int BW=36, BH=20;

    void update(float gameSp, float birdSp, float dt) {
        x -= (isBird ? birdSp : gameSp)*dt;
        if (isBird) {
            birdTimer+=dt;
            if (birdTimer>0.06f){birdTimer=0;birdWing=(birdWing+1)%4;}
        }
    }
    bool offScreen() const {
        return isBird ? (x+BW<0) : (x+clusterW<0);
    }

    std::vector<SDL_Rect> hitboxes() const {
        std::vector<SDL_Rect> v;
        if (isBird) {
            v.push_back({(int)x+2, birdTopY()+5, BW-4, 12});
        } else {
            int ox=(int)x;
            for (int i=0;i<(int)cacti.size();++i) {
                const auto& c=cacti[i];
                int lx=ox+cOffsets[i];
                v.push_back({lx, GROUND_Y-c.h, c.totalW(), c.h});
            }
        }
        return v;
    }

    void draw(SDL_Renderer* r) const {
        if (isBird) {
            int ty=birdTopY(), cx=(int)x+BW/2;
            setCol(r,DARK);
            int wOff=(birdWing==0)?-6:(birdWing==2)?6:0;
            fillRect(r,cx-10,ty+7+wOff,12,4); // left wing
            fillRect(r,cx+2, ty+8+wOff,12,4); // right wing
            fillRect(r,cx-8, ty+7,     14,9); // body
            fillRect(r,cx-14,ty+5,      8,7); // head
            fillRect(r,cx-20,ty+7,      6,3); // beak
            setCol(r,BG);
            fillRect(r,cx-12,ty+6,2,2);       // eye
        } else {
            int ox=(int)x;
            for (int i=0;i<(int)cacti.size();++i)
                drawCactus(r, cacti[i], ox+cOffsets[i]);
        }
    }
};

// ── Player ───────────────────────────────────────────────────────────────
class Player {
public:
    float x=100.f, y;
    float velY=0.f;
    bool  grounded=true, ducking=false, shocked=false;
    float legPhase=0.f, jumpCooldown=0.f;
    WheelSprite wheel;

    static const int WR = 11;   // wheel radius
    static const int CH = 90;   // total character height

    float groundedY() const { return GROUND_Y-CH; }
    Player(SDL_Renderer* r): y(GROUND_Y-CH) { wheel.build(r,WR,8); }

    bool jump() {
        if (grounded&&jumpCooldown<=0.f){velY=JUMP_VEL;grounded=false;ducking=false;return true;}
        return false;
    }
    void cutJump() { if (!grounded&&velY<0) velY*=0.42f; }
    void setDuck(bool d) { ducking=d; }

    bool update(float dt, float spd) {
        bool wasAir=!grounded;
        if (!grounded) {
            velY+=GRAVITY*dt; y+=velY*dt;
            if (y>=groundedY()){y=groundedY();velY=0;grounded=true;jumpCooldown=0.17f;}
        }
        if (jumpCooldown>0.f) jumpCooldown-=dt;
        legPhase += (grounded?14.f:10.f)*dt;
        wheel.update(dt);
        (void)spd;
        return wasAir&&grounded;
    }

    // Full hitbox including wheels
    SDL_Rect bounds() const {
        int PX=(int)x, by=(int)y;
        int rwx=PX-16, fwx=PX+16;
        int top = ducking&&grounded ? by+28 : by;
        return {rwx-WR, top, (fwx+WR)-(rwx-WR), (by+CH)-top};
    }

    // ── drawNormal ───────────────────────────────────────────────────
    void drawNormal(SDL_Renderer* r, int PX, int by) const {
        setCol(r, DARK);

        // TAIL — curves up-left from rump (Chrome Dino style arc)
        fillRect(r, PX-26, by+52,  8, 12); // root (behind rear wheel)
        fillRect(r, PX-40, by+42, 16, 12); // lower
        fillRect(r, PX-52, by+32, 14, 12); // mid
        fillRect(r, PX-62, by+22, 12, 12); // upper
        fillRect(r, PX-70, by+14,  9, 10); // tip

        // BODY — pear shape, wide hips narrowing toward chest
        fillRect(r, PX-22, by+62, 38,  6); // hip bottom
        fillRect(r, PX-22, by+54, 30,  8); // lower body (seat level)
        fillRect(r, PX-18, by+46, 26,  8); // mid body
        fillRect(r, PX-14, by+38, 22,  8); // upper body
        fillRect(r, PX-10, by+30, 18,  8); // chest

        // NECK — vertical, slightly forward of body center
        fillRect(r, PX+0,  by+16, 10, 16);

        // HEAD — Chrome Dino / T-Rex silhouette (facing right)
        fillRect(r, PX+6,  by+0,  14,  4); // crown bump (narrow top crest)
        fillRect(r, PX+0,  by+4,  24, 12); // skull (wide block)
        fillRect(r, PX+10, by+13, 18,  6); // upper snout (overlaps skull bottom, protrudes right)
        // mouth gap at by+19..20
        fillRect(r, PX+10, by+21, 14,  5); // lower jaw

        // EYE — BG-coloured cutout inside skull
        setCol(r, BG);
        if (shocked) {
            fillRect(r, PX+3, by+5, 8, 8);
            setCol(r, DARK);
            fillRect(r, PX+6, by+8, 3, 3);
        } else {
            fillRect(r, PX+3, by+5, 6, 6);
        }

        // ARMS — tiny T-Rex arms, point forward
        setCol(r, DARK);
        fillRect(r, PX+4,  by+34, 10, 5);
        fillRect(r, PX+10, by+38,  7, 4);
    }

    // ── drawCrouched ─────────────────────────────────────────────────
    void drawCrouched(SDL_Renderer* r, int PX, int by) const {
        setCol(r, DARK);

        // TAIL — flatter arc when crouching
        fillRect(r, PX-24, by+52,  8, 12);
        fillRect(r, PX-38, by+46, 16, 10);
        fillRect(r, PX-52, by+40, 16, 10);
        fillRect(r, PX-64, by+36, 14,  8);
        fillRect(r, PX-72, by+32, 10,  8);

        // BODY — same as normal stance
        fillRect(r, PX-22, by+62, 38,  6);
        fillRect(r, PX-22, by+54, 30,  8);
        fillRect(r, PX-18, by+46, 26,  8);
        fillRect(r, PX-14, by+38, 22,  8);
        fillRect(r, PX-10, by+30, 18,  8);

        // NECK — leans forward horizontally
        fillRect(r, PX+4,  by+30, 20, 10);

        // HEAD — Chrome Dino ducking pose: forward and low
        fillRect(r, PX+26, by+18, 14,  4); // crown
        fillRect(r, PX+20, by+22, 24, 12); // skull
        fillRect(r, PX+34, by+30, 18,  6); // upper snout
        // mouth gap at by+36..37
        fillRect(r, PX+32, by+38, 14,  5); // lower jaw

        // EYE
        setCol(r, BG);
        if (shocked) {
            fillRect(r, PX+22, by+24, 8, 8);
            setCol(r, DARK);
            fillRect(r, PX+25, by+27, 3, 3);
        } else {
            fillRect(r, PX+22, by+25, 6, 6);
        }

        // ARMS
        setCol(r, DARK);
        fillRect(r, PX+14, by+40, 10, 5);
        fillRect(r, PX+20, by+44,  6, 4);
    }

    void draw(SDL_Renderer* r) const {
        int PX=(int)x, by=(int)y;

        // Bike — narrow, ≈ dino width
        int fwx=PX+16, rwx=PX-16;
        int wy=by+CH-WR;          // wheel centre Y
        int bbx=PX;               // bottom bracket X (between wheels)
        int bby=wy+2;             // bottom bracket Y

        // Frame geometry
        int sx=PX-4, sy=by+54;   // seat post
        int hx=PX+12,hy=by+46;   // head tube

        setCol(r,DARK);
        SDL_RenderDrawLine(r,rwx,wy, sx,sy);    // seat stay
        SDL_RenderDrawLine(r,rwx,wy, bbx,bby);  // chain stay
        SDL_RenderDrawLine(r,bbx,bby,sx,sy);    // seat tube
        SDL_RenderDrawLine(r,sx,sy,  hx,hy);    // top tube
        SDL_RenderDrawLine(r,hx,hy,  bbx,bby);  // down tube
        SDL_RenderDrawLine(r,hx,hy,  fwx,wy);   // fork
        // Handlebar
        fillRect(r,hx-2,hy-6,5,8);
        fillRect(r,hx-5,hy-8,11,3);
        // Seat
        fillRect(r,sx-6,sy-3,14,3);

        wheel.draw(r,fwx,wy);
        wheel.draw(r,rwx,wy);

        // L-shaped legs: thigh HORIZONTAL (hip→knee), shin VERTICAL (knee→pedal)
        // This is anatomically correct — knee is forward, shin drops down.
        setCol(r,DARK);
        int hipX=PX-10, hipY=by+72;  // hip anchor
        const int ORBIT=4, LW=4;     // orbit radius, leg thickness
        int f1x=bbx+(int)(ORBIT*std::cos(legPhase)),
            f1y=bby+(int)(ORBIT*std::sin(legPhase));
        int f2x=bbx+(int)(ORBIT*std::cos(legPhase+(float)M_PI)),
            f2y=bby+(int)(ORBIT*std::sin(legPhase+(float)M_PI));

        auto drawLeg=[&](int fx, int fy){
            // Knee is directly at (fx, hipY) — forward of hip, at hip height
            int kx=fx, ky=hipY;
            // Thigh (horizontal hip→knee)
            int tx=std::min(hipX,kx), tw=std::abs(kx-hipX)+LW;
            fillRect(r, tx, ky-LW/2, tw, LW);
            // Shin (vertical knee→pedal)
            int sy2=std::min(ky,fy), sh=std::abs(fy-ky)+LW;
            fillRect(r, kx-LW/2, sy2, LW, sh);
        };
        drawLeg(f1x,f1y);
        drawLeg(f2x,f2y);

        if (ducking&&grounded) drawCrouched(r,PX,by);
        else                   drawNormal(r,PX,by);
    }
};

// ── Game ─────────────────────────────────────────────────────────────────
enum class State { INFO, PLAY, OVER };

class Game {
    SDL_Window*   win =nullptr;
    SDL_Renderer* rnd =nullptr;
    TTF_Font*     font=nullptr;   // 19pt TTF — info/gameover text
    TTF_Font*     fBig=nullptr;   // 36pt — title / GAME OVER

    State   state=State::INFO;
    Player* player=nullptr;
    std::vector<Obstacle> obs;
    bool    paused=false;

    struct Cloud{float x,y;int w;};
    std::vector<Cloud> clouds;
    struct Dune{float x;int w,h;};
    std::vector<Dune> dunes;
    struct GDot{int rx,ry,w,h;};
    std::vector<GDot> gdots;
    static const int GDOT_REP=600;

    float groundScroll=0;

    // Score counter (visual, independent rate from game speed)
    float scoreDisplay=0;
    float scoreRate=10.f;        // pts shown per second, starts at 10
    float scoreAccelFactor=1.08f;// multiplier each accel (diminishes)
    int   nextAccelAt=80;        // score threshold for next accel
    float highScore=0;           // persists across rounds

    float speed=INIT_SPEED, speedTimer=0;
    float birdSpeed=INIT_SPEED*BIRD_SPEED_FRAC;

    float obstTimer=0, obstInterval=2.f;
    bool  running=true;
    Uint32 prevTick=0;
    int    displayFPS=60;
    bool   keyLeft=false,keyRight=false,keyDown=false,keyJump=false;

    std::mt19937 rng{std::random_device{}()};

    // ── Text (TTF) ──────────────────────────────────────────────────
    void drawText(const std::string& s,TTF_Font* f,Uint8 gray,int x,int y,bool center=false){
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
    void resetRound(){
        delete player; player=new Player(rnd);
        obs.clear();
        scoreDisplay=0; scoreRate=10.f; scoreAccelFactor=1.08f; nextAccelAt=80;
        speed=INIT_SPEED; birdSpeed=INIT_SPEED*BIRD_SPEED_FRAC;
        speedTimer=0; obstTimer=0; obstInterval=2.f; groundScroll=0;
        keyLeft=keyRight=keyDown=keyJump=false;

        clouds.clear();
        auto uid=[&](int a,int b){return std::uniform_int_distribution<int>(a,b)(rng);};
        for (int i=0;i<4;++i)
            clouds.push_back({float(i*220+uid(0,60)),float(uid(8,55)),uid(55,110)});

        dunes.clear();
        for (int i=0;i<7;++i)
            dunes.push_back({float(i*130+uid(0,60)),uid(60,180),uid(4,16)});

        gdots.clear();
        for (int i=0;i<48;++i)
            gdots.push_back({uid(0,GDOT_REP-1),uid(2,12),2,1});
        for (int i=0;i<16;++i)
            gdots.push_back({uid(0,GDOT_REP-1),uid(2,8),uid(4,8),uid(2,3)});
    }

    // ── Jump physics helpers ────────────────────────────────────────
    // Max cactus height = 75% of jump peak height
    float jumpMaxCactusH() const {
        float peakH=(JUMP_VEL*JUMP_VEL)/(2.f*GRAVITY);
        return peakH*0.75f;
    }
    // Max cluster width = 90% of horizontal jump distance at current speed
    float jumpMaxClusterW() const {
        float airTime=2.f*(-JUMP_VEL)/GRAVITY;
        return speed*airTime*0.90f;
    }

    // ── Cactus cluster spawn ────────────────────────────────────────
    Obstacle spawnCactusCluster(){
        auto uid=[&](int a,int b){return std::uniform_int_distribution<int>(a,b)(rng);};
        float maxH=jumpMaxCactusH();
        float maxW=jumpMaxClusterW();

        // Cluster size: 25% chance 1, 40% chance 2, 25% chance 3, 10% chance 4
        int nr=uid(0,99);
        int nc=(nr<25)?1:(nr<65)?2:(nr<90)?3:4;

        std::vector<CactusParams> cv;
        std::vector<int> ov;
        int cur=0;
        for (int i=0;i<nc;++i){
            CactusParams c=genCactus(rng,maxH);
            cv.push_back(c);
            ov.push_back(cur);
            int gap=(i<nc-1)?uid(1,4):0;
            cur+=c.totalW()+gap;
            if (cur>=(int)maxW&&i<nc-1){ nc=i+1; break; }
        }
        int tw=cur>0?cur:(!cv.empty()?cv[0].totalW():20);
        return Obstacle::makeCluster((float)WIN_W+10,std::move(cv),std::move(ov),tw);
    }

    // ── Obstacle spawn ─────────────────────────────────────────────
    void spawnObstacle(){
        int roll=std::uniform_int_distribution<int>(0,9)(rng);
        if (roll<3){
            int alt=std::uniform_int_distribution<int>(0,1)(rng);
            obs.push_back(Obstacle::makeBird((float)WIN_W+10,alt));
        } else {
            obs.push_back(spawnCactusCluster());
        }
    }

    // ── HUD pixel score ────────────────────────────────────────────
    void renderHUD(){
        // "HI 00000 00000" right-aligned top corner
        std::string hi ="HI "+fmtScore((int)highScore);
        std::string sc =" "+fmtScore((int)scoreDisplay);
        std::string full=hi+sc;
        int tw=glyphStrW(full);
        setCol(rnd,DARK);
        drawGlyphStr(rnd,full, WIN_W-tw-8, 6);
    }

    // ── Info screen ────────────────────────────────────────────────
    void renderInfo(){
        setCol(rnd,BG); SDL_RenderClear(rnd);
        setCol(rnd,DARK); fillRect(rnd,0,GROUND_Y,WIN_W,2);
        setCol(rnd,MED);  fillRect(rnd,0,GROUND_Y+2,WIN_W,WIN_H-GROUND_Y-2);
        if (player) player->draw(rnd);

        setCol(rnd,DARK); fillRect(rnd,WIN_W/2-148,6,296,44);
        drawText("DINO BIKE",fBig,BG,0,12,true);

        const int BX=WIN_W/2-265,BW=530;
        auto box=[&](int y,int h){
            setCol(rnd,MED); fillRect(rnd,BX,y,BW,h);
            setCol(rnd,DARK); drawRect(rnd,BX,y,BW,h);
        };
        box(60,32);  drawText("Автор: Kayle  |  ИТИП  |  Вариант 10  |  C++ / SDL2",font,DARK,0,68,true);
        box(100,32); drawText("Цель: уехать как можно дальше, перепрыгивая препятствия!",font,DARK,0,108,true);
        box(140,86);
        drawText("Управление:",font,DARK,0,147,true);
        drawText("ПРОБЕЛ / ВВЕРХ — прыжок  (держать = выше)",font,DARK,0,165,true);
        drawText("ВНИЗ — пригнуться",font,DARK,0,183,true);
        drawText("ВЛЕВО / ВПРАВО — движение     ESC — выход",font,DARK,0,201,true);

        if ((SDL_GetTicks()/500)%2==0)
            drawText("Нажмите ПРОБЕЛ для начала",font,DARK,0,242,true);

        SDL_RenderPresent(rnd);
    }

    // ── Game Over overlay ──────────────────────────────────────────
    void renderGameOver(){
        drawText("GAME  OVER",fBig,DARK,0,24,true);

        // Pixel-font score line centred
        std::string hi ="HI "+fmtScore((int)highScore);
        std::string sc =" "+fmtScore((int)scoreDisplay);
        std::string full=hi+sc;
        int tw=glyphStrW(full);
        setCol(rnd,DARK);
        drawGlyphStr(rnd,full,(WIN_W-tw)/2,80);

        if ((SDL_GetTicks()/500)%2==0)
            drawText("ПРОБЕЛ — заново     ESC — выход",font,DARK,0,120,true);
    }

    // ── World render ───────────────────────────────────────────────
    void renderWorld(){
        setCol(rnd,BG); SDL_RenderClear(rnd);

        // Clouds
        setCol(rnd,MED);
        for (auto& c:clouds){
            int cx=(int)c.x;
            fillRect(rnd,cx,       (int)c.y+4, c.w/2,8);
            fillRect(rnd,cx+c.w/4,(int)c.y,    c.w/2,12);
            fillRect(rnd,cx+c.w/2,(int)c.y+3,  c.w/2,9);
        }

        // Dunes
        setCol(rnd,LITE);
        for (auto& d:dunes){
            int dx=(int)d.x;
            fillRect(rnd,dx,GROUND_Y-d.h,d.w,d.h);
            if (d.w>50) fillRect(rnd,dx+d.w/5,GROUND_Y-d.h-2,d.w*3/5,2);
        }

        setCol(rnd,DARK); fillRect(rnd,0,GROUND_Y,WIN_W,2);
        setCol(rnd,MED);  fillRect(rnd,0,GROUND_Y+2,WIN_W,WIN_H-GROUND_Y-2);

        // Ground dots
        setCol(rnd,DARK);
        int gsc=(int)groundScroll;
        for (auto& dot:gdots){
            int base=((dot.rx-gsc%GDOT_REP)%GDOT_REP+GDOT_REP)%GDOT_REP;
            for (int rep=-1;rep<=1;++rep){
                int dx=base+rep*GDOT_REP;
                if (dx>=-10&&dx<WIN_W+10)
                    fillRect(rnd,dx,GROUND_Y+dot.ry,dot.w,dot.h);
            }
        }

        for (auto& o:obs) o.draw(rnd);
        player->draw(rnd);
        renderHUD();

        // Pause overlay
        if (paused){
            SDL_SetRenderDrawColor(rnd,BG,BG,BG,150);
            SDL_SetRenderDrawBlendMode(rnd,SDL_BLENDMODE_BLEND);
            SDL_Rect full{0,0,WIN_W,WIN_H}; SDL_RenderFillRect(rnd,&full);
            SDL_SetRenderDrawBlendMode(rnd,SDL_BLENDMODE_NONE);
            drawText("— ПАУЗА —",font,DARK,0,WIN_H/2-12,true);
        }
    }

    // ── Update ─────────────────────────────────────────────────────
    void update(float dt){
        // Score counter: grows at scoreRate pts/sec
        scoreDisplay += scoreRate*dt;
        // Live highscore update
        if (scoreDisplay>highScore) highScore=scoreDisplay;
        // Accelerate score rate every 80 pts (diminishing each time)
        if ((int)scoreDisplay>=nextAccelAt){
            scoreRate       += scoreRate*(scoreAccelFactor-1.f);
            scoreAccelFactor = 1.f+(scoreAccelFactor-1.f)*0.88f;
            nextAccelAt     += 80;
            // Bird speed grows proportionally with scoreRate
            birdSpeed = INIT_SPEED*BIRD_SPEED_FRAC*(scoreRate/10.f);
        }

        groundScroll+=speed*dt;

        const float MS=160.f;
        if (keyLeft &&player->x>55.f)  player->x-=MS*dt;
        if (keyRight&&player->x<280.f) player->x+=MS*dt;
        player->setDuck(keyDown);

        speedTimer+=dt;
        if (speedTimer>=1.f){speed+=SPEED_INCR;speedTimer=0;}

        auto uid=[&](int a,int b){return std::uniform_int_distribution<int>(a,b)(rng);};

        // Clouds
        for (auto& c:clouds){
            c.x-=40.f*dt;
            if (c.x+c.w<0){c.x=WIN_W+10.f;c.y=float(uid(8,55));c.w=uid(55,110);}
        }
        // Dunes
        for (auto& d:dunes){
            d.x-=speed*0.22f*dt;
            if (d.x+d.w<0){d.x=float(WIN_W+uid(0,80));d.w=uid(60,180);d.h=uid(4,16);}
        }

        bool justLanded=player->update(dt,speed);
        if (justLanded&&keyJump){ if (player->jump()) playSound(g_jumpSnd); }

        obstTimer+=dt;
        if (obstTimer>=obstInterval){
            obstTimer=0;
            obstInterval=std::uniform_real_distribution<float>(1.0f,2.4f)(rng)*(INIT_SPEED/speed);
            spawnObstacle();
        }

        for (auto& o:obs) o.update(speed,birdSpeed,dt);
        obs.erase(std::remove_if(obs.begin(),obs.end(),
            [](const Obstacle& o){return o.offScreen();}),obs.end());

        // Collision
        SDL_Rect pb=player->bounds();
        for (auto& o:obs){
            for (auto& ob:o.hitboxes()){
                SDL_Rect ix;
                if (SDL_IntersectRect(&pb,&ob,&ix)){
                    if (scoreDisplay>highScore) highScore=scoreDisplay;
                    player->shocked=true;
                    state=State::OVER;
                    playSound(g_dieSnd);
                    return;
                }
            }
        }
    }

    // ── Events ─────────────────────────────────────────────────────
    void handleEvents(){
        SDL_Event e;
        while (SDL_PollEvent(&e)){
            if (e.type==SDL_QUIT){running=false;return;}

            // Focus → pause
            if (e.type==SDL_WINDOWEVENT){
                if (e.window.event==SDL_WINDOWEVENT_FOCUS_LOST){
                    paused=true;
                } else if (e.window.event==SDL_WINDOWEVENT_FOCUS_GAINED){
                    paused=false;
                    prevTick=SDL_GetTicks(); // avoid dt spike
                }
            }

            if (e.type==SDL_KEYDOWN){
                switch(e.key.keysym.sym){
                case SDLK_ESCAPE: running=false; return;
                case SDLK_SPACE: case SDLK_UP:
                    keyJump=true;
                    if (!e.key.repeat){
                        if      (state==State::INFO) state=State::PLAY;
                        else if (state==State::PLAY){ if (player->jump()) playSound(g_jumpSnd); }
                        else if (state==State::OVER){ resetRound(); state=State::PLAY; }
                    }
                    break;
                case SDLK_DOWN:  keyDown=true;  break;
                case SDLK_LEFT:  keyLeft=true;  break;
                case SDLK_RIGHT: keyRight=true; break;
                default: break;
                }
            }
            if (e.type==SDL_KEYUP){
                switch(e.key.keysym.sym){
                case SDLK_SPACE: case SDLK_UP:
                    keyJump=false; if (player) player->cutJump(); break;
                case SDLK_DOWN:  keyDown=false;  break;
                case SDLK_LEFT:  keyLeft=false;  break;
                case SDLK_RIGHT: keyRight=false; break;
                default: break;
                }
            }
        }
    }

public:
    ~Game(){
        delete player;
        if (fBig) TTF_CloseFont(fBig);
        if (font) TTF_CloseFont(font);
        if (rnd)  SDL_DestroyRenderer(rnd);
        if (win)  SDL_DestroyWindow(win);
        if (g_audio) SDL_CloseAudioDevice(g_audio);
        TTF_Quit(); SDL_Quit();
    }

    bool init(){
        if (SDL_Init(SDL_INIT_VIDEO|SDL_INIT_AUDIO)<0) return false;
        if (TTF_Init()<0) return false;
        win=SDL_CreateWindow("Dino Bike",
            SDL_WINDOWPOS_CENTERED,SDL_WINDOWPOS_CENTERED,WIN_W,WIN_H,SDL_WINDOW_SHOWN);
        if (!win) return false;
        rnd=SDL_CreateRenderer(win,-1,SDL_RENDERER_ACCELERATED|SDL_RENDERER_PRESENTVSYNC);
        if (!rnd) return false;

        SDL_DisplayMode mode;
        if (SDL_GetCurrentDisplayMode(0,&mode)==0&&mode.refresh_rate>0)
            displayFPS=mode.refresh_rate;

        const char* fp="C:/Windows/Fonts/arial.ttf";
        font=TTF_OpenFont(fp,19);
        fBig=TTF_OpenFont(fp,36);
        if (!font||!fBig) return false;

        initAudio();
        resetRound();
        prevTick=SDL_GetTicks();
        return true;
    }

    void run(){
        while (running){
            Uint32 now=SDL_GetTicks();
            float dt=std::min((now-prevTick)/1000.f,0.05f);
            prevTick=now;

            handleEvents();
            if (!running) break;

            if (state==State::INFO){
                renderInfo();
            } else {
                if (state==State::PLAY&&!paused) update(dt);
                renderWorld();
                if (state==State::OVER) renderGameOver();
                SDL_RenderPresent(rnd);
            }

            Uint32 elapsed=SDL_GetTicks()-now;
            Uint32 frame=1000u/(Uint32)displayFPS;
            if (elapsed<frame) SDL_Delay(frame-elapsed);
        }
    }
};

// ─────────────────────────────────────────────────────────────────────────
int main(int,char**){
    Game game;
    if (!game.init()){
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR,"Init error",SDL_GetError(),nullptr);
        return 1;
    }
    game.run();
    return 0;
}
