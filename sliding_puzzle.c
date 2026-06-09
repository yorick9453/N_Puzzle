#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <SDL2/SDL_ttf.h>
#include "tinyfiledialogs.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define UI_SCALE 2
#define S(v) ((v) * UI_SCALE)

#define WINDOW_WIDTH S(500)
#define WINDOW_HEIGHT S(550)
#define PUZZLE_SIZE S(400)
#define MAX_GRID 13

#define MAXSZ 169
#define MAX_SOL 2000000
#define INF 1000000000

typedef enum {
    MENU,
    IMPORT,
    PLAYING,
    WIN
} GameState;

GameState currentState = MENU;

typedef struct {
    int number;
    SDL_Rect srcRect;
    SDL_Rect dstRect;
} Tile;

Tile tiles[MAX_GRID][MAX_GRID];

SDL_Texture *imageTexture = NULL;
TTF_Font *font = NULL;

int gridSize = 4;
int tileSize;

int emptyRow, emptyCol;

Uint32 startTime = 0;
Uint32 finishTime = 0;

SDL_Rect button3x3   = {S(150),S(110),S(200),S(55)};
SDL_Rect button4x4   = {S(150),S(190),S(200),S(55)};
SDL_Rect button5x5   = {S(150),S(270),S(200),S(55)};
SDL_Rect button13x13 = {S(150),S(350),S(200),S(55)};

SDL_Rect importButton = {S(150),S(240),S(200),S(60)};
SDL_Rect verifyButton = {S(180),S(500),S(140),S(35)};

SDL_Rect backButton = {S(10),S(10),S(160),S(40)};
SDL_Rect returnButton = {S(150),S(320),S(200),S(60)};

SDL_Rect autoButton = {S(150),S(35),S(200),S(30)};
SDL_Rect prevButton = {S(10),S(520),S(60),S(25)};
SDL_Rect nextButton = {S(430),S(520),S(60),S(25)};
SDL_Rect autoRunButton = {S(430),S(490),S(60),S(25)};

int solveMoves[MAX_SOL];
int solveLen = 0;
int solveIdx = 0;
int autoSolve = 0;
int moveCount = 0;              // Count visible moves made by the player / auto replay

int holdAutoDir = 0;          // -1: hold <, 1: hold >, 0: no hold
Uint32 nextHoldStepTime = 0;
#define HOLD_FIRST_DELAY 250
#define HOLD_REPEAT_DELAY 35

int autoPlay = 0;             // 1: automatically keep pressing > until solved
Uint32 nextAutoPlayStepTime = 0;
#define AUTOPLAY_REPEAT_DELAY 35

SDL_Color BLACK = {0,0,0,255};

void drawTextCenter(SDL_Renderer *r, const char *text, SDL_Rect box, SDL_Color color)
{
    SDL_Surface *s = TTF_RenderText_Blended(font, text, color);
    SDL_Texture *t = SDL_CreateTextureFromSurface(r, s);

    SDL_Rect rect = {
        box.x + (box.w - s->w)/2,
        box.y + (box.h - s->h)/2,
        s->w, s->h
    };

    SDL_RenderCopy(r, t, NULL, &rect);

    SDL_FreeSurface(s);
    SDL_DestroyTexture(t);
}

void initBoard()
{
    int num = 1;

    emptyRow = gridSize - 1;
    emptyCol = gridSize - 1;

    int size = PUZZLE_SIZE / gridSize;

    for(int r=0;r<gridSize;r++)
    for(int c=0;c<gridSize;c++)
    {
        int idx = r * gridSize + c;

        if(idx == gridSize*gridSize - 1)
            tiles[r][c].number = 0;
        else
            tiles[r][c].number = num++;

        tiles[r][c].srcRect = (SDL_Rect){
            c * size,
            r * size,
            size,
            size
        };

        tiles[r][c].dstRect = (SDL_Rect){
            c * tileSize + S(50),
            r * tileSize + S(70),
            tileSize,
            tileSize
        };
    }
}

void renderMenu(SDL_Renderer *r)
{
    SDL_SetRenderDrawColor(r,245,245,245,255);
    SDL_RenderClear(r);

    drawTextCenter(r,"SELECT DIFFICULTY",(SDL_Rect){S(100),S(40),S(300),S(50)},BLACK);

    SDL_SetRenderDrawColor(r,180,200,255,255);
    SDL_RenderFillRect(r,&button3x3);
    SDL_RenderFillRect(r,&button4x4);
    SDL_RenderFillRect(r,&button5x5);
    SDL_RenderFillRect(r,&button13x13);

    drawTextCenter(r,"3 x 3",button3x3,BLACK);
    drawTextCenter(r,"4 x 4",button4x4,BLACK);
    drawTextCenter(r,"5 x 5",button5x5,BLACK);
    drawTextCenter(r,"13 x 13",button13x13,BLACK);

    SDL_RenderPresent(r);
}

void renderImport(SDL_Renderer *r)
{
    SDL_SetRenderDrawColor(r,245,245,245,255);
    SDL_RenderClear(r);

    drawTextCenter(r,"IMPORT IMAGE",(SDL_Rect){S(100),S(120),S(300),S(50)},BLACK);

    SDL_SetRenderDrawColor(r,180,200,255,255);
    SDL_RenderFillRect(r,&importButton);
    drawTextCenter(r,"OPEN IMAGE",importButton,BLACK);

    SDL_SetRenderDrawColor(r,255,170,170,255);
    SDL_RenderFillRect(r,&backButton);
    drawTextCenter(r,"BACK",backButton,BLACK);

    SDL_RenderPresent(r);
}

