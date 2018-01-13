#include <iostream>
#include <string>
#include <cmath>
#include <algorithm>
#include <cstdlib>
#include <vector>
#include <cassert>
#include <ctime>
#include <bitset>
#include "jsoncpp/json.h"
using namespace std;
 
#define MAPWIDTH 10
#define MAPHEIGHT 20
 
// 我所在队伍的颜色（0为红，1为蓝，仅表示队伍，不分先后）
int currBotColor;
int enemyColor;
 
// 先y后x，记录地图状态，0为空，1为以前放置，2为刚刚放置，负数为越界
// （2用于在清行后将最后一步撤销再送给对方）
int gridInfo[2][MAPHEIGHT + 2][MAPWIDTH + 2] = { 0 };
 
// 代表分别向对方转移的行
int trans[2][4][MAPWIDTH + 2] = { 0 };
 
// 转移行数
int transCount[2] = { 0 };
 
// 运行eliminate后的当前高度
int maxHeight[2] = { 0 };
 
// 总消去行数的分数之和
int elimTotal[2] = { 0 };
 
// 一次性消去行数对应分数
const int elimBonus[] = { 0, 1, 3, 5, 7 };
 
// 给对应玩家的各类块的数目总计
int typeCountForColor[2][7] = { 0 };

vector<int> succ[10];

const int blockShape[7][4][8] = {
	{ { 0,0,1,0,-1,0,-1,-1 },{ 0,0,0,1,0,-1,1,-1 },{ 0,0,-1,0,1,0,1,1 },{ 0,0,0,-1,0,1,-1,1 } },
	{ { 0,0,-1,0,1,0,1,-1 },{ 0,0,0,-1,0,1,1,1 },{ 0,0,1,0,-1,0,-1,1 },{ 0,0,0,1,0,-1,-1,-1 } },
	{ { 0,0,1,0,0,-1,-1,-1 },{ 0,0,0,1,1,0,1,-1 },{ 0,0,-1,0,0,1,1,1 },{ 0,0,0,-1,-1,0,-1,1 } },
	{ { 0,0,-1,0,0,-1,1,-1 },{ 0,0,0,-1,1,0,1,1 },{ 0,0,1,0,0,1,-1,1 },{ 0,0,0,1,-1,0,-1,-1 } },
	{ { 0,0,-1,0,0,1,1,0 },{ 0,0,0,-1,-1,0,0,1 },{ 0,0,1,0,0,-1,-1,0 },{ 0,0,0,1,1,0,0,-1 } },
	{ { 0,0,0,-1,0,1,0,2 },{ 0,0,1,0,-1,0,-2,0 },{ 0,0,0,1,0,-1,0,-2 },{ 0,0,-1,0,1,0,2,0 } },
	{ { 0,0,0,1,-1,0,-1,1 },{ 0,0,-1,0,0,-1,-1,-1 },{ 0,0,0,-1,1,-0,1,-1 },{ 0,0,1,0,0,1,1,1 } }
};// 7种形状(长L| 短L| 反z| 正z| T| 直一| 田格)，4种朝向(上左下右)，8:每相邻的两个分别为x，y
 
const int rotateBlank[7][4][10] = {
	{ { 1,1,0,0 },{ -1,1,0,0 },{ -1,-1,0,0 },{ 1,-1,0,0 } },
	{ { -1,-1,0,0 },{ 1,-1,0,0 },{ 1,1,0,0 },{ -1,1,0,0 } },
	{ { 1,1,0,0 },{ -1,1,0,0 },{ -1,-1,0,0 },{ 1,-1,0,0 } },
	{ { -1,-1,0,0 },{ 1,-1,0,0 },{ 1,1,0,0 },{ -1,1,0,0 } },
	{ { -1,-1,-1,1,1,1,0,0 },{ -1,-1,-1,1,1,-1,0,0 },{ -1,-1,1,1,1,-1,0,0 },{ -1,1,1,1,1,-1,0,0 } },
	{ { 1,-1,-1,1,-2,1,-1,2,-2,2 } ,{ 1,1,-1,-1,-2,-1,-1,-2,-2,-2 } ,{ -1,1,1,-1,2,-1,1,-2,2,-2 } ,{ -1,-1,1,1,2,1,1,2,2,2 } },
	{ { 0,0 },{ 0,0 } ,{ 0,0 } ,{ 0,0 } }
}; // 旋转的时候需要为空的块相对于旋转中心的坐标
 
class Tetris
{
public:
	const int blockType;   // 标记方块类型的序号 0~6
	int blockX;            // 旋转中心的x轴坐标
	int blockY;            // 旋转中心的y轴坐标
	int orientation;       // 标记方块的朝向 0~3
	const int(*shape)[8]; // 当前类型方块的形状定义
 
	int color;
 
	Tetris(int t, int color) : blockType(t), shape(blockShape[t]), color(color)
	{ }
 
	inline Tetris &set(int x = -1, int y = -1, int o = -1)
	{
		blockX = x == -1 ? blockX : x;
		blockY = y == -1 ? blockY : y;
		orientation = o == -1 ? orientation : o;
		return *this;
	}
 
	// 判断当前位置是否合法
	inline bool isValid(int x = -1, int y = -1, int o = -1)
	{
		x = x == -1 ? blockX : x;
		y = y == -1 ? blockY : y;
		o = o == -1 ? orientation : o;
		if (o < 0 || o > 3)
			return false;
 
		int i, tmpX, tmpY;
		for (i = 0; i < 4; i++)
			{
				tmpX = x + shape[o][2 * i];
				tmpY = y + shape[o][2 * i + 1];
				if (tmpX < 1 || tmpX > MAPWIDTH ||
					tmpY < 1 || tmpY > MAPHEIGHT ||
					gridInfo[color][tmpY][tmpX] != 0)
					return false;
			}
		return true;
	}
 
