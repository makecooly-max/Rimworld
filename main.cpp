#include <SDL2/SDL.h>
#include <SDL2/SDL_events.h>
#include <SDL2/SDL_image.h>
#include <SDL2/SDL_keyboard.h>
#include <SDL2/SDL_mouse.h>
#include <SDL2/SDL_render.h>
#include <SDL2/SDL_surface.h>
#include <SDL2/SDL_ttf.h>
#include <SDL2/SDL_video.h>
#include <cstdlib>
#include <ctime>
#include <vector>
#include <string>
#include <cmath>
#include <algorithm>
using namespace std;

struct Tree { int x,y; bool alive = true; int type; };
struct Food { int x,y; bool alive = true; int nutrition = 20; };
struct Chicken { int x,y,dx,dy; bool alive = true; bool red=false; int hp=3; int attackTimer=0; };
struct Cell { int x,y; };
struct Particle { float x,y,vx,vy; int life; Uint8 r,g,b,a; };
struct Fox { int x,y; float vx,vy; bool alive=true; int hp=12; };

static inline int sgn(int v){ return (v>0)-(v<0); }

static vector<Cell> lineCells(Cell a, Cell b){
    vector<Cell> out;
    int x0=a.x,y0=a.y,x1=b.x,y1=b.y;
    int dx=abs(x1-x0), sx=x0<x1?1:-1;
    int dy=-abs(y1-y0), sy=y0<y1?1:-1;
    int err=dx+dy;
    for(;;){
        out.push_back({x0,y0});
        if(x0==x1 && y0==y1) break;
        int e2=2*err;
        if(e2>=dy){ err+=dy; x0+=sx; }
        if(e2<=dx){ err+=dx; y0+=sy; }
    }
    return out;
}

static bool hasCell(const vector<Cell>& v, int x, int y){
    for(auto& c:v) if(c.x==x && c.y==y) return true;
    return false;
}

