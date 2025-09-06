#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <SDL2/SDL_ttf.h>
#include <SDL2/SDL_mixer.h>
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <ctime>
#include <iomanip>
#include <iostream>
#include <limits>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

const int SCREEN_WIDTH  = 1360;
const int SCREEN_HEIGHT = 1024;

float CAM_Y_ANCHOR = 0.68f;
int   CAM_Y_PIXELS = 0;
const float CAM_ANCHOR_MIN = 0.30f, CAM_ANCHOR_MAX = 0.90f;
const float CAM_ANCHOR_STEP = 0.02f;
const int   CAM_PIXEL_STEP  = 4;

float GRAVITY_NORMAL  = 0.50f;
float GRAVITY_MOON    = 0.18f;
float JUMP_IMP_NORMAL = -12.0f;
float JUMP_IMP_MOON   = -10.5f;
bool  MOON_MODE       = false;

int HITBOX_SHRINK_X = 150;
int HITBOX_SHRINK_Y = 4;
int OUTER_SHRINK_X  = 200;
int OUTER_SHRINK_Y  = 100;

const int WORLD_GROUND_TOP = 950;

float AVOCADO_SCALE         = 0.38f;
int   AVOCADO_HP            = 2;
float AVOCADO_WALK_SPEED    = 2.2f;
int   AVOCADO_CONTACT_DMG   = 6;
Uint32 AVOCADO_DMG_COOLDOWN = 300;
Uint32 AVOCADO_SPLIT_SHOWMS = 650;

int    KNIFE_CLIP          = 3;
bool   KNIFE_UNLIMITED     = false;
float  KNIFE_SPEED         = 3.0f;
float  KNIFE_SCALE         = 3.90f;
int    KNIFE_DMG           = 1;
Uint32 KNIFE_RECHARGE_MS   = 900;
Uint32 KNIFE_LIFETIME_MS   = 4000;
double KNIFE_SPIN_DPS      = 720.0;

Uint32 THROW_POSE_MS       = 400;

const float KNIFE_SPAWN_OFF_X     = 22.0f;
const float KNIFE_SPAWN_OFF_Y     = 30.0f;
const float KNIFE_SPAWN_HAND_FRAC = 0.42f;

const std::string kSpaceGIF    = "game/assets/images/test.gif";
const std::string kPlayerPNG   = "game/nft/nft8.png";         
const std::string kFontTTF     = "game/assets/fonts/pixeldeklein.ttf";
const std::string kWeaponPNG   = "game/assets/images/weapon.png";
const std::string kAvoPNG      = "game/assets/images/avocado.png";
const std::string kAvoSplitPNG = "game/assets/images/avocado_split.png";
const std::string kSndSlice    = "game/assets/images/slice.mp3";
const std::string kSndDeath    = "game/assets/images/lost_sound.mp3";
const std::string kSndThrow    = "game/assets/images/throweffect.mp3";
const std::string kSndWeb      = "game/assets/images/web.mp3";

const int  WEB_LOCK_ROW_1BASE = 3; 
const int  WEB_LOCK_COL_1BASE = 8;   
const int  WEB_LOCK_ROW = WEB_LOCK_ROW_1BASE - 1; 
const int  WEB_LOCK_COL = WEB_LOCK_COL_1BASE - 1;

float WEB_ANCHOR_U = 0.78f; 
float WEB_ANCHOR_V = 0.33f;  
const bool WEB_ANCHOR_MIRROR = true;

static void renderText(SDL_Renderer* r, TTF_Font* f, const std::string& text, int x, int y, SDL_Color col={255,255,255,255}) {
  SDL_Surface* surf = TTF_RenderText_Blended(f, text.c_str(), col);
  if (!surf) return;
  SDL_Texture* tex = SDL_CreateTextureFromSurface(r, surf);
  SDL_Rect dst{ x, y, surf->w, surf->h };
  SDL_RenderCopy(r, tex, nullptr, &dst);
  SDL_FreeSurface(surf);
  SDL_DestroyTexture(tex);
}
static bool overlapX(const SDL_Rect& a, const SDL_Rect& b) {
  return (a.x + a.w > b.x) && (a.x < b.x + b.w);
}
static float clampf(float v, float a, float b){ return std::max(a, std::min(b, v)); }

struct AnimFrame {
  SDL_Texture* tex{};
  int w{0}, h{0};
  int delay_ms{100};
};
class AnimatedTiledBG {
public:
  bool parallax = true;
  float parallaxFactorX = 0.25f, parallaxFactorY = 0.25f;
  float scale = 1.0f;
  bool loaded = false;

  bool load(SDL_Renderer* ren, const std::string& path) {
#if SDL_IMAGE_VERSION_ATLEAST(2,6,0)
    if (IMG_Animation* a = IMG_LoadAnimation(path.c_str())) {
      frames.reserve(a->count);
      for (int i = 0; i < a->count; ++i) {
        SDL_Surface* s = a->frames[i];
        if (!s) continue;
        SDL_Texture* t = SDL_CreateTextureFromSurface(ren, s);
        if (!t) continue;
        AnimFrame f; f.tex = t; f.w = s->w; f.h = s->h;
        int raw = a->delays ? a->delays[i] : 10;
        f.delay_ms = std::max(10, std::min(2000, raw <= 0 ? 10 : raw));
        frames.push_back(f);
      }
      IMG_FreeAnimation(a);
      loaded = !frames.empty();
      lastTick = SDL_GetTicks();
      return loaded;
    }
#endif
    if (SDL_Surface* s = IMG_Load(path.c_str())) {
      frameStatic.tex = SDL_CreateTextureFromSurface(ren, s);
      frameStatic.w = s->w; frameStatic.h = s->h;
      SDL_FreeSurface(s);
      useStatic = (frameStatic.tex != nullptr);
      loaded = useStatic; lastTick = SDL_GetTicks();
      return loaded;
    }
    return false;
  }