	// 判断是否落地
	inline bool onGround()
	{
		if (isValid() && !isValid(-1, blockY - 1))
			return true;
		return false;
	}
 
	// 将方块放置在场地上
	inline bool place()
	{
		if (!onGround())
			return false;
 
		int i, tmpX, tmpY;
		for (i = 0; i < 4; i++)
			{
				tmpX = blockX + shape[orientation][2 * i];
				tmpY = blockY + shape[orientation][2 * i + 1];
				gridInfo[color][tmpY][tmpX] = 2;
			}
		return true;
	}
 
	// 检查能否逆时针旋转自己到o
	inline bool rotation(int o)
	{
		if (o < 0 || o > 3)
			return false;
 
		if (orientation == o)
			return true;
 
		int fromO = orientation;
		int i, blankX, blankY;
		while (true)
		{
			if (!isValid(-1, -1, fromO))
				return false;
 
			if (fromO == o)
				break;
	            
	        // 检查旋转碰撞
	        for (i = 0; i < 5; i++) {
	            blankX = blockX + rotateBlank[blockType][fromO][2 * i];
	            blankY = blockY + rotateBlank[blockType][fromO][2 * i + 1];
	            if (blankX == blockX && blankY == blockY)
	                break;
	            if (gridInfo[color][blankY][blankX] != 0)
	                return false;
	        }
 
			fromO = (fromO + 1) % 4;
		}
		return true;
	}

};
 
// 围一圈护城河
void init()
{
	int i;
	for (i = 0; i < MAPHEIGHT + 2; i++)
		{
			gridInfo[1][i][0] = gridInfo[1][i][MAPWIDTH + 1] = -2;
			gridInfo[0][i][0] = gridInfo[0][i][MAPWIDTH + 1] = -2;
		}
	for (i = 0; i < MAPWIDTH + 2; i++)
		{
			gridInfo[1][0][i] = gridInfo[1][MAPHEIGHT + 1][i] = -2;
			gridInfo[0][0][i] = gridInfo[0][MAPHEIGHT + 1][i] = -2;
		}
}
 
namespace Util
{
 
	// 检查能否从场地顶端直接落到当前位置
	inline bool checkDirectDropTo(int color, int blockType, int x, int y, int o)
	{
		auto &def = blockShape[blockType][o];
		for (; y <= MAPHEIGHT; y++)
			for (int i = 0; i < 4; i++)
				{
					int _x = def[i * 2] + x, _y = def[i * 2 + 1] + y;
					if (_y > MAPHEIGHT)
						continue;
					if (_y < 1 || _x < 1 || _x > MAPWIDTH || gridInfo[color][_y][_x])
						return false;
				}
		return true;
	}
 
	// 消去行
	void eliminate(int color)
	{
		int &count = transCount[color] = 0;
		int i, j, emptyFlag, fullFlag;
		maxHeight[color] = MAPHEIGHT;
		for (i = 1; i <= MAPHEIGHT; i++)
			{
				emptyFlag = 1;
				fullFlag = 1;
				for (j = 1; j <= MAPWIDTH; j++)
					{
						if (gridInfo[color][i][j] == 0)
							fullFlag = 0;
						else
							emptyFlag = 0;
					}
				if (fullFlag)
					{
						for (j = 1; j <= MAPWIDTH; j++)
							{
								// 注意这里只转移以前的块，不包括最后一次落下的块（“撤销最后一步”）
								trans[color][count][j] = gridInfo[color][i][j] == 1 ? 1 : 0;
								gridInfo[color][i][j] = 0;
							}
						count++;
					}
				else if (emptyFlag)
					{
						maxHeight[color] = i - 1;
						break;
					}
				else
					for (j = 1; j <= MAPWIDTH; j++)
						{
							gridInfo[color][i - count][j] =
								gridInfo[color][i][j] > 0 ? 1 : gridInfo[color][i][j];
							if (count)
								gridInfo[color][i][j] = 0;
						}
			}
		maxHeight[color] -= count;
		elimTotal[color] += elimBonus[count];
	}
 
	// 转移双方消去的行，返回-1表示继续，否则返回输者
	int transfer()
	{
		int color1 = 0, color2 = 1;
		if (transCount[color1] == 0 && transCount[color2] == 0)
			return -1;
		if (transCount[color1] == 0 || transCount[color2] == 0)
			{
				if (transCount[color1] == 0 && transCount[color2] > 0)
					swap(color1, color2);
				int h2;
				maxHeight[color2] = h2 = maxHeight[color2] + transCount[color1];
				if (h2 > MAPHEIGHT)
					return color2;
				int i, j;
 
				for (i = h2; i > transCount[color1]; i--)
					for (j = 1; j <= MAPWIDTH; j++)
						gridInfo[color2][i][j] = gridInfo[color2][i - transCount[color1]][j];
 
				for (i = transCount[color1]; i > 0; i--)
					for (j = 1; j <= MAPWIDTH; j++)
						gridInfo[color2][i][j] = trans[color1][i - 1][j];
				return -1;
			}
		else
			{
				int h1, h2;
				maxHeight[color1] = h1 = maxHeight[color1] + transCount[color2];//从color1处移动count1去color2
				maxHeight[color2] = h2 = maxHeight[color2] + transCount[color1];
 
				if (h1 > MAPHEIGHT) return color1;
				if (h2 > MAPHEIGHT) return color2;
 
				int i, j;
				for (i = h2; i > transCount[color1]; i--)
					for (j = 1; j <= MAPWIDTH; j++)
						gridInfo[color2][i][j] = gridInfo[color2][i - transCount[color1]][j];
 
				for (i = transCount[color1]; i > 0; i--)
					for (j = 1; j <= MAPWIDTH; j++)
						gridInfo[color2][i][j] = trans[color1][i - 1][j];
 
				for (i = h1; i > transCount[color2]; i--)
					for (j = 1; j <= MAPWIDTH; j++)
						gridInfo[color1][i][j] = gridInfo[color1][i - transCount[color2]][j];
 
				for (i = transCount[color2]; i > 0; i--)
					for (j = 1; j <= MAPWIDTH; j++)
						gridInfo[color1][i][j] = trans[color2][i - 1][j];
 
				return -1;
			}
	}
 
