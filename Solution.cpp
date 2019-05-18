#ifndef Solution_h
#define Solution_h
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include "API.h"

class API;

class Solution {
#define DIRS 4
#define FORWARD 0
#define LEFT 1
#define BACKWARD 2
#define RIGHT 3

#define MAP_SIZE 30
#define UNSEEN 0
#define EMPTY 1
#define UNKNOWN 2  // wall or enemy
#define WALL 3
#define ENEMY 4

#define STAND_STILL 0
#define MOVE_FORWARD 1
#define MOVE_BACKWARD 2
#define TURN_RIGHT 3
#define TURN_LEFT 4

  int robotHeading = 0;
  int robotX = MAP_SIZE / 2;
  int robotY = MAP_SIZE / 2;
  int map[MAP_SIZE][MAP_SIZE] =
      {};  // possible values: UNSEEN, EMPTY, UNKNOWN, WALL
  char canvas[MAP_SIZE][MAP_SIZE] = {};
  int enemyCount;
  int enemyState[DIRS];  // possible values: UNKNOWN, WALL, ENEMY
  int enemyX[DIRS];  // -1: no enemy for certain, 0-MAP_SIZE: coord for enemy
  int enemyY[DIRS];  // -1: no enemy for certain, 0-MAP_SIZE: coord for enemy
 public:
  Solution() {
    // If you need initialization code, you can write it here!
    // Do not remove.
  }

  int wrap(int i) { return (i % DIRS + DIRS) % DIRS; }

  void moveRobot(int action) {
    switch (action) {
      case STAND_STILL:
        // do nothing
        break;
      case MOVE_FORWARD:
        moveForward();
        break;
      case MOVE_BACKWARD:
        moveBackward();
        break;
      case TURN_RIGHT:
        turnRight();
        break;
      case TURN_LEFT:
        turnLeft();
        break;
      default:
        // do nothing
        break;
    }
  }

  void turnRight() {
    API::turnRight();
    robotHeading--;
    robotHeading = wrap(robotHeading);
  }

  void turnLeft() {
    API::turnLeft();
    robotHeading++;
    robotHeading = wrap(robotHeading);
  }

  void turnToDirLocal(int dirLocal) {
    switch (dirLocal) {
      case LEFT:
        turnLeft();
        break;
      case BACKWARD:
        turnLeft();
        break;
      case RIGHT:
        turnRight();
        break;
      default:
        cout << "incorrect dir in turnToDirLocal" << endl;
        break;
    }
  }

  void moveForward() {
    if (API::lidarFront() > 1) {
      API::moveForward();
      addXY(robotHeading, 1, &robotX, &robotY);  // update position
    } else {
      cout << "TRIED TO GO INTO A WALL" << endl;
    }
  }

  void moveBackward() {
    if (API::lidarBack() > 1) {
      API::moveBackward();
      addXY(robotHeading, -1, &robotX, &robotY);  // update position
    } else {
      cout << "TRIED TO GO INTO A WALL" << endl;
    }
  }

  bool coinToss(float probabilityTrue) {
    int temp = rand() % 100000;
    return (temp < ((int)probabilityTrue * 100000));
  }