void renderWin(SDL_Renderer *r)
{
    SDL_SetRenderDrawColor(r,245,245,245,255);
    SDL_RenderClear(r);

    drawTextCenter(r,"YOU WIN!",(SDL_Rect){S(100),S(100),S(300),S(50)},BLACK);

    char buf[128];
    sprintf(buf,"SOLVE TIME: %.2f sec", finishTime / 1000.0f);
    drawTextCenter(r, buf, (SDL_Rect){S(100),S(170),S(300),S(50)}, BLACK);

    sprintf(buf,"MOVES USED: %d", moveCount);
    drawTextCenter(r, buf, (SDL_Rect){S(100),S(225),S(300),S(50)}, BLACK);

    drawTextCenter(r,"CLICK ANYWHERE TO RETURN MENU",
                   (SDL_Rect){S(80),S(300),S(340),S(50)}, BLACK);

    SDL_RenderPresent(r);
}

int canMove(int r,int c)
{
    return abs(r-emptyRow)+abs(c-emptyCol)==1;
}

void moveTile(int r,int c)
{
    if(!canMove(r,c)) return;

    tiles[emptyRow][emptyCol].number = tiles[r][c].number;
    tiles[r][c].number = 0;

    emptyRow = r;
    emptyCol = c;
}

void moveTileByNumber(int tile)
{
    for(int r=0;r<gridSize;r++)
    for(int c=0;c<gridSize;c++)
    {
        if(tiles[r][c].number == tile)
        {
            moveTile(r,c);
            return;
        }
    }
}

void shuffleBoard()
{
    /*
       Strong solvable shuffle:
       Start from solved board and make many valid blank moves.
       This guarantees solvability, but avoids the old problem where
       random moves often immediately undo the previous move.
    */
    int shuffleSteps;

    if(gridSize == 3)      shuffleSteps = 300;
    else if(gridSize == 4) shuffleSteps = 1200;
    else if(gridSize == 5) shuffleSteps = 2500;
    else                  shuffleSteps = gridSize * gridSize * 220;  // 13x13: 37180 moves

    int prevDir = -1;

    for(int i=0;i<shuffleSteps;i++)
    {
        int validDir[4];
        int validCount = 0;

        for(int d=0; d<4; d++)
        {
            if(prevDir != -1 && d == (prevDir ^ 1))
                continue;   // do not immediately reverse last move

            int r = emptyRow;
            int c = emptyCol;

            if(d==0) r--;
            else if(d==1) r++;
            else if(d==2) c--;
            else c++;

            if(r>=0 && r<gridSize && c>=0 && c<gridSize)
                validDir[validCount++] = d;
        }

        if(validCount == 0)
        {
            prevDir = -1;
            i--;
            continue;
        }

        int d = validDir[rand() % validCount];
        int r = emptyRow;
        int c = emptyCol;

        if(d==0) r--;
        else if(d==1) r++;
        else if(d==2) c--;
        else c++;

        moveTile(r,c);
        prevDir = d;
    }
}

int checkWin()
{
    int k=1;

    for(int r=0;r<gridSize;r++)
    for(int c=0;c<gridSize;c++)
    {
        if(r==gridSize-1 && c==gridSize-1)
        {
            if(tiles[r][c].number!=0) return 0;
        }
        else if(tiles[r][c].number!=k) return 0;

        k++;
    }

    return 1;
}

int loadImage(SDL_Renderer *r)
{
    const char *f[]={"*.jpg","*.png"};

    char *file = tinyfd_openFileDialog("Select Image","",2,f,NULL,0);
    if(!file) return 0;

    SDL_Surface *img = IMG_Load(file);
    if(!img) return 0;

    SDL_Surface *scaled = SDL_CreateRGBSurface(0,PUZZLE_SIZE,PUZZLE_SIZE,32,0,0,0,0);
    if(!scaled)
    {
        SDL_FreeSurface(img);
        return 0;
    }

    SDL_BlitScaled(img,NULL,scaled,NULL);

    if(imageTexture) SDL_DestroyTexture(imageTexture);
    imageTexture = SDL_CreateTextureFromSurface(r,scaled);

    SDL_FreeSurface(img);
    SDL_FreeSurface(scaled);

    return imageTexture != NULL;
}

//====================================================
// UNIVERSAL SOLVER
//====================================================
int solverN, solverSZ;

int sdr[4] = {-1, 1, 0, 0};
int sdc[4] = {0, 0, -1, 1};

int targetValueAt(int idx)
{
    if(idx == solverSZ - 1) return 0;
    return idx + 1;
}

void getCurrentBoard(int b[])
{
    int k = 0;
    for(int r=0;r<gridSize;r++)
    for(int c=0;c<gridSize;c++)
        b[k++] = tiles[r][c].number;
}

void pushSolveMove(int tile)
{
    if(solveLen < MAX_SOL)
        solveMoves[solveLen++] = tile;
}

