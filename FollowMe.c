/*
*  FollowMe game solver
*
*  Copyright (c) 2013	Serraino Alessio
*
*  This program is free software; you can redistribute it and/or modify
*  it under the terms of the GNU General Public License as published by
*  the Free Software Foundation; either version 2 of the License, or
*  (at your option) any later version.
*
*  This program is distributed in the hope that it will be useful,
*  but WITHOUT ANY WARRANTY; without even the implied warranty of
*  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
*  GNU General Public License for more details.
*
*  You should have received a copy of the GNU General Public License
*  along with this program; If not, see <http://www.gnu.org/licenses/>.
*/

/*
*  FollowMe is a game where you have to join two points of the same color with a path
*  the paths can't cross eachoter and they have to fill all the grid of the game.
*/

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <time.h>
#include <sys/time.h>

#define OPTIMIZATION //Comment this line to remove every optimization
//#define ANIMATION
//#define RANDOM_COLOR

/* 
*  Data.dat file format
*  It is a text file, the first line is of the form "%d %d", the first number is the width of the game grid,
*  the second is the height.
*  The second line is of the form "%d" and this number contanis the number of paths
*  each line is in the form "%d %d %d %d"
*  the first number is x_start, the second y_start, the third x_end, the forth y_end.
*  each line represents the two endpoints of the path.
*/
struct _Cell { //This is one cell of the game grid
    //Each cell can be part of a path or a void cell or a external cell (this option does not exist in the game)
    long unsigned Path: (sizeof(long unsigned) << 3) - 1; //ID of the path, starting from 1, 0 is a special value
    //if this ID is 1 means that the cell is part of the path that joins the first couple of points,
    //if it is 2 means that the cell is patrt of the path that joins the second couple of points, etc.
    //0 is the only special value, it means that the cell does not be part of any path
    long unsigned Joined: 1; //one bit, if it is 1 the piece of the path is joined to the end, otherwise not
    long unsigned ID; //ID of the piece in the path, so if it is 0 it represents the start endpoint, if it is ULONG_MAX
    //it represents the end endpoint, if it is 1 represents the first piece of the path, 2 the second, etc.
    //if Path is 0, ID does not represents anything, it olny means that the cell is not part of any path.
};
#define ULONG_MAX (~(long unsigned)0) //The maximum value can a long unsigned contains

struct _PathPoint { //Struct that represents the endpoints of a path
    unsigned x_start; //x coordinate of the start endpoint
    unsigned y_start; //y coordinate of the start endpoint
    unsigned x_end; //x coordinate of the end endpoint
    unsigned y_end; //y coordinate of the end endpoint
};

typedef struct _Cell Cell;
typedef struct _PathPoint PathPoint;

Cell *GameGrid;
PathPoint *Points;
int Width, Height;
int Points_Len;
unsigned char *FloodFillData;
int FloodFillData_Len;
/*
__inline__ rol(long unsigned op, unsigned bits){
    __asm__ __volatile__("rol %%cl, %%eax"
    : "=r" (op)
    : "cl"  (op), "eax" (bits) );
}
__inline__ ror(long unsigned op, unsigned bits){
    __asm__ __volatile__("ror %%cl, %%eax"
    : "=r" (op)
    : "cl"  (op), "eax" (bits) );
}
*/
#define rol(operator, bit) (((operator) << (bit)) | ((operator) >> ((sizeof(operator) << 3) - (bit))))
#define ror(operator, bit) (((operator) >> (bit)) | ((operator) << ((sizeof(operator) << 3) - (bit))))

#define GET_N_BIT(num, pos) ((num & (1 << pos))?(1):(0))
#define SET_N_BIT(num, pos, new_val) ((new_val)?((num) |= (0x01 << pos)):((num) &= (~(0x1 << pos))))

