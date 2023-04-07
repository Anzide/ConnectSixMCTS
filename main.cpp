#include <vector>
#include <fstream>
#include "iostream"
#include "unordered_map"
#include "cstdio"
#include "cstdlib"
#include "cstring"
#include "cmath"
#include "unordered_set"
#include "ctime"

//#define DEBUG_MODE

using namespace std;

// ==== Run Settings ====

enum RunMode {
    TRAIN, RUN
};

enum DataSource {
    AUTO, NONE
};

// ==== Structs ====

struct GlobalSettings;
struct GlobalInfo;
struct Table;
struct TableHashFunc;
struct TableInfo;
struct MoverCounter;
struct Move;
struct MoveHashFunc;
struct GlobalTimer;

// ==== Global Variables ====

int trainingCounter = 0;
// Represent the player who made the move. -1 = 空, 0 = 先手, 1 = 后手
int mp[15][15];
vector<Move> simulationPath;

// ==== Functions Declarations ====

void debug(int code);

void init();

bool isFileExist(const string &fileName);

void loadData();

void saveData();

void simulation(bool);

void train();

void run();

void doMove(Move move);

float getUCT(const TableInfo &fa, const TableInfo &son);

void longRun();

bool check1MoveWin(int mover);

bool check2MoveWin(int mover);

// 该函数用于重整check2MoveWinMap中两个元素的的插入顺序。
void insert2MoveWin(Move m1, Move m2);

void initCurrentTable();

inline void combineHash(std::size_t &s, const short &v) {
    s ^= v + 0x9e3779b9 + (s << 6) + (s >> 2);
}

unordered_map<string, pair<clock_t, clock_t>> timeMap;

void printMap() {
    printf("=== MAP BEGIN ===\n");
    printf(" ");
    for (int i = 0; i < 15; i++)
        printf(" %d", i % 10);
    printf("\n");
    for (int i = 0; i < 15; i++) {
        printf("%d ", i % 10);
        for (int j = 0; j < 15; j++) {
            if (mp[i][j] == -1)printf("  ");
            else printf("%d ", mp[i][j]);
        }
        printf("\n");
    }
    printf("=== MAP END ===\n");
    fflush(stdout);
}

struct GlobalTimer {

    // pair<accumulation, startTime>

    static void startTimer(const string &name) {
        timeMap[name].second = clock();
    }

    static void endTimer(const string &name) {
        timeMap[name].first += clock() - timeMap[name].second;
    }

    static void printTime() {
        for (auto &item: timeMap) {
            cout << item.first << ": " << (double) item.second.first * 1000 / CLOCKS_PER_SEC << "ms" << endl;
        }
    }

};


unsigned char fileUtil_buf[100];

struct FileUtil {

    template<class T>
    static inline T get_num(FILE *fp) {
#ifdef DEBUG_MODE
        GlobalTimer::startTimer("FileUtil");
#endif
        fread(fileUtil_buf, sizeof(T), 1, fp);
        T num = 0;
        for (int i = 0; i < sizeof(T); i++) {
            num |= (fileUtil_buf[i] << i * 8);
        }
#ifdef DEBUG_MODE
        GlobalTimer::endTimer("FileUtil");
#endif
        return num;
    }

    template<class T>
    static inline void put_num(FILE *fp, T num) {
#ifdef DEBUG_MODE
        GlobalTimer::startTimer("FileUtil");
#endif
        fwrite(&num, sizeof(T), 1, fp);
#ifdef DEBUG_MODE
        GlobalTimer::endTimer("FileUtil");
#endif
    }
};

// ==== Structs Definitions ====

struct GlobalSettings {
    RunMode runMode = TRAIN;
    DataSource dataSource = AUTO;
    const int trainSimulationTimes = 1000;
    // Version of source data file. -1 = no data file
    // New data will be saved to file with version dataFileVersion + 1
    int dataFileVersion = -1;
    const string dataFolder = "D:/OneDrive/CourseDesign/data/";
//    const string dataFolder = "./data/";
    const string dataFileName = "data";
    const string dataFileSuffix = ".dat";

    string getDataFile(int version) {
        return dataFolder + dataFileName + to_string(version) + dataFileSuffix;
    }
};

GlobalSettings globalSettings;