	// 颜色方还能否继续游戏
	inline bool canPut(int color, int blockType)
	{
		Tetris t(blockType, color);
		for (int y = MAPHEIGHT; y >= 1; y--)
			for (int x = 1; x <= MAPWIDTH; x++)
				for (int o = 0; o < 4; o++)
					{
						t.set(x, y, o);
						if (t.isValid() && checkDirectDropTo(color, blockType, x, y, o))
							return true;
					}
		return false;
	}
 
	// 打印场地用于调试
	inline void printField()
	{
#ifndef _BOTZONE_ONLINE
		static const char *i2s[] = {
			"~~",
			"~~",
			"  ",
			"[]",
			"##"
		};
		cout << "~~：墙，[]：块，##：新块" << endl;
		for (int y = MAPHEIGHT + 1; y >= 0; y--)
			{
				for (int x = 0; x <= MAPWIDTH + 1; x++)
					cout << i2s[gridInfo[0][y][x] + 2];
				for (int x = 0; x <= MAPWIDTH + 1; x++)
					cout << i2s[gridInfo[1][y][x] + 2];
				cout << endl;
			}
#endif
	}
}
int now[30][30][30], h[30], A[30][30], tmp[8];
int qx[1010], qy[1010], qo[1010], qf, qr;
int v[30][30][5], rd = 0;

const int dx[] = {-1, 1, 0, 0};
const int dy[] = {0, 0, -1, 1};

inline int idex(int x, int y, int o) {
	return o * 200 + (x - 1) * 20 + y - 1;
}

void findSucc(bitset<205> &a, int block, int dep, int col) {
	for (int i = 0; i < 4; i++) {
		for (int j = 1; j <= MAPWIDTH; j++)
			for (int k = 1; k <= MAPHEIGHT; k++) {
				now[i][j][k] = -1;
				for (int ii = 0; ii < 4; ii++) {
					int px = j + blockShape[block][i][2 * ii];
					int py = k + blockShape[block][i][2 * ii + 1];
					if (px < 1 || px > MAPWIDTH || py < 1 || py > MAPHEIGHT || a[(px - 1) * 20 + py - 1]) now[i][j][k] = 0;
				}
				if (now[i][j][k] == -1) now[i][j][k] = 1;
			}
	}
	
	qf = 1, qr = 0, ++rd;
	for (int n = 20; n >= 18; --n)
		for (int i = 1; i <= 10; i++)
			for (int j = 0; j < 4; ++j)
				if (Util :: checkDirectDropTo(col, block, i, n, j))
					qx[++qr] = i, qy[qr] = n, qo[qr] = j, v[i][n][j] = rd;

	Tetris aaa(block, col);
	for ( ; qf <= qr; ) {
		int x = qx[qf], y = qy[qf], o = qo[qf++];
		for (int k = 0; k < 4; ++k) {
			int xx = x + dx[k], yy = y + dy[k];
			if (xx < 1 || xx > MAPWIDTH || yy < 1 || yy > MAPHEIGHT) continue;
			if (!now[o][xx][yy] || v[xx][yy][o] == rd) continue;
			qx[++qr] = xx, qy[qr] = yy, qo[qr] = o, v[xx][yy][o] = rd;
		}
		int nxt = (o + 1) % 4;
		if (!aaa.set(x, y, o).rotation(nxt) || !now[nxt][x][y] || v[x][y][nxt] == rd) continue;
		qx[++qr] = x, qy[qr] = y, qo[qr] = nxt, v[x][y][nxt] = rd;
	}

	succ[dep].clear();
	for (int i = 1; i <= qr; ++i) {
		int x = qx[i], y = qy[i], o = qo[i];
		if (block == 6 && o != 0) continue;
		if ((block == 5 || block == 2 || block == 3) && (o > 1)) continue;
		if (!aaa.set(x, y, o).onGround()) continue;
		succ[dep].push_back(idex(x, y, o));
	}
}

inline void decompress(int player, bitset <205> &cur) {
	for (int i = 1; i <= MAPWIDTH; ++i)
		for (int j = 1; j <= MAPHEIGHT; ++j) {
			gridInfo[player][j][i] = cur[(i - 1) * 20 + j - 1];
		}
}
	
inline void compress(int player, bitset <205> &cur) {
	for (int i = 1; i <= MAPWIDTH; ++i)
		for (int j = 1; j <= MAPHEIGHT; ++j) {
			cur[(i - 1) * 20 + j - 1] = gridInfo[player][j][i] > 0;
		}
}

int mH[7][4];

void prePutblock() {
	for (int block = 0; block < 7; ++block) {
		for (int o = 0; o < 4; ++o) {
			for (int i = 0; i < 4; ++i) {
				if (blockShape[block][o][2 * i + 1] > mH[block][o]) mH[block][o] = blockShape[block][o][2 * i + 1];
			}
		}
	}
}

int putBlock(bitset <205> &cur, int block, int info, int tp) {
	int y = info % 20 + 1, x = ((info - (y - 1)) % 200) / 20 + 1, o = (info - (x - 1) * 20 - (y - 1)) / 200;
	for (int i = 0; i < 4; ++i) {
		int xx = x + blockShape[block][o][2 * i], yy = y + blockShape[block][o][2 * i + 1];
		cur[(xx - 1) * 20 + yy - 1] = tp;
	}
	return y + mH[block][o];
}

const int defaultDep = 7;