#define GET_FLOOD_FILL_DATA(x, y) GET_N_BIT(FloodFillData[(((unsigned)(x) % Width) + ((unsigned)(y) % Height)*Width) >> 3], \
((((unsigned)(x) % Width) + ((unsigned)(y) % Height)*Width) & 0x7))
#define SET_FLOOD_FILL_DATA(x, y, new_val) SET_N_BIT(FloodFillData[(((unsigned)(x) % Width) + ((unsigned)(y) % Height)*Width) >> 3],  \
((((unsigned)(x) % Width) + ((unsigned)(y) % Height)*Width) & 0x07), \
(new_val))
int PrintPoint_x, PrintPoint_y;
unsigned long long int TriedOutPaths = 0;
#define GameCell(x, y) GameGrid[((unsigned)(x) % Width) + ((unsigned)(y) % Height)*Width]
#define IsEmpty(c)     (!((c).Path | (c).ID))
#define IsNotEmpty(c)  ( ((c).Path | (c).ID))
//Otptimized, but unsafe
// #define GetCell(x, y) GameGrid[(x), (y)*Height]

#define Color_Reset printf("\033[0m")

#ifdef RANDOM_COLOR
int Randm = 25, Randq = 28;
int Random(int Srand){
    int Tests = 2,	//Number of tests
    Success = 1, //Number of successes
    //Success : Tests = x : MAX
    probability, //Probability of success
    Rand; //Temp random number
    srand(Srand*Randm+Randq);
    for(probability = 128; Tests < 1000; Tests++){
        Rand = rand() % 256;
        if (Rand < probability){ //Success!
            Success++;
        }
        probability = Success*256/Tests;
    }
    //probability : MAX = x : 256
    return probability;
}
#endif

void TxColor(int N){
#ifdef RANDOM_COLOR
    printf("\033[38;5;%dm", Random(N));
#else
    printf("\033[38;5;%dm", N);
#endif
}

void BkColor(int N){
#ifdef RANDOM_COLOR
    printf("\033[48;5;%dm", Random(N));
#else
    printf("\033[48;5;%dm", N);
#endif
}