//====================================================
// IDA* + Manhattan + Linear Conflict
//====================================================
int idaBoard[MAXSZ];
int idaZero;
int idaPath[MAX_SOL];
int idaLen;
int idaFound;
int idaTimeout;
Uint32 idaStartTime;

int idaTimedOut()
{
    return SDL_GetTicks() - idaStartTime >= 3000;
}

int idaGoal()
{
    for(int i=0;i<solverSZ-1;i++)
        if(idaBoard[i] != i+1) return 0;

    return idaBoard[solverSZ-1] == 0;
}

int heuristicLC()
{
    int h = 0;

    for(int i=0;i<solverSZ;i++)
    {
        int v = idaBoard[i];
        if(v == 0) continue;

        int r = i / solverN;
        int c = i % solverN;
        int gr = (v-1) / solverN;
        int gc = (v-1) % solverN;

        h += abs(r-gr) + abs(c-gc);
    }

    for(int r=0;r<solverN;r++)
    {
        for(int c1=0;c1<solverN;c1++)
        {
            int v1 = idaBoard[r*solverN+c1];
            if(v1 == 0) continue;

            int gr1 = (v1-1) / solverN;
            int gc1 = (v1-1) % solverN;

            if(gr1 != r) continue;

            for(int c2=c1+1;c2<solverN;c2++)
            {
                int v2 = idaBoard[r*solverN+c2];
                if(v2 == 0) continue;

                int gr2 = (v2-1) / solverN;
                int gc2 = (v2-1) % solverN;

                if(gr2 == r && gc1 > gc2)
                    h += 2;
            }
        }
    }

    for(int c=0;c<solverN;c++)
    {
        for(int r1=0;r1<solverN;r1++)
        {
            int v1 = idaBoard[r1*solverN+c];
            if(v1 == 0) continue;

            int gr1 = (v1-1) / solverN;
            int gc1 = (v1-1) % solverN;

            if(gc1 != c) continue;

            for(int r2=r1+1;r2<solverN;r2++)
            {
                int v2 = idaBoard[r2*solverN+c];
                if(v2 == 0) continue;

                int gr2 = (v2-1) / solverN;
                int gc2 = (v2-1) % solverN;

                if(gc2 == c && gr1 > gr2)
                    h += 2;
            }
        }
    }

    return h;
}

int idaDFS(int g, int bound, int prevMove)
{
    if(idaTimedOut())
    {
        idaTimeout = 1;
        return INF;
    }

    int h = heuristicLC();
    int f = g + h;

    if(f > bound) return f;

    if(idaGoal())
    {
        idaFound = 1;
        return g;
    }

    int minNext = INF;
    int zr = idaZero / solverN;
    int zc = idaZero % solverN;

    for(int d=0; d<4; d++)
    {
        if(prevMove != -1 && d == (prevMove ^ 1))
            continue;

        int nr = zr + sdr[d];
        int nc = zc + sdc[d];

        if(nr<0 || nr>=solverN || nc<0 || nc>=solverN)
            continue;

        int nz = nr * solverN + nc;
        int tile = idaBoard[nz];

        idaBoard[idaZero] = tile;
        idaBoard[nz] = 0;

        int oldZero = idaZero;
        idaZero = nz;

        idaPath[idaLen++] = tile;

        int t = idaDFS(g+1, bound, d);

        if(idaFound) return t;
        if(idaTimeout) return INF;
        if(t < minNext) minNext = t;

        idaLen--;

        idaZero = oldZero;
        idaBoard[nz] = tile;
        idaBoard[idaZero] = 0;
    }

    return minNext;
}

int solveIDA3sec(int startBoard[])
{
    memcpy(idaBoard, startBoard, sizeof(int) * solverSZ);

    for(int i=0;i<solverSZ;i++)
        if(idaBoard[i] == 0)
            idaZero = i;

    idaLen = 0;
    idaFound = 0;
    idaTimeout = 0;
    idaStartTime = SDL_GetTicks();

    int bound = heuristicLC();

    while(!idaFound)
    {
        int t = idaDFS(0, bound, -1);

        if(idaFound) break;
        if(idaTimeout) return 0;
        if(t >= INF) return 0;

        bound = t;
    }

    solveLen = 0;
    solveIdx = 0;

    for(int i=0;i<idaLen;i++)
        pushSolveMove(idaPath[i]);

    return 1;
}

//====================================================
// Row-by-row fallback
//====================================================
int rbBoard[MAXSZ];
int rbPos[MAXSZ];
int rbLocked[MAXSZ];

void rbRebuildPos()
{
    for(int i=0;i<solverSZ;i++)
        rbPos[rbBoard[i]] = i;
}

int rbAdjacent(int a, int b)
{
    int ar=a/solverN, ac=a%solverN;
    int br=b/solverN, bc=b%solverN;
    return abs(ar-br)+abs(ac-bc)==1;
}

void rbMoveBlankTo(int newBlankPos)
{
    int z = rbPos[0];
    int tile = rbBoard[newBlankPos];

    rbBoard[z] = tile;
    rbBoard[newBlankPos] = 0;

    rbPos[0] = newBlankPos;
    rbPos[tile] = z;

    pushSolveMove(tile);
}

int rbEncodeState(int obj[], int objCount, int blank)
{
    int key = 0;

    for(int i=0;i<objCount;i++)
        key = key * solverSZ + obj[i];

    key = key * solverSZ + blank;

    return key;
}

