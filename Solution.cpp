#ifndef Solution_h
#define Solution_h
#include "API.h"
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

class API;

class Solution {
#define MAP_SIZE 30
#define UNSEEN 0
#define EMPTY 1
#define UNKNOWN 2 // wall or spider
#define WALL 3
#define SPIDER 4

  int heading = 0;
  int x = MAP_SIZE/2;
  int y = MAP_SIZE/2;
  int map[MAP_SIZE][MAP_SIZE] = {}; // possible values: UNSEEN, EMPTY, UNKNOWN, WALL
  int spiderCount; //how many known spiders are seen currently
  int spiderState[4]; // for every dirLocal: UNKNOWN, WALL, SPIDER
  int spiderX[4]; //for every dirLocal: -1: no spider for certain, 0-MAP_SIZE: coord for spider
  int spiderY[4]; //for every dirLocal: -1: no spider for certain, 0-MAP_SIZE: coord for spider
 public:
  Solution() {
    // If you need initialization code, you can write it here!
    // Do not remove.
  }
  
  int wrap(int i){
    return (i % 4 + 4) % 4;
  }
    
  void turnRight(){
    API::turnRight();
    heading--;
    heading = wrap(heading);
  }
  
  void turnLeft(){
    API::turnLeft();
    heading++;
    heading = wrap(heading);
  }
  
  void turnToDirLocal(int dirLocal){
    switch(dirLocal) {
      case 1 : turnLeft(); break;
      case 2 : turnLeft(); break;
      case 3 : turnRight(); break;
      default : cout << "incorrect dir in turnToDirLocal" << endl; break;
    }
  }
  
  void moveForward(){
    API::moveForward();
    map[x][y] = EMPTY;//previous pos is now empty space
    getXY(heading, 1, &x, &y);//update position
  }
  
  bool blockF(){
    return API::lidarFront() <= 1;
  }
  
  bool blockB(){
    return API::lidarBack() <= 1;
  }
  
  bool blockR(){
    return API::lidarRight() <= 1;
  }
  
  bool blockL(){
    return API::lidarLeft() <= 1;
  }
  
  bool coinToss(float probabilityTrue){
    int temp = rand() % 100000;
    return (temp < ((int)probabilityTrue*100000));
  }
  
  //total number of turns required to reach direction
  int dir2TurnCount(int dir){
    switch(dir) {
      case 0 : return 0;
      case 1 : return 1;
      case 2 : return 2;
      case 3 : return 1;
      default : 
        cout << "incorrect dir in dir2TurnCount" << endl;
        return 0;//error
    }
  }

  /**
   * Executes a single step of the tank's programming. The tank can only move, 
   * turn, or fire its cannon once per turn. Between each update, the tank's 
   * engine remains running and consumes 1 fuel. This function will be called 
   * repeatedly until there are no more targets left on the grid, or the tank 
   * runs out of fuel.
   */
  void update(){
    updateMap();
    drawMap();

    //prio 1: shot/turn towards nearest spider //TODO unless nearer unexplored dir?
    //prio 2: turn to nearest unexplored dir //TODO skip sometimes if far away?
    //prio 3: drive to random empty space //TODO to direction which would reveal the most unexplored directions
    
    int nearestSpiderDirLocal = 0;
    int nearestSpiderDist = MAP_SIZE;
    for (int dirLocal = 0; dirLocal < 4; dirLocal++) {
      if(spiderState[dirLocal] == SPIDER){
        int tempDist = lidarLocal(dirLocal) + dir2TurnCount(dirLocal);
        if(tempDist < nearestSpiderDist){
          nearestSpiderDirLocal = dirLocal;
          nearestSpiderDist = tempDist;
        }
      }
    }
    
    int nearestUnknownDirLocal = 0;
    int nearestUnknownDist = MAP_SIZE;
    for (int dirLocal = 0; dirLocal < 4; dirLocal++) {
      if(spiderState[dirLocal] == UNKNOWN){
        int tempDist = lidarLocal(dirLocal) + dir2TurnCount(dirLocal);
        if(tempDist < nearestUnknownDist){
          nearestUnknownDirLocal = dirLocal;
          nearestUnknownDist = tempDist;
        }
      }
    }
    
    cout << "Heading: " << heading << endl;
    cout << "Fuel: " << API::currentFuel() << endl;
    cout << "nearestSpiderDirLocal: " << nearestSpiderDirLocal << " nearestSpiderDist: " << nearestSpiderDist << endl;
    cout << "nearestUnknownDirLocal: " << nearestUnknownDirLocal << " nearestUnknownDist: " << nearestUnknownDist << endl;

    if(nearestSpiderDist < MAP_SIZE && (nearestSpiderDist <= nearestUnknownDist || nearestUnknownDist > 4)){
      if(nearestSpiderDirLocal == 0){
        API::fireCannon();
        cout << "Action: Fire" << endl;
      }else{
        turnToDirLocal(nearestSpiderDirLocal);
        cout << "Action: Turn to spider" << endl;
      }
    }else if(nearestUnknownDist <= 6){
      turnToDirLocal(nearestUnknownDirLocal);
      cout << "Action: Turn to unknown" << endl;
    }else{
      moveRandom();
      cout << "Action: Random walk" << endl;

    }
  }
  