void PrintGrid(){
    int x, y,  // x and y coordinate
    x_end = Width*2 -1,
    y_end = Height*2-1,
    print_x = PrintPoint_x+1, //x and y coordinates of the print point
    print_y = PrintPoint_y+1,
    tmpx, tmpy;
    Cell ccell; //Current Cell
    char samepath;
    printf("\033[%d;%dH", print_y++, print_x); //Move the cursor to 0; 0
    for (x = 0; x < Width; x++){
        printf("+---");
    }
    printf("+\033[%d;%dH", print_y++, print_x);
    for (y = 0; y < y_end; y++){
        tmpy = y >> 1;
        if (y % 2){
            for (x = 0; x < Width; x++){
                if ((GameCell(x, tmpy).Path == GameCell(x, tmpy + 1).Path) &&
                ((GameCell(x, tmpy).ID == (GameCell(x, tmpy + 1).ID + 1)) ||
                ((GameCell(x, tmpy).ID + 1) == GameCell(x, tmpy + 1).ID) ||
                ((GameCell(x, tmpy).ID == ULONG_MAX) && (GameCell(x, tmpy + 1).Joined)) ||
                ((GameCell(x, tmpy + 1).ID == ULONG_MAX) && (GameCell(x, tmpy).Joined)))){
                    printf("+-");
                    TxColor(GameCell(x, tmpy).Path);
                    //Between two cells of the same path, so join them with their colour
                    printf("|");
                    Color_Reset;
                    printf("-");
                } else {
                    Color_Reset;
                    printf("+---");
                }
            }
            printf("+\033[%d;%dH", print_y++, print_x);
        } else {
            printf("|");
            for (x = 0; x < x_end; x++){
                tmpx = x >> 1;
                if (x % 2) {
                    if ((GameCell(tmpx, tmpy).Path == GameCell(tmpx + 1, tmpy).Path) &&
                    ((GameCell(tmpx, tmpy).ID == (GameCell(tmpx + 1, tmpy).ID + 1)) ||
                    ((GameCell(tmpx, tmpy).ID + 1) == GameCell(tmpx + 1, tmpy).ID) ||
                    ((GameCell(tmpx, tmpy).ID == ULONG_MAX) && (GameCell(tmpx + 1, tmpy).Joined)) ||
                    ((GameCell(tmpx + 1, tmpy).ID == ULONG_MAX) && (GameCell(tmpx, tmpy).Joined)))){
                        TxColor(GameCell(x >> 1, y >> 1).Path);
                        //Between two cells of the same path, so join them with their colour
                        printf("-");
                        Color_Reset;
                    } else {
                        Color_Reset;
                        printf("|");
                    }
                } else {
                    if (GameCell(tmpx, tmpy).Path != 0){
                        if (GameCell(tmpx, tmpy).ID == 0) {
                            //The cell contains the start or the end of a path
                            samepath = 0;
                            ccell = GameCell(tmpx, tmpy);
                            //Left cell
                            if (((tmpx > 0) && (GameCell(tmpx -1, tmpy).Path == ccell.Path)) && (GameCell(tmpx - 1, tmpy).ID == 1))
                                    samepath |= 0x1;
                            //Right cell
                            if (((tmpx + 1 < Width) && (GameCell(tmpx + 1, tmpy).Path == ccell.Path)) && (GameCell(tmpx + 1, tmpy).ID == 1))
                                    samepath |= 0x2;
                            //Above cell
                            if (((tmpy > 0) && (GameCell(tmpx, tmpy - 1).Path == ccell.Path)) && (GameCell(tmpx, tmpy - 1).ID == 1))
                                    samepath |= 0x4;
                            //Below cell
                            if (((tmpy + 1 < Height) && (GameCell(tmpx, tmpy + 1).Path == ccell.Path)) && (GameCell(tmpx, tmpy + 1).ID == 1))
                                    samepath |= 0x8;
                            //The cells arround the current that are in the same path are stored in samepath
                            //There are 5 possibilities: none (0), only left (0x1), only right (0x2), only above (0x4), only below(0x8)
                            switch(samepath){
                                case 0x0: // None
                                    printf(" ");
                                    BkColor(ccell.Path);
                                    printf(" ");
                                    Color_Reset;
                                    printf(" ");
                                    break;
                                case 0x1: // Left
                                    TxColor(ccell.Path);
                                    printf("-");
                                    Color_Reset;
                                    BkColor(ccell.Path);
                                    printf(" ");
                                    Color_Reset;
                                    printf(" ");
                                    break;
                                case 0x2: // Right
                                    printf(" ");
                                    BkColor(ccell.Path);
                                    printf(" ");
                                    Color_Reset;
                                    TxColor(ccell.Path);
                                    printf("-");
                                    Color_Reset;
                                    break;
                                case 0x4: // Above
                                    printf(" ");
                                    BkColor(ccell.Path);
                                    printf(" ");
                                    Color_Reset;
                                    printf(" ");
                                    break;
                                case 0x8: // Below
                                    printf(" ");
                                    BkColor(ccell.Path);
                                    printf(" ");
                                    Color_Reset;
                                    printf(" ");
                                    break;
                                default:
                                    fprintf(stderr, "Error drawing the grid\n");
                                    return;
                            }
                        } else if (GameCell(tmpx, tmpy).ID == ULONG_MAX) {
                            samepath = 0;
                            ccell = GameCell(tmpx, tmpy);
                            //Left cell
                            if (((tmpx > 0) && (GameCell(tmpx -1, tmpy).Path == ccell.Path)) && (GameCell(tmpx - 1, tmpy).Joined))
                                    samepath |= 0x1;
                            //Right cell
                            if (((tmpx + 1 < Width) && (GameCell(tmpx + 1, tmpy).Path == ccell.Path)) && (GameCell(tmpx + 1, tmpy).Joined))
                                    samepath |= 0x2;
                            //Above cell
                            if (((tmpy > 0) && (GameCell(tmpx, tmpy - 1).Path == ccell.Path)) && (GameCell(tmpx, tmpy - 1).Joined))
                                    samepath |= 0x4;
                            //Below cell
                            if (((tmpy + 1 < Height) && (GameCell(tmpx, tmpy + 1).Path == ccell.Path)) && (GameCell(tmpx, tmpy + 1).Joined))
                                    samepath |= 0x8;
                            //The cells arround the current that are in the same path are stored in samepath
                            //There are 5 possibilities: none (0), only left (0x1), only right (0x2), only above (0x4), only below(0x8)
                            switch(samepath){
                                case 0x0: // None
                                    printf(" ");
                                    BkColor(ccell.Path);
                                    printf(" ");
                                    Color_Reset;
                                    printf(" ");
                                    break;
                                case 0x1: // Left
                                    TxColor(ccell.Path);
                                    printf("-");
                                    Color_Reset;
                                    BkColor(ccell.Path);
                                    printf(" ");
                                    Color_Reset;
                                    printf(" ");
                                    break;
                                case 0x2: // Right
                                    printf(" ");
                                    BkColor(ccell.Path);
                                    printf(" ");
                                    Color_Reset;
                                    TxColor(ccell.Path);
                                    printf("-");
                                    Color_Reset;
                                    break;
                                case 0x4: // Above
                                    printf(" ");
                                    BkColor(ccell.Path);
                                    printf(" ");
                                    Color_Reset;
                                    printf(" ");
                                    break;
                                case 0x8: // Below
                                    printf(" ");
                                    BkColor(ccell.Path);
                                    printf(" ");
                                    Color_Reset;
                                    printf(" ");
                                    break;
                                default:
                                    fprintf(stderr, "Error drawing the grid\n");
                                    return;
                            }
                        } else { //Cell is part of a path
                            samepath = 0; //Samepath is used to store wich of the adiacent cells are part
                            //of the same path of the current cell
                            ccell = GameCell(tmpx, tmpy);
                            //Left cell
                            if ((tmpx > 0) && (GameCell(tmpx -1, tmpy).Path == ccell.Path))
                                if ((GameCell(tmpx - 1, tmpy).ID == ccell.ID + 1) ||
                                (GameCell(tmpx - 1, tmpy).ID + 1 == ccell.ID) ||
                                ((GameCell(tmpx -1, tmpy).ID == ULONG_MAX) && (ccell.Joined)))
                                    samepath |= 0x1;
                            //Right cell
                            if ((tmpx + 1 < Width) && (GameCell(tmpx + 1, tmpy).Path == ccell.Path))
                                if ((GameCell(tmpx + 1, tmpy).ID == ccell.ID + 1) ||
                                (GameCell(tmpx + 1, tmpy).ID + 1 == ccell.ID) ||
                                ((GameCell(tmpx +1, tmpy).ID == ULONG_MAX) && (ccell.Joined)))
                                    samepath |= 0x2;
                            //Above cell
                            if ((tmpy > 0) && (GameCell(tmpx, tmpy - 1).Path == ccell.Path))
                                if ((GameCell(tmpx, tmpy - 1).ID == ccell.ID + 1) ||
                                (GameCell(tmpx, tmpy - 1).ID + 1 == ccell.ID) ||
                                ((GameCell(tmpx, tmpy -1).ID == ULONG_MAX) && (ccell.Joined)))
                                    samepath |= 0x4;
                            //Below cell
                            if ((tmpy + 1 < Height) && (GameCell(tmpx, tmpy + 1).Path == ccell.Path))
                                if ((GameCell(tmpx, tmpy + 1).ID == ccell.ID + 1) ||
                                (GameCell(tmpx, tmpy + 1).ID + 1 == ccell.ID) ||
                                ((GameCell(tmpx, tmpy +1).ID == ULONG_MAX) && (ccell.Joined)))
                                    samepath |= 0x8;
                            //Changes the text color
                            TxColor(ccell.Path);
                            //The cells arround the current that are in the same path are stored in samepath
                            //There are 10 possibilities: only left (0x1), only right (0x2), only above (0x4), only below(0x8)
                            //                            left and right (0x3), above and below (0xC), above and left (0x5), above and right (0x6)
                            //                            below and left (0x9), below and right (0xA);
                            switch(samepath){
                                case 0x1:  // Left
                                    printf(" + ");
                                    break;
                                case 0x2: // Right
                                    printf(" + ");
                                    break;
                                case 0x3: // Left-Right
                                    printf("---");
                                    break;
                                case 0x4: // Above
                                    printf(" + ");
                                    break;
                                case 0x5: // Left - Above
                                    printf("-' ");
                                    break;
                                case 0x6: // Right - Abobe
                                    printf(" '-");
                                    break;
                                case 0x8: // Below
                                    printf(" + ");
                                    break;
                                case 0x9: // Left - Below
                                    printf("-. ");
                                    break;
                                case 0xA: // Right - Below
                                    printf(" .-");
                                    break;
                                case 0xC: // Above - Below
                                    printf(" | ");
                                    break;
                                default:
                                    fprintf(stderr, "Error drawing the grid\n");
                                    return;
                            }
                            Color_Reset;
                        }
                    } else { // Empty cell
                        printf("   ");
                    }
                }
            }
            printf("|\033[%d;%dH", print_y++, print_x);
        }
    }
    for (x = 0; x < Width; x++){
        printf("+---");
    }
    printf("+\033[%d;%dH", PrintPoint_y + y_end + 2, PrintPoint_x);
}