  // total number of turns required to reach direction
  int dir2TurnCount(int dir) {
    switch (dir) {
      case FORWARD:
        return 0;
      case LEFT:
        return 1;
      case BACKWARD:
        return 2;
      case RIGHT:
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
    for (int dirLocal = 0; dirLocal < DIRS; dirLocal++) {
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
    for (int dirLocal = 0; dirLocal < DIRS; dirLocal++) {
      if (enemyState[dirLocal] == UNKNOWN) {
        int tempDist = lidarLocal(dirLocal) + dir2TurnCount(dirLocal);
        if (tempDist < nearestUnknownDist) {
          nearestUnknownDirLocal = dirLocal;
          nearestUnknownDist = tempDist;
        }
      }
    }

    cout << "Heading: " << robotHeading << endl;
    cout << "Fuel: " << API::currentFuel() << endl;
    cout << "nearestEnemyDirLocal: " << nearestEnemyDirLocal
         << " nearestEnemyDist: " << nearestEnemyDist << endl;
    cout << "nearestUnknownDirLocal: " << nearestUnknownDirLocal
         << " nearestUnknownDist: " << nearestUnknownDist << endl;

    bool hasThreats = (nearestEnemyDist < MAP_SIZE) || (nearestUnknownDist < 3);
    bool prioritizeEnemies = (nearestEnemyDist <= (nearestUnknownDist + 2));
    if (hasThreats) {
      if (prioritizeEnemies) {
        if (nearestEnemyDirLocal == FORWARD) {  // enemy in front
          if (nearestEnemyDist <= 3) {    // close enough to shoot
            API::fireCannon();
            cout << "Action: Fire" << endl;
          } else {
            moveForward();
            cout << "Action: Enemy ahead" << endl;
          }
        } else {
          turnToDirLocal(nearestEnemyDirLocal);
          cout << "Action: Turn to enemy" << endl;
        }
      } else {
        turnToDirLocal(nearestUnknownDirLocal);
        cout << "Action: Turn to unknown" << endl;
      }
    } else {
#define MOVES_NUM 15
#define CANDIDATES_NUM 1000
      int pathCandidates[MOVES_NUM][CANDIDATES_NUM];  // 5 moves, 10 candidates
      int bestScore = 0;
      int bestFirstMove = STAND_STILL;
      int candidateMap[MAP_SIZE][MAP_SIZE];

      for (int i = 0; i < CANDIDATES_NUM; i++) {
        // reset candidate map
        for (int n = 0; n < MAP_SIZE; n++) {    // x
          for (int m = 0; m < MAP_SIZE; m++) {  // y
            candidateMap[n][m] = map[n][m];
          }
        }
        int candidateX = robotX;
        int candidateY = robotY;
        int candidateHeading = robotHeading;
        int candidateFirstMove;
        int candidateScore = 0;
        for (int j = 0; j < MOVES_NUM; j++) {
          // execute move
          int move = chooseMove(candidateHeading, candidateX, candidateY);
          if (j == 0) {
            candidateFirstMove = move;
          }
          switch (move) {
            case MOVE_FORWARD:
              addXY(candidateHeading, 1, &candidateX, &candidateY);
              break;
            case TURN_RIGHT:
              candidateHeading--;
              candidateHeading = wrap(candidateHeading);
              break;
            case TURN_LEFT:
              candidateHeading++;
              candidateHeading = wrap(candidateHeading);
              break;
          }
          // evaulate position
          for (int dirLocal = 0; dirLocal < DIRS; dirLocal++) {
            bool forward = dirLocal == FORWARD;
            // seek with lidar
            int lidarX = candidateX;
            int lidarY = candidateY;
            int dist = 0;
            bool done = false;
            while (!done) {
              addXY(dirLocal2dirWorld(candidateHeading, dirLocal), 1, &lidarX,
                    &lidarY);
              dist++;
              if (!isInMap(lidarX, lidarY)) {
                done = true;
              } else {
                switch (candidateMap[lidarX][lidarY]) {
                  case UNSEEN:
                    if (forward) {
                      candidateMap[lidarX][lidarY] = WALL;
                      candidateScore += 10;
                    } else {
                      candidateMap[lidarX][lidarY] = UNKNOWN;
                      candidateScore += 7;
                      if (dist == 1) {
                        candidateScore -=
                            6;  // penalty for going close to unknown stuff
                      }
                    }
                    done = true;
                    break;
                  case EMPTY:
                    break;
                  case UNKNOWN:
                    if (forward) {
                      candidateMap[lidarX][lidarY] = WALL;
                      candidateScore += 3;
                    }
                    if (dist == 1) {
                      candidateScore -=
                          1;  // penalty for going close to unknown stuff
                    }
                    done = true;
                    break;
                    break;
                  case WALL:
                    done = true;
                    break;
                  default:  // error
                    break;
                }
              }
            }
          }
          if (candidateScore > bestScore) {
            bestScore = candidateScore;
            bestFirstMove = candidateFirstMove;
          }
        }
      }
      if (bestScore > 0) {
        moveRobot(bestFirstMove);
      } else {
        moveRobot(chooseMove(robotHeading, robotX, robotY));
      }

      cout << "Action: Random walk" << endl;
    }
  }

  void updateMap() {
    // possible values for map: UNSEEN, EMPTY, UNKNOWN, WALL
    // possible values for enemyState:        UNKNOWN, WALL, ENEMY
    map[robotX][robotY] = EMPTY;  // robot pos is always empty space
    int measX;
    int measY;
    enemyCount = 0;
    for (int dirWorld = 0; dirWorld < DIRS; dirWorld++) {
      int range = lidarWorld(dirWorld);
      for (int i = 1; i < range; i++) {  // fill empty space
        measX = robotX;
        measY = robotY;
        addXY(dirWorld, i, &measX, &measY);
        if (isInMap(measX, measY)) {
          map[measX][measY] = EMPTY;
        }
      }
      measX = robotX;
      measY = robotY;
      addXY(dirWorld, range, &measX, &measY);
      if (isInMap(measX, measY)) {
        if (dirWorld == robotHeading) {  // forward lidar
          if (API::identifyTarget()) {   // enemy only temporarily occupies
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
      int dirLocal = dirWorld2dirLocal(robotHeading, dirWorld);
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
      switch (robotHeading) {
        case FORWARD:
          c = '>';
          break;
        case LEFT:
          c = '^';
          break;
        case BACKWARD:
          c = '<';
          break;
        case RIGHT:
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
    for (int i = 0; i < DIRS; i++) {
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

  // will add dist in dir to xy coordinate
  void addXY(int dir, int dist, int* xPtr, int* yPtr) {
    switch (dir) {
      case FORWARD:
        *xPtr += dist;
        break;
      case LEFT:
        *yPtr += dist;
        break;
      case BACKWARD:
        *xPtr -= dist;
        break;
      case RIGHT:
        *yPtr -= dist;
        break;
      default:
        cout << "incorrect dir in getXY" << endl;
        return;  // error
    }
  }

  // returns range for lidar in dirWorld direction
  int lidarWorld(int dirWorld) {
    return lidarLocal(dirWorld2dirLocal(robotHeading, dirWorld));
  }

  // returns range for lidar in dirWorld direction
  int lidarLocal(int dirLocal) {
    switch (dirLocal) {
      case FORWARD:
        return API::lidarFront();
      case LEFT:
        return API::lidarLeft();
      case BACKWARD:
        return API::lidarBack();
      case RIGHT:
        return API::lidarRight();
      default:
        cout << "incorrect dirWorld in lidarWorld" << endl;
        return 0;  // error
    }
  }

  // dirLocal = dirWorld - heading
  int dirWorld2dirLocal(int heading, int dirWorld) {
    return wrap(dirWorld - heading);
  }

  // dirWorld = dirLocal + heading
  int dirLocal2dirWorld(int heading, int dirLocal) {
    return wrap(dirLocal + heading);
  }

  bool freeToMove(int dirLocal, int heading, int x, int y) {
    addXY(dirLocal2dirWorld(heading, dirLocal), 1, &x, &y);
    if (isInMap(x, y)) {
      switch (map[x][y]) {
        case UNSEEN:
        case EMPTY:
          return true;
          break;
        case UNKNOWN:
        case WALL:
          return false;
          break;
        default:
          cout << "incorrect map element in  in blockF" << endl;
          return false;
          break;
      }
    } else {
      return false;
    }
  }

  int chooseMove(int heading, int x, int y) {
    int temp = rand() % 6;
    switch (temp) {
      case 0:
        return TURN_LEFT;
        break;
      case 1:
        return TURN_RIGHT;
        break;
      case 2:
        if (freeToMove(BACKWARD, heading, x, y)) {
          return MOVE_BACKWARD;
        } else {
          return chooseMove(heading, x, y);
        }
        break;
      default:
        if (freeToMove(FORWARD, heading, x, y)) {
          return MOVE_FORWARD;
        } else {
          return chooseMove(heading, x, y);
        }
        break;
    }
  }
};

#endif