struct Table {
    // 0 = 无子，1 = 有子
    short exist[15]{0};
    // 0 = 先手，1 = 后手。 当exist=0时，occupier必须为0
    short occupier[15]{0};

    Table() = default;

    // Load Table to mp
    void loadToMap() {
        for (int i = 0; i < 15; i++) {
            for (int j = 0; j < 15; j++) {
                if (getExist(i, j)) {
                    mp[i][j] = getOccupier(i, j);
                } else {
                    mp[i][j] = -1;
                }
            }
        }
    }

    static Table getTableFromMap() {
        Table table;
        for (int i = 0; i < 15; i++) {
            for (int j = 0; j < 15; j++) {
                if (mp[i][j] != -1) {
                    table.setExist(i, j, true);
                    table.setOccupier(i, j, mp[i][j]);
                }
            }
        }
        return table;
    }

    static void printBinary(short x) {
        for (int i = 0; i < 15; i++) {
            if (x & (1 << i))cout << 1;
            else cout << 0;
        }
        cout << endl;
    }

    inline void setExist(int x, int y, bool value) {
        // 置1
        if (value)exist[x] |= 1 << y;
        else exist[x] &= ~(1 << y); // 置0
    }

    inline bool getExist(int x, int y) {
        return exist[x] & (1 << y);
    }

    inline void setOccupier(int x, int y, int value) {
        // 置1
        if (value)occupier[x] |= 1 << y;
        else occupier[x] &= ~(1 << y); // 置0
    }

    inline int getOccupier(int x, int y) {
        return occupier[x] & (1 << y);
    }

    // -1 = 无子 0 = 先手 1 = 后手
    inline void set(int x, int y, int value) {
        if (value == -1) {
            setExist(x, y, false);
            setOccupier(x, y, 0);
            return;
        }
        setExist(x, y, true);
        setOccupier(x, y, value);
    }

    inline int get(int x, int y) {
        return !getExist(x, y) ? -1 : getOccupier(x, y);
    }

    inline int count() {
        int cnt = 0;
        for (int i = 0; i < 15; i++) {
            for (int j = 0; j < 15; j++) {
                if (getExist(i, j))cnt++;
            }
        }
        return cnt;
    }

    void printTableBinary() {
        cout << "=== Binary Begin: Exist ===" << endl;
        for (int i = 0; i < 15; i++)
            printBinary(exist[i]);
        cout << "=== Binary Begin: Occupier ===" << endl;
        for (int i = 0; i < 15; i++)
            printBinary(occupier[i]);
        cout << "=== Binary End ===" << endl;
    }

    bool operator==(const Table &rhs) const {
        for (int i = 0; i < 15; i++) {
            if (exist[i] != rhs.exist[i] || occupier[i] != rhs.occupier[i])
                return false;
        }
        return true;
    }
};

struct TableHashFunc {
    size_t operator()(const Table &table) const {
        size_t hash = 0;
        for (int i = 0; i < 15; i++) {
            combineHash(hash, table.exist[i]);
            combineHash(hash, table.occupier[i]);
        }
        return hash;
    }
};

struct TableInfo {
    // win:
    int win = 0, visited = 0;
    // 依据尚能下的步数，是否有下一手下棋者下1/2步后必赢的方法。-1=未知,0=无，1=1步，2=2步
    char mustWin = -1;
};

Table currentTable;
unordered_map<Table, TableInfo, TableHashFunc> tables;

// Represent the last mover
// 0,1 = first hand player 2,3 = second hand player
struct MoverCounter {
    int x;
    int total;

    MoverCounter() = delete;

    inline void increase() {
        x = (x + 1) % 4;
        total++;
    }

    inline void decrease() {
        x = (x + 3) % 4;
        total--;
    }

    inline int lastMover() const {
        return x / 2;
    }

    inline int nextMover() const {
        return ((x + 1) % 4) / 2;
    }

    inline int nextMoveTimes() const {
        return x == 0 || x == 2 ? 1 : 2;
    }

    void initByMap() {
        total = 0;
        for (int i = 0; i < 15; i++)
            for (int j = 0; j < 15; j++)
                if (mp[i][j] != -1)
                    total++;
        x = total % 4;
    }
};

// Warn: set init value carefully
MoverCounter moverCounter{0};