void ClearGrid(){
    int x, y,  // x and y coordinate
    x_end = Width,
    y_end = Height*2 + 1,
    print_x = PrintPoint_x+1, //x and y coordinates of the print point
    print_y = PrintPoint_y+1;
    printf("\033[%d;%dH", print_y++, print_x); //Move the cursor to 0; 0
    for (y = 0; y < y_end; y++){
        for (x = 0; x < x_end; x++)
            printf("    "); //Print spaces instead of the grid
        printf(" \033[%d;%dH", print_y++, print_x);
    }
    PrintPoint_x -= (Width * 4 + 5);
}
#undef PrintColor
#undef Color_Reset

int CheckGame(){
    int x, y;
    for (y = 0; y < Height; y++)
        for (x = 0; x < Width; x++)
            if (IsEmpty(GameCell(x, y)))
                return 0; //No solution was found
    //if no Cell was empty
    PrintGrid();
    PrintPoint_x += Width * 4 + 5;
    return 1; //a solution found!
}

#ifdef ANIMATION
void Wait(int millisecs){
    int end = clock() + millisecs*CLOCKS_PER_SEC/1000;
    while (clock() < end){}
}
#endif

int FloodFill(int px, int py, int endpx, int endpy){
    if (GET_FLOOD_FILL_DATA(px, py)) //Cell must be void, otherwise the algorithm will be too long
        return 0;
    SET_FLOOD_FILL_DATA(px, py, (1));
    //Above
    if (px + 1 < Width){
        if ((px+1 == endpx) && (py == endpy))
            return 1;
        if (IsEmpty(GameCell(px+1, py))){
            if (FloodFill(px+1, py, endpx, endpy)) //when we know that one point can be reached from anoteher is unuseles going on the algoritm
                return 1;
        }
    }
    //Below
    if (px > 0) {
        if ((px-1 == endpx) && (py == endpy))
            return 1;
        if (IsEmpty(GameCell(px-1, py))){
            if (FloodFill(px-1, py, endpx, endpy))
                return 1;
        }
    }
    //Right
    if (py + 1 < Height) {
        if ((px == endpx) && (py+1 == endpy))
            return 1;
        if (IsEmpty(GameCell(px, py+1))){
            if (FloodFill(px, py+1, endpx, endpy))
                return 1;
        }
    }
    //Left
    if (py > 0) {
        if ((px == endpx) && (py-1 == endpy))
            return 1;
        if (IsEmpty(GameCell(px, py-1))){
            if (FloodFill(px, py-1, endpx, endpy))
                return 1;
        }
    }
    return 0;
}