const double FHeight = -6.500158825082766, FRowTransitions = -5.2178882868487753, FColTransitions = -9.348695305445199, FHoles = -11.899265427351652, FWells = -2.6855972247263626, FSubh = 1.5, Frealholes = -4.9, Fsumholes = -12.9, Fdelay = -9.9333333333, Fcover = -8.17328742;//, Fgourd = -4.00000000;
double Feliminated = 7.4181268101392694;

/*
  int RD = 0;
  int gridv[2][30][30];

  int findGourd(int player, int l, int r, int h) {
  for (int i = l; i <= r; ++i) gridv[player][h][i] = RD;		
  for (int x = 1; x + h <= MAPHEIGHT; ++x) {
  int p = l;
  for ( ; p <= r && gridInfo[player][h + x][p]; ++p);
  if (p == r + 1) return 1; // return 0; 横着的一排算不算葫芦？
  int k = p;
  for ( ; k <= r && !gridInfo[player][h + x][k]; ++k);
  --k;
  if (x == 1 && p == l && k == r) return 0; //第一行一定要严格减少
  if (p == l && k == r && (!gridInfo[player][h + x][p - 1] || !gridInfo[player][h + x][k + 1])) return 1;
  for (int t = p; t <= k; ++t) gridv[player][h + x][t] = RD;
  l = p, r = k;
  }
  return 1;
  }
*/
/*
double value(int player) {	
	int row = 0, col = 0;
	for (int i = 1; i <= MAPHEIGHT; ++i) { //Row Transitions
		for (int j = 1; j <= MAPWIDTH + 1; ++j)
			if ((gridInfo[player][i][j] != 0 && gridInfo[player][i][j - 1] == 0) || (gridInfo[player][i][j] == 0 && gridInfo[player][i][j - 1] != 0)) ++row;
	}
	for (int i = 1; i <= MAPWIDTH; ++i) { //Colomn Transitions
		for (int j = 1; j <= MAPHEIGHT; ++j)
			if ((gridInfo[player][j - 1][i] != 0 && gridInfo[player][j][i] == 0) || (gridInfo[player][j - 1][i] == 0 && gridInfo[player][j][i] != 0)) ++col;
		if (gridInfo[player][MAPHEIGHT][i]) ++col;
	}

	qf = 1, qr = 0, ++rd;
	for (int i = 1; i <= MAPWIDTH; ++i) if (!gridInfo[player][MAPHEIGHT][i]) qx[++qr] = i, qy[qr] = MAPHEIGHT, v[i][MAPHEIGHT][0] = rd;
	for ( ; qf <= qr; ) {
		int x = qx[qf], y = qy[qf++];
		for (int k = 0; k < 4; ++k) {
			int xx = x + dx[k], yy = y + dy[k];
			if (xx < 1 || xx > MAPWIDTH || yy < 1 || yy > MAPHEIGHT) continue;
			if (v[xx][yy][0] == rd || gridInfo[player][yy][xx]) continue;
			qx[++qr] = xx, qy[qr] = yy, v[xx][yy][0] = rd;
		}
	}
	
	int holes = MAPHEIGHT * MAPWIDTH, maxH = 0, underblock = 0, lst = 0, diff = 0;
	for (int i = 1; i <= MAPWIDTH; ++i) {
		for (int j = MAPHEIGHT; j >= 1; --j) {
			if (gridInfo[player][j][i]) {
				if (i > 1 && abs(j - lst) >= 5) {
					diff += abs(j - lst) * (abs(j - lst) + 1) / 2;
				}
				lst = j;
				maxH = max(maxH, j);
				for (int k = j; k >= 1; --k)
					if (gridInfo[player][k][i]) --holes;
					else ++underblock;
				break;
			}
		}
	}
	holes -= qr;
	underblock -= holes;
	
	int wells = 0;
	for (int i = 1; i <= MAPWIDTH; ++i) {
		for (int j = 1; j <= MAPHEIGHT; ++j)
			if (!gridInfo[player][j][i] && gridInfo[player][j][i - 1] && gridInfo[player][j][i + 1]) {
				int k = j;
				for ( ; k <= MAPHEIGHT && !gridInfo[player][k][i] && gridInfo[player][k][i - 1] && gridInfo[player][k][i + 1]; ++k);
				//if (maxH < 17 && k - j >= 3 && k - j <= 4 && maxWells == 0) maxWells = k - j;
				wells += (k - j) * (k - j + 1) / 2;
				//++wells;
				j = k - 1;
			}
	}
	//wells = wells * wells;
	//if (player == enemyColor) maxWells = 0;
	//else maxWells = maxWells * (maxWells + 1) / 2;
	
	  int gourd = 0;
	  for (int j = 1; j < MAPHEIGHT; ++j) {
	  for (int i = 1; i < MAPWIDTH; ++i)
	  if (gridInfo[player][j][i - 1] && !gridInfo[player][j][i] && !gridInfo[player][j][i + 1] && gridInfo[player][j][i + 2]) {
	  int s = (gridInfo[player][j + 1][i] > 0) + (gridInfo[player][j + 1][i + 1] > 0);
	  if (s != 1) continue;
	  ++gourd;
	  }
	  }
	
	return FHeight * maxH + FRowTransitions * row + FColTransitions * col + Funderblock * underblock + FHoles * holes + FWells * wells + diff *  Fdiff;// + Fgourd * gourd;
	}*/
double subh = 0;
int f[1010], maxh[11] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

int fullline[21];
int originalFullline[2][21];