  void render(SDL_Renderer* ren, int camX, int camY, int screenW, int screenH) {
    if (!loaded) return; advanceFrame();
    AnimFrame f = currentFrame();
    int tw = int(f.w * scale), th = int(f.h * scale);
    if (tw <= 0 || th <= 0) return;

    int originX = 0, originY = 0;
    if (parallax) {
      originX = -int((camX * parallaxFactorX)) % tw;
      originY = -int((camY * parallaxFactorY)) % th;
      if (originX > 0) originX -= tw; if (originY > 0) originY -= th;
    }
    for (int y = originY; y < screenH; y += th)
      for (int x = originX; x < screenW; x += tw) {
        SDL_Rect dst{ x, y, tw, th }; SDL_RenderCopy(ren, f.tex, nullptr, &dst);
      }
  }
  void setParallax(bool on) { parallax = on; }
  void addScale(float d)    { scale = std::max(0.05f, scale + d); }
  void resetScale()         { scale = 1.0f; }
private:
  SDL_Renderer* renderer{};
  std::vector<AnimFrame> frames;
  AnimFrame frameStatic; bool useStatic = false;
  size_t frameIndex = 0; Uint32 lastTick = 0;
  AnimFrame currentFrame() const { return useStatic ? frameStatic : frames[frameIndex]; }
  void advanceFrame() {
    if (useStatic || frames.empty()) return;
    Uint32 now = SDL_GetTicks();
    if (now - lastTick >= (Uint32)frames[frameIndex].delay_ms) {
      frameIndex = (frameIndex + 1) % frames.size(); lastTick = now;
    }
  }
};

struct Tile {
  SDL_Rect rect;
  SDL_Texture* texture{};
  SDL_Color glowColor{0,0,0,0};
  bool isDynamic{false};
  Uint32 glowUntilMS{0};
};

static SDL_Texture* makeNeonTexture(SDL_Renderer* r, int w, int h, SDL_Color baseBg, SDL_Color line1, SDL_Color line2) {
  SDL_Surface* s = SDL_CreateRGBSurfaceWithFormat(0, w, h, 32, SDL_PIXELFORMAT_RGBA32);
  if (!s) return nullptr;
  SDL_FillRect(s, nullptr, SDL_MapRGBA(s->format, baseBg.r, baseBg.g, baseBg.b, 255));
  for (int y=0; y<h; ++y) {
    float t = std::sin((y/float(h))*6.28318f*2.0f) * 0.5f + 0.5f;
    Uint8 rr = Uint8(line2.r * (0.2f + 0.8f*t));
    Uint8 gg = Uint8(line2.g * (0.2f + 0.8f*t));
    Uint8 bb = Uint8(line2.b * (0.2f + 0.8f*t));
    Uint32* row = (Uint32*)((Uint8*)s->pixels + y*s->pitch);
    for (int x=0; x<w; ++x) row[x] = (row[x] & 0xFF000000) | (rr<<16) | (gg<<8) | bb;
  }
  int cell = std::max(8, w/24);
  for (int y=0; y<h; ++y) {
    bool hLine = (y % cell) == 0;
    Uint32* row = (Uint32*)((Uint8*)s->pixels + y*s->pitch);
    for (int x=0; x<w; ++x) if (hLine || (x % cell) == 0)
      row[x] = SDL_MapRGBA(s->format, line1.r, line1.g, line1.b, 255);
  }
  SDL_Texture* tex = SDL_CreateTextureFromSurface(r, s);
  SDL_FreeSurface(s);
  SDL_SetTextureBlendMode(tex, SDL_BLENDMODE_BLEND);
  return tex;
}
static void renderGlowRect(SDL_Renderer* ren, const SDL_Rect& worldRect, int camX, int camY, SDL_Color color, float pulse01) {
  Uint8 baseA = Uint8(120 + 135*pulse01);
  SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_ADD);
  for (int i=0; i<5; ++i) {
    SDL_Rect rr{ worldRect.x - camX - i*3, worldRect.y - camY - i*3, worldRect.w + i*6, worldRect.h + i*6 };
    Uint8 a = Uint8(std::max(0, int(baseA) - i*20));
    SDL_SetRenderDrawColor(ren, color.r, color.g, color.b, a); SDL_RenderDrawRect(ren, &rr);
  }
  SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_BLEND);
}

enum class FlipType { None, Back, Front };

class Sprite {
public:
  Sprite(SDL_Renderer* ren, const std::string& file, int rows=3, int cols=8)
  : renderer(ren), rows(rows), cols(cols) {
    SDL_Surface* surf = IMG_Load(file.c_str());
    if (!surf) throw std::runtime_error(std::string("IMG_Load: ") + IMG_GetError());
    texture.reset(SDL_CreateTextureFromSurface(ren, surf)); SDL_FreeSurface(surf);
    int imgW, imgH; SDL_QueryTexture(texture.get(), nullptr, nullptr, &imgW, &imgH);
    frameW = imgW / cols; frameH = imgH / rows;
    dst.w = int(frameW * scale); dst.h = int(frameH * scale);
    updateCrop();
  }

  void setWebAnchorUV(float u, float v, bool mirror=true) {
    webU = clampf(u, 0.f, 1.f); webV = clampf(v, 0.f, 1.f); webMirror = mirror;
  }
  void nudgeWebUV(float du, float dv) {
    setWebAnchorUV(webU + du, webV + dv, webMirror);
  }
  float getWebU() const { return webU; }
  float getWebV() const { return webV; }

  SDL_FPoint webAnchorWorld() const {
    float u = webU;
    if (webMirror && facing == SDL_FLIP_HORIZONTAL) u = 1.0f - u;
    return SDL_FPoint{ x + dst.w * u, y + dst.h * webV };
  }

  void placeOnTopOf(int tileTop) { y = (float)tileTop - dst.h; }

  int handleInput(const Uint8* keys, const std::vector<Tile>& tiles, bool inputEnabled) {
    float dx = 0.0f;
    if (inputEnabled) { if (keys[SDL_SCANCODE_A]) dx = -1.0f; if (keys[SDL_SCANCODE_D]) dx = +1.0f; }
    x += dx * speed; if (dx < 0) facing = SDL_FLIP_HORIZONTAL; else if (dx > 0) facing = SDL_FLIP_NONE;

    if (frameLock) {
      currentRow = lockRow; currentFrame = lockCol; updateCrop();
    } else if (throwPoseUntilMS && SDL_GetTicks() < throwPoseUntilMS) {
      currentRow = throwPoseRow; currentFrame = throwPoseCol; updateCrop();
    } else {
      throwPoseUntilMS = 0;
      (dx != 0) ? playAnim(0, 7, 2, 100) : idle(120);
    }

    const float g = MOON_MODE ? GRAVITY_MOON : GRAVITY_NORMAL;
    float prevY = y;
    SDL_Rect cPrev = collisionRect(); float prevBottom = cPrev.y + cPrev.h;

    velY += g; y += velY; onGround = false;

    const float EPS = 0.5f, MAX_STEP = 24.0f;
    SDL_Rect cNow = collisionRect(); float nowBottom = cNow.y + cNow.h;
    bool landed = false; int bestTop = std::numeric_limits<int>::min(), landedIndex = -1;

    for (size_t i=0; i<tiles.size(); ++i) {
      const auto& t = tiles[i];
      if (!((cNow.x + cNow.w > t.rect.x) && (cNow.x < t.rect.x + t.rect.w))) continue;
      if (velY >= 0.0f) {
        if (prevBottom <= t.rect.y + EPS && nowBottom >= t.rect.y - EPS) {
          if ((nowBottom - prevBottom) <= (MAX_STEP + std::max(0.0f, velY))) {
            if (t.rect.y > bestTop) { bestTop = t.rect.y; landed = true; landedIndex = (int)i; }
          }
        }
      }
    }
    if (landed) { y = (float)bestTop - (float)(collisionRect().h + HITBOX_SHRINK_Y); velY = 0.0f; onGround = true; jumpCount = 0; flipping = false; flipAngle = 0.0; }
    updateFlip();
    return landedIndex;
  }