struct Move {
    int mover = -1, x = 0, y = 0;

    bool operator==(const Move &rhs) const {
        return mover == rhs.mover &&
               x == rhs.x &&
               y == rhs.y;
    }
};

struct MoveHashFunc {
    size_t operator()(const Move &move) const {
        size_t s = 0;
        combineHash(s, (short) move.mover);
        combineHash(s, (short) move.x);
        combineHash(s, (short) move.y);
        return s;
    }
};


// 警告:不当的调用顺序会导致问题。不能递归调用。
unordered_set<Move, MoveHashFunc> check1MoveWinSet;
unordered_map<Move, unordered_set<Move, MoveHashFunc>, MoveHashFunc> check2MoveWinMap;

// ==== Functions Definitions ====

int main() {

//    clock_t t1 = clock();
//    Sleep(500);
//    cout<<clock()-t1<<endl;
//
//    return 0;

    init();
#ifdef DEBUG
    GlobalTimer::startTimer("Total");
#endif

    if (!isFileExist(globalSettings.getDataFile(0)))
        return 0;
    
    loadData();
//    run();
    train();
//    debug(6);
    saveData();
#ifdef DEBUG
    GlobalTimer::endTimer("Total");
    GlobalTimer::printTime();
#endif

    return 0;
}

void init() {
    std::srand(time(nullptr));
}

bool isFileExist(const string &fileName) {
    ifstream infile(fileName);
    return infile.good();
}

/*
 * 数据格式:
 * 1. table数
 * 2. 对于每个table，先是table本体，再是tableInfo
 */

void loadData() {

    // Get latest data version
    while (isFileExist(globalSettings.getDataFile(globalSettings.dataFileVersion + 1))) {
        globalSettings.dataFileVersion++;
    }

    // AUTO: load data
    if (globalSettings.dataSource == AUTO && globalSettings.dataFileVersion != -1) {
        FILE *fp = fopen(globalSettings.getDataFile(globalSettings.dataFileVersion).c_str(), "rb");
        int size = FileUtil::get_num<int>(fp);
        for (int i = 0; i < size; i++) {
            Table table;
            for (int j = 0; j < 15; j++) {
                table.exist[j] = FileUtil::get_num<short>(fp);
                table.occupier[j] = FileUtil::get_num<short>(fp);
            }
            TableInfo tableInfo;
            tableInfo.win = FileUtil::get_num<int>(fp);
            tableInfo.visited = FileUtil::get_num<int>(fp);
            tableInfo.mustWin = FileUtil::get_num<char>(fp);
            tables[table] = tableInfo;
        }
    }
}

void saveData() {

    FILE *fp = fopen(globalSettings.getDataFile(globalSettings.dataFileVersion + 1).c_str(), "wb");
    int size = tables.size();

#ifdef DEBUG
    printf("size: %d\n", size);
    fflush(stdout);
#endif

    // TODO: 只保留count较小的
    FileUtil::put_num(fp, size);
    for (auto &p: tables) {
        for (int i = 0; i < 15; i++) {
            FileUtil::put_num<short>(fp, p.first.exist[i]);
            FileUtil::put_num<short>(fp, p.first.occupier[i]);
        }
        FileUtil::put_num<int>(fp, p.second.win);
        FileUtil::put_num<int>(fp, p.second.visited);
        FileUtil::put_num<char>(fp, p.second.mustWin);
    }
}

inline void doMove(Move m) {
    mp[m.x][m.y] = m.mover;
    currentTable.set(m.x, m.y, m.mover);
    moverCounter.increase();
}

// setExist=false必须清空occupier！！！！！！否则hash会爆炸
inline void redoMove(Move m) {
    mp[m.x][m.y] = -1;
    currentTable.set(m.x, m.y, -1);
    moverCounter.decrease();
}

inline void doSimulationMove(Move m) {
    simulationPath.push_back(m);
    doMove(m);
}

void updateCurrentMustWin(int mover) {
    if (tables[currentTable].mustWin == -1) {
        if (check1MoveWin(mover)) {
            tables[currentTable].mustWin = 1;
        } else if (moverCounter.nextMoveTimes() == 2 && check2MoveWin(mover)) {
            tables[currentTable].mustWin = 2;
        } else {
            tables[currentTable].mustWin = 0;
        }
    }
}