inline int find(int x) {
	return (f[x] == x ? x : f[x] = find(f[x]));
}	
inline double value(int player) {
	for (int j = 1; j <= MAPWIDTH; ++j) {
		maxh[j] = 0;
 		for (int i = MAPHEIGHT; i >= 1; --i)
			if (gridInfo[player][i][j]) {maxh[j] = i; break;}
	}
	subh = 0;
	if (player == 0 && maxh[7] == 7 && maxh[6] == 8)
		player = 0;
	int _maxH = maxh[MAPWIDTH], no = 0;
	for (int j = 1; j < MAPWIDTH; ++j)
		subh += pow(abs(maxh[j + 1] - maxh[j]), 1.2), _maxH = max(_maxH, maxh[j]);
	if (_maxH == 20) no = -400;
	if (_maxH == 19) no = -100;
	int row = 0, col = 0;
	for (int i = 1; i <= MAPHEIGHT; ++i) { //Row Transitions
		for (int j = 1; j <= MAPWIDTH + 1; ++j)
			if ((gridInfo[player][i][j] != 0 && gridInfo[player][i][j - 1] == 0) || (gridInfo[player][i][j] == 0 && gridInfo[player][i][j - 1] != 0)) ++row;
	}
	for (int i = 1; i <= MAPWIDTH; ++i) { //Colomn Transitions
		for (int j = 1; j <= MAPHEIGHT; ++j)
			if ((gridInfo[player][j - 1][i] != 0 && gridInfo[player][j][i] == 0) || (gridInfo[player][j - 1][i] == 0 && gridInfo[player][j][i] != 0)) ++col;
		if (gridInfo[player][MAPHEIGHT][i]) ++col;
	}

	int holes = 0, maxH = 0, minH = 22;
	for (int i = 1; i <= MAPWIDTH; ++i) {
		for (int j = MAPHEIGHT; j >= 1; --j) {
			if (gridInfo[player][j][i]) {
				maxH = max(maxH, j), minH = min(minH, j);
				for (int k = j; k >= 1; --k) if (!gridInfo[player][k][i]) ++holes;
				break;
			}
		}
	}
	for (int i = 1; i <= 10; i++)
		for (int j = 1; j <= 20; j++)
			f[(i - 1) * 20 + j] = (i - 1) * 20 + j;
	for (int i = 1; i <= 10; i++)
		for (int j = 1; j <= 20; j++) {
			if (gridInfo[player][j][i]) continue;
			if (i - 1 >= 1 && gridInfo[player][j][i - 1] == 0) f[find((i - 1) * 20 + j)] = find((i - 2) * 20 + j);
			if (j - 1 >= 1 && gridInfo[player][j - 1][i] == 0) f[find((i - 1) * 20 + j)] = find((i - 1) * 20 + j - 1);
		}
	f[1001] = 1001;
	for (int i = 1; i <= 10; i++)
		if (!gridInfo[player][20][i]) f[find(i * 20)] = 1001;
	int realholes = 0, sumholes = 0;
	for (int j = 1; j <= 19; j++) {
		int flag = 0;
		for (int i = 1; i <= 10; i++)
			if (!gridInfo[player][j][i] && find((i - 1) * 20 + j) != 1001) ++realholes, flag = 1;
		if (flag) sumholes++;
	}
	holes -= realholes;
	realholes -= sumholes;
	sumholes = sumholes * 2;

	for (int i = 1; i <= MAPHEIGHT; ++i) {
		fullline[i] = 0; int cntF = 0;
		for (int j = 1; j <= MAPWIDTH; ++j)
			if (gridInfo[player][i][j] > 0) ++cntF;
			else fullline[i] = j;
		if (cntF < MAPWIDTH - 1) fullline[i] = 0;
	}
	
	int wells = 0, delay = 0;
	for (int i = 1; i <= MAPWIDTH; ++i) {
		for (int j = 1; j <= MAPHEIGHT; ++j)
			if (!gridInfo[player][j][i] && gridInfo[player][j][i - 1] && gridInfo[player][j][i + 1]) {
				int k = j;
				bool flag = 1;
				for ( ; k <= MAPHEIGHT && !gridInfo[player][k][i] && gridInfo[player][k][i - 1] && gridInfo[player][k][i + 1]; ++k) if (!fullline[k]) flag = 0;
				wells += (k - j) * (k - j + 1) / 2;
				//if (j + 1 == k) continue;
				if (j < maxh[i] && flag) delay += (k - j) * (k - j + 1);
				j = k - 1;
			}
	}

	int cover = 0;
	for (int i = 1; i <= MAPHEIGHT; ++i) {
		if (!fullline[i]) continue;
		int j = i + 1;
		for ( ; j <= MAPHEIGHT && gridInfo[player][j][fullline[i]]; ++j);
		if (j - i - 1 > originalFullline[player][i] && j - i <= 4 && j - i > 1) cover += j - i - 1;
	}
	
	double sum = FHeight * maxH + FRowTransitions * row + FColTransitions * col + FHoles * holes + sumholes * Fsumholes + Frealholes * realholes + FWells * wells - FSubh * subh + no + Fdelay * delay + cover * Fcover;// + Fgourd * gourd;
	return sum;
}

double maxValue(int player, bitset <205> &grid, int block) {
	findSucc(grid, block, defaultDep, player);
	double res = -1e9;
	for (int i = 0, end = succ[defaultDep].size(); i < end; ++i) {
	    putBlock(grid, block, succ[defaultDep][i], 1);
		decompress(player, grid);
		Util :: eliminate(player);
		double cVal = value(player);
		res = max(res, cVal + transCount[player] * 2 * Feliminated);
		putBlock(grid, block, succ[defaultDep][i], 0);
	}
	return res;
}

int maxMyDep, maxEnemyDep;
int significantMove; //possbility -> e^(x / FAC) wieghted average
const int infinity = (int)1e9;

struct node {
	int board, eli;
	double evalue;
};

bool operator < (const node &a, const node &b) { return a.evalue > b.evalue; }

int finalAction;

double givenBlock[7];
bool canGive[7];

double maxV(vector <node> &c) {
	double res = 1e9;
	for (auto k = c.begin(); k != c.end(); ++k) res = min(res, fabs(k -> evalue));
	return res;
}