void rbDecodeState(int key, int obj[], int objCount, int *blank)
{
    *blank = key % solverSZ;
    key /= solverSZ;

    for(int i=objCount-1;i>=0;i--)
    {
        obj[i] = key % solverSZ;
        key /= solverSZ;
    }
}

int rbBfsPlaceTiles(int vals[], int targets[], int objCount)
{
    int maxStates = 1;
    for(int i=0;i<objCount+1;i++)
        maxStates *= solverSZ;

    int *parent = (int*)malloc(sizeof(int) * maxStates);
    int *action = (int*)malloc(sizeof(int) * maxStates);
    int *queue  = (int*)malloc(sizeof(int) * maxStates);

    if(!parent || !action || !queue)
    {
        free(parent);
        free(action);
        free(queue);
        return 0;
    }

    for(int i=0;i<maxStates;i++)
    {
        parent[i] = -2;
        action[i] = -1;
    }

    int startObj[2];

    for(int i=0;i<objCount;i++)
        startObj[i] = rbPos[vals[i]];

    int start = rbEncodeState(startObj, objCount, rbPos[0]);

    int head=0, tail=0;
    queue[tail++] = start;
    parent[start] = -1;

    int goalKey = -1;

    while(head < tail)
    {
        int key = queue[head++];

        int obj[2];
        int blank;
        rbDecodeState(key, obj, objCount, &blank);

        int ok = 1;
        for(int i=0;i<objCount;i++)
        {
            if(obj[i] != targets[i])
            {
                ok = 0;
                break;
            }
        }

        if(ok)
        {
            goalKey = key;
            break;
        }

        int br = blank / solverN;
        int bc = blank % solverN;

        for(int d=0;d<4;d++)
        {
            int nr = br + sdr[d];
            int nc = bc + sdc[d];

            if(nr<0 || nr>=solverN || nc<0 || nc>=solverN)
                continue;

            int nb = nr * solverN + nc;

            if(rbLocked[nb])
                continue;

            int nextObj[2];

            for(int i=0;i<objCount;i++)
                nextObj[i] = obj[i];

            for(int i=0;i<objCount;i++)
            {
                if(nb == obj[i])
                {
                    nextObj[i] = blank;
                    break;
                }
            }

            int nextKey = rbEncodeState(nextObj, objCount, nb);

            if(parent[nextKey] == -2)
            {
                parent[nextKey] = key;
                action[nextKey] = nb;
                queue[tail++] = nextKey;
            }
        }
    }

    if(goalKey == -1)
    {
        free(parent);
        free(action);
        free(queue);
        return 0;
    }

    int *path = (int*)malloc(sizeof(int) * tail);
    if(!path)
    {
        free(parent);
        free(action);
        free(queue);
        return 0;
    }
    int len = 0;

    int cur = goalKey;

    while(parent[cur] != -1)
    {
        path[len++] = action[cur];
        cur = parent[cur];
    }

    for(int i=len-1;i>=0;i--)
        rbMoveBlankTo(path[i]);

    free(path);
    free(parent);
    free(action);
    free(queue);

    return 1;
}

int rbEncodeFinal(int cells[])
{
    int key = 0;

    for(int i=0;i<4;i++)
        key = key * solverSZ + rbBoard[cells[i]];

    return key;
}

void rbDecodeFinal(int key, int val[])
{
    for(int i=3;i>=0;i--)
    {
        val[i] = key % solverSZ;
        key /= solverSZ;
    }
}

int rbSolveFinal2x2()
{
    int cells[4];
    int cnt = 0;

    for(int i=0;i<solverSZ;i++)
        if(!rbLocked[i])
            cells[cnt++] = i;

    if(cnt != 4) return 0;

    int startKey = rbEncodeFinal(cells);

    int goalVal[4];

    for(int i=0;i<4;i++)
        goalVal[i] = targetValueAt(cells[i]);

    int goalKey = 0;

    for(int i=0;i<4;i++)
        goalKey = goalKey * solverSZ + goalVal[i];

    static int parent[400000];
    static int action[400000];
    static int queue[400000];

    int maxStates = 1;

    for(int i=0;i<4;i++)
        maxStates *= solverSZ;

    for(int i=0;i<maxStates;i++)
    {
        parent[i] = -2;
        action[i] = -1;
    }

    int head=0, tail=0;
    queue[tail++] = startKey;
    parent[startKey] = -1;

    int foundKey = -1;

    while(head < tail)
    {
        int key = queue[head++];

        if(key == goalKey)
        {
            foundKey = key;
            break;
        }

        int val[4];
        rbDecodeFinal(key, val);

        int zeroIdx = -1;

        for(int i=0;i<4;i++)
            if(val[i] == 0)
                zeroIdx = i;

        int zeroCell = cells[zeroIdx];

        for(int i=0;i<4;i++)
        {
            if(i == zeroIdx) continue;
            if(!rbAdjacent(zeroCell, cells[i])) continue;

            int nextVal[4];

            for(int j=0;j<4;j++)
                nextVal[j] = val[j];

            int movedTile = nextVal[i];

            nextVal[zeroIdx] = movedTile;
            nextVal[i] = 0;

            int nextKey = 0;

            for(int j=0;j<4;j++)
                nextKey = nextKey * solverSZ + nextVal[j];

            if(parent[nextKey] == -2)
            {
                parent[nextKey] = key;
                action[nextKey] = movedTile;
                queue[tail++] = nextKey;
            }
        }
    }

    if(foundKey == -1)
        return 0;

    int revMoves[1000];
    int len = 0;
    int cur = foundKey;

    while(parent[cur] != -1)
    {
        revMoves[len++] = action[cur];
        cur = parent[cur];
    }

    for(int i=len-1;i>=0;i--)
        rbMoveBlankTo(rbPos[revMoves[i]]);

    return 1;
}