int Next(int current_x, int current_y){ //this is the most important funcion in the game
    int CurrentPathID = GameCell(current_x, current_y).Path;
    int solution = 0, tmpsolution;
#ifdef ANIMATION
    static int counter = 0;
#endif
#ifdef OPTIMIZATION /*This optimizes the algorithm avoiding it continue to solve an unsolvable position
    *The prupose of the game is to join dots with a line wich can't cross another line,
    *so if we found there is no way to connect two points of the game the function ends and returns 0
    *(no solution). to check if we are in this situation for each point we expand an area from the point
    *unless this area touch an alrelady present line, so the area expands itself only on white cells
    *like a flood fill. When it's impossible to expand this area we find if the second point is inside,
    *if not there's no way to connect them.
    */
    int op_c, op_p; //Optimizatior counter, Optimizatior point
    //Frist reset the buffer
    for (op_p = CurrentPathID; op_p < Points_Len; op_p++){
        for (op_c = 0; op_c < FloodFillData_Len; op_c++)
            FloodFillData[op_c] = 0x00;
        if(!FloodFill(Points[op_p].x_start, Points[op_p].y_start, Points[op_p].x_end, Points[op_p].y_end))
            return 0;
    }
#endif
    if (CurrentPathID == 0){
        //The start cell must not be empty
        return -1; //Exit with error
    }
#ifdef ANIMATION
    if (counter == 1000){
        PrintGrid();
        Wait(50);
        counter = 0;
    }
    counter++;
#endif
    TriedOutPaths++; //Increase the number of tried out path
    //Try to put the next piece of the path in the cell above the current one
    //Above cell
    if (current_x + 1 < Width) { //first make sure there is at least one cell above
        if (IsEmpty(GameCell(current_x + 1, current_y)) ){ //Chek if the cell is free
            //if it is we start filling it with the next piece of the path
            GameCell(current_x + 1, current_y).Path = CurrentPathID;
            GameCell(current_x + 1, current_y).ID = GameCell(current_x, current_y).ID+1;
            tmpsolution = Next(current_x + 1, current_y); //Recursive function
            if (solution < 0 || tmpsolution < 0) //if solutions contains already an error or if an error occurred in the before call to 'Next'
                solution = tmpsolution; //reset the number of solution and store the error number
            else
                solution+=tmpsolution; //Add the solutions found
            GameCell(current_x + 1, current_y).Path = 0;
            GameCell(current_x + 1, current_y).ID = 0;
        } else if ((GameCell(current_x + 1, current_y).Path == CurrentPathID) && (GameCell(current_x + 1, current_y).ID == ULONG_MAX)){
            //If it is not free, but contains the end endpoint of the path
            if (CurrentPathID < Points_Len){ //Try to go to the next path, if there are no more path the points are joined
                //another path was found
                int X_end = Points[CurrentPathID].x_end, //et some variables
                Y_end = Points[CurrentPathID].y_end;
                GameCell(current_x, current_y).Joined = 1; //The cell was joined to the endpoint
                tmpsolution = Next(Points[CurrentPathID].x_start, Points[CurrentPathID].y_start); //Recursive function
                if (solution < 0 || tmpsolution < 0)
                    solution = tmpsolution;
                else
                    solution+=tmpsolution;
                GameCell(current_x, current_y).Joined = 0;
            } else {
                //Every point was joined, check the solution (there must not be any empty cells), and then print it
                GameCell(current_x, current_y).Joined = 1;
                solution = CheckGame();
                GameCell(current_x, current_y).Joined = 0;
                return solution;
            }
        }
    }
    //Below cell
    if (current_x > 0) {
        if (IsEmpty(GameCell(current_x - 1, current_y)) ){
            GameCell(current_x - 1, current_y).Path = CurrentPathID;
            GameCell(current_x - 1, current_y).ID = GameCell(current_x, current_y).ID+1;
            tmpsolution = Next(current_x-1, current_y); //Recursive function
            if (solution < 0 || tmpsolution < 0)
                solution = tmpsolution;
            else
                solution+=tmpsolution;
            GameCell(current_x - 1, current_y).Path = 0;
            GameCell(current_x - 1, current_y).ID = 0;
        } else if ((GameCell(current_x - 1, current_y).Path == CurrentPathID) && (GameCell(current_x - 1, current_y).ID == ULONG_MAX)){
            if (CurrentPathID < Points_Len){
                int X_end = Points[CurrentPathID].x_end,
                Y_end = Points[CurrentPathID].y_end;
                GameCell(current_x, current_y).Joined = 1; //The cell was joined to the endpoint
                tmpsolution = Next(Points[CurrentPathID].x_start, Points[CurrentPathID].y_start); //Recursive function
                if (solution < 0 || tmpsolution < 0)
                    solution = tmpsolution;
                else
                    solution+=tmpsolution;
                GameCell(current_x, current_y).Joined = 0;
            } else {
                //Every point was joined, check the solution (there must not be any empty cells), and then print it
                GameCell(current_x, current_y).Joined = 1;
                solution = CheckGame();
                GameCell(current_x, current_y).Joined = 0;
                return solution;
            }
        }
    }
    //Right cell
    if (current_y + 1 < Height) {
        if (IsEmpty(GameCell(current_x, current_y + 1)) ){
            GameCell(current_x, current_y + 1).Path = CurrentPathID;
            GameCell(current_x, current_y + 1).ID = GameCell(current_x, current_y).ID+1;
            tmpsolution = Next(current_x, current_y + 1); //Recursive function
            if (solution < 0 || tmpsolution < 0)
                solution = tmpsolution;
            else
                solution+=tmpsolution;
            GameCell(current_x, current_y + 1).Path = 0;
            GameCell(current_x, current_y + 1).ID = 0;
        } else if ((GameCell(current_x, current_y + 1).Path == CurrentPathID) && (GameCell(current_x, current_y + 1).ID == ULONG_MAX)){
            if (CurrentPathID < Points_Len){
                int X_end = Points[CurrentPathID].x_end,
                Y_end = Points[CurrentPathID].y_end;
                GameCell(current_x, current_y).Joined = 1; //The cell was joined to the endpoint
                tmpsolution = Next(Points[CurrentPathID].x_start, Points[CurrentPathID].y_start); //Recursive function
                if (solution < 0 || tmpsolution < 0)
                    solution = tmpsolution;
                else
                    solution+=tmpsolution;
                GameCell(current_x, current_y).Joined = 0;
            } else {
                //Every point was joined, check the solution (there must not be any empty cells), and then print it
                GameCell(current_x, current_y).Joined = 1;
                solution = CheckGame();
                GameCell(current_x, current_y).Joined = 0;
                return solution;
            }
        }
    }
    //left cell
    if (current_y > 0) {
        if (IsEmpty(GameCell(current_x, current_y - 1)) ){
            GameCell(current_x, current_y - 1).Path = CurrentPathID;
            GameCell(current_x, current_y - 1).ID = GameCell(current_x, current_y).ID+1;
            tmpsolution = Next(current_x, current_y - 1); //Recursive function
            if (solution < 0 || tmpsolution < 0)
                solution = tmpsolution;
            else
                solution+=tmpsolution;
            GameCell(current_x, current_y - 1).Path = 0;
            GameCell(current_x, current_y - 1).ID = 0;
        } else if ((GameCell(current_x, current_y - 1).Path == CurrentPathID) && (GameCell(current_x, current_y - 1).ID == ULONG_MAX)){
            if (CurrentPathID < Points_Len){
                int X_end = Points[CurrentPathID].x_end,
                Y_end = Points[CurrentPathID].y_end;
                GameCell(current_x, current_y).Joined = 1; //The cell was joined to the endpoint
                tmpsolution = Next(Points[CurrentPathID].x_start, Points[CurrentPathID].y_start); //Recursive function
                if (solution < 0 || tmpsolution < 0)
                    solution = tmpsolution;
                else
                    solution+=tmpsolution;
                GameCell(current_x, current_y).Joined = 0;
            } else {
                //Every point was joined, check the solution (there must not be any empty cells), and then print it
                GameCell(current_x, current_y).Joined = 1;
                solution = CheckGame();
                GameCell(current_x, current_y).Joined = 0;
                return solution;
            }
        }
    }
    return solution; //it returns the number of solution or the error number
}