int main(int argc, char* argv[]) {
    srand(time(0));
    SDL_Init(SDL_INIT_VIDEO);
    IMG_Init(IMG_INIT_PNG);
    TTF_Init();
    SDL_Window* window = SDL_CreateWindow("Survival Craft", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 1024, 640, SDL_WINDOW_SHOWN);
    SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    SDL_Event event;

    SDL_Rect player = { 5000, 5000, 64, 64 };
    SDL_FPoint camf = { float(player.x + player.w/2 - 512), float(player.y + player.h/2 - 320) };
    SDL_Rect Camera = { int(camf.x), int(camf.y), 1024, 640 };

    SDL_Rect INV = { (1024 - 460) / 2, 580, 460, 50 };
    SDL_Rect Place1 = { INV.x, INV.y, 50, 50 };
    SDL_Rect Place2 = { INV.x + 55, INV.y, 50, 50 };
    SDL_Rect Place3 = { INV.x + 110, INV.y, 50, 50 };
    SDL_Rect Place4 = { INV.x + 165, INV.y, 50, 50 };

    SDL_Rect Tree1InInv = { Place1.x + 5, Place1.y + 5, 30, 30 };
    SDL_Rect Tree2InInv = { Place2.x + 5, Place2.y + 5, 30, 30 };
    SDL_Rect Tree3InInv = { Place3.x + 5, Place3.y + 5, 30, 30 };
    SDL_Rect WoodInInv  = { Place4.x + 5, Place4.y + 5, 30, 30 };

    SDL_Surface* PlayerSurface = IMG_Load("images/player.png");
    SDL_Surface* Tree1Surface = IMG_Load("images/tree.png");
    SDL_Surface* Tree2Surface = IMG_Load("images/tree2.png");
    SDL_Surface* Tree3Surface = IMG_Load("images/tree3.png");
    SDL_Surface* FoodSurface   = IMG_Load("images/food.png");
    SDL_Surface* ChickenSurface = IMG_Load("images/chicken.png");
    SDL_Surface* WoodSurface = IMG_Load("images/wood.png");

    SDL_Texture* PlayerTex = SDL_CreateTextureFromSurface(renderer, PlayerSurface);
    SDL_Texture* Tree1Tex  = SDL_CreateTextureFromSurface(renderer, Tree1Surface);
    SDL_Texture* Tree2Tex  = SDL_CreateTextureFromSurface(renderer, Tree2Surface);
    SDL_Texture* Tree3Tex  = SDL_CreateTextureFromSurface(renderer, Tree3Surface);
    SDL_Texture* FoodTex   = SDL_CreateTextureFromSurface(renderer, FoodSurface);
    SDL_Texture* ChickenTex = SDL_CreateTextureFromSurface(renderer, ChickenSurface);
    SDL_Texture* WoodTex = SDL_CreateTextureFromSurface(renderer, WoodSurface);

    vector<Tree> Trees;
    vector<Food> Foods;
    vector<Chicken> Chickens;
    vector<Cell> Walls;
    vector<Particle> Particles;
    vector<Fox> Foxes;

    for (int i=0;i<350;i++){ Tree t{rand()%10000,rand()%10000,true,1}; Trees.push_back(t); }
    for (int i=0;i<350;i++){ Tree t{rand()%10000,rand()%10000,true,2}; Trees.push_back(t); }
    for (int i=0;i<350;i++){ Tree t{rand()%10000,rand()%10000,true,3}; Trees.push_back(t); }
    for (int i=0;i<220;i++){ Food f{rand()%10000,rand()%10000,true,20}; Foods.push_back(f); }
    for (int i=0;i<60;i++){ Chicken c; c.x=rand()%10000; c.y=rand()%10000; c.dx=(rand()%3)-1; c.dy=(rand()%3)-1; c.red = (rand()%100)<15; c.hp = c.red?8:4; Chickens.push_back(c); }

    const Uint8* state = SDL_GetKeyboardState(NULL);
    bool running = true;
    bool InvPlace1Active=false,InvPlace2Active=false,InvPlace3Active=false,InvPlace4Active=false;

    int Tree1Count=0, Tree2Count=0, Tree3Count=0;
    int WoodCount=0;

    int health = 100;
    int hunger = 100;
    int maxHealth = 100;
    Uint32 lastHungerTick = SDL_GetTicks();

    TTF_Font* font = TTF_OpenFont("fonts/font.ttf", 18);
    SDL_Color textColor = {255,255,255,255};
    SDL_Color dimText = {200,200,200,255};
    bool inventoryOpen=false;
    bool craftPressed=false;

    auto drawText=[&](const string& txt,int x,int y,SDL_Color col){
        if(!font) return;
        SDL_Surface* s=TTF_RenderUTF8_Blended(font,txt.c_str(),col);
        if(!s) return;
        SDL_Texture* t=SDL_CreateTextureFromSurface(renderer,s);
        int w=0,h=0; SDL_QueryTexture(t,NULL,NULL,&w,&h);
        SDL_Rect dst={x,y,w,h};
        SDL_RenderCopy(renderer,t,NULL,&dst);
        SDL_DestroyTexture(t);
        SDL_FreeSurface(s);
    };

    auto drawPanel=[&](SDL_Rect r, Uint8 aBack, Uint8 aEdge){
        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(renderer,20,20,25,aBack); SDL_RenderFillRect(renderer,&r);
        SDL_SetRenderDrawColor(renderer,255,255,255,aEdge);
        SDL_RenderDrawRect(renderer,&r);
        SDL_Rect shadow={r.x+6,r.y+6,r.w,r.h};
        SDL_SetRenderDrawColor(renderer,0,0,0,60); SDL_RenderFillRect(renderer,&shadow);
        SDL_SetRenderDrawColor(renderer,255,255,255,25);
        SDL_Rect topGlow={r.x,r.y,r.w,6}; SDL_RenderFillRect(renderer,&topGlow);
    };

    SDL_Rect invBackdrop={0,0,1024,640};
    SDL_Rect invPanel={100,40,824,540};
    SDL_Rect gridArea={invPanel.x+24,invPanel.y+70, 420, 360};
    SDL_Rect craftPanel={invPanel.x+470, invPanel.y+70, 310, 360};
    SDL_Rect craftBtn={craftPanel.x+18, craftPanel.y+270, craftPanel.w-36, 56};

    const int cellSize = 50;
    bool buildDragging=false;
    Cell buildStart{0,0};
    vector<Cell> buildPreview;

    auto worldToCell=[&](int wx,int wy)->Cell{
        int cx = wx/cellSize;
        int cy = wy/cellSize;
        return {cx,cy};
    };
    auto cellToRect=[&](Cell c)->SDL_Rect{
        return SDL_Rect{ c.x*cellSize - Camera.x, c.y*cellSize - Camera.y, cellSize, cellSize };
    };

    int woodSpentTotal=0;
    int wallsBuiltTotal=0;
    int foodsEatenTotal=0;
    int daysSurvived=0;
    int score=0;
    int chickensKilled=0;
    int foxesKilled=0;

    float timeOfDay=0.f;
    const float secondsPerDay=180.f;
    Uint32 startTicks=SDL_GetTicks();

    bool campfirePlaced=false;
    SDL_Point campfirePos{0,0};
    int campfireFuel=0;
    bool hasShelter=false;

    auto rectsIntersect=[&](const SDL_Rect& a, const SDL_Rect& b){
        return (a.x < b.x + b.w && a.x + a.w > b.x && a.y < b.y + b.h && a.y + a.h > b.y);
    };

    auto collideWithWalls=[&](SDL_Rect& box){
        SDL_Rect r=box;
        for(auto& w:Walls){
            SDL_Rect wr={w.x*cellSize, w.y*cellSize, cellSize, cellSize};
            if(rectsIntersect(r,wr)){
                int dx1 = (wr.x + wr.w) - r.x;
                int dx2 = (r.x + r.w) - wr.x;
                int dy1 = (wr.y + wr.h) - r.y;
                int dy2 = (r.y + r.h) - wr.y;
                int minx = dx1<dx2?dx1:-dx2;
                int miny = dy1<dy2?dy1:-dy2;
                if(abs(minx) < abs(miny)) r.x += minx;
                else r.y += miny;
            }
        }
        box=r;
    };

    auto addBurst=[&](int x,int y,Uint8 r,Uint8 g,Uint8 b, int count=16){
        for(int i=0;i<count;i++){
            float ang = float(rand()%360)*3.14159f/180.f;
            float spd = 1.f + (rand()%100)/60.f;
            Particle p; p.x=x; p.y=y; p.vx=cosf(ang)*spd; p.vy=sinf(ang)*spd; p.life=30+rand()%20; p.r=r;p.g=g;p.b=b;p.a=220;
            Particles.push_back(p);
        }
    };

    Uint32 lastAttackTime = 0;
    const Uint32 attackCooldown = 220;

    while(running) {
        int mouseX=0, mouseY=0;
        Uint32 mstate = SDL_GetMouseState(&mouseX,&mouseY);

        while(SDL_PollEvent(&event)) {
            if(event.type==SDL_QUIT) running=false;

            if(event.type==SDL_KEYDOWN && event.key.keysym.scancode==SDL_SCANCODE_E) inventoryOpen=!inventoryOpen;
            if(event.type==SDL_KEYDOWN && event.key.keysym.scancode==SDL_SCANCODE_ESCAPE) running=false;

            if(event.type==SDL_MOUSEBUTTONDOWN && event.button.button==SDL_BUTTON_LEFT) {
                if(inventoryOpen){
                    if(event.button.x>=craftBtn.x && event.button.x<=craftBtn.x+craftBtn.w && event.button.y>=craftBtn.y && event.button.y<=craftBtn.y+craftBtn.h){
                        craftPressed=true;
                        int totalTrees = Tree1Count+Tree2Count+Tree3Count;
                        if(totalTrees>0){
                            if(Tree1Count>0) Tree1Count--;
                            else if(Tree2Count>0) Tree2Count--;
                            else if(Tree3Count>0) Tree3Count--;
                            WoodCount++;
                            score+=2;
                        }
                    }
                } else {
                    if(event.button.x>=Place1.x&&event.button.x<=Place1.x+Place1.w&&event.button.y>=Place1.y&&event.button.y<=Place1.y+Place1.h){InvPlace1Active=true;InvPlace2Active=InvPlace3Active=InvPlace4Active=false;}
                    else if(event.button.x>=Place2.x&&event.button.x<=Place2.x+Place2.w&&event.button.y>=Place2.y&&event.button.y<=Place2.y+Place2.h){InvPlace2Active=true;InvPlace1Active=InvPlace3Active=InvPlace4Active=false;}
                    else if(event.button.x>=Place3.x&&event.button.x<=Place3.x+Place3.w&&event.button.y>=Place3.y&&event.button.y<=Place3.y+Place3.h){InvPlace3Active=true;InvPlace1Active=InvPlace2Active=InvPlace4Active=false;}
                    else if(event.button.x>=Place4.x&&event.button.x<=Place4.x+Place4.w&&event.button.y>=Place4.y&&event.button.y<=Place4.y+Place4.h){InvPlace4Active=true;InvPlace1Active=InvPlace2Active=InvPlace3Active=false;}
                    else {
                        int worldX=event.button.x+Camera.x, worldY=event.button.y+Camera.y;
                        bool attacked=false;
                        Uint32 nowClick = SDL_GetTicks();
                        if(nowClick - lastAttackTime >= attackCooldown){
                            for(auto& ch:Chickens){
                                if(!ch.alive) continue;
                                SDL_Rect cr={ch.x-30,ch.y-30,120,120};
                                if(worldX>=cr.x && worldX<=cr.x+cr.w && worldY>=cr.y && worldY<=cr.y+cr.h){
                                    ch.hp -= 2;
                                    lastAttackTime = nowClick;
                                    attacked = true;
                                    addBurst(ch.x+30,ch.y+30,255,60,60,8);
                                    if(ch.hp<=0){
                                        ch.alive=false;
                                        chickensKilled++;
                                        Food nf{ch.x,ch.y,true,30};
                                        Foods.push_back(nf);
                                        score+=10;
                                        addBurst(ch.x+30,ch.y+30,255,200,80,12);
                                    }
                                    break;
                                }
                            }
                            for(auto& f:Foxes){
                                if(!f.alive) continue;
                                SDL_Rect fr={f.x-28,f.y-28,112,112};
                                if(worldX>=fr.x && worldX<=fr.x+fr.w && worldY>=fr.y && worldY<=fr.y+fr.h){
                                    f.hp -= 3;
                                    lastAttackTime = nowClick;
                                    attacked = true;
                                    addBurst(f.x+28,f.y+28,255,60,60,10);
                                    if(f.hp<=0){
                                        f.alive=false;
                                        foxesKilled++;
                                        Food nf{f.x,f.y,true,40};
                                        Foods.push_back(nf);
                                        score+=20;
                                        addBurst(f.x+28,f.y+28,255,100,100,15);
                                    }
                                    break;
                                }
                            }
                        }
                        if(attacked) continue;
                        if(InvPlace4Active){
                            if(WoodCount>=5 && !campfirePlaced){
                                campfirePlaced=true;
                                campfirePos={worldX,worldY};
                                WoodCount-=5;
                                woodSpentTotal+=5;
                                score+=15;
                                campfireFuel=1200;
                                addBurst(worldX,worldY,255,200,80,20);
                            }else if(WoodCount>=3){
                                buildDragging=true;
                                buildPreview.clear();
                                buildStart = worldToCell(worldX,worldY);
                                buildPreview.push_back(buildStart);
                            }
                        } else {
                            if(InvPlace1Active && Tree1Count>0){ Trees.push_back({worldX-50,worldY-50,true,1}); Tree1Count--; score+=1; }
                            else if(InvPlace2Active && Tree2Count>0){ Trees.push_back({worldX-50,worldY-50,true,2}); Tree2Count--; score+=1; }
                            else if(InvPlace3Active && Tree3Count>0){ Trees.push_back({worldX-50,worldY-50,true,3}); Tree3Count--; score+=1; }
                            else {
                                bool chopped=false;
                                for(auto& t:Trees){
                                    if(!t.alive) continue;
                                    SDL_Rect r={t.x,t.y,100,100};
                                    if(worldX>=r.x&&worldX<=r.x+r.w&&worldY>=r.y&&worldY<=r.y+r.h){
                                        t.alive=false;
                                        if(t.type==1) Tree1Count++;
                                        else if(t.type==2) Tree2Count++;
                                        else if(t.type==3) Tree3Count++;
                                        chopped=true;
                                        addBurst(worldX,worldY,120,200,120,8);
                                        score+=2;
                                        break;
                                    }
                                }
                            }
                        }
                    }
                }
            }
            if(event.type==SDL_MOUSEMOTION){
                if(buildDragging && !inventoryOpen){
                    int wx=event.motion.x+Camera.x, wy=event.motion.y+Camera.y;
                    Cell cur = worldToCell(wx,wy);
                    buildPreview = lineCells(buildStart, cur);
                }
            }
            if(event.type==SDL_MOUSEBUTTONUP && event.button.button==SDL_BUTTON_LEFT) {
                craftPressed=false;
                if(buildDragging && !inventoryOpen){
                    for(auto& c:buildPreview){
                        if(WoodCount<=0) break;
                        if(!hasCell(Walls,c.x,c.y)){
                            Walls.push_back(c);
                            WoodCount--;
                            woodSpentTotal++;
                            wallsBuiltTotal++;
                            score+=3;
                        }
                    }
                    buildPreview.clear();
                    buildDragging=false;
                }
            }
            if(event.type==SDL_MOUSEBUTTONDOWN && event.button.button==SDL_BUTTON_RIGHT) {
                if(!inventoryOpen){
                    if(event.button.x>=Place1.x&&event.button.x<=Place1.x+Place1.w&&event.button.y>=Place1.y&&event.button.y<=Place1.y+Place1.h)InvPlace1Active=false;
                    if(event.button.x>=Place2.x&&event.button.x<=Place2.x+Place2.w&&event.button.y>=Place2.y&&event.button.y<=Place2.y+Place2.h)InvPlace2Active=false;
                    if(event.button.x>=Place3.x&&event.button.x<=Place3.x+Place3.w&&event.button.y>=Place3.y&&event.button.y<=Place3.y+Place3.h)InvPlace3Active=false;
                    if(event.button.x>=Place4.x&&event.button.x<=Place4.x+Place4.w&&event.button.y>=Place4.y&&event.button.y<=Place4.y+Place4.h)InvPlace4Active=false;
                    buildDragging=false; buildPreview.clear();
                }
            }
        }

        float speed = (inventoryOpen?0.f: (state[SDL_SCANCODE_LSHIFT]||state[SDL_SCANCODE_RSHIFT]) ? 6.5f : 4.2f);
        if(!inventoryOpen){
            if(state[SDL_SCANCODE_D]) player.x+=int(speed);
            if(state[SDL_SCANCODE_A]) player.x-=int(speed);
            if(state[SDL_SCANCODE_W]) player.y-=int(speed);
            if(state[SDL_SCANCODE_S]) player.y+=int(speed);
        }

        SDL_Rect playerBox={player.x,player.y,player.w,player.h};
        collideWithWalls(playerBox);
        player.x=playerBox.x; player.y=playerBox.y;

        float targetCamX = float(player.x + player.w/2 - Camera.w/2);
        float targetCamY = float(player.y + player.h/2 - Camera.h/2);
        camf.x += (targetCamX - camf.x)*0.15f;
        camf.y += (targetCamY - camf.y)*0.15f;
        Camera.x=int(camf.x);
        Camera.y=int(camf.y);

        if(Camera.x < 0) Camera.x = 0;
        if(Camera.y < 0) Camera.y = 0;
        if(Camera.x + Camera.w > 10000) Camera.x = 10000 - Camera.w;
        if(Camera.y + Camera.h > 10000) Camera.y = 10000 - Camera.h;
        camf.x = Camera.x; camf.y = Camera.y;

        Uint32 now = SDL_GetTicks();
        float elapsedSec = (now - startTicks)/1000.f;
        timeOfDay = fmodf(elapsedSec/secondsPerDay,1.f);
        static int lastDayMark=-1;
        int curDay=int(elapsedSec/secondsPerDay);
        if(curDay!=lastDayMark){
            if(lastDayMark!=-1){
                daysSurvived++;
                score+=30;
                if(daysSurvived % 3 == 0) maxHealth = min(150, maxHealth + 5);
            }
            lastDayMark=curDay;
        }

        if(SDL_GetTicks()-lastHungerTick> (state[SDL_SCANCODE_LSHIFT]||state[SDL_SCANCODE_RSHIFT] ? 1500 : 2200)){
            hunger--; if(hunger<0) hunger=0; lastHungerTick=SDL_GetTicks();
        }
        if(hunger<=0){ health--; if(health<0) health=0; }
        if(campfirePlaced && campfireFuel>0){
            int dx=player.x-campfirePos.x, dy=player.y-campfirePos.y;
            if(dx*dx+dy*dy<250*250){
                if(hunger<100 && (now%400)<5) hunger++;
                if((now%800)<5) health=min(maxHealth,health+1);
                campfireFuel--;
            }
        }

        hasShelter = false;
        for(auto& w:Walls){
            int dx=player.x-w.x*cellSize, dy=player.y-w.y*cellSize;
            if(dx*dx+dy*dy<300*300){
                hasShelter = true;
                break;
            }
        }

        if(timeOfDay>0.7f && !hasShelter && (now%1000)<10){
            health = max(0, health-1);
        }

        for(auto& f:Foods){
            if(!f.alive) continue;
            SDL_Rect r={f.x-25,f.y-25,100,100};
            if(rectsIntersect(playerBox,r)){
                f.alive=false;
                hunger = min(100, hunger + f.nutrition);
                foodsEatenTotal++;
                score+=8;
                addBurst(f.x,f.y,255,255,120,10);
            }
        }

        for(auto& c:Chickens){
            if(!c.alive) continue;
            if(c.attackTimer>0) c.attackTimer--;
            if(c.red){
                int dxp = player.x - c.x;
                int dyp = player.y - c.y;
                int distSq = dxp*dxp + dyp*dyp;
                if(distSq < 600*600){
                    c.dx = sgn(dxp);
                    c.dy = sgn(dyp);
                    c.x += c.dx * (2 + (rand()%2));
                    c.y += c.dy * (2 + (rand()%2));
                } else {
                    if(rand()%100<3){ c.dx=(rand()%3)-1; c.dy=(rand()%3)-1; }
                    c.x+=c.dx; c.y+=c.dy;
                }
            } else {
                int seeRadius=300;
                int bestd=1e9, bx=-1, by=-1;
                for(auto& f:Foods){
                    if(!f.alive) continue;
                    int dx=f.x-c.x, dy=f.y-c.y;
                    int d=dx*dx+dy*dy;
                    if(d<seeRadius*seeRadius && d<bestd){ bestd=d; bx=f.x; by=f.y; }
                }
                if(bx!=-1){ c.dx = sgn(bx - c.x); c.dy = sgn(by - c.y); }
                else if(rand()%100<3){ c.dx=(rand()%3)-1; c.dy=(rand()%3)-1; }
                c.x+=c.dx; c.y+=c.dy;
                if(rand()%1200==0 && Chickens.size() < 100){
                    Chicken nc; nc.x=c.x+(rand()%61)-30; nc.y=c.y+(rand()%61)-30; nc.dx=(rand()%3)-1; nc.dy=(rand()%3)-1; nc.red=(rand()%100)<8; nc.hp=nc.red?10:5;
                    Chickens.push_back(nc);
                }
            }
            if(rand()%800==0){
                Food nf{c.x+(rand()%51)-25,c.y+(rand()%51)-25,true,15};
                Foods.push_back(nf);
            }
            if(c.x<0) c.x=0; if(c.y<0) c.y=0;
            if(c.x>10000) c.x=10000; if(c.y>10000) c.y=10000;
            SDL_Rect cr={c.x-30,c.y-30,120,120};
            collideWithWalls(cr); c.x=cr.x+30; c.y=cr.y+30;
            SDL_Rect pb={player.x,player.y,player.w,player.h};
            if(rectsIntersect(cr,pb)){
                if(c.red){
                    if(c.attackTimer==0){
                        health = max(0, health - 8);
                        c.attackTimer = 40 + rand()%30;
                        addBurst(c.x,c.y,200,80,80,6);
                    }
                } else {
                    if(rand()%300==0){
                        health = max(0, health - 2);
                    }
                }
            }
        }

        if(timeOfDay>0.65f && Foxes.size()<12 && rand()%50==0){
            Fox fx; fx.x=Camera.x + (rand()%2? -200: Camera.w+200); fx.y=Camera.y+rand()%Camera.h; fx.vx= (fx.x<player.x? 2.0f: -2.0f); fx.vy=0.f; Foxes.push_back(fx);
        }
        for(auto& f:Foxes){
            if(!f.alive) continue;
            float tx=float(player.x), ty=float(player.y);
            float dx=tx-f.x, dy=ty-f.y;
            float d=sqrtf(dx*dx+dy*dy)+0.0001f;
            f.vx += (dx/d)*0.05f; f.vy += (dy/d)*0.05f;
            float spd=sqrtf(f.vx*f.vx+f.vy*f.vy);
            if(spd>4.0f){ f.vx*=4.0f/spd; f.vy*=4.0f/spd; }
            f.x+=int(f.vx); f.y+=int(f.vy);
            SDL_Rect fr={f.x-28,f.y-28,112,112};
            collideWithWalls(fr); f.x=fr.x+28; f.y=fr.y+28;
            SDL_Rect pb={player.x,player.y,player.w,player.h};
            if(rectsIntersect(fr,pb)){
                if((now%20)==0) health=max(0,health-3);
            }
            for(auto& ch:Chickens){
                if(!ch.alive) continue;
                SDL_Rect cr={ch.x-30,ch.y-30,120,120};
                if(rectsIntersect(fr,cr)){
                    ch.alive=false;
                    Food nf{ch.x,ch.y,true,25};
                    Foods.push_back(nf);
                    score+=5;
                }
            }
        }

        for(auto& p:Particles){
            p.x+=p.vx; p.y+=p.vy; p.vy+=0.03f;
            if(p.life>0) p.life--; if(p.a>3) p.a-=3;
        }
        Particles.erase(remove_if(Particles.begin(),Particles.end(),[](const Particle& p){return p.life<=0 || p.a<=3;}),Particles.end());

        int bgR = int(60 + 40*sin(timeOfDay*6.28318f));
        int bgG = int(90 + 60*sin(timeOfDay*6.28318f));
        int bgB = int(40 + 20*sin(timeOfDay*6.28318f));
        SDL_SetRenderDrawColor(renderer,bgR,bgG,bgB,255);
        SDL_RenderClear(renderer);

        SDL_SetRenderDrawColor(renderer,70,120,70,60);
        for(int gx=((Camera.x/cellSize)-1)*cellSize; gx<Camera.x+Camera.w+cellSize; gx+=cellSize){
            SDL_RenderDrawLine(renderer, gx-Camera.x, 0, gx-Camera.x, Camera.h);
        }
        for(int gy=((Camera.y/cellSize)-1)*cellSize; gy<Camera.y+Camera.h+cellSize; gy+=cellSize){
            SDL_RenderDrawLine(renderer, 0, gy-Camera.y, Camera.w, gy-Camera.y);
        }

        for(auto& t:Trees){
            if(!t.alive)continue;
            SDL_Rect r={t.x-Camera.x,t.y-Camera.y,100,100};
            if(t.type==1) SDL_RenderCopy(renderer,Tree1Tex,NULL,&r);
            else if(t.type==2) SDL_RenderCopy(renderer,Tree2Tex,NULL,&r);
            else if(t.type==3) SDL_RenderCopy(renderer,Tree3Tex,NULL,&r);
        }
        for(auto& f:Foods){ if(!f.alive)continue; SDL_Rect r={f.x-25-Camera.x,f.y-25-Camera.y,50,50}; SDL_RenderCopy(renderer,FoodTex,NULL,&r); }
        for(auto& c:Chickens){ if(!c.alive)continue; SDL_Rect r={c.x-30-Camera.x,c.y-30-Camera.y,60,60}; if(c.red) SDL_SetTextureColorMod(ChickenTex,255,100,100); else SDL_SetTextureColorMod(ChickenTex,255,255,255); SDL_RenderCopy(renderer,ChickenTex,NULL,&r); }
        SDL_SetTextureColorMod(ChickenTex,255,255,255);

        for(auto& w:Walls){
            SDL_Rect r = cellToRect(w);
            if(WoodTex) SDL_RenderCopy(renderer,WoodTex,NULL,&r);
            else { SDL_SetRenderDrawColor(renderer,160,110,60,255); SDL_RenderFillRect(renderer,&r); }
            SDL_SetRenderDrawColor(renderer,0,0,0,120); SDL_RenderDrawRect(renderer,&r);
        }

        if(buildDragging && !inventoryOpen){
            SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
            for(auto& c:buildPreview){
                SDL_Rect r = cellToRect(c);
                SDL_SetRenderDrawColor(renderer,200,200,255,90);
                SDL_RenderFillRect(renderer,&r);
                SDL_SetRenderDrawColor(renderer,255,255,255,140);
                SDL_RenderDrawRect(renderer,&r);
            }
        }

        if(campfirePlaced){
            SDL_Rect r={campfirePos.x-Camera.x-24,campfirePos.y-Camera.y-24,48,48};
            SDL_SetTextureColorMod(WoodTex,255,180,80);
            SDL_RenderCopy(renderer,WoodTex,NULL,&r);
            SDL_SetTextureColorMod(WoodTex,255,255,255);
            if(campfireFuel>0){
                SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_ADD);
                int rad=140;
                SDL_SetRenderDrawColor(renderer,255,180,80,40);
                SDL_Rect glow={r.x+24-rad, r.y+24-rad, rad*2, rad*2};
                SDL_RenderFillRect(renderer,&glow);
                SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
            }
        }

        SDL_Rect playerScreen={player.x-Camera.x,player.y-Camera.y,player.w,player.h};
        SDL_RenderCopy(renderer,PlayerTex,NULL,&playerScreen);

        SDL_SetTextureColorMod(ChickenTex,255,80,80);
        for(auto& f:Foxes){
            if(!f.alive) continue;
            SDL_Rect r={f.x-28-Camera.x,f.y-28-Camera.y,56,56};
            SDL_RenderCopy(renderer,ChickenTex,NULL,&r);
        }
        SDL_SetTextureColorMod(ChickenTex,255,255,255);

        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
        for(auto& p:Particles){
            SDL_SetRenderDrawColor(renderer,p.r,p.g,p.b,p.a);
            SDL_Rect pr={int(p.x)-Camera.x,int(p.y)-Camera.y,4,4};
            SDL_RenderFillRect(renderer,&pr);
        }

        SDL_SetRenderDrawColor(renderer,0,0,0,255);
        SDL_RenderFillRect(renderer,&INV);
        auto drawSlot=[&](SDL_Rect place,bool active){ SDL_SetRenderDrawColor(renderer,active?255:144,active?0:144,active?0:144,255); SDL_RenderFillRect(renderer,&place); };
        drawSlot(Place1,InvPlace1Active); drawSlot(Place2,InvPlace2Active); drawSlot(Place3,InvPlace3Active); drawSlot(Place4,InvPlace4Active);

        if(Tree1Count>0) SDL_RenderCopy(renderer,Tree1Tex,NULL,&Tree1InInv);
        if(Tree2Count>0) SDL_RenderCopy(renderer,Tree2Tex,NULL,&Tree2InInv);
        if(Tree3Count>0) SDL_RenderCopy(renderer,Tree3Tex,NULL,&Tree3InInv);
        if(WoodCount>0) SDL_RenderCopy(renderer,WoodTex,NULL,&WoodInInv);

        auto drawCountSmall=[&](int count,SDL_Rect place){
            if(font){ char buf[16]; snprintf(buf,sizeof(buf),"%d",count);
                SDL_Surface* s=TTF_RenderText_Solid(font,buf,textColor);
                if(s){ SDL_Texture* t=SDL_CreateTextureFromSurface(renderer,s);
                    if(t){ int w,h; SDL_QueryTexture(t,NULL,NULL,&w,&h);
                        SDL_Rect dst={place.x+place.w-w-4,place.y+place.h-h-2,w,h};
                        SDL_RenderCopy(renderer,t,NULL,&dst); SDL_DestroyTexture(t); }
                        SDL_FreeSurface(s);
                }
            }
        };
        drawCountSmall(Tree1Count,Place1);
        drawCountSmall(Tree2Count,Place2);
        drawCountSmall(Tree3Count,Place3);
        drawCountSmall(WoodCount,Place4);

        SDL_Rect healthBG={20,20,204,20};
        SDL_Rect hungerBG={20,50,204,20};
        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(renderer,0,0,0,120); SDL_RenderFillRect(renderer,&healthBG);
        SDL_SetRenderDrawColor(renderer,0,0,0,120); SDL_RenderFillRect(renderer,&hungerBG);
        SDL_Rect healthBar={20,20,health*2,20};
        SDL_Rect hungerBar={20,50,hunger*2,20};
        SDL_SetRenderDrawColor(renderer,255,70,70,230); SDL_RenderFillRect(renderer,&healthBar);
        SDL_SetRenderDrawColor(renderer,90,230,90,230); SDL_RenderFillRect(renderer,&hungerBar);
        drawText("Здоровье: " + to_string(health) + "/" + to_string(maxHealth), healthBG.x+210, healthBG.y, dimText);
        drawText("Сытость: " + to_string(hunger) + "/100", hungerBG.x+210, hungerBG.y, dimText);

        SDL_Rect questPanel={Camera.w-300,20,280,160};
        drawPanel(questPanel,120,60);
        drawText("Задачи", questPanel.x+14, questPanel.y+10, textColor);
        drawText(string("Соберите дерева: ")+to_string(woodSpentTotal)+"/20", questPanel.x+14, questPanel.y+40, woodSpentTotal>=20?textColor:dimText);
        drawText(string("Постройте стены: ")+to_string(wallsBuiltTotal)+"/10", questPanel.x+14, questPanel.y+64, wallsBuiltTotal>=10?textColor:dimText);
        drawText(string("Съешьте еды: ")+to_string(foodsEatenTotal)+"/5", questPanel.x+14, questPanel.y+88, foodsEatenTotal>=5?textColor:dimText);
        drawText(string("Убейте врагов: ")+to_string(chickensKilled+foxesKilled)+"/15", questPanel.x+14, questPanel.y+112, (chickensKilled+foxesKilled)>=15?textColor:dimText);
        drawText(string("Выживите дней: ")+to_string(daysSurvived)+"/7", questPanel.x+14, questPanel.y+136, daysSurvived>=7?textColor:dimText);

        if(woodSpentTotal>=20 && wallsBuiltTotal>=10 && foodsEatenTotal>=5 && (chickensKilled+foxesKilled)>=15 && daysSurvived>=7){
            drawText("ВЫ ПОБЕДИЛИ!", Camera.w/2-60, Camera.h/2-30, textColor);
            drawText("Очки: " + to_string(score), Camera.w/2-40, Camera.h/2, textColor);
            SDL_RenderPresent(renderer);
            SDL_Delay(3000);
            break;
        }

        SDL_Rect mini={20,Camera.h-140,200,120};
        drawPanel(mini,110,50);
        int mapW=10000, mapH=10000;
        float sx=200.0f/mapW, sy=120.0f/mapH;
        auto drawPoint=[&](int wx,int wy, int size, Uint8 r,Uint8 g,Uint8 b){
            int px=mini.x+int(wx*sx);
            int py=mini.y+int(wy*sy);
            SDL_SetRenderDrawColor(renderer,r,g,b,220);
            SDL_Rect pr={px,py,size,size};
            SDL_RenderFillRect(renderer,&pr);
        };
        for(int i=0;i<(int)Walls.size();i+=max(1,(int)Walls.size()/300)) drawPoint(Walls[i].x*cellSize, Walls[i].y*cellSize, 2, 180,120,60);
        for(int i=0;i<(int)Foods.size();i+=max(1,(int)Foods.size()/200)) if(Foods[i].alive) drawPoint(Foods[i].x,Foods[i].y,2, 240,240,120);
        for(int i=0;i<(int)Chickens.size();i+=max(1,(int)Chickens.size()/200)) if(Chickens[i].alive) drawPoint(Chickens[i].x,Chickens[i].y,2, (Chickens[i].red?220:200), (Chickens[i].red?80:200), (Chickens[i].red?80:255));
        for(int i=0;i<(int)Foxes.size();i+=max(1,(int)Foxes.size()/100)) if(Foxes[i].alive) drawPoint(Foxes[i].x,Foxes[i].y,3, 220,80,80);
        drawPoint(player.x,player.y,3, 255,255,255);

        if(inventoryOpen){
            SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
            SDL_SetRenderDrawColor(renderer,0,0,0,170); SDL_RenderFillRect(renderer,&invBackdrop);

            drawPanel(invPanel,160,90);
            drawText("ИНВЕНТАРЬ", invPanel.x+24, invPanel.y+18, textColor);

            drawPanel(gridArea,110,50);
            drawText("Ресурсы", gridArea.x, gridArea.y-28, dimText);

            SDL_Rect icon1={gridArea.x+16, gridArea.y+16, 64,64};
            SDL_Rect icon2={gridArea.x+16, gridArea.y+16+84, 64,64};
            SDL_Rect icon3={gridArea.x+16, gridArea.y+16+168, 64,64};
            SDL_Rect iconWood={gridArea.x+16, gridArea.y+16+252, 64,64};

            SDL_RenderCopy(renderer,Tree1Tex,NULL,&icon1);
            SDL_RenderCopy(renderer,Tree2Tex,NULL,&icon2);
            SDL_RenderCopy(renderer,Tree3Tex,NULL,&icon3);
            SDL_RenderCopy(renderer,WoodTex,NULL,&iconWood);

            drawText("Tree A: "+to_string(Tree1Count), icon1.x+80, icon1.y+20, textColor);
            drawText("Tree B: "+to_string(Tree2Count), icon2.x+80, icon2.y+20, textColor);
            drawText("Tree C: "+to_string(Tree3Count), icon3.x+80, icon3.y+20, textColor);
            drawText("Wood:  "+to_string(WoodCount), iconWood.x+80, iconWood.y+20, textColor);

            drawPanel(craftPanel,110,50);
            drawText("Крафт", craftPanel.x, craftPanel.y-28, dimText);

            SDL_Rect recipeIconL={craftPanel.x+18, craftPanel.y+18, 56,56};
            SDL_Rect recipeIconR={craftPanel.x+craftPanel.w-74, craftPanel.y+18, 56,56};
            SDL_RenderCopy(renderer,Tree1Tex,NULL,&recipeIconL);
            drawText("или", craftPanel.x+craftPanel.w/2-12, craftPanel.y+36, dimText);
            SDL_RenderCopy(renderer,Tree2Tex,NULL,&recipeIconR);

            SDL_Rect plusMid={craftPanel.x+18, craftPanel.y+18+56+10, craftPanel.w-36, 2};
            SDL_SetRenderDrawColor(renderer,255,255,255,30); SDL_RenderFillRect(renderer,&plusMid);

            SDL_Rect recipeIconL2={craftPanel.x+18, craftPanel.y+18+56+24, 56,56};
            SDL_RenderCopy(renderer,Tree3Tex,NULL,&recipeIconL2);
            drawText("=>", craftPanel.x+craftPanel.w/2-8, recipeIconL2.y+16, dimText);
            SDL_Rect resultIcon={craftPanel.x+craftPanel.w-74, recipeIconL2.y, 56,56};
            SDL_RenderCopy(renderer,WoodTex,NULL,&resultIcon);

            int totalTrees = Tree1Count+Tree2Count+Tree3Count;
            bool canCraft = totalTrees>0;
            Uint8 btnAlpha = craftPressed?180:120;
            SDL_Color btnText = canCraft?SDL_Color{255,255,255,255}:SDL_Color{160,160,160,255};
            drawPanel(craftBtn, btnAlpha, 80);
            drawText(canCraft?string("СОЗДАТЬ ДЕРЕВО"):string("НУЖНО ДЕРЕВО"), craftBtn.x+18, craftBtn.y+16, btnText);
            drawText("ЛКМ по слоту Дерево: постройка стен", craftPanel.x, craftPanel.y+240, dimText);
            drawText("5 дерева: костер (ЛКМ по миру)", craftPanel.x, craftPanel.y+220, dimText);

            drawText("E — закрыть", invPanel.x+invPanel.w-160, invPanel.y+18, dimText);
        }

        if(health<=0){
            SDL_Surface* s=TTF_RenderText_Solid(font,"GAME OVER",textColor);
            if(s){ SDL_Texture* t=SDL_CreateTextureFromSurface(renderer,s);
                if(t){ int w,h; SDL_QueryTexture(t,NULL,NULL,&w,&h);
                    SDL_Rect dst={Camera.w/2-w/2,Camera.h/2-h/2,w,h}; SDL_RenderCopy(renderer,t,NULL,&dst); SDL_DestroyTexture(t);}
                    SDL_FreeSurface(s);
            }
            drawText("Очки: " + to_string(score), Camera.w/2-40, Camera.h/2+30, textColor);
            SDL_RenderPresent(renderer);
            SDL_Delay(2000);
            break;
        }

        int nightAlpha = int(160*max(0.f, sinf((timeOfDay-0.25f)*3.14159f*2.f)));
        if(nightAlpha>0 && !hasShelter){
            SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
            SDL_SetRenderDrawColor(renderer,0,0,30,nightAlpha);
            SDL_Rect dark={0,0,Camera.w,Camera.h};
            SDL_RenderFillRect(renderer,&dark);
        }
        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(renderer,0,0,0,90);
        SDL_Rect vg1={0,0,Camera.w,20}; SDL_RenderFillRect(renderer,&vg1);
        SDL_Rect vg2={0,Camera.h-20,Camera.w,20}; SDL_RenderFillRect(renderer,&vg2);
        SDL_Rect vg3={0,0,20,Camera.h}; SDL_RenderFillRect(renderer,&vg3);
        SDL_Rect vg4={Camera.w-20,0,20,Camera.h}; SDL_RenderFillRect(renderer,&vg4);

        drawText("День: "+to_string(daysSurvived+1)+"  Очки: "+to_string(score), Camera.w/2-80, 18, dimText);
        drawText("WASD — движение, Shift — бег, E — инвентарь, ЛКМ — действие / атаковать", 20, Camera.h-170, dimText);

        SDL_RenderPresent(renderer);
        SDL_Delay(16);
    }

    if(font) TTF_CloseFont(font);
    SDL_DestroyTexture(PlayerTex);
    SDL_DestroyTexture(Tree1Tex);
    SDL_DestroyTexture(Tree2Tex);
    SDL_DestroyTexture(Tree3Tex);
    SDL_DestroyTexture(FoodTex);
    SDL_DestroyTexture(ChickenTex);
    SDL_DestroyTexture(WoodTex);
    SDL_FreeSurface(PlayerSurface);
    SDL_FreeSurface(Tree1Surface);
    SDL_FreeSurface(Tree2Surface);
    SDL_FreeSurface(Tree3Surface);
    SDL_FreeSurface(FoodSurface);
    SDL_FreeSurface(ChickenSurface);
    SDL_FreeSurface(WoodSurface);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    TTF_Quit(); IMG_Quit(); SDL_Quit();
    return 0;
}
