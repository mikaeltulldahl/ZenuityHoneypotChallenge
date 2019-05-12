#ifndef Solution_h
#define Solution_h
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include "API.h"

class API;

class Solution {
#define MAP_SIZE 30
#define UNSEEN 0
#define EMPTY 1
#define UNKNOWN 2  // wall or enemy
#define WALL 3
#define ENEMY 4

  int heading = 0;
  int robotX = MAP_SIZE / 2;
  int robotY = MAP_SIZE / 2;
  int map[MAP_SIZE][MAP_SIZE] =
      {};  // possible values: UNSEEN, EMPTY, UNKNOWN, WALL
  char canvas[MAP_SIZE][MAP_SIZE] = {};
  int enemyCount;
  int enemyState[4];  // possible values: UNKNOWN, WALL, ENEMY
  int enemyX[4];      // -1: no enemy for certain, 0-MAP_SIZE: coord for enemy
  int enemyY[4];      // -1: no enemy for certain, 0-MAP_SIZE: coord for enemy
 public:
  Solution() {
    // If you need initialization code, you can write it here!
    // Do not remove.
  }

  int wrap(int i) { return (i % 4 + 4) % 4; }

  void turnRight() {
    API::turnRight();
    heading--;
    heading = wrap(heading);
  }

  void turnLeft() {
    API::turnLeft();
    heading++;
    heading = wrap(heading);
  }

  void turnToDirLocal(int dirLocal) {
    switch (dirLocal) {
      case 1:
        turnLeft();
        break;
      case 2:
        turnLeft();
        break;
      case 3:
        turnRight();
        break;
      default:
        cout << "incorrect dir in turnToDirLocal" << endl;
        break;
    }
  }

  void moveForward() {
    API::moveForward();
    map[robotX][robotY] = EMPTY;          // previous pos is now empty space
    getXY(heading, 1, &robotX, &robotY);  // update position
  }

  bool blockF() { return API::lidarFront() <= 1; }

  bool blockB() { return API::lidarBack() <= 1; }

  bool blockR() { return API::lidarRight() <= 1; }

  bool blockL() { return API::lidarLeft() <= 1; }

  bool coinToss(float probabilityTrue) {
    int temp = rand() % 100000;
    return (temp < ((int)probabilityTrue * 100000));
  }

  // total number of turns required to reach direction
  int dir2TurnCount(int dir) {
    switch (dir) {
      case 0:
        return 0;
      case 1:
        return 1;
      case 2:
        return 2;
      case 3:
        return 1;
      default:
        cout << "incorrect dir in dir2TurnCount" << endl;
        return 0;  // error
    }
  }

  /**
   * Executes a single step of the tank's programming. The tank can only move,
   * turn, or fire its cannon once per turn. Between each update, the tank's
   * engine remains running and consumes 1 fuel. This function will be called
   * repeatedly until there are no more targets left on the grid, or the tank
   * runs out of fuel.
   */
  void update() {
    updateMap();
    plotMapOnCanvas();
    plotRobotOnCanvas();
    plotEnemiesOnCanvas();
    drawCanvas();

    // prio 1: shot/turn towards nearest enemy
    // prio 2: turn to nearest unexplored dir
    // prio 3: drive to random empty space
    // TODO skip prio 1 if nearer unexplored dir?
    // TODO skip prio 2 sometimes if far away?
    // TODO to direction which would reveal the most unexplored directions

    int nearestEnemyDirLocal = 0;
    int nearestEnemyDist = MAP_SIZE;
    for (int dirLocal = 0; dirLocal < 4; dirLocal++) {
      if (enemyState[dirLocal] == ENEMY) {
        int tempDist = lidarLocal(dirLocal) + dir2TurnCount(dirLocal);
        if (tempDist < nearestEnemyDist) {
          nearestEnemyDirLocal = dirLocal;
          nearestEnemyDist = tempDist;
        }
      }
    }

    int nearestUnknownDirLocal = 0;
    int nearestUnknownDist = MAP_SIZE;
    for (int dirLocal = 0; dirLocal < 4; dirLocal++) {
      if (enemyState[dirLocal] == UNKNOWN) {
        int tempDist = lidarLocal(dirLocal) + dir2TurnCount(dirLocal);
        if (tempDist < nearestUnknownDist) {
          nearestUnknownDirLocal = dirLocal;
          nearestUnknownDist = tempDist;
        }
      }
    }

    cout << "Heading: " << heading << endl;
    cout << "Fuel: " << API::currentFuel() << endl;
    cout << "nearestEnemyDirLocal: " << nearestEnemyDirLocal
         << " nearestEnemyDist: " << nearestEnemyDist << endl;
    cout << "nearestUnknownDirLocal: " << nearestUnknownDirLocal
         << " nearestUnknownDist: " << nearestUnknownDist << endl;

    if (nearestEnemyDist < MAP_SIZE &&
        (nearestEnemyDist <= nearestUnknownDist || nearestUnknownDist > 4)) {
      if (nearestEnemyDirLocal == 0) {
        API::fireCannon();
        cout << "Action: Fire" << endl;
      } else {
        turnToDirLocal(nearestEnemyDirLocal);
        cout << "Action: Turn to enemy" << endl;
      }
    } else if (nearestUnknownDist <= 6) {
      turnToDirLocal(nearestUnknownDirLocal);
      cout << "Action: Turn to unknown" << endl;
    } else {
      moveRandom();
      cout << "Action: Random walk" << endl;
    }
  }