//====================================================
// Hybrid solver for 4x4 / 5x5
// Layer-by-layer until only a 3x3 area remains,
// then solve that unlocked 3x3 area using IDA* + Manhattan + Linear Conflict.
//====================================================

int solveLayerPrefixTo3x3(int startBoard[])
{
    memcpy(rbBoard, startBoard, sizeof(int) * solverSZ);
    rbRebuildPos();
    memset(rbLocked, 0, sizeof(rbLocked));

    solveLen = 0;
    solveIdx = 0;

    // Leave exactly bottom-right 3x3 unlocked.
    // 4x4: k = 0 only
    // 5x5: k = 0, 1
    for(int k=0;k<solverN-3;k++)
    {
        // Place current layer's top row, except the last two cells.
        for(int c=k;c<solverN-2;c++)
        {
            int idx = k * solverN + c;
            int val = targetValueAt(idx);

            int vals[1] = {val};
            int targets[1] = {idx};

            if(rbPos[val] != idx)
            {
                if(!rbBfsPlaceTiles(vals, targets, 1))
                    return 0;
            }

            rbLocked[idx] = 1;
        }

        // Place the last two cells of the current layer's top row together.
        {
            int idx1 = k * solverN + (solverN-2);
            int idx2 = k * solverN + (solverN-1);

            int v1 = targetValueAt(idx1);
            int v2 = targetValueAt(idx2);

            int vals[2] = {v1, v2};
            int targets[2] = {idx1, idx2};

            if(rbPos[v1] != idx1 || rbPos[v2] != idx2)
            {
                if(!rbBfsPlaceTiles(vals, targets, 2))
                    return 0;
            }

            rbLocked[idx1] = 1;
            rbLocked[idx2] = 1;
        }

        // Place current layer's left column, except the last two cells.
        for(int r=k+1;r<solverN-2;r++)
        {
            int idx = r * solverN + k;
            int val = targetValueAt(idx);

            int vals[1] = {val};
            int targets[1] = {idx};

            if(rbPos[val] != idx)
            {
                if(!rbBfsPlaceTiles(vals, targets, 1))
                    return 0;
            }

            rbLocked[idx] = 1;
        }

        // Place the last two cells of the current layer's left column together.
        {
            int idx1 = (solverN-2) * solverN + k;
            int idx2 = (solverN-1) * solverN + k;

            int v1 = targetValueAt(idx1);
            int v2 = targetValueAt(idx2);

            int vals[2] = {v1, v2};
            int targets[2] = {idx1, idx2};

            if(rbPos[v1] != idx1 || rbPos[v2] != idx2)
            {
                if(!rbBfsPlaceTiles(vals, targets, 2))
                    return 0;
            }

            rbLocked[idx1] = 1;
            rbLocked[idx2] = 1;
        }
    }

    return 1;
}

int idaGoalUnlocked3x3()
{
    for(int i=0;i<solverSZ;i++)
    {
        if(rbLocked[i]) continue;
        if(idaBoard[i] != targetValueAt(i)) return 0;
    }

    return 1;
}

int heuristicLCUnlocked3x3()
{
    int h = 0;

    // Manhattan distance inside the unlocked 3x3 region.
    for(int i=0;i<solverSZ;i++)
    {
        if(rbLocked[i]) continue;

        int v = idaBoard[i];
        if(v == 0) continue;

        int target = v - 1;
        if(target < 0 || target >= solverSZ) continue;

        int r = i / solverN;
        int c = i % solverN;
        int gr = target / solverN;
        int gc = target % solverN;

        h += abs(r-gr) + abs(c-gc);
    }

    // Row linear conflict, restricted to unlocked cells.
    for(int r=0;r<solverN;r++)
    {
        for(int c1=0;c1<solverN;c1++)
        {
            int idx1 = r * solverN + c1;
            if(rbLocked[idx1]) continue;

            int v1 = idaBoard[idx1];
            if(v1 == 0) continue;

            int gr1 = (v1-1) / solverN;
            int gc1 = (v1-1) % solverN;

            if(gr1 != r) continue;

            for(int c2=c1+1;c2<solverN;c2++)
            {
                int idx2 = r * solverN + c2;
                if(rbLocked[idx2]) continue;

                int v2 = idaBoard[idx2];
                if(v2 == 0) continue;

                int gr2 = (v2-1) / solverN;
                int gc2 = (v2-1) % solverN;

                if(gr2 == r && gc1 > gc2)
                    h += 2;
            }
        }
    }

    // Column linear conflict, restricted to unlocked cells.
    for(int c=0;c<solverN;c++)
    {
        for(int r1=0;r1<solverN;r1++)
        {
            int idx1 = r1 * solverN + c;
            if(rbLocked[idx1]) continue;

            int v1 = idaBoard[idx1];
            if(v1 == 0) continue;

            int gr1 = (v1-1) / solverN;
            int gc1 = (v1-1) % solverN;

            if(gc1 != c) continue;

            for(int r2=r1+1;r2<solverN;r2++)
            {
                int idx2 = r2 * solverN + c;
                if(rbLocked[idx2]) continue;

                int v2 = idaBoard[idx2];
                if(v2 == 0) continue;

                int gr2 = (v2-1) / solverN;
                int gc2 = (v2-1) % solverN;

                if(gc2 == c && gr1 > gr2)
                    h += 2;
            }
        }
    }

    return h;
}