double myMCTS(int dep, bitset <205> grid, int block) {
	if (block == -1) {
		decompress(currBotColor, grid);
		Util :: eliminate(currBotColor);
		bitset <205> nextGrid;
		compress(currBotColor, nextGrid);
		
		int maxC = -1, minC = 101;
		for (int i = 0; i < 7; ++i) {
			if (typeCountForColor[1 - enemyColor][i] > maxC) maxC = typeCountForColor[1 - enemyColor][i];
			if (typeCountForColor[1 - enemyColor][i] < minC) minC = typeCountForColor[1 - enemyColor][i];
		}
		
		vector <node> expand;
		for (int i = 0; i < 7; ++i) {
			if (maxC - minC == 2 && typeCountForColor[1 - enemyColor][i] == maxC) continue;
			decompress(currBotColor, nextGrid);
			expand.push_back((node){i, 0, maxValue(currBotColor, nextGrid, i)});
		}

		int sz = expand.size();
		sort(expand.begin(), expand.end());

		
		for (int i = sz - 1, end = max(0, sz - significantMove); i >= end; --i) {
			++typeCountForColor[1 - enemyColor][expand[i].board];
			expand[i].evalue = myMCTS(dep, nextGrid, expand[i].board);
			//expand[i].evalue = myMCTS(dep, nextGrid, expand[i].board);
			--typeCountForColor[1 - enemyColor][expand[i].board];			
		}

		double resW = 0, sumW = 0, FAC = 1e9;
		for (int i = sz - 1, end = max(0, sz - significantMove); i >= end; --i) FAC = min(FAC, fabs(expand[i].evalue));
		for (double k = 1; ; k *= 2) if (k > FAC) { FAC = k / 2; break; }
		for (int i = sz - 1, end = max(0, sz - significantMove); i >= end; --i) sumW += exp(-expand[i].evalue / FAC);
		for (int i = sz - 1, end = max(0, sz - significantMove); i >= end; --i) {
			double fac = exp(-expand[i].evalue / FAC) / sumW;
			resW += fac * expand[i].evalue;
		}
		return resW;
	}
	else {
		bitset <205> nextGrid = grid;
		decompress(currBotColor, grid);
		findSucc(grid, block, dep, currBotColor);
		vector <node> expand;
		for (int i = 0, end = (int)succ[dep].size(); i < end; ++i) {
			putBlock(nextGrid, block, succ[dep][i], 1);
			decompress(currBotColor, nextGrid);
			Util :: eliminate(currBotColor);
			double cVal = value(currBotColor);
			expand.push_back((node){succ[dep][i], transCount[currBotColor], cVal + transCount[currBotColor] * 2 * Feliminated});
			putBlock(nextGrid, block, succ[dep][i], 0);
		}

		int sz = expand.size();
		sort(expand.begin(), expand.end());
		if (!sz) return -1e9;
		
		int res = -1;
		double resW = 0;
		for (int i = 0, end = min(sz, significantMove); i < end; ++i) {
			if (dep < maxMyDep) {
				putBlock(nextGrid, block, expand[i].board, 1);
				expand[i].evalue = myMCTS(dep + 1, nextGrid, -1) + Feliminated * expand[i].eli * 2;
				//expand[i].evalue = myMCTS(dep + 1, nextGrid, -1);
				putBlock(nextGrid, block, expand[i].board, 0);
			}
			if (res == -1 || expand[i].evalue > expand[res].evalue) res = i;
		}
		resW = expand[res].evalue;
		return finalAction = expand[res].board, resW;
	}
}

//double enemyVal;

int maxGap;

double enemyMCTS(int dep, bitset <205> grid, int block, double WG) { //这个里的dep要比上面那个dep的初始值大1
	if (block == -1) {
		decompress(enemyColor, grid);
		Util :: eliminate(enemyColor);
		bitset <205> nextGrid;
		compress(enemyColor, nextGrid);

		int maxC = -1, minC = 101;
		for (int i = 0; i < 7; ++i) {
			if (typeCountForColor[enemyColor][i] > maxC) maxC = typeCountForColor[enemyColor][i];
			if (typeCountForColor[enemyColor][i] < minC) minC = typeCountForColor[enemyColor][i];
		}

		vector <node> expand;
		for (int i = 0; i < 7; ++i) {
			if (maxC - minC == maxGap && typeCountForColor[enemyColor][i] == maxC) continue;
			decompress(enemyColor, nextGrid);
			expand.push_back((node){i, 0, maxValue(enemyColor, nextGrid, i)});
		}

		int sz = expand.size();
		sort(expand.begin(), expand.end());
		
		double resW = 1e9;
		for (int i = sz - 1, end = max(0, sz - significantMove); i >= end; --i) {
			++typeCountForColor[enemyColor][expand[i].board];
			expand[i].evalue = enemyMCTS(dep, nextGrid, expand[i].board, WG);
			//expand[i].evalue = enemyMCTS(dep, nextGrid, expand[i].board, WG);
			--typeCountForColor[enemyColor][expand[i].board];
			if (dep == 1) givenBlock[expand[i].board] += WG * expand[i].evalue;
			if (expand[i].evalue < resW) resW = expand[i].evalue;
		}
		return resW;
	}
	else {
		bitset <205> nextGrid = grid;
		decompress(enemyColor, grid);
		findSucc(grid, block, dep, enemyColor);
		vector <node> expand;
		for (int i = 0, end = (int)succ[dep].size(); i < end; ++i) {
			putBlock(nextGrid, block, succ[dep][i], 1);
			decompress(enemyColor, nextGrid);
			Util :: eliminate(enemyColor);
			double cVal = value(enemyColor);
			expand.push_back((node){succ[dep][i], transCount[enemyColor], cVal + transCount[enemyColor] * 2 * Feliminated});
			putBlock(nextGrid, block, succ[dep][i], 0);
		}

		int sz = expand.size();
		sort(expand.begin(), expand.end());
		if (!sz) return -1e9;
		
		for (int i = 0, end = min(sz, significantMove); i < end; ++i) {
			if (dep < maxEnemyDep) {
				putBlock(nextGrid, block, expand[i].board, 1);
				expand[i].evalue = enemyMCTS(dep + 1, nextGrid, -1, WG) + Feliminated * expand[i].eli * 2;
				//expand[i].evalue = enemyMCTS(dep + 1, nextGrid, -1, WG);
				putBlock(nextGrid, block, expand[i].board, 0);
			}
		}
		
		double resW = 0, sumW = 0, FAC = 1e9;
		for (int i = 0, end = min(sz, significantMove); i < end; ++i) FAC = min(FAC, fabs(expand[i].evalue));
		for (double k = 1; ; k *= 2) if (k > FAC) { FAC = k / 2; break; }
		for (int i = 0, end = min(sz, significantMove); i < end; ++i) sumW += exp(expand[i].evalue / FAC);
		for (int i = 0, end = min(sz, significantMove); i < end; ++i) {
			double fac = exp(expand[i].evalue / FAC) / sumW;
			resW += fac * expand[i].evalue;
		}		
		return resW;
	}
}