  void jump(bool inputEnabled) {
    if (!inputEnabled) return;
    if (onGround || jumpCount < 2) { velY = MOON_MODE ? JUMP_IMP_MOON : JUMP_IMP_NORMAL; onGround = false; jumpCount++; }
  }

  void render(SDL_Renderer* r, int camX, int camY, bool debug, bool dead=false) {
    SDL_Rect dstR = dst; dstR.x = (int)(x - camX); dstR.y = (int)(y - camY);
    SDL_Point center{ dstR.w / 2, dstR.h / 2 };
    double angle = dead ? 90.0 : (flipping ? (flipSign * flipAngle) : 0.0);
    SDL_RenderCopyEx(r, texture.get(), &crop, &dstR, angle, &center, facing);
    if (debug) {
      SDL_Rect outer = interactionRect(); SDL_Rect outerScr{ outer.x - camX, outer.y - camY, outer.w, outer.h };
      SDL_SetRenderDrawColor(r, 255, 40, 40, 200); SDL_RenderDrawRect(r, &outerScr);
      SDL_Rect c = collisionRect(); SDL_Rect cScr{ c.x - camX, c.y - camY, c.w, c.h };
      SDL_SetRenderDrawColor(r, 0, 220, 255, 220); SDL_RenderDrawRect(r, &cScr);
      // draw small cross at web anchor
      SDL_FPoint wa = webAnchorWorld();
      int ax = int(wa.x) - camX, ay = int(wa.y) - camY;
      SDL_SetRenderDrawColor(r, 255, 255, 0, 200);
      SDL_RenderDrawLine(r, ax-4, ay, ax+4, ay);
      SDL_RenderDrawLine(r, ax, ay-4, ax, ay+4);
    }
  }

  void playAnim(int f, int l, int r, float speedMS) {
    if (frameLock) return;
    firstFrame = f; lastFrame = l; currentRow = r;
    if (SDL_GetTicks() - lastAnimTick >= (Uint32)speedMS) { currentFrame = (currentFrame + 1 > lastFrame) ? firstFrame : currentFrame + 1; updateCrop(); lastAnimTick = SDL_GetTicks(); }
  }
  void idle(float speedMS) {
    if (frameLock) return;
    if (SDL_GetTicks() - lastIdleTick >= (Uint32)speedMS) { currentRow = 0; currentFrame = (currentFrame + 1) % cols; updateCrop(); lastIdleTick = SDL_GetTicks(); }
  }

  void startFlip(FlipType type, bool inputEnabled) {
    if (!inputEnabled) return; if (onGround || flipping) return;
    flipping = true; flipType = type; flipAngle = 0.0; flipStartMS = SDL_GetTicks();
    bool facingLeft = (facing == SDL_FLIP_HORIZONTAL);
    if (type == FlipType::Back)  flipSign = facingLeft ? +1 : -1; else flipSign = facingLeft ? -1 : +1;
  }
  void updateFlip() {
    if (!flipping) return; double dur = 600.0;
    double t = (SDL_GetTicks() - flipStartMS) / dur;
    if (t >= 1.0) { flipping = false; flipAngle = 0.0; flipType = FlipType::None; return; }
    double eased = 0.5 - 0.5 * std::cos(t * M_PI); flipAngle = 360.0 * eased;
  }

  void triggerThrowPose(Uint32 holdMS = THROW_POSE_MS) {
    if (frameLock) return; 
    throwPoseUntilMS = SDL_GetTicks() + holdMS; currentRow = throwPoseRow; currentFrame = throwPoseCol; updateCrop();
  }

  void setFrameLock(bool on, int row=WEB_LOCK_ROW, int col=WEB_LOCK_COL) {
    frameLock = on; if (on) { lockRow = row; lockCol = col; currentRow = row; currentFrame = col; updateCrop(); }
  }

  SDL_Rect collisionRect() const {
    int x0 = (int)x + HITBOX_SHRINK_X;
    int y0 = (int)y + HITBOX_SHRINK_Y;
    int w  = dst.w - HITBOX_SHRINK_X * 2;
    int h  = dst.h - HITBOX_SHRINK_Y * 2;
    if (w < 4) w = 4; if (h < 4) h = 4;
    return { x0, y0, w, h };
  }
  SDL_Rect interactionRect() const {
    int x0 = (int)x + OUTER_SHRINK_X;
    int y0 = (int)y + OUTER_SHRINK_Y;
    int w  = dst.w - OUTER_SHRINK_X * 2;
    int h  = dst.h - OUTER_SHRINK_Y * 2;
    if (w < 4) w = 4; if (h < 4) h = 4;
    return { x0, y0, w, h };
  }
  SDL_Rect worldRect() const { return { (int)x, (int)y, dst.w, dst.h }; }
  float centerX() const { return x + dst.w * 0.5f; }
  float centerY() const { return y + dst.h * 0.5f; }

  float getX() const { return x; } float getY() const { return y; }
  int   getW() const { return dst.w; } int getH() const { return dst.h; }
  float getVelY() const { return velY; }
  void  addVelocity(float ax, float ay){ x += ax; y += ay; }
  bool  isOnGround() const { return onGround; }
  int   getJumpCount() const { return jumpCount; }
  SDL_RendererFlip getFacing() const { return facing; }
  void  addVelY(float d){ velY += d; }
  void  setVel(float vx_, float vy_){ velX = vx_; velY = vy_; }
  void  addVel(float dx, float dy){ velX += dx; velY += dy; }
  float getVelX() const { return velX; }