int idaDFSUnlocked3x3(int g, int bound, int prevMove)
{
    if(idaTimedOut())
    {
        idaTimeout = 1;
        return INF;
    }

    int h = heuristicLCUnlocked3x3();
    int f = g + h;

    if(f > bound) return f;

    if(idaGoalUnlocked3x3())
    {
        idaFound = 1;
        return g;
    }

    int minNext = INF;
    int zr = idaZero / solverN;
    int zc = idaZero % solverN;

    for(int d=0; d<4; d++)
    {
        if(prevMove != -1 && d == (prevMove ^ 1))
            continue;

        int nr = zr + sdr[d];
        int nc = zc + sdc[d];

        if(nr<0 || nr>=solverN || nc<0 || nc>=solverN)
            continue;

        int nz = nr * solverN + nc;

        // Important: final IDA* may only move inside the unlocked 3x3 area.
        if(rbLocked[nz])
            continue;

        int tile = idaBoard[nz];

        idaBoard[idaZero] = tile;
        idaBoard[nz] = 0;

        int oldZero = idaZero;
        idaZero = nz;

        idaPath[idaLen++] = tile;

        int t = idaDFSUnlocked3x3(g+1, bound, d);

        if(idaFound) return t;
        if(idaTimeout) return INF;
        if(t < minNext) minNext = t;

        idaLen--;

        idaZero = oldZero;
        idaBoard[nz] = tile;
        idaBoard[idaZero] = 0;
    }

    return minNext;
}

int solveIDAUnlocked3x3FromRB()
{
    memcpy(idaBoard, rbBoard, sizeof(int) * solverSZ);

    for(int i=0;i<solverSZ;i++)
    {
        if(idaBoard[i] == 0)
        {
            idaZero = i;
            break;
        }
    }

    // The blank must be inside the remaining unlocked 3x3 region.
    if(rbLocked[idaZero])
        return 0;

    idaLen = 0;
    idaFound = 0;
    idaTimeout = 0;
    idaStartTime = SDL_GetTicks();

    int oldSolveLen = solveLen;
    int bound = heuristicLCUnlocked3x3();

    while(!idaFound)
    {
        int t = idaDFSUnlocked3x3(0, bound, -1);

        if(idaFound) break;
        if(idaTimeout) return 0;
        if(t >= INF) return 0;

        bound = t;
    }

    // Append the final 3x3 IDA* moves after the layer-by-layer moves.
    solveLen = oldSolveLen;
    for(int i=0;i<idaLen;i++)
        pushSolveMove(idaPath[i]);

    return 1;
}

int solveLayerTo3x3ThenIDA(int startBoard[])
{
    if(!solveLayerPrefixTo3x3(startBoard))
        return 0;

    if(solveIDAUnlocked3x3FromRB())
        return 1;

    // Safety fallback: for small boards we can still finish with the old 2x2 method.
    // For 13x13, do not call full 2x2 fallback because its old final-state table would be huge.
    if(solverN <= 5)
        return solveRowByRow(startBoard);

    return 0;
}

int solveRowByRow(int startBoard[])
{
    memcpy(rbBoard, startBoard, sizeof(int) * solverSZ);
    rbRebuildPos();

    memset(rbLocked, 0, sizeof(rbLocked));

    solveLen = 0;
    solveIdx = 0;

    for(int k=0;k<solverN-2;k++)
    {
        for(int c=k;c<solverN-2;c++)
        {
            int idx = k * solverN + c;
            int val = targetValueAt(idx);

            int vals[1] = {val};
            int targets[1] = {idx};

            if(rbPos[val] != idx)
            {
                if(!rbBfsPlaceTiles(vals, targets, 1))
                    return 0;
            }

            rbLocked[idx] = 1;
        }

        {
            int idx1 = k * solverN + (solverN-2);
            int idx2 = k * solverN + (solverN-1);

            int v1 = targetValueAt(idx1);
            int v2 = targetValueAt(idx2);

            int vals[2] = {v1, v2};
            int targets[2] = {idx1, idx2};

            if(rbPos[v1] != idx1 || rbPos[v2] != idx2)
            {
                if(!rbBfsPlaceTiles(vals, targets, 2))
                    return 0;
            }

            rbLocked[idx1] = 1;
            rbLocked[idx2] = 1;
        }

        for(int r=k+1;r<solverN-2;r++)
        {
            int idx = r * solverN + k;
            int val = targetValueAt(idx);

            int vals[1] = {val};
            int targets[1] = {idx};

            if(rbPos[val] != idx)
            {
                if(!rbBfsPlaceTiles(vals, targets, 1))
                    return 0;
            }

            rbLocked[idx] = 1;
        }

        {
            int idx1 = (solverN-2) * solverN + k;
            int idx2 = (solverN-1) * solverN + k;

            int v1 = targetValueAt(idx1);
            int v2 = targetValueAt(idx2);

            int vals[2] = {v1, v2};
            int targets[2] = {idx1, idx2};

            if(rbPos[v1] != idx1 || rbPos[v2] != idx2)
            {
                if(!rbBfsPlaceTiles(vals, targets, 2))
                    return 0;
            }

            rbLocked[idx1] = 1;
            rbLocked[idx2] = 1;
        }
    }

    return rbSolveFinal2x2();
}