int etop;

void getFullline() {
	for (int player = 0; player <= 1; ++player) {
		for (int i = 1; i <= MAPHEIGHT; ++i) {
			int cntF = 0;
			for (int j = 1; j <= MAPWIDTH; ++j) cntF += (gridInfo[player][i][j] > 0);
			if (cntF < MAPWIDTH - 1) originalFullline[player][i] = 0;
			else {
				for (int k = 1; k <= MAPWIDTH; ++k)
					if (!gridInfo[player][i][k]) {
						int j = i + 1;
						for ( ; j <= MAPHEIGHT && gridInfo[player][j][k]; ++j);
						if (j - i - 1 > 0) originalFullline[player][i] = j - i - 1;
						else originalFullline[player][i] = 0;
					}
			}			
		}		
	}
}

int getMyOperations(int turnID, int block) {
	bitset <205> start;
	compress(currBotColor, start);
	int maxH = 0;
	for (int i = 1; i <= MAPWIDTH; ++i) {
		for (int j = MAPHEIGHT; j >= 1; --j)
			if (gridInfo[currBotColor][j][i]) { if (j > maxH) maxH = j; break; }
	}
    if (turnID > 5) maxMyDep = 3, significantMove = 3;
	else maxMyDep = 0, significantMove = 3;
	//if (maxH >= 18) Feliminated += (maxH - 17) * 25;
	//if (etop >= 19) Feliminated += (etop - 18) * 20, maxMyDep = 0, significantMove = 5;
	finalAction = -1;
	myMCTS(0, start, block);
	decompress(currBotColor, start);
	assert(finalAction != -1);
	return finalAction;
}

int getEnemyOperations(int turnID, int block) {
	getFullline();
	bitset <205> start;
	compress(enemyColor, start);
	int minH = 22;
	for (int i = 1; i <= MAPWIDTH; ++i) {
		for (int j = MAPHEIGHT; j >= 1; --j)
			if (gridInfo[enemyColor][j][i]) { if (j < minH) minH = j; break; }
	}    
	maxEnemyDep = 2, significantMove = 3;
	if (turnID <= 7) maxGap = 1;
	else maxGap = 2;
	//maxGap = 2;
	
	for (int i = 0; i < 7; ++i) givenBlock[i] = 0, canGive[i] = 0;
	int maxC = -1, minC = 101;
	for (int i = 0; i < 7; ++i) {
		if (typeCountForColor[enemyColor][i] > maxC) maxC = typeCountForColor[enemyColor][i];
		if (typeCountForColor[enemyColor][i] < minC) minC = typeCountForColor[enemyColor][i];
	}
	
	for (int i = 0; i < 7; ++i)
		if (maxC - minC < maxGap || typeCountForColor[enemyColor][i] < maxC) canGive[i] = 1;

	vector <node> expand;
	decompress(enemyColor, start);
	findSucc(start, block, defaultDep, enemyColor);
	for (int i = 0, end = (int)succ[defaultDep].size(); i < end; ++i) {
		putBlock(start, block, succ[defaultDep][i], 1);
		decompress(enemyColor, start);
		Util :: eliminate(enemyColor);
		double cVal = value(enemyColor);
		expand.push_back((node){succ[defaultDep][i], transCount[enemyColor], cVal + transCount[enemyColor] * 2 * Feliminated});
		putBlock(start, block, succ[defaultDep][i], 0);
	}

	int sz = expand.size();
	sort(expand.begin(), expand.end());
	
	double sumW = 0;
	double FAC = 1e9;
	for (int i = 0, end = min(sz, significantMove); i < end; ++i) FAC = min(FAC, fabs(expand[i].evalue));
	for (double k = 1; ; k *= 2) if (k > FAC) { FAC = k / 2; break; }
	for (int i = 0, end = min(sz, significantMove); i < end; ++i) sumW += exp(expand[i].evalue / FAC);
	
	for (int i = 0, end = min(sz, significantMove); i < end; ++i) {		
		putBlock(start, block, expand[i].board, 1);
		enemyMCTS(1, start, -1, exp(expand[i].evalue / FAC) / sumW);
		putBlock(start, block, expand[i].board, 0);
	}

	expand.clear(), sumW = 0;
	for (int i = 0; i < 7; ++i) {
		if (canGive[i]) expand.push_back((node){i, 0, -givenBlock[i]});
	}

	decompress(enemyColor, start);

	sort(expand.begin(), expand.end());
	if (turnID > 30 || turnID % 10) return expand[0].board;
	for ( ; expand.size() > 3; expand.pop_back()); //随机调整
	FAC = maxV(expand);
	for (int i = 0, end = expand.size(); i < end; ++i) expand[i].evalue = exp(expand[i].evalue / FAC), sumW += expand[i].evalue;

	double res = sumW * (rand() / (double)RAND_MAX), cur = 0;
	for (int i = 0, end = expand.size(); i < end; ++i)
		if (res >= cur && res <= cur + expand[i].evalue) return expand[i].board;
		else cur += expand[i].evalue;
	
	return sz > 0 ? expand[0].board : -1;
}