  void hardResetPosition(int tileTop) {
    flipping = false; flipAngle = 0.0; flipType = FlipType::None;
    velX = 0.0f; velY = 0.0f; onGround = false; jumpCount = 0;
    throwPoseUntilMS = 0; setFrameLock(false);
    currentRow = 0; currentFrame = 0; updateCrop(); placeOnTopOf(tileTop);
  }

private:
  std::unique_ptr<SDL_Texture, void(*)(SDL_Texture*)> texture{nullptr, SDL_DestroyTexture};
  SDL_Renderer* renderer{};
  SDL_Rect crop{}, dst{};
  int frameW{}, frameH{}, rows = 3, cols = 8;
  int currentFrame = 0, firstFrame = 0, lastFrame = 7, currentRow = 0;
  Uint32 lastAnimTick = 0, lastIdleTick = 0;

  float x = 0.0f, y = 0.0f;
  float velX = 0.0f, velY = 0.0f, speed = 4.0f;
  float scale = 3.0f; bool onGround = false; int jumpCount = 0;
  SDL_RendererFlip facing = SDL_FLIP_NONE;

  bool   flipping = false; double flipAngle = 0.0; Uint32 flipStartMS = 0; int flipSign = -1; FlipType flipType = FlipType::None;

  Uint32 throwPoseUntilMS = 0; int throwPoseRow = 2; int throwPoseCol = 5;

  bool frameLock = false; int lockRow = WEB_LOCK_ROW; int lockCol = WEB_LOCK_COL;

  float webU = WEB_ANCHOR_U, webV = WEB_ANCHOR_V; bool webMirror = WEB_ANCHOR_MIRROR;

  void updateCrop(){ crop.x = currentFrame * frameW; crop.y = currentRow * frameH; crop.w = frameW; crop.h = frameH; dst.w = int(frameW*scale); dst.h = int(frameH*scale); }
};

struct Knife { float x{}, y{}, vx{}, vy{}; int w{}, h{}; Uint32 bornMS{}; bool active{true}; double angleDeg{0.0}; double spinDPS{KNIFE_SPIN_DPS}; int dir{+1}; };

struct Avocado { float x{}, y{}, vx{}, vy{}; int w{}, h{}; int hp{AVOCADO_HP}; bool split{false}; Uint32 splitUntilMS{0}; Uint32 lastTouchDmgMS{0}; bool counted{false}; };

enum class WebState { None, Shooting, Latched };
struct Web {
  WebState state{WebState::None};
  bool  rmbHeld{false};
  int   anchorTile{-1};
  Uint32 startedMS{0};

  float ax{0.f}, ay{0.f};  

  float targetLen{0.f};     
  float curLen{0.f};   
  float minLen{70.f};
  float maxLen{1800.f};
  float shootSpeed{1400.f};

  float damping{0.04f};
  float reelRate{550.f};
  float pumpGain{0.45f};

  float lastTension{0.f};
  bool  reelIn{false};
  bool  reelOut{false};
  bool  pump{false};
};

static void drawWigglyWeb(SDL_Renderer* ren, int x1, int y1, int x2, int y2, float t) {
  SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_BLEND);
  SDL_SetRenderDrawColor(ren, 255,255,255,220);
  const int segments = 18;
  const float amp = 3.5f;
  float dx = float(x2 - x1), dy = float(y2 - y1);
  float len = std::max(1.0f, std::sqrt(dx*dx + dy*dy));
  float nx = -dy/len, ny = dx/len; 
  int px = x1, py = y1;
  for (int i=1;i<=segments;i++){
    float s = i/(float)segments;
    float wig = std::sin((s*6.28318f*2.0f) + t*7.0f) * amp * (0.3f + 0.7f*(1.0f - std::fabs(0.5f-s)*2.0f));
    int qx = int(x1 + dx*s + nx*wig);
    int qy = int(y1 + dy*s + ny*wig);
    SDL_RenderDrawLine(ren, px, py, qx, qy);
    px=qx; py=qy;
  }
}

