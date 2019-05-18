public class Solution {
    static final int DIRS = 4;
    static final int FORWARD = 0;
    static final int LEFT = 1;
    static final int BACKWARD = 2;
    static final int RIGHT = 3;

    static final int MAP_SIZE = 30;
    static final int UNSEEN = 0;
    static final int EMPTY = 1;
    static final int UNKNOWN = 2; // wall or enemy
    static final int WALL = 3;
    static final int ENEMY = 4;

    public static enum Action {
        STAND_STILL, MOVE_FORWARD, MOVE_BACKWARD, TURN_RIGHT, TURN_LEFT, SHOOT;

        private Action() {
        }

        public static Action random(Pose p, int[][] map) {
            switch ((int) (Math.random() * 6.0f)) {
            case 0:
                return Action.TURN_LEFT;
            case 1:
                return Action.TURN_RIGHT;
            case 2:
                if (freeToMove(BACKWARD, p, map)) {
                    return Action.MOVE_BACKWARD;
                } else {
                    return random(p, map);
                }
            default:
                if (freeToMove(FORWARD, p, map)) {
                    return Action.MOVE_FORWARD;
                } else {
                    return random(p, map);
                }
            }
        }

        public void execute() {
            switch (this) {
            case STAND_STILL:
                // do nothing
                break;
            case MOVE_FORWARD:
                API.moveForward();
                break;
            case MOVE_BACKWARD:
                API.moveBackward();
                break;
            case TURN_RIGHT:
                API.turnRight();
                break;
            case TURN_LEFT:
                API.turnLeft();
                break;
            case SHOOT:
                API.fireCannon();
                break;
            }
        }

    }

    static final int MOVES_NUM = 15;
    static final int CANDIDATES_NUM = 1000;

    static class Pos {
        public int x;
        public int y;

        public Pos(int x, int y) {
            this.x = x;
            this.y = y;
        }

        public Pos(Pos p) {
            this.x = p.x;
            this.y = p.y;
        }

        public void set(Pos p) {
            this.x = p.x;
            this.y = p.y;
        }

        // will add dist in dir to xy coordinate
        public void addXY(int dir, int dist) {
            switch (dir) {
            case FORWARD:
                x += dist;
                break;
            case LEFT:
                y += dist;
                break;
            case BACKWARD:
                x -= dist;
                break;
            case RIGHT:
                y -= dist;
                break;
            default:
                System.out.print("incorrect dir in getXY");
                return; // error
            }
        }
    }

    static class Pose {
        Pos p;
        int h;

        public Pose(int x, int y, int heading) {
            p = new Pos(x, y);
            h = heading;
        }

        public Pose(Pose p2) {
            p = new Pos(p2.p);
            h = p2.h;
        }

        public void update(Action a) {
            switch (a) {
            case STAND_STILL:
                // do nothing
                break;
            case MOVE_FORWARD:
                p.addXY(h, 1); // update position
                break;
            case MOVE_BACKWARD:
                p.addXY(h, -1); // update position
                break;
            case TURN_RIGHT:
                h--;
                h = wrap(h);
                break;
            case TURN_LEFT:
                h++;
                h = wrap(h);
                break;
            case SHOOT:
                // do nothing
                break;
            }
        }
    }

    static class Enemy {
        Pos p;
        int dir = 0;
        Variant variant;
        boolean coolDown = false;

        public static enum Variant {
            MINE, YELLOW, PURPLE //possibly have separate yellow for row and col
        }

        public Enemy(Pos p, Variant v) {
            p = new Pos(p);
            variant = v;
        }

        public void update(Pose robot) {
            switch (variant) {
            case MINE:
                // do nothing
                break;
            case YELLOW:
            Pos p2 = new Pos(p);
                p2.addXY(dir, 1);
                //p2 == robot.p -> attack
                //p2 == wall -> change dir

                break;
            case PURPLE:

                break;
            }
        }

    }

    Pose robotPose = new Pose(MAP_SIZE / 2, MAP_SIZE / 2, 0);
    int[][] map = new int[MAP_SIZE][MAP_SIZE];// possible values: UNSEEN, EMPTY, UNKNOWN, WALL
    char[][] canvas = new char[MAP_SIZE][MAP_SIZE];

    int enemyCount;
    int[] enemyState = new int[DIRS]; // possible values: UNKNOWN, WALL, ENEMY
    Pos[] enemyPos = new Pos[DIRS]; // -1: no enemy for certain, 0-MAP_SIZE: coord for enemy

    public Solution() {
        // If you need initialization code, you can write it here!
        // Do not remove.
    }

    public void update() {
        updateMap();
        plotMapOnCanvas();
        plotRobotOnCanvas();
        plotEnemiesOnCanvas();
        drawCanvas();

        int nearestEnemyDirLocal = 0;
        int nearestEnemyDist = MAP_SIZE;
        int[] enemyDist = new int[] { -1, -1, -1, -1 };
        for (int dirLocal = 0; dirLocal < DIRS; dirLocal++) {
            if (enemyState[dirLocal] == ENEMY) {
                enemyDist[dirLocal] = lidarLocal(dirLocal);
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

        System.out.print("Heading: ");
        System.out.print(robotPose.h);
        System.out.print(" Fuel: ");
        System.out.println(API.currentFuel());
        System.out.print("nearestEnemyDirLocal: ");
        System.out.print(nearestEnemyDirLocal);
        System.out.print(" nearestEnemyDist: ");
        System.out.println(nearestEnemyDist);
        System.out.print("nearestUnknownDirLocal: ");
        System.out.print(nearestUnknownDirLocal);
        System.out.print(" nearestUnknownDist: ");
        System.out.println(nearestUnknownDist);

        boolean hasThreats = (nearestEnemyDist < MAP_SIZE) || (nearestUnknownDist < 3);
        boolean prioritizeEnemies = (nearestEnemyDist <= (nearestUnknownDist + 2));

        Action chosenAction = Action.STAND_STILL;

        if (hasThreats) {
            if (prioritizeEnemies) {
                if (nearestEnemyDirLocal == FORWARD) { // enemy in front
                    if (nearestEnemyDist <= 3) { // close enough to shoot
                        chosenAction = Action.SHOOT;
                    } else {
                        chosenAction = Action.MOVE_FORWARD;
                        System.out.print("Enemy ahead");
                    }
                } else {
                    chosenAction = turnToDirLocal(nearestEnemyDirLocal);
                    System.out.print("Turn to enemy");
                }
            } else {
                chosenAction = turnToDirLocal(nearestUnknownDirLocal);
                System.out.print("Turn to unknown");
            }
        } else {
            int bestScore = 0;
            Action bestFirstMove = Action.STAND_STILL;
            int[][] candidateMap = new int[MAP_SIZE][MAP_SIZE];

            for (int i = 0; i < CANDIDATES_NUM; i++) {
                // reset candidate map
                for (int n = 0; n < MAP_SIZE; n++) { // x
                    for (int m = 0; m < MAP_SIZE; m++) { // y
                        candidateMap[n][m] = map[n][m];
                    }
                }
                Pose candidatePose = new Pose(robotPose);
                Action candidateFirstMove = Action.TURN_RIGHT;
                int candidateScore = 0;
                for (int j = 0; j < MOVES_NUM; j++) {
                    // execute move
                    Action move = Action.random(candidatePose, candidateMap);
                    if (j == 0) {
                        candidateFirstMove = move;
                    }
                    candidatePose.update(move);
                    // evaulate position
                    for (int dirLocal = 0; dirLocal < DIRS; dirLocal++) {
                        boolean forward = dirLocal == FORWARD;
                        // seek with lidar
                        Pos lidarPos = new Pos(candidatePose.p);
                        int dist = 0;
                        boolean done = false;
                        int timeout = 0;
                        while (!done || timeout == 100) {
                            timeout++;

                            lidarPos.addXY(dirLocal2dirWorld(candidatePose.h, dirLocal), 1);
                            dist++;
                            if (!isInMap(lidarPos)) {
                                done = true;
                            } else {
                                switch (candidateMap[lidarPos.x][lidarPos.y]) {
                                case UNSEEN:
                                    if (forward) {
                                        candidateMap[lidarPos.x][lidarPos.y] = WALL;
                                        candidateScore += 10;
                                    } else {
                                        candidateMap[lidarPos.x][lidarPos.y] = UNKNOWN;
                                        candidateScore += 7;
                                        if (dist == 1) {
                                            candidateScore -= 6; // penalty for going close to unknown stuff
                                        }
                                    }
                                    done = true;
                                    break;
                                case EMPTY:
                                    break;
                                case UNKNOWN:
                                    if (forward) {
                                        candidateMap[lidarPos.x][lidarPos.y] = WALL;
                                        candidateScore += 3;
                                    }
                                    if (dist == 1) {
                                        candidateScore -= 1; // penalty for going close to unknown stuff
                                    }
                                    done = true;
                                    break;
                                case WALL:
                                    done = true;
                                    break;
                                default: // error
                                    break;
                                }
                            }
                        }
                        if (timeout == 100) {
                            System.out.println("Time out!!!!!");
                        }
                    }
                    if (candidateScore > bestScore) {
                        bestScore = candidateScore;
                        bestFirstMove = candidateFirstMove;
                    }
                }
            }
            if (bestScore > 0) {
                chosenAction = bestFirstMove;
                ;
            } else {
                chosenAction = Action.random(robotPose, map);
            }

            System.out.print("Action: Random walk");
        }
        robotPose.update(chosenAction);
        chosenAction.execute();
        System.out.print("Action: ");
        System.out.println(chosenAction.name());
    }

    public static int wrap(int i) {
        return (i % DIRS + DIRS) % DIRS;
    }

    public static Action turnToDirLocal(int dirLocal) {
        switch (dirLocal) {
        case LEFT:
            return Action.TURN_LEFT;
        case BACKWARD:
            return Action.TURN_LEFT;
        case RIGHT:
            return Action.TURN_RIGHT;
        default:
            System.out.print("incorrect dir in turnToDirLocal");
            return Action.STAND_STILL;
        }
    }

    // total number of turns required to reach direction
    public int dir2TurnCount(int dir) {
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
            // System.out.print("incorrect dir in dir2TurnCount");
            return 0; // error
        }
    }

    void updateMap() {
        // possible values for map: UNSEEN, EMPTY, UNKNOWN, WALL
        // possible values for enemyState: UNKNOWN, WALL, ENEMY
        map[robotPose.p.x][robotPose.p.y] = EMPTY; // robot pos is always empty space
        Pos measPos = new Pos(robotPose.p);
        enemyCount = 0;
        for (int dirWorld = 0; dirWorld < DIRS; dirWorld++) {
            int range = lidarWorld(dirWorld);
            for (int i = 1; i < range; i++) { // fill empty space
                measPos.set(robotPose.p);
                measPos.addXY(dirWorld, i);
                if (isInMap(measPos)) {
                    map[measPos.x][measPos.y] = EMPTY;
                }
            }
            measPos.set(robotPose.p);
            measPos.addXY(dirWorld, range);
            if (isInMap(measPos)) {
                if (dirWorld == robotPose.h) { // forward lidar
                    if (API.identifyTarget()) { // enemy only temporarily occupies
                                                // empty space, therefore empty space
                        map[measPos.x][measPos.y] = EMPTY;
                    } else { // wall
                        map[measPos.x][measPos.y] = WALL;
                    }
                } else {
                    switch (map[measPos.x][measPos.y]) {
                    case UNSEEN:
                        map[measPos.x][measPos.y] = UNKNOWN;
                        break;
                    case EMPTY: // previously empty, now occupied -> enemy!
                        break;
                    case UNKNOWN: // remain unknown
                        break;
                    case WALL: // remain wall
                        break;
                    default: // error
                        break;
                    }
                }
            }
            int dirLocal = dirWorld2dirLocal(robotPose.h, dirWorld);
            if (isInMap(measPos)) {
                switch (map[measPos.x][measPos.y]) {
                case UNKNOWN:
                    enemyState[dirLocal] = UNKNOWN;
                    break;
                case EMPTY: // empty <-> enemy
                    enemyState[dirLocal] = ENEMY;
                    break;
                case WALL:
                    enemyState[dirLocal] = WALL;
                    break;
                default: // error
                    enemyState[dirLocal] = UNKNOWN;
                    System.out.print("incorrect map element");
                    break;
                }
                if (map[measPos.x][measPos.y] == EMPTY) {
                    enemyCount++;
                    enemyPos[dirLocal] = new Pos(measPos);
                } else {
                    enemyPos[dirLocal] = null; // no enemy
                }
            } else {
                enemyState[dirLocal] = WALL;
                enemyPos[dirLocal] = null; // no enemy
            }
        }
    }

    static boolean isInMap(Pos p) {
        return p.x >= 0 && p.x < MAP_SIZE && p.y >= 0 && p.y < MAP_SIZE;
    }

    void plotMapOnCanvas() {
        for (int i = 0; i < MAP_SIZE; i++) { // x
            for (int j = 0; j < MAP_SIZE; j++) { // y
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
                    System.out.print("incorrect map element");
                    break; // error
                }
                canvas[i][j] = c;
            }
        }
    }

    void plotRobotOnCanvas() {
        if (isInMap(robotPose.p)) {
            char c;
            switch (robotPose.h) {
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
                System.out.print("incorrect heading");
                break; // error
            }
            canvas[robotPose.p.x][robotPose.p.y] = c;
        }
    }

    void plotEnemiesOnCanvas() {
        for (int i = 0; i < DIRS; i++) {
            if (enemyPos[i] != null && isInMap(enemyPos[i])) {
                canvas[enemyPos[i].x][enemyPos[i].y] = 'O';
            }
        }
    }

    void drawCanvas() {
        // Line above
        for (int j = 0; j < MAP_SIZE; j++) {
            System.out.print('_');
        }
        System.out.println();

        for (int j = (MAP_SIZE - 1); j >= 0; j--) { // y
            for (int i = 0; i < MAP_SIZE; i++) { // x
                System.out.print(canvas[i][j]);
            }
            System.out.println();
        }

        // Line below
        for (int j = 0; j < MAP_SIZE; j++) {
            System.out.print('_');
        }
        System.out.println();
    }

    // returns range for lidar in dirWorld direction
    int lidarWorld(int dirWorld) {
        return lidarLocal(dirWorld2dirLocal(robotPose.h, dirWorld));
    }

    // returns range for lidar in dirWorld direction
    int lidarLocal(int dirLocal) {
        switch (dirLocal) {
        case FORWARD:
            return API.lidarFront();
        case LEFT:
            return API.lidarLeft();
        case BACKWARD:
            return API.lidarBack();
        case RIGHT:
            return API.lidarRight();
        default:
            System.out.println("incorrect dirWorld in lidarWorld");
            return 0; // error
        }
    }

    // dirLocal = dirWorld - heading
    static int dirWorld2dirLocal(int heading, int dirWorld) {
        return wrap(dirWorld - heading);
    }

    // dirWorld = dirLocal + heading
    static int dirLocal2dirWorld(int heading, int dirLocal) {
        return wrap(dirLocal + heading);
    }

    static boolean freeToMove(int dirLocal, Pose p1, int[][] m) {
        Pose p2 = new Pose(p1);
        p2.p.addXY(dirLocal2dirWorld(p2.h, dirLocal), 1);
        if (isInMap(p2.p)) {
            switch (m[p2.p.x][p2.p.y]) {
            case UNSEEN:
            case EMPTY:
                return true;
            case UNKNOWN:
            case WALL:
                return false;
            default:
                System.out.print("incorrect map element in  in blockF");
                return false;
            }
        } else {
            return false;
        }
    }
}