  void updateMap() {
    // possible values for map: UNSEEN, EMPTY, UNKNOWN, WALL
    // possible values for enemyState:        UNKNOWN, WALL, ENEMY
    int measX;
    int measY;
    enemyCount = 0;
    for (int dirWorld = 0; dirWorld < 4; dirWorld++) {
      int range = lidarWorld(dirWorld);
      for (int i = 1; i < range; i++) {  // fill empty space
        getXY(dirWorld, i, &measX, &measY);
        if (isInMap(measX, measY)) {
          map[measX][measY] = EMPTY;
        }
      }
      getXY(dirWorld, range, &measX, &measY);
      if (isInMap(measX, measY)) {
        if (dirWorld == heading) {      // forward lidar
          if (API::identifyTarget()) {  // enemy only temporarily occupies
                                        // empty space, therefore empty space
            map[measX][measY] = EMPTY;
          } else {  // wall
            map[measX][measY] = WALL;
          }
        } else {
          switch (map[measX][measY]) {
            case UNSEEN:
              map[measX][measY] = UNKNOWN;
              break;
            case EMPTY:  // previously empty, now occupied -> enemy!
              break;
            case UNKNOWN:  // remain unknown
              break;
            case WALL:  // remain wall
              break;
            default:  // error
              break;
          }
        }
      }
      int dirLocal = dirWorld2dirLocal(dirWorld);
      switch (map[measX][measY]) {
        case UNKNOWN:
          enemyState[dirLocal] = UNKNOWN;
          break;
        case EMPTY:  // empty <-> enemy
          enemyState[dirLocal] = ENEMY;
          break;
        case WALL:
          enemyState[dirLocal] = WALL;
          break;
        default:  // error
          enemyState[dirLocal] = UNKNOWN;
          cout << "incorrect map element" << endl;
          break;
      }
      if (map[measX][measY] == EMPTY) {
        enemyCount++;
        enemyX[dirLocal] = measX;
        enemyY[dirLocal] = measY;
      } else {
        enemyX[dirLocal] = -1;  // no enemy
        enemyY[dirLocal] = -1;  // no enemy
      }
    }
  }

  bool isInMap(int x, int y) {
    return x >= 0 && x < MAP_SIZE && y >= 0 && y < MAP_SIZE;
  }

  void plotMapOnCanvas() {
    for (int i = 0; i < MAP_SIZE; i++) {    // x
      for (int j = 0; j < MAP_SIZE; j++) {  // y
        char c;
        switch (map[i][j]) {
          case UNSEEN:
            c = '-';
            break;
          case EMPTY:
            c = ' ';
            break;
          case UNKNOWN:
            c = '?';
            break;
          case WALL:
            c = '#';
            break;
          default:
            c = 'e';
            cout << "incorrect map element" << endl;
            break;  // error
        }
        canvas[i][j] = c;
      }
    }
  }

  void plotRobotOnCanvas() {
    if (isInMap(robotX, robotY)) {
      char c;
      switch (heading) {
        case 0:
          c = '>';
          break;
        case 1:
          c = '^';
          break;
        case 2:
          c = '<';
          break;
        case 3:
          c = 'v';
          break;
        default:
          c = 'Q';
          cout << "incorrect heading" << endl;
          break;  // error
      }
      canvas[robotX][robotY] = c;
    }
  }

  void plotEnemiesOnCanvas() {
    for (int i = 0; i < 4; i++) {
      if (isInMap(enemyX[i], enemyY[i])) {
        canvas[enemyX[i]][enemyY[i]] = 'O';
      }
    }
  }

  void drawCanvas() {
    // Line above
    for (int j = 0; j < MAP_SIZE; j++) {
      cout << '_';
    }
    cout << endl;

    for (int j = (MAP_SIZE - 1); j >= 0; j--) {  // y
      for (int i = 0; i < MAP_SIZE; i++) {       // x
        cout << canvas[i][j];
      }
      cout << endl;
    }

    // Line below
    for (int j = 0; j < MAP_SIZE; j++) {
      cout << '_';
    }
    cout << endl;
  }

  // will return x and y coordinate relative robot
  void getXY(int dirWorld, int dist, int* xPtr, int* yPtr) {
    *xPtr = robotX;
    *yPtr = robotY;
    switch (dirWorld) {
      case 0:
        *xPtr += dist;
        break;
      case 1:
        *yPtr += dist;
        break;
      case 2:
        *xPtr -= dist;
        break;
      case 3:
        *yPtr -= dist;
        break;
      default:
        cout << "incorrect dir in getXY" << endl;
        return;  // error
    }
  }

  // returns range for lidar in dirWorld direction
  int lidarWorld(int dirWorld) {
    return lidarLocal(dirWorld2dirLocal(dirWorld));
  }

  // returns range for lidar in dirWorld direction
  int lidarLocal(int dirLocal) {
    switch (dirLocal) {
      case 0:
        return API::lidarFront();
      case 1:
        return API::lidarLeft();
      case 2:
        return API::lidarBack();
      case 3:
        return API::lidarRight();
      default:
        cout << "incorrect dirWorld in lidarWorld" << endl;
        return 0;  // error
    }
  }

  int dirWorld2dirLocal(int dirWorld) { return wrap(dirWorld - heading); }

  void moveRandom() {
    if (blockF() && blockB() && blockR() && blockL()) {  // nowhere to move
      turnRight();
    } else if (blockF() && blockB() && blockL()) {  // only free right
      turnRight();
    } else if (blockF() && blockB() && blockR()) {  // only free left
      turnLeft();
    } else if (blockF() && blockR() && blockL()) {  // only free backwards
      turnRight();
    } else {  // multiple free directions
      int temp = rand() % 8;
      switch (temp) {
        case 0:
          if (blockL()) {
            moveRandom();
          } else {
            turnLeft();
          }
          break;
        case 1:
          if (blockR()) {
            moveRandom();
          } else {
            turnRight();
          }
          break;
        default:
          if (blockF()) {
            moveRandom();
          } else {
            moveForward();
          }
      }
    }
  }
};

#endif