int main() {
  srand((unsigned)time(nullptr));

  if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) != 0) { std::cerr << "SDL_Init: " << SDL_GetError() << "\n"; return 1; }
  int imgFlags = IMG_INIT_PNG | IMG_INIT_JPG | IMG_INIT_TIF | IMG_INIT_WEBP | IMG_INIT_AVIF; IMG_Init(imgFlags);
  if (TTF_Init() != 0) { std::cerr << "TTF_Init: " << TTF_GetError() << "\n"; }
  int mixFlags = MIX_INIT_MP3 | MIX_INIT_OGG; Mix_Init(mixFlags);
  if (Mix_OpenAudio(44100, MIX_DEFAULT_FORMAT, 2, 2048) < 0) { std::cerr << "Mix_OpenAudio: " << Mix_GetError() << "\n"; }
  Mix_AllocateChannels(32); Mix_Volume(-1, int(MIX_MAX_VOLUME * 0.90));

  SDL_Window* win = SDL_CreateWindow("Moki vs Avocados — Swing v2", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, SCREEN_WIDTH, SCREEN_HEIGHT, SDL_WINDOW_RESIZABLE);
  SDL_Renderer* ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);

  TTF_Font* font = TTF_OpenFont(kFontTTF.c_str(), 17);
  TTF_Font* fontBig = TTF_OpenFont(kFontTTF.c_str(), 72);

  Mix_Chunk* sliceSnd = Mix_LoadWAV(kSndSlice.c_str());
  Mix_Chunk* deathSnd = Mix_LoadWAV(kSndDeath.c_str());
  Mix_Chunk* throwSnd = Mix_LoadWAV(kSndThrow.c_str());
  Mix_Chunk* webSnd   = Mix_LoadWAV(kSndWeb.c_str());

  AnimatedTiledBG spaceBG; spaceBG.load(ren, kSpaceGIF);

  auto makeBluePlatformTex = [&](int w, int h)->SDL_Texture*{
    SDL_Color bg{10, 20, 40, 255}, line{60, 200, 255, 255}, bands{20, 80, 220, 255};
    return makeNeonTexture(ren, w, h, bg, line, bands);
  };
  auto makePurplePlatformTex = [&](int w, int h)->SDL_Texture*{
    SDL_Color bg{20, 10, 35, 255}, line{200, 100, 255, 255}, bands{120, 30, 200, 255};
    return makeNeonTexture(ren, w, h, bg, line, bands);
  };

  std::vector<Tile> tiles;
  tiles.push_back({
    {-5000, WORLD_GROUND_TOP, 10000, 50},
    makeBluePlatformTex(10000, 50),
    SDL_Color{60, 200, 255, 255}, false, 0
  });
  std::vector<Tile> dynPlatforms;

  Sprite player(ren, kPlayerPNG, 3, 8);
  player.placeOnTopOf(WORLD_GROUND_TOP);
  player.setWebAnchorUV(WEB_ANCHOR_U, WEB_ANCHOR_V, WEB_ANCHOR_MIRROR);

  SDL_Texture* texKnife = nullptr; if (SDL_Surface* s = IMG_Load(kWeaponPNG.c_str())) { texKnife = SDL_CreateTextureFromSurface(ren, s); SDL_FreeSurface(s); SDL_SetTextureBlendMode(texKnife, SDL_BLENDMODE_BLEND); }
  SDL_Texture* texAvo = nullptr, *texAvoSplit = nullptr;
  if (SDL_Surface* s1 = IMG_Load(kAvoPNG.c_str())) { texAvo = SDL_CreateTextureFromSurface(ren, s1); SDL_FreeSurface(s1); }
  if (SDL_Surface* s2 = IMG_Load(kAvoSplitPNG.c_str())) { texAvoSplit = SDL_CreateTextureFromSurface(ren, s2); SDL_FreeSurface(s2); }

  std::vector<Knife>   knives;
  std::vector<Avocado> avocados;
  int playerHP = 100, clipKnives = KNIFE_CLIP; Uint32 lastRecharge = SDL_GetTicks();
  int avocadosCollected = 0;
  bool fullscreen = false, debugBoxes = false, showHUD = true, nHeld = false;
  bool gameOver = false, deathPlayed = false;

  Web web;

  Uint32 prevTick = SDL_GetTicks();
  float dt = 1.0f/60.0f;

  auto resetGame = [&](){
    knives.clear(); avocados.clear();
    for (auto& t : dynPlatforms) if (t.texture) SDL_DestroyTexture(t.texture);
    dynPlatforms.clear();
    playerHP = 100; clipKnives = KNIFE_CLIP; lastRecharge = SDL_GetTicks();
    MOON_MODE = false; CAM_Y_ANCHOR = 0.68f; CAM_Y_PIXELS = 0;
    gameOver = false; deathPlayed = false; avocadosCollected = 0; web = Web{};
    player.hardResetPosition(WORLD_GROUND_TOP);
  };

  auto spawnKnife = [&](int dir){
    if (!texKnife || gameOver) return;
    if (!KNIFE_UNLIMITED && clipKnives <= 0) return;
    SDL_Rect pRect = player.worldRect();
    int kw, kh; SDL_QueryTexture(texKnife, nullptr, nullptr, &kw, &kh);
    float handX = pRect.x + pRect.w * 0.5f + dir * KNIFE_SPAWN_OFF_X;
    float handY = pRect.y + pRect.h * KNIFE_SPAWN_HAND_FRAC + KNIFE_SPAWN_OFF_Y;
    Knife k{}; k.w = int(kw * KNIFE_SCALE); k.h = int(kh * KNIFE_SCALE);
    k.x = handX - k.w * 0.5f; k.y = handY - k.h * 0.5f; k.vx = KNIFE_SPEED * (dir >= 0 ? 1.0f : -1.0f);
    k.vy = 0.0f; k.bornMS = SDL_GetTicks(); k.active = true; k.dir = (dir >= 0 ? +1 : -1);
    knives.push_back(k);
    if (throwSnd) Mix_PlayChannel(-1, throwSnd, 0);
    player.triggerThrowPose(THROW_POSE_MS);
    if (!KNIFE_UNLIMITED) clipKnives--;
  };

  auto spawnAvocado = [&](float fromX, float fromY){
    if (!texAvo || gameOver) return;
    int aw, ah; SDL_QueryTexture(texAvo, nullptr, nullptr, &aw, &ah);
    Avocado a{}; a.w = int(aw * AVOCADO_SCALE); a.h = int(ah * AVOCADO_SCALE);
    a.x = fromX; a.y = fromY - a.h; avocados.push_back(a);
  };

  auto avocadoAI = [&](Avocado& a, const std::vector<Tile>& allTiles){
    a.vy += MOON_MODE ? GRAVITY_MOON : GRAVITY_NORMAL;
    float playerCenterX = player.centerX(); float aCenterX = a.x + a.w*0.5f;
    float dir = (playerCenterX > aCenterX) ? +1.0f : -1.0f;
    float targetVX = AVOCADO_WALK_SPEED * dir;
    float prevY = a.y; a.y += a.vy;
    SDL_Rect aRect{ int(a.x), int(a.y), a.w, a.h };
    const float EPS = 0.5f, MAX_STEP = 24.0f; bool grounded = false;
    for (const auto& t : allTiles) {
      if (!overlapX(aRect, t.rect)) continue;
      float prevBottom = prevY + a.h, nowBottom  = a.y  + a.h;
      if (a.vy >= 0.0f && prevBottom <= t.rect.y + EPS && nowBottom >= t.rect.y - EPS) {
        if ((nowBottom - prevBottom) <= (MAX_STEP + std::max(0.0f, a.vy))) {
          a.y = float(t.rect.y - a.h); a.vy = 0.0f; grounded = true; aRect.y = int(a.y);
        }
      }
    }
    if (grounded && !a.split) a.vx = targetVX; else a.vx *= 0.97f; a.x += a.vx;
  };

  auto renderBG = [&](int camX, int camY){
    int outW = SCREEN_WIDTH, outH = SCREEN_HEIGHT; SDL_GetRendererOutputSize(ren, &outW, &outH);
    spaceBG.render(ren, camX, camY, outW, outH);
  };

  bool running = true;
  while (running) {
    Uint32 nowTick = SDL_GetTicks();
    dt = std::max(1.0f/240.0f, std::min(1.0f/30.0f, (nowTick - prevTick) / 1000.0f));
    prevTick = nowTick;

    SDL_Event e;
    while (SDL_PollEvent(&e)) {
      if (e.type == SDL_QUIT) running = false;

      if (e.type == SDL_KEYDOWN) {
        if (e.key.keysym.sym == SDLK_r) { resetGame(); }
        if (!gameOver) {
          if (e.key.keysym.sym == SDLK_SPACE) player.jump(true);
          if (e.key.keysym.sym == SDLK_b) { if (!player.isOnGround()) player.startFlip(FlipType::Back, true); }
          if (e.key.keysym.sym == SDLK_v) { if (!player.isOnGround()) player.startFlip(FlipType::Front, true); }
          if (e.key.keysym.sym == SDLK_m) { MOON_MODE = !MOON_MODE; }
          if (e.key.keysym.sym == SDLK_f) { fullscreen = !fullscreen; SDL_SetWindowFullscreen(win, fullscreen ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0); }
          if (e.key.keysym.sym == SDLK_h)  { debugBoxes = !debugBoxes; }
          if (e.key.keysym.sym == SDLK_F1) { showHUD = !showHUD; }
          if (e.key.keysym.sym == SDLK_t)  { KNIFE_UNLIMITED = !KNIFE_UNLIMITED; }
          if (e.key.keysym.sym == SDLK_o) { float far = (rand()%2==0) ? (player.getX() - 1200.0f) : (player.getX() + 1200.0f); spawnAvocado(far, WORLD_GROUND_TOP - 8); }
          if (e.key.keysym.sym == SDLK_LEFTBRACKET)  CAM_Y_ANCHOR = std::max(CAM_ANCHOR_MIN, CAM_Y_ANCHOR - CAM_ANCHOR_STEP);
          if (e.key.keysym.sym == SDLK_RIGHTBRACKET) CAM_Y_ANCHOR = std::min(CAM_ANCHOR_MAX, CAM_Y_ANCHOR + CAM_ANCHOR_STEP);
          if (e.key.keysym.sym == SDLK_SEMICOLON)    CAM_Y_PIXELS -= CAM_PIXEL_STEP;
          if (e.key.keysym.sym == SDLK_QUOTE)        CAM_Y_PIXELS += CAM_PIXEL_STEP;
          if (e.key.keysym.sym == SDLK_g) spaceBG.setParallax(!spaceBG.parallax);
          if (e.key.keysym.sym == SDLK_9) { if (SDL_GetModState() & KMOD_SHIFT) spaceBG.resetScale(); else spaceBG.addScale(-0.05f); }
          if (e.key.keysym.sym == SDLK_0) { if (SDL_GetModState() & KMOD_SHIFT) spaceBG.resetScale(); else spaceBG.addScale(+0.05f); }

          if (e.key.keysym.sym == SDLK_e) web.reelIn  = true;
          if (e.key.keysym.sym == SDLK_q) web.reelOut = true;
          if (e.key.keysym.sym == SDLK_LSHIFT || e.key.keysym.sym == SDLK_RSHIFT) web.pump = true;

          float step = (SDL_GetModState() & KMOD_SHIFT) ? 0.03f : 0.01f;
          if (e.key.keysym.sym == SDLK_i) player.nudgeWebUV(0.f, -step);
          if (e.key.keysym.sym == SDLK_k) player.nudgeWebUV(0.f, +step);
          if (e.key.keysym.sym == SDLK_j) player.nudgeWebUV(-step, 0.f);
          if (e.key.keysym.sym == SDLK_l) player.nudgeWebUV(+step, 0.f);

          if (e.key.keysym.sym == SDLK_ESCAPE) running = false; 
        } else {
          if (e.key.keysym.sym == SDLK_r) resetGame();
          if (e.key.keysym.sym == SDLK_ESCAPE) running = false;
        }
      }
      if (e.type == SDL_KEYUP) {
        if (e.key.keysym.sym == SDLK_e) web.reelIn  = false;
        if (e.key.keysym.sym == SDLK_q) web.reelOut = false;
        if (e.key.keysym.sym == SDLK_LSHIFT || e.key.keysym.sym == SDLK_RSHIFT) web.pump = false;
      }

      if (e.type == SDL_MOUSEWHEEL) {
        if (web.state == WebState::Latched) {
          float delta = (e.wheel.y > 0 ? -1.f : +1.f) * web.reelRate * 0.02f;
          web.targetLen = clampf(web.targetLen + delta, web.minLen, web.maxLen);
        }
      }

      if (e.type == SDL_KEYDOWN || e.type == SDL_KEYUP) if (e.key.keysym.sym == SDLK_n) nHeld = (e.type == SDL_KEYDOWN);

      if (!gameOver) {
        if (e.type == SDL_MOUSEBUTTONDOWN) {
          if (e.button.button == SDL_BUTTON_LEFT) {
            int dir = (player.getFacing() == SDL_FLIP_HORIZONTAL) ? -1 : +1; spawnKnife(dir);
          }
          if (e.button.button == SDL_BUTTON_RIGHT) {
            int camX = int(std::floor(player.centerX() - SCREEN_WIDTH *0.5f));
            int camY = int(std::floor(player.centerY() - SCREEN_HEIGHT*CAM_Y_ANCHOR + CAM_Y_PIXELS));
            int mx, my; SDL_GetMouseState(&mx, &my);
            int worldX = mx + camX, worldY = my + camY;

            if (nHeld) {
              SDL_Rect r{ worldX - 64, worldY - 12, 128, 24 };
              Tile t; t.rect = r; t.texture = makePurplePlatformTex(r.w, r.h);
              t.glowColor = SDL_Color{200, 100, 255, 255}; t.isDynamic = true; dynPlatforms.push_back(t);
            } else {
              std::vector<Tile> allTiles = tiles; allTiles.insert(allTiles.end(), dynPlatforms.begin(), dynPlatforms.end());
              int hitIndex = -1; SDL_Point p{worldX, worldY};
              for (size_t i=0;i<allTiles.size();++i){ if (SDL_PointInRect(&p, &allTiles[i].rect)) { hitIndex = (int)i; break; } }
              if (hitIndex != -1) {
                web.state = WebState::Shooting;
                web.ax = (float)worldX; web.ay = (float)worldY;
                SDL_FPoint wa = player.webAnchorWorld();
                float dx = web.ax - wa.x; float dy = web.ay - wa.y;
                float d  = std::sqrt(dx*dx + dy*dy);
                web.targetLen = clampf(d, web.minLen, web.maxLen);
                web.curLen = 0.0f;
                web.anchorTile = hitIndex;
                web.startedMS = SDL_GetTicks();
                web.rmbHeld = true;
                player.setFrameLock(true, WEB_LOCK_ROW, WEB_LOCK_COL);
                if (webSnd) Mix_PlayChannel(-1, webSnd, 0);
              }
            }
          }
        }
        if (e.type == SDL_MOUSEBUTTONUP && e.button.button == SDL_BUTTON_RIGHT) {
          web.rmbHeld = false;
          player.setFrameLock(false); 
        }
      }
    }

    std::vector<Tile> allTiles = tiles; allTiles.insert(allTiles.end(), dynPlatforms.begin(), dynPlatforms.end());

    const Uint8* keys = SDL_GetKeyboardState(nullptr);
    int landedIdx = player.handleInput(keys, allTiles, !gameOver);

    if (landedIdx >= 0) {
      Uint32 now = SDL_GetTicks();
      if (landedIdx < (int)tiles.size()) tiles[landedIdx].glowUntilMS = now + 120;
      else {
        int d = landedIdx - (int)tiles.size();
        if (d >= 0 && d < (int)dynPlatforms.size()) dynPlatforms[d].glowUntilMS = now + 120;
      }
    }

    int camX = int(std::floor(player.centerX() - SCREEN_WIDTH *0.5f));
    int camY = int(std::floor(player.centerY() - SCREEN_HEIGHT*CAM_Y_ANCHOR + CAM_Y_PIXELS));

    if (!gameOver && !KNIFE_UNLIMITED) {
      Uint32 now = SDL_GetTicks();
      if (clipKnives < KNIFE_CLIP && now - lastRecharge >= KNIFE_RECHARGE_MS) { clipKnives++; lastRecharge = now; }
    }

    Uint32 nowMS = SDL_GetTicks();
    for (auto& k : knives) {
      if (!k.active) continue;
      k.x += k.vx; k.y += k.vy;
      double elapsed = (nowMS - k.bornMS) / 1000.0; k.angleDeg = k.dir * k.spinDPS * elapsed;
      if (nowMS - k.bornMS >= KNIFE_LIFETIME_MS) k.active = false;
    }

    if (web.state == WebState::Shooting) {
      web.curLen += web.shootSpeed * dt;
      if (web.curLen >= web.targetLen) {
        web.curLen = web.targetLen;
        web.state = WebState::Latched;
      }
    }

    if (web.state == WebState::Latched && web.rmbHeld) {
      SDL_FPoint wa = player.webAnchorWorld();
      float px = wa.x, py = wa.y;
      
      float vx = player.getVelX();
      float vy = player.getVelY();

      float rx = px - web.ax;
      float ry = py - web.ay;
      float dist = std::max(1.0f, std::sqrt(rx*rx + ry*ry));
      float invDist = 1.0f / dist;
      float rnx = rx * invDist;
      float rny = ry * invDist;

      if (web.reelIn)  web.targetLen -= web.reelRate * dt;
      if (web.reelOut) web.targetLen += web.reelRate * dt;
      web.targetLen = clampf(web.targetLen, web.minLen, web.maxLen);
      float L = web.targetLen;

      float vrad = vx * rnx + vy * rny;
      float txv = -rny, tyv = rnx;
      float vtan = vx * txv + vy * tyv;

      vx -= vx * web.damping * dt;
      vy -= vy * web.damping * dt;

      float nx = web.ax + rnx * L;
      float ny = web.ay + rny * L;
      player.addVelocity(nx - px, ny - py); 

      vx -= vrad * rnx;
      vy -= vrad * rny;

      if (web.pump) {
        float g = (MOON_MODE ? GRAVITY_MOON : GRAVITY_NORMAL);
        float bias = 1.0f - std::fabs(rny); 
        vtan += web.pumpGain * g * dt * (0.5f + 0.5f * bias);
      }

      static float lastL = -1.f;
      if (lastL < 0.f) lastL = L;
      if (L < lastL * 0.999f) {
        float scale = std::max(0.5f, std::min(2.0f, lastL / L));
        vtan *= scale;
      }
      lastL = L;

      vx = txv * vtan;
      vy = tyv * vtan;
      player.setVel(vx, vy);

      float g = (MOON_MODE ? GRAVITY_MOON : GRAVITY_NORMAL);
      web.lastTension = (vtan*vtan) / std::max(1.f, L) + g * (-rny);
    }

    if (!web.rmbHeld && web.state != WebState::None) {
      if (web.state == WebState::Latched) {
        SDL_FPoint wa = player.webAnchorWorld();
        float rx = wa.x - web.ax, ry = wa.y - web.ay;
        float dist = std::max(1.0f, std::sqrt(rx*rx + ry*ry));
        float txv = -ry / dist, tyv = rx / dist;
        float vmag = std::sqrt(player.getVelX()*player.getVelX() + player.getVelY()*player.getVelY());
        float boost = std::min(2.2f, 0.35f + 0.15f * (vmag / 10.0f));
        player.addVel(txv * boost, tyv * boost);
      }
      web.state = WebState::None;
      player.setFrameLock(false); 
    }

    if (!gameOver) for (auto& a : avocados) {
      if (a.split && SDL_GetTicks() >= a.splitUntilMS) { a.w = a.h = 0; continue; }
      avocadoAI(a, allTiles);
    }

    if (!gameOver) for (auto& k : knives) {
      if (!k.active) continue;
      SDL_Rect kr{ int(k.x), int(k.y), k.w, k.h };
      for (auto& a : avocados) {
        if (a.w==0 || a.h==0) continue;
        SDL_Rect ar{ int(a.x), int(a.y), a.w, a.h };
        if (SDL_HasIntersection(&kr, &ar)) {
          k.active = false; if (!a.split) {
            a.hp -= KNIFE_DMG; if (sliceSnd) Mix_PlayChannel(-1, sliceSnd, 0);
            if (a.hp <= 0) { a.split = true; a.splitUntilMS = SDL_GetTicks() + AVOCADO_SPLIT_SHOWMS; if (!a.counted) { a.counted = true; avocadosCollected++; } }
          }
          break;
        }
      }
    }

    if (!gameOver) {
      SDL_Rect pRect = player.worldRect();
      for (auto& a : avocados) {
        if (a.w==0 || a.h==0 || a.split) continue;
        SDL_Rect ar{ int(a.x), int(a.y), a.w, a.h };
        if (SDL_HasIntersection(&pRect, &ar)) {
          Uint32 now = SDL_GetTicks();
          if (now - a.lastTouchDmgMS >= AVOCADO_DMG_COOLDOWN) { playerHP = std::max(0, playerHP - AVOCADO_CONTACT_DMG); a.lastTouchDmgMS = now; }
        }
      }
    }

    knives.erase(std::remove_if(knives.begin(), knives.end(), [](const Knife& k){ return !k.active; }), knives.end());
    avocados.erase(std::remove_if(avocados.begin(), avocados.end(), [](const Avocado& a){ return a.w==0 || a.h==0; }), avocados.end());

    if (!gameOver && playerHP <= 0) { gameOver = true; if (!deathPlayed && deathSnd) { Mix_PlayChannel(-1, deathSnd, 0); deathPlayed = true; } }

    SDL_SetRenderDrawColor(ren, 0,0,0,255); SDL_RenderClear(ren);
    renderBG(camX, camY);

    auto drawPlatform = [&](const Tile& t){
      SDL_Rect r = t.rect; r.x -= camX; r.y -= camY;
      if (t.texture) SDL_RenderCopy(ren, t.texture, nullptr, &r);
      else { SDL_SetRenderDrawColor(ren, 80,80,80,255); SDL_RenderFillRect(ren, &r); }
      Uint32 now = SDL_GetTicks();
      if (now < t.glowUntilMS) { float pulse = 0.5f + 0.5f*std::sin(now * 0.02f); renderGlowRect(ren, t.rect, camX, camY, t.glowColor, pulse); }
    };

    drawPlatform(tiles[0]);
    for (size_t i = 1; i < tiles.size(); ++i) drawPlatform(tiles[i]);
    for (auto& t : dynPlatforms) drawPlatform(t);

    for (auto& a : avocados) {
      if (a.w==0 || a.h==0) continue;
      SDL_Rect dst{ int(a.x - camX), int(a.y - camY), a.w, a.h };
      SDL_Texture* tex = a.split ? texAvoSplit : texAvo;
      if (tex) SDL_RenderCopy(ren, tex, nullptr, &dst);
    }

    for (auto& k : knives) {
      if (!k.active) continue;
      SDL_Rect dst{ int(k.x - camX), int(k.y - camY), k.w, k.h };
      if (texKnife) { SDL_Point center{ dst.w/2, dst.h/2 }; SDL_RenderCopyEx(ren, texKnife, nullptr, &dst, k.angleDeg, &center, SDL_FLIP_NONE); }
    }

    if (web.state != WebState::None) {
      SDL_FPoint wa = player.webAnchorWorld();
      float dx = web.ax - wa.x, dy = web.ay - wa.y;
      float dist = std::sqrt(dx*dx+dy*dy);
      float shownLen = (web.state==WebState::Shooting ? web.curLen : web.targetLen);
      float s = (dist > 1.0f) ? (clampf(shownLen, 0.f, dist) / dist) : 0.f;
      int hx = int(wa.x + dx*s) - camX;
      int hy = int(wa.y + dy*s) - camY;
      int px = int(wa.x) - camX, py = int(wa.y) - camY;
      drawWigglyWeb(ren, px, py, hx, hy, SDL_GetTicks()/1000.0f);
    }

    player.render(ren, camX, camY, debugBoxes, gameOver);

    if (showHUD) {
      int barW = 320, barH = 18;
      SDL_Rect hb{ 20, 20, barW, barH };
      SDL_SetRenderDrawColor(ren, 60,60,60,220); SDL_RenderFillRect(ren, &hb);
      int filled = int(barW * (std::max(0, playerHP) / 100.0f));
      SDL_Rect hf{ 20, 20, std::max(0, filled), barH };
      SDL_SetRenderDrawColor(ren, 50,230,90,255); SDL_RenderFillRect(ren, &hf);
      SDL_SetRenderDrawColor(ren, 255,255,255,255); SDL_RenderDrawRect(ren, &hb);
      if (font) {
        std::stringstream htx; htx << "HP: " << std::max(0, playerHP) << "%"; renderText(ren, font, htx.str(), 24, 42);
        int ax = 20, ay = 70;
        std::stringstream atx; atx << "Knives: " << (KNIFE_UNLIMITED ? std::string("∞") : (std::to_string(clipKnives) + " / " + std::to_string(KNIFE_CLIP)));
        renderText(ren, font, atx.str(), ax, ay);
        if (!KNIFE_UNLIMITED && clipKnives < KNIFE_CLIP) {
          Uint32 now = SDL_GetTicks(); float p = float(now - lastRecharge) / float(KNIFE_RECHARGE_MS); p = std::max(0.f, std::min(1.f, p));
          int rw = 180, rh = 10; SDL_Rect rb{ ax, ay+22, rw, rh }; SDL_SetRenderDrawColor(ren, 60,60,60,200); SDL_RenderFillRect(ren, &rb);
          SDL_Rect rf{ ax, ay+22, int(rw*p), rh }; SDL_SetRenderDrawColor(ren, 255,200,80,255); SDL_RenderFillRect(ren, &rf);
          SDL_SetRenderDrawColor(ren, 255,255,255,255); SDL_RenderDrawRect(ren, &rb); renderText(ren, font, "recharge", ax+rw+8, ay+16);
        }

        std::string wstate = (web.state==WebState::None ? "None" : (web.state==WebState::Shooting ? "Shooting" : "Latched"));
        std::stringstream wtx; wtx << "Web: " << wstate;
        renderText(ren, font, wtx.str(), 20, ay + 44);
        if (web.state == WebState::Latched) {
          std::stringstream vis;
          vis << "Len " << int(web.targetLen) << "  Tension " << std::fixed << std::setprecision(1) << web.lastTension;
          renderText(ren, font, vis.str(), 20, ay + 64);
          renderText(ren, font, "[E/Q] reel  [Shift] pump  [Wheel] fine-reel", 20, ay + 84);
        }

        std::ostringstream uv; uv << "WebAnchor u=" << std::fixed << std::setprecision(2) << player.getWebU()
                                  << " v=" << std::fixed << std::setprecision(2) << player.getWebV()
                                  << "  (J/L, I/K)";
        renderText(ren, font, uv.str(), 20, ay + 108);
      }
    }

    if (gameOver) {
      SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_BLEND);
      SDL_SetRenderDrawColor(ren, 0,0,0,160);
      SDL_Rect full{0,0,SCREEN_WIDTH,SCREEN_HEIGHT}; SDL_RenderFillRect(ren, &full);
      if (fontBig) renderText(ren, fontBig, "Knocked DF Out", SCREEN_WIDTH/2 - 120, SCREEN_HEIGHT/2 - 120, {255,80,80,255});
      if (font) renderText(ren, font, "Press [R] to Restart   |   [Esc] to Exit", SCREEN_WIDTH/2 - 220, SCREEN_HEIGHT/2, {255,255,255,255});
    }

    SDL_RenderPresent(ren);
  }

  for (auto& t : tiles) if (t.texture) SDL_DestroyTexture(t.texture);
  for (auto& t : dynPlatforms) if (t.texture) SDL_DestroyTexture(t.texture);
  if (texKnife) SDL_DestroyTexture(texKnife);
  if (texAvo) SDL_DestroyTexture(texAvo);
  if (texAvoSplit) SDL_DestroyTexture(texAvoSplit);
  if (font) TTF_CloseFont(font);
  if (fontBig) TTF_CloseFont(fontBig);
  if (sliceSnd) Mix_FreeChunk(sliceSnd);
  if (deathSnd) Mix_FreeChunk(deathSnd);
  if (throwSnd) Mix_FreeChunk(throwSnd);
  if (webSnd) Mix_FreeChunk(webSnd);
  Mix_CloseAudio(); Mix_Quit();
  SDL_DestroyRenderer(ren); SDL_DestroyWindow(win);
  IMG_Quit(); TTF_Quit(); SDL_Quit();
  return 0;
}