  void updateMap(){
	// possible values for map: UNSEEN, EMPTY, UNKNOWN, WALL
	// possible values for spiderState:        UNKNOWN, WALL, SPIDER
    int measX;
    int measY;
    spiderCount = 0;
    for (int dirWorld = 0; dirWorld < 4; dirWorld++) {
      int range = lidarWorld(dirWorld);
      for (int i = 1; i < range; i++) {//fill empty space
        getXY(dirWorld, i, &measX, &measY);
        if(isInMap(measX, measY)){
          map[measX][measY] = EMPTY;
        }
      }
      getXY(dirWorld, range, &measX, &measY);
	  if(isInMap(measX, measY)){
		if(dirWorld == heading){//forward lidar
          if(API::identifyTarget()){//spider only temporarily occupies empty space, therefore empty space
            map[measX][measY] = EMPTY;
          }else{//wall
            map[measX][measY] = WALL;
          }
        }else{
			switch(map[measX][measY]){
				case UNSEEN: map[measX][measY] = UNKNOWN; break;
				case EMPTY: break; // previously empty, now occupied -> spider!
				case UNKNOWN: break; //remain unknown
				case WALL: break;//remain wall
				default: break;//error
				
			}
		}
      }
      int dirLocal = dirWorld2dirLocal(dirWorld);
      switch(map[measX][measY]){
        case UNKNOWN : spiderState[dirLocal] = UNKNOWN; break;
        case EMPTY : spiderState[dirLocal] = SPIDER; break; //empty <-> spider
        case WALL : spiderState[dirLocal] = WALL; break;
        default : //error
			spiderState[dirLocal] = UNKNOWN;
			cout << "incorrect map element" << endl;
        break;
      }
      if(map[measX][measY] == EMPTY){
        spiderCount++;
        spiderX[dirLocal] = measX;
        spiderY[dirLocal] = measY;
      }else{
        spiderX[dirLocal] = -1;//no spider
        spiderY[dirLocal] = -1;//no spider
      }
    }
  }
  
  bool isInMap(int x, int y){
    return x >= 0 && x < MAP_SIZE && y >= 0 && y < MAP_SIZE;
  }
  
  void drawMap(){
    for (int j = 0; j < MAP_SIZE; j++) {
      cout << '_';
    }
    cout << endl;
      
    for (int j = (MAP_SIZE - 1); j >= 0; j--) {//y
      for (int i = 0; i < MAP_SIZE; i++) {//x
        bool tileHasSpider = false;
        for (int n = 0; n < 4; n++) {
          if(spiderX[n] == i && spiderY[n] == j){
            tileHasSpider = true;
            break;
          }
        }
        char c = 'e';// if 'e' is shown, something went wrong
        if(tileHasSpider){//spider
          c = 'O';
        }else if(x==i && y == j){//robot
            switch(heading) {
            case 0 : c = '>'; break;
            case 1 : c = '^'; break;
            case 2 : c = '<'; break;
            case 3 : c = 'v'; break;
            default : 
            c = 'Q'; 
            cout << "incorrect heading" << endl;
            break;//error
          }
        }else{
          switch(map[i][j]){
            case UNSEEN : c = '-'; break;
            case EMPTY : c = ' '; break;
			case UNKNOWN : c = '?'; break;
            case WALL : c = '#'; break;
            default : 
            c = 'A'; 
            cout << "incorrect map element" << endl;
            break;//error
          }
        }
        cout << c;
      }
      cout << endl;
    }
    for (int j = 0; j < MAP_SIZE; j++) {
      cout << '_';
    }
    cout << endl;
  }
  
  //will return x and y coordinate relative robot
  void getXY(int dirWorld, int dist, int *xPtr, int *yPtr){
    *xPtr = x;
    *yPtr = y;
    switch(dirWorld) {
      case 0 : *xPtr += dist; break;
      case 1 : *yPtr += dist; break;
      case 2 : *xPtr -= dist; break;
      case 3 : *yPtr -= dist; break;
      default : 
        cout << "incorrect dir in getXY" << endl;
        return;//error
    }
  }

  //returns range for lidar in dirWorld direction
  int lidarWorld(int dirWorld){
    return lidarLocal(dirWorld2dirLocal(dirWorld));
  }
  
  //returns range for lidar in dirWorld direction
  int lidarLocal(int dirLocal){
    switch(dirLocal) {
      case 0 : return API::lidarFront();
      case 1 : return API::lidarLeft();
      case 2 : return API::lidarBack();
      case 3 : return API::lidarRight();
      default : 
        cout << "incorrect dirWorld in lidarWorld" << endl;
        return 0;//error
    }
  }
  
  int dirWorld2dirLocal(int dirWorld){
    return wrap(dirWorld - heading);
  }
  
  void moveRandom(){
    if(blockF() && blockB() && blockR() && blockL()){//nowhere to move
      turnRight();
    }else if(blockF() && blockB() && blockL()){//only free right
      turnRight();
    }else if(blockF() && blockB() && blockR()){//only free left
      turnLeft();
    }else if(blockF() && blockR() && blockL()){//only free backwards
      turnRight();
    }else{//multiple free directions
      int temp = rand() % 8;
      switch(temp) {
      case 0 :  if(blockL()){
                  moveRandom();
                }else{
                  turnLeft();
                }
              break;
      case 1 :  if(blockR()){
                  moveRandom();
                }else{
                  turnRight();
                }
              break;
      default : 
              if(blockF()){
                moveRandom();
              }else{
                moveForward();
              }
      }
    }
  }
};

#endif