// TODO: 拦截对方的必胜步
// TODO: 剪枝
void simulation(bool training) {

    simulationPath.clear();
    initCurrentTable();
    moverCounter.initByMap();

    int rootChessNum = moverCounter.total;
    bool endSimulation = false, win = false;
    while (moverCounter.total < 255) {

        int mover = moverCounter.nextMover();
        updateCurrentMustWin(mover);

        const TableInfo currentTableInfo = tables[currentTable];
        if (currentTableInfo.mustWin == 1) {
            check1MoveWin(mover);
            Move m = *check1MoveWinSet.begin();
            doSimulationMove(m);
            win = true;
            endSimulation = true;
        } else if (moverCounter.nextMoveTimes() == 2 && currentTableInfo.mustWin == 2) {
            check2MoveWin(mover);
            auto p = *check2MoveWinMap.begin();
            Move m1 = p.first, m2 = *p.second.begin();
            doSimulationMove(m1);
            updateCurrentMustWin(mover);
            doSimulationMove(m2);
            win = true;
            endSimulation = true;
        }
        if (endSimulation)break;

        int endLoop = false;
        float maxUCT = -1.0f;
        Move maxMove;
        for (int i = 0; i < 15 && !endLoop; i++)
            for (int j = 0; j < 15; j++) {
                if (mp[i][j] != -1)continue;

                // 剪枝1: 如果5x5内没有其他棋子，则不走这步
                // 考虑调整为 7x7
                if (moverCounter.total != 0) {
                    bool bypass = true;
                    int ped = min(i + 2, 14), qed = min(j + 2, 14);
                    for (int p = max(i - 2, 0); p <= ped; p++) {
                        for (int q = max(j - 2, 0); q <= qed; q++)
                            if (mp[p][q] != -1) {
                                bypass = false;
                                break;
                            }
                        if (!bypass)break;
                    }
                    if (bypass)continue;
                }

                Table t = currentTable;
                t.set(i, j, mover);
                // 未访问过
                if (tables.find(t) == tables.end()) {
//                if (tables[t].visited == 0) {
                    maxMove = {mover, i, j};
                    endLoop = true;
                    break;
                } else { // 访问过
                    float uct = getUCT(currentTableInfo, tables[t]);
                    if (uct > maxUCT) {
                        maxUCT = uct;
                        maxMove = {mover, i, j};
                    }
                }
            }
        doSimulationMove(maxMove);
    }

    int winner = win ? moverCounter.lastMover() : rand() % 2;

//    if (trainingCounter % 1000 == 0) {
//        printf("simulation finishied, winner: %d, total: %d\n", winner, moverCounter.total);
//        for (int i = 0; i < 15; i++) {
//            for (int j = 0; j < 15; j++)
//                if (mp[i][j] == -1)printf("  ");
//                else printf("%d ", mp[i][j]);
//            printf("\n");
//        }
//        fflush(stdout);
//    }

    for (auto it = simulationPath.rbegin(); it != simulationPath.rend(); it++) {
        Move m = *it;
        // 仅在一定深度内更新数据，防止爆内存
        if (moverCounter.total - rootChessNum <= 30) {
            tables[currentTable].visited++;
            if (winner == m.mover)tables[currentTable].win++;
        }
        redoMove(m);
    }
    tables[currentTable].visited++;
    if (winner == moverCounter.lastMover())tables[currentTable].win++;

}

void train() {

    memset(mp, -1, sizeof(mp));
    for (trainingCounter = 1; trainingCounter <= globalSettings.trainSimulationTimes; trainingCounter++) {
        simulation(true);
    }

}