int solveCurrentPuzzle()
{
    int startBoard[MAXSZ];

    solverN = gridSize;
    solverSZ = gridSize * gridSize;

    getCurrentBoard(startBoard);

    solveLen = 0;
    solveIdx = 0;

    // 3x3: use IDA* directly.
    if(gridSize == 3)
    {
        if(solveIDA3sec(startBoard))
            return 1;

        return solveRowByRow(startBoard);
    }

    // 4x4 / 5x5:
    // Do NOT run full-board IDA*. First solve layers until only 3x3 remains,
    // then use IDA* only inside that 3x3 region.
    return solveLayerTo3x3ThenIDA(startBoard);
}

void renderPuzzle(SDL_Renderer *r)
{
    SDL_SetRenderDrawColor(r,230,230,230,255);
    SDL_RenderClear(r);

    int size = PUZZLE_SIZE / gridSize;

    for(int i=0;i<gridSize;i++)
    for(int j=0;j<gridSize;j++)
    {
        SDL_Rect rect = tiles[i][j].dstRect;

        if(tiles[i][j].number == 0)
        {
            SDL_SetRenderDrawColor(r,40,40,40,255);
            SDL_RenderFillRect(r,&rect);
        }
        else
        {
            int n = tiles[i][j].number - 1;

            SDL_Rect src = {
                (n % gridSize) * size,
                (n / gridSize) * size,
                size,
                size
            };

            SDL_RenderCopy(r,imageTexture,&src,&rect);
        }

        SDL_SetRenderDrawColor(r,0,0,0,255);
        SDL_RenderDrawRect(r,&rect);
    }

    char buf[64];
    Uint32 t = SDL_GetTicks() - startTime;
    sprintf(buf,"TIME: %.2f", t/1000.0f);
    drawTextCenter(r,buf,(SDL_Rect){S(300),S(5),S(180),S(30)},BLACK);

    SDL_SetRenderDrawColor(r,120,220,120,255);
    SDL_RenderFillRect(r,&verifyButton);
    drawTextCenter(r,"VERIFY",verifyButton,BLACK);

    SDL_SetRenderDrawColor(r,180,220,255,255);
    SDL_RenderFillRect(r,&autoButton);
    drawTextCenter(r,"AUTO SOLVE",autoButton,BLACK);

    if(autoSolve)
    {
        SDL_SetRenderDrawColor(r,255,200,150,255);
        SDL_RenderFillRect(r,&prevButton);
        SDL_RenderFillRect(r,&nextButton);
        SDL_RenderFillRect(r,&autoRunButton);

        drawTextCenter(r,"<",prevButton,BLACK);
        drawTextCenter(r,">",nextButton,BLACK);
        drawTextCenter(r,"AUTO",autoRunButton,BLACK);
    }

    SDL_RenderPresent(r);
}

void autoStepOnce(int dir)
{
    if(!autoSolve) return;

    if(dir > 0)
    {
        if(solveIdx < solveLen)
        {
            moveTileByNumber(solveMoves[solveIdx]);
            solveIdx++;
            moveCount++;
        }
    }
    else if(dir < 0)
    {
        if(solveIdx > 0)
        {
            solveIdx--;
            moveTileByNumber(solveMoves[solveIdx]);
            moveCount++;
        }
    }
}