int main()
{
#ifndef _BOTZONE_ONLINE
	freopen("main.in", "r", stdin);
	freopen("main.out","w", stdout);
#endif
	
	// 加速输入
	istream::sync_with_stdio(false);
	srand(time(NULL));
	init();
	
	int turnID, blockType;
	int nextTypeForColor[2];
	for (int i = 1; i <= 100; ++i) turnID = rand();
	// 读入JSON
	string str;
	getline(cin, str);
	Json::Reader reader;
	Json::Value input;
	reader.parse(str, input);
 
	// 先读入第一回合，得到自己的颜色
	// 双方的第一块肯定是一样的
	turnID = input["responses"].size() + 1;
	auto &first = input["requests"][(Json::UInt) 0];
 
	blockType = first["block"].asInt();
	currBotColor = first["color"].asInt();
 
	enemyColor = 1 - currBotColor;
	nextTypeForColor[0] = blockType;
	nextTypeForColor[1] = blockType;
	typeCountForColor[0][blockType]++;
	typeCountForColor[1][blockType]++;
 
 
	// 然后分析以前每回合的输入输出，并恢复状态
	// 循环中，color 表示当前这一行是 color 的行为
	// 平台保证所有输入都是合法输入
	for (int i = 1; i < turnID; i++)
		{
			int currTypeForColor[2] = { nextTypeForColor[0], nextTypeForColor[1] };
			int x, y, o;
			// 根据这些输入输出逐渐恢复状态到当前回合
 
			// 先读自己的输出，也就是自己的行为
			// 自己的输出是一个序列，但是只有最后一步有用
			// 所以只保留最后一步
			// 然后模拟最后一步放置块
			auto &myOutput = input["responses"][i - 1];
			blockType = myOutput["block"].asInt();
			x = myOutput["x"].asInt();
			y = myOutput["y"].asInt();
			o = myOutput["o"].asInt();
 
			// 我当时把上一块落到了 x y o！
			Tetris myBlock(currTypeForColor[currBotColor], currBotColor);
			myBlock.set(x, y, o).place();
 
			// 我给对方什么块来着？
			typeCountForColor[enemyColor][blockType]++;
			nextTypeForColor[enemyColor] = blockType;
 
			// 然后读自己的输入，也就是对方的行为
			// 裁判给自己的输入是对方的最后一步
			auto &myInput = input["requests"][i];
			blockType = myInput["block"].asInt();
			x = myInput["x"].asInt();
			y = myInput["y"].asInt();
			o = myInput["o"].asInt();
 
			// 对方当时把上一块落到了 x y o！
			Tetris enemyBlock(currTypeForColor[enemyColor], enemyColor);
			enemyBlock.set(x, y, o).place();
 
			// 对方给我什么块来着？
			typeCountForColor[currBotColor][blockType]++;
			nextTypeForColor[currBotColor] = blockType;
 
			// 检查消去
			Util::eliminate(0);
			Util::eliminate(1);
 
			// 进行转移
			Util::transfer();
		}
 
 
	int blockForEnemy, finalX, finalY, finalO;
 
	// 做出决策（你只需修改以下部分）
 
	// 遇事不决先输出（平台上运行不会输出）
	Util::printField();
  
	// 决策结束，输出结果（你只需修改以上部分）
 
	Json::Value output;
	Json::FastWriter writer;
	
	prePutblock();
	//myBlock = 21;
	blockForEnemy = getEnemyOperations(turnID, nextTypeForColor[enemyColor]);
	int myBlock = getMyOperations(turnID, nextTypeForColor[currBotColor]);
		
	int maxC = -1, minC = 101;
	for (int i = 0; i < 7; ++i) {
		if (typeCountForColor[enemyColor][i] > maxC) maxC = typeCountForColor[enemyColor][i];
		if (typeCountForColor[enemyColor][i] < minC) minC = typeCountForColor[enemyColor][i];
	}
	
	/*if (turnID <= 22) {
		int tp = rand() % 20;
		if (tp < 12 && typeCountForColor[enemyColor][5] < maxC) blockForEnemy = 5;
		}*/
	
	//blockForEnemy = enemy :: work();
	//myBlock = 255;
	finalY = myBlock % 20 + 1;
	finalX = (myBlock - (finalY - 1)) % 200 / 20 + 1;
	finalO = (myBlock - (finalX - 1) * 20 - (finalY - 1)) / 200;
	
	output["response"]["block"] = blockForEnemy;
 
	output["response"]["x"] = finalX;
	output["response"]["y"] = finalY;
	output["response"]["o"] = finalO;
 
	cout << writer.write(output);
	//cout << myBlock << endl;	
	Tetris OUT(nextTypeForColor[currBotColor], currBotColor);
	OUT.set(finalX, finalY, finalO).place();
	
	//Tetris outE(nextTypeForColor[enemyColor], enemyColor);
	//myBlock = 656;
	//finalY = myBlock % 20 + 1;
	//finalX = (myBlock - (finalY - 1)) % 200 / 20 + 1;
	//finalO = (myBlock - (finalX - 1) * 20 - (finalY - 1)) / 200;
	//outE.set(finalX, finalY, finalO).place();
	
	Util::printField();
	
	cout << myBlock << " " << blockForEnemy << endl;
	return 0;
}