void run() {

    memset(mp, -1, sizeof(mp));
    initCurrentTable();
    moverCounter.initByMap();

    // 0=我是先手 1=我是后手
    int mover = 0;
    int n;
    cin >> n;

    for (int i = 1; i <= n * 2 - 1; i++) {
        int x1, y1, x2, y2;
        cin >> x1 >> y1 >> x2 >> y2;
        if (x1 != -1 && y1 != -1) {
            doMove({mover, x1, y1});
            if (x2 != -1 && y2 != -1)
                doMove({mover, x2, y2});
        }
        mover = 1 - mover;
    }

    // 当前，mover为我自己（0或1）
    // 假设输入为-1,-1,-1,-1，那么mover是0（我先手）
    // 1.我走棋 2.读入对方走棋

    clock_t begin = clock();

    while (true) {

        // 检查自己一步赢
        if (check1MoveWin(mover)) {
            Move m1 = *check1MoveWinSet.begin(), m2;
            doMove(m1);
            // 随便找个空位下
            bool bk = false;
            for (int i = 0; i < 15 && !bk; i++)
                for (int j = 0; j < 15; j++)
                    if (mp[i][j] == -1) {
                        bk = true;
                        doMove({mover, i, j});
                        m2 = {mover, i, j};
                        break;
                    }
            printf("%d %d %d %d\n", m1.x, m1.y, m2.x, m2.y);
            break;
        }
        // 检查自己二步赢
        if (check2MoveWin(mover)) {
            auto p = *check2MoveWinMap.begin();
            Move m1 = p.first, m2 = *p.second.begin();
            doMove(m1);
            doMove(m2);
            printf("%d %d %d %d\n", m1.x, m1.y, m2.x, m2.y);
            break;
        }

        int remains = moverCounter.nextMoveTimes();
        vector<Move> myMove;
        // 拦截对方的1步必胜步
        if (check1MoveWin(1 - mover)) {
            for (Move m: check1MoveWinSet) {
                if (remains == 0)
                    break;
                Move myM = {mover, m.x, m.y};
                doMove(myM);
                myMove.push_back(myM);
                remains--;
            }
        }
        // 拦截对方的2步必胜步
        if (remains != 0 && check2MoveWin(1 - mover)) {
            for (auto p: check2MoveWinMap) {
                if (remains == 0)
                    break;
                Move m1 = p.first, m2 = *p.second.begin();
                if (mp[m1.x][m1.y] != -1 || mp[m2.x][m2.y] != -1)
                    continue;
                Move myM = {mover, m1.x, m1.y};
                doMove(myM);
                myMove.push_back(myM);
                remains--;
            }
        }

        // 自己的第一步
        if (remains == 2) {

//            int t2 = 100;
//            while (t2--) {
            while (clock() - begin < 0.5 * CLOCKS_PER_SEC) {
                simulation(false);
            }
            // 走最好的
            Move m1;
            int maxVisited = -1;
            for (int i = 0; i < 15; i++)
                for (int j = 0; j < 15; j++) {
                    if (mp[i][j] != -1)continue;
                    Table t = currentTable;
                    t.set(i, j, mover);
                    // 未访问过
                    if (tables[t].visited > maxVisited) {
                        maxVisited = tables[t].visited;
                        m1 = {mover, i, j};
                    }
                }
            doMove(m1);
            myMove.push_back(m1);
            remains--;
        }
        // 自己的第二步
        if (remains == 1) {
            while (clock() - begin < 0.95 * CLOCKS_PER_SEC) {
                simulation(false);
            }
            // 走最好的
            Move m2;
            int maxVisited = -1;
            for (int i = 0; i < 15; i++)
                for (int j = 0; j < 15; j++) {
                    if (mp[i][j] != -1)continue;
                    Table t = currentTable;
                    t.set(i, j, mover);
                    // 未访问过
                    if (tables[t].visited > maxVisited) {
                        maxVisited = tables[t].visited;
                        m2 = {mover, i, j};
                    }
                }
            doMove(m2);
            myMove.push_back(m2);
            remains--;
        }

        if (myMove.size() == 1) {
            myMove.push_back({mover, -1, -1});
        }
        printf("%d %d %d %d\n", myMove[0].x, myMove[0].y, myMove[1].x, myMove[1].y);

#ifdef DEBUG_MODE
        printMap();
#endif

        longRun();
        cout << flush;

        int x1, y1, x2, y2;
        cin >> x1 >> y1 >> x2 >> y2;
        begin = clock();
        doMove({1 - mover, x1, y1});
        doMove({1 - mover, x2, y2});

    }

}

inline float getUCT(const TableInfo &fa, const TableInfo &son) {
    return ((float) son.win) / (float) son.visited + (float) sqrt(2 * log(fa.visited) / son.visited);
}

void longRun() { cout << ">>>BOTZONE_REQUEST_KEEP_RUNNING<<<" << endl; }