int main()
{
    SDL_Init(SDL_INIT_VIDEO);
    IMG_Init(IMG_INIT_PNG|IMG_INIT_JPG);
    TTF_Init();

    SDL_Window *w = SDL_CreateWindow("Puzzle",
        SDL_WINDOWPOS_CENTERED,SDL_WINDOWPOS_CENTERED,
        WINDOW_WIDTH,WINDOW_HEIGHT,0);

    SDL_Renderer *r = SDL_CreateRenderer(w,-1,0);

    font = TTF_OpenFont("/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",24 * UI_SCALE);

    srand(time(NULL));

    tileSize = PUZZLE_SIZE / gridSize;

    initBoard();
    shuffleBoard();

    SDL_Event e;
    int running=1;

    while(running)
    {
        while(SDL_PollEvent(&e))
        {
            if(e.type==SDL_QUIT) running=0;

            if(e.type==SDL_MOUSEBUTTONUP)
            {
                holdAutoDir = 0;
            }

            if(e.type==SDL_MOUSEBUTTONDOWN)
            {
                int x=e.button.x,y=e.button.y;

                if(currentState==MENU)
                {
                    if(x>=button3x3.x && y>=button3x3.y &&
                       x<=button3x3.x+button3x3.w &&
                       y<=button3x3.y+button3x3.h)
                        gridSize=3;

                    else if(x>=button4x4.x && y>=button4x4.y &&
                            x<=button4x4.x+button4x4.w &&
                            y<=button4x4.y+button4x4.h)
                        gridSize=4;

                    else if(x>=button5x5.x && y>=button5x5.y &&
                            x<=button5x5.x+button5x5.w &&
                            y<=button5x5.y+button5x5.h)
                        gridSize=5;

                    else if(x>=button13x13.x && y>=button13x13.y &&
                            x<=button13x13.x+button13x13.w &&
                            y<=button13x13.y+button13x13.h)
                        gridSize=13;
                    else
                        continue;

                    tileSize = PUZZLE_SIZE / gridSize;
                    currentState = IMPORT;
                }

                else if(currentState==IMPORT)
                {
                    if(x>=backButton.x && y>=backButton.y &&
                       x<=backButton.x+backButton.w &&
                       y<=backButton.y+backButton.h)
                        currentState = MENU;

                    else if(x>=importButton.x && y>=importButton.y &&
                            x<=importButton.x+importButton.w &&
                            y<=importButton.y+importButton.h)
                    {
                        if(loadImage(r))
                        {
                            initBoard();
                            shuffleBoard();
                            startTime = SDL_GetTicks();
                            currentState = PLAYING;

                            autoSolve = 0;
                            autoPlay = 0;
                            holdAutoDir = 0;
                            solveLen = 0;
                            solveIdx = 0;
                            moveCount = 0;
                        }
                    }
                }

                else if(currentState==PLAYING)
                {
                    if(x>=autoButton.x && y>=autoButton.y &&
                       x<=autoButton.x+autoButton.w &&
                       y<=autoButton.y+autoButton.h)
                    {
                        if(autoSolve)
                        {
                            autoSolve = 0;
                            autoPlay = 0;
                            holdAutoDir = 0;
                            solveLen = 0;
                            solveIdx = 0;
                        }
                        else
                        {
                            if(solveCurrentPuzzle())
                                autoSolve = 1;
                        }

                        continue;
                    }

                    if(!autoSolve &&
                       x>=verifyButton.x && y>=verifyButton.y &&
                       x<=verifyButton.x+verifyButton.w &&
                       y<=verifyButton.y+verifyButton.h)
                    {
                        if(checkWin())
                        {
                            finishTime = SDL_GetTicks() - startTime;
                            currentState = WIN;
                        }

                        continue;
                    }

                    if(autoSolve)
                    {
                        if(x>=autoRunButton.x && y>=autoRunButton.y &&
                           x<=autoRunButton.x+autoRunButton.w &&
                           y<=autoRunButton.y+autoRunButton.h)
                        {
                            autoPlay = !autoPlay;
                            holdAutoDir = 0;
                            nextAutoPlayStepTime = SDL_GetTicks();
                            continue;
                        }

                        if(x>=nextButton.x && y>=nextButton.y &&
                           x<=nextButton.x+nextButton.w &&
                           y<=nextButton.y+nextButton.h)
                        {
                            autoPlay = 0;
                            autoStepOnce(1);
                            holdAutoDir = 1;
                            nextHoldStepTime = SDL_GetTicks() + HOLD_FIRST_DELAY;
                            continue;
                        }

                        if(x>=prevButton.x && y>=prevButton.y &&
                           x<=prevButton.x+prevButton.w &&
                           y<=prevButton.y+prevButton.h)
                        {
                            autoPlay = 0;
                            autoStepOnce(-1);
                            holdAutoDir = -1;
                            nextHoldStepTime = SDL_GetTicks() + HOLD_FIRST_DELAY;
                            continue;
                        }

                        continue;
                    }

                    x -= S(50);
                    y -= S(70);

                    if(x>=0 && y>=0 && x<PUZZLE_SIZE && y<PUZZLE_SIZE)
                    {
                        int mr = y / tileSize;
                        int mc = x / tileSize;

                        if(canMove(mr, mc))
                        {
                            moveTile(mr, mc);
                            moveCount++;
                        }
                    }
                }

                else if(currentState==WIN)
                {
                    currentState = MENU;
                }
            }
        }

        if(currentState==PLAYING && autoSolve && autoPlay)
	{
	    Uint32 now = SDL_GetTicks();

	    if(solveIdx >= solveLen)
	    {
		autoPlay = 0;
	    }
	    else if(now >= nextAutoPlayStepTime)
	    {
		autoStepOnce(1);
		nextAutoPlayStepTime = now + AUTOPLAY_REPEAT_DELAY;

		if(solveIdx >= solveLen)
		{
		    autoPlay = 0;
		}
	    }
	}

        if(currentState==PLAYING && autoSolve && !autoPlay && holdAutoDir != 0)
        {
            Uint32 now = SDL_GetTicks();
            if(now >= nextHoldStepTime)
            {
                autoStepOnce(holdAutoDir);
                nextHoldStepTime = now + HOLD_REPEAT_DELAY;
            }
        }

        if(currentState==MENU) renderMenu(r);
        else if(currentState==IMPORT) renderImport(r);
        else if(currentState==PLAYING) renderPuzzle(r);
        else if(currentState==WIN) renderWin(r);

        SDL_Delay(16);
    }

    SDL_DestroyRenderer(r);
    SDL_DestroyWindow(w);
    TTF_Quit();
    IMG_Quit();
    SDL_Quit();

    return 0;
}