int Solve(){
    if (Points_Len)
        return Next(Points[0].x_start, Points[0].y_start);
    else
        return 0; //Could not solve
}

int main(int argc, char * argv[]){
    unsigned x, y;
    int solutionsfound;
    FILE * F;
    struct timeval start, end;
    gettimeofday(&start, NULL);
    //chek that the user passed at least one argument
    if (argc < 2){
        fprintf(stderr, "Error, insufficient args\nYou have to pass at least the game data file\n");
        abort();
    }
    //Open the file
    if ((F = fopen(argv[1], "r")) == NULL){ //the first argument should be the file containing the game data
        fprintf(stderr, "Error opening file\nMaybe the file does not exist\n");
        abort();
    }
    //Start reading the file
    //The first two numbers are the width and the heigt of the game grid
    if (fscanf(F, "%d %d", &Width, &Height) != 2){ //if fscanf reads less than 2 numbers there is an error in the file
        fprintf(stderr, "Error reading file, data corrupted\nI wasn't able to read the size of the game\n");
        abort();
    }
    //the third number written in the file is the number of lines
    if (fscanf(F, "%d", &Points_Len) != 1){
        fprintf(stderr, "Error reading file, data corrupted\nI wasn't able to read the number of paths\n");
        abort();
    }
    //Allocating memory for points
    if ((Points = malloc(Points_Len * sizeof(PathPoint))) == NULL){
        fprintf(stderr, "Error allocating memory\n");
        abort();
    }
    for (x = 0; x < Points_Len; x++){
        //reading the coordinates of the endpoints of the paths
        if (fscanf(F, "%d\t%d\t%d\t%d", &(Points[x].x_start), &(Points[x].y_start), &(Points[x].x_end), &(Points[x].y_end)) != 4) {
            fprintf(stderr, "Error reading file, data corrupted\nI wasn't able to read one or more of the coordinates of the endpoints of the path %d\n", x+1);
            free(Points);
            abort();
        }
        //Checking thath the endpoins are inside the grid size
        if ((Points[x].x_start >= Width) || (Points[x].x_end >= Width) || (Points[x].y_start >= Height) || (Points[x].x_end >= Height)){
            fprintf(stderr, "Error: data conflict\nThe endpoints of the path %d must be inside the game grid, check th grid's size\n", x+1);
            free(Points);
            abort();
        }
        //Cheking that the endpoints of the pats are differents
        if ((Points[x].x_start == Points[x].x_end) && (Points[x].y_start == Points[x].y_end)){
            fprintf(stderr, "Error: data conflict\nThe two endpoints of the path %d must be differents\n", x+1);
            free(Points);
            abort();
        }
        //Checking that no endpoints are in the same cell
        for (y = 0; y < x; y++){
            if (((Points[x].x_start == Points[y].x_start) && (Points[x].y_start == Points[y].y_start)) ||
            ((Points[x].x_end == Points[y].x_end)     && (Points[x].y_end == Points[y].y_end))	  ||
            ((Points[x].x_end == Points[y].x_start)   && (Points[x].y_end == Points[y].y_start))   ||
            ((Points[x].x_start == Points[y].x_end)   && (Points[x].y_start == Points[y].y_end)) ) {
                fprintf(stderr, "Error: data conflict\nThe endpoints of the path %d must be different from the path %d's one\n", x+1, y+1);
                free(Points);
                abort();
            }
        }
    }
    //Finish reading the file!

    //Initialiting matrix
    GameGrid = malloc(Width*Height*sizeof(Cell));
    //Cause of there can be different size game it's better to use a multidimensional array,
    //but the code to manage a multidimensional dynamic array is to long and difficoult, so the easiest
    //way to do so is using a monodimensional dynamic array

    FloodFillData_Len = ceil(Width*Height/8.0);
    FloodFillData = malloc(FloodFillData_Len);

    //Reset the GameGrid Array
    //The GameGrid Array is mono-dimensional, but it is used as a multidimensional array
    //The reset of the whole array it is faster with a single cycle instead of two nested cycles
    for (x = 0; x < Width*Height; x++){
        GameGrid[x].Path = GameGrid[x].Joined = GameGrid[x].ID = 0;
    }
    //For a description of the format of the grid's cell go where I defined the struct _Cell
    for (x = 0; x < Points_Len; x++){
        GameCell(Points[x].x_start, Points[x].y_start).Path = (x+1);
        GameCell(Points[x].x_start, Points[x].y_start).ID   = 0;
        GameCell(Points[x].x_end  , Points[x].y_end  ).Path = (x+1);
        GameCell(Points[x].x_end  , Points[x].y_end  ).ID   = ULONG_MAX;
    }
#ifdef RANDOM_COLOR
    srand(time(NULL));  //Starts the random generator
    Randq = rand();
    Randm = rand();
#endif
    PrintPoint_x = 1;
    PrintPoint_y = 1; //Prints the upper left corner of the grid in the point (1; 1)
    printf("\033[2J"); //Clear the screen
    solutionsfound = Solve(); //Solves the game
    ClearGrid();
    if (solutionsfound < 0) {
        fprintf(stderr, "Error solving the game\n");
        free(GameGrid);
        free(Points);
        exit(-1);
    } else {
        gettimeofday(&end, NULL);
        printf("\n\n.----------.\n| Success! |   Time taken: %.6f seconds\n'----------'   Tried out %llu paths\n", ((end.tv_sec + (double)end.tv_usec/1e6) - (start.tv_sec + (double)start.tv_usec/1e6)), TriedOutPaths);
        if (solutionsfound == 0)
            printf("Were found no solutions :(\n");
        else if (solutionsfound == 1)
            printf("Were found only one solution :)\n");
        else
            printf("Were found %d solutions :)\n", solutionsfound);
    }
    free(FloodFillData);
    free(GameGrid);
    free(Points);
    return 0;
}