// 必须保证此时不处于已经有人赢的状态，否则会将< -1, -1 >添加至结果。
bool check1MoveWin(int mover) {

    check1MoveWinSet.clear();
    int counter, counterWithMove;
    pair<int, int> moveLoc;

    // 横向检查
    for (int i = 0; i < 15; i++) {
        counter = 0, counterWithMove = 0;
        moveLoc = {-1, -1};
        for (int j = 0; j < 15; j++) {
            if (mp[i][j] == mover) {
                counter++, counterWithMove++;
            } else {
                if (mp[i][j] == -1) {
                    counterWithMove = counter + 1;
                    moveLoc = {i, j};
                } else counterWithMove = 0;
                counter = 0;
            }
            if (counterWithMove == 6) {
                check1MoveWinSet.insert({mover, moveLoc.first, moveLoc.second});
                counterWithMove = counter;
            }
        }
    }

    // 竖向检查
    for (int i = 0; i < 15; i++) {
        counter = 0, counterWithMove = 0;
        moveLoc = {-1, -1};
        for (int j = 0; j < 15; j++) {
            if (mp[j][i] == mover) {
                counter++, counterWithMove++;
            } else {
                if (mp[j][i] == -1) {
                    counterWithMove = counter + 1;
                    moveLoc = {j, i};
                } else counterWithMove = 0;
                counter = 0;
            }
            if (counterWithMove == 6) {
                check1MoveWinSet.insert({mover, moveLoc.first, moveLoc.second});
                counterWithMove = counter;
            }
        }
    }
    // 斜向，列0-10
    for (int i = 0; i <= 9; i++) {
        counter = 0, counterWithMove = 0;
        moveLoc = {-1, -1};
        for (int j = 0; j <= 14 - i; j++) {
            if (mp[0 + j][i + j] == mover) {
                counter++, counterWithMove++;
            } else {
                if (mp[0 + j][i + j] == -1) {
                    counterWithMove = counter + 1;
                    moveLoc = {0 + j, i + j};
                } else counterWithMove = 0;
                counter = 0;
            }
            if (counterWithMove == 6) {
                check1MoveWinSet.insert({mover, moveLoc.first, moveLoc.second});
                counterWithMove = counter;
            }
        }
    }
    // 斜向，行1-10
    for (int i = 1; i <= 9; i++) {
        counter = 0, counterWithMove = 0;
        moveLoc = {-1, -1};
        for (int j = 0; j <= 14 - i; j++) {
            if (mp[i + j][0 + j] == mover) {
                counter++, counterWithMove++;
            } else {
                if (mp[i + j][0 + j] == -1) {
                    counterWithMove = counter + 1;
                    moveLoc = {i + j, 0 + j};
                } else counterWithMove = 0;
                counter = 0;
            }
            if (counterWithMove == 6) {
                check1MoveWinSet.insert({mover, moveLoc.first, moveLoc.second});
                counterWithMove = counter;
            }
        }
    }
    return !check1MoveWinSet.empty();
}

// 调用前必须保证checkWin==false && check1MoveWin==false
bool check2MoveWin(int mover) {

    check2MoveWinMap.clear();
    int c0, c1, c2;
    pair<int, int> m1, m2;

    // 横向检查
    for (int i = 0; i < 15; i++) {
        c0 = 0, c1 = 0, c2 = 0;
        m1 = {-1, -1};
        m2 = {-1, -1};
        for (int j = 0; j < 15; j++) {
            if (mp[i][j] == mover) {
                c0++, c1++, c2++;
            } else {
                if (mp[i][j] == -1) {
                    c2 = c1 + 1;
                    c1 = c0 + 1;
                    m1 = m2;
                    m2 = {i, j};
                } else {
                    c2 = c1 = 0;
                }
                c0 = 0;
            }
            if (c2 >= 6) {
                insert2MoveWin({mover, m1.first, m1.second}, {mover, m2.first, m2.second});
            }
        }
    }
    // 竖向检查
    for (int i = 0; i < 15; i++) {
        c0 = 0, c1 = 0, c2 = 0;
        m1 = {-1, -1};
        m2 = {-1, -1};
        for (int j = 0; j < 15; j++) {
            if (mp[j][i] == mover) {
                c0++, c1++, c2++;
            } else {
                if (mp[j][i] == -1) {
                    c2 = c1 + 1;
                    c1 = c0 + 1;
                    m1 = m2;
                    m2 = {j, i};
                } else {
                    c2 = c1 = 0;
                }
                c0 = 0;
            }
            if (c2 >= 6) {
                insert2MoveWin({mover, m1.first, m1.second}, {mover, m2.first, m2.second});
            }
        }
    }
    // 斜向，列0-10
    for (int i = 0; i <= 9; i++) {
        c0 = 0, c1 = 0, c2 = 0;
        m1 = {-1, -1};
        m2 = {-1, -1};
        for (int j = 0; j <= 14 - i; j++) {
            if (mp[0 + j][i + j] == mover) {
                c0++, c1++, c2++;
            } else {
                if (mp[0 + j][i + j] == -1) {
                    c2 = c1 + 1;
                    c1 = c0 + 1;
                    m1 = m2;
                    m2 = {0 + j, i + j};
                } else {
                    c2 = c1 = 0;
                }
                c0 = 0;
            }
            if (c2 >= 6) {
                insert2MoveWin({mover, m1.first, m1.second}, {mover, m2.first, m2.second});
            }
        }
    }
    // 斜向，行1-10
    for (int i = 1; i <= 9; i++) {
        c0 = 0, c1 = 0, c2 = 0;
        m1 = {-1, -1};
        m2 = {-1, -1};
        for (int j = 0; j <= 14 - i; j++) {
            if (mp[i + j][0 + j] == mover) {
                c0++, c1++, c2++;
            } else {
                if (mp[i + j][0 + j] == -1) {
                    c2 = c1 + 1;
                    c1 = c0 + 1;
                    m1 = m2;
                    m2 = {i + j, 0 + j};
                } else {
                    c2 = c1 = 0;
                }
                c0 = 0;
            }
            if (c2 >= 6) {
                insert2MoveWin({mover, m1.first, m1.second}, {mover, m2.first, m2.second});
            }
        }
    }
    return !check2MoveWinMap.empty();
}

void insert2MoveWin(Move m1, Move m2) {
    // 先x由小到大，后y由小到大
    if (m1.x > m2.x || (m1.x == m2.x && m1.y > m2.y)) {
        Move tmp = m1;
        m1 = m2;
        m2 = tmp;
    }
    check2MoveWinMap[m1].insert(m2);
}

void initCurrentTable() {
    currentTable = Table::getTableFromMap();
}

void debug(int code) {
    switch (code) {
        // Table get / set test
        case 1: {
            Table t;
            t.setExist(15, 15, true);
            t.setOccupier(2, 1, 1);
            t.printTableBinary();
            cout << t.getExist(0, 5) << endl;
            cout << t.getExist(15, 15) << endl;
            break;
        }
            // Short hash test
        case 2: {
            hash<short> h;
            short s = 5;
            cout << h(s);
            break;
        }
            // Table hash test
        case 3: {
            TableHashFunc f;
            Table t;
            cout << f(t) << endl;
            t.setExist(1, 1, true);
            cout << f(t) << endl;
            break;
        }
            // UCT test
        case 4: {
            TableInfo fa, sonA, sonB;
            fa.visited = 64;
            sonA.win = 8;
            sonA.visited = 10;
            sonB.win = 3;
            sonB.visited = 6;
            cout << "UCT of the more visited one: " << getUCT(fa, sonA) << endl;
            cout << "UCT of the less visited one: " << getUCT(fa, sonB) << endl;
        }

            // MoverCounter test
        case 5: {
            MoverCounter c{0};
            cout << c.x << " " << c.lastMover() << " " << c.nextMover() << endl;
            c.decrease();
            cout << c.x << " " << c.lastMover() << " " << c.nextMover() << endl;
            c.increase();
            c.increase();
            cout << c.x << " " << c.lastMover() << " " << c.nextMover() << endl;
        }

            // Performance test
        case 6: {
            clock_t start = clock();
            for (int i = 1; i <= 10000000; i++) {
                currentTable.set(i % 15, i % 15, i % 2);
            }
            printf("Time used: %.3fs", (double) (clock() - start) / CLOCKS_PER_SEC);
        }

    }
}
