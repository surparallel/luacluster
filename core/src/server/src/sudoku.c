/* docker.c - worker thread
*
* Copyright(C) 2021 - 2022, sun shuo <sun.shuo@surparallel.org>
* All rights reserved.
*
* This program is free software : you can redistribute it and / or modify
* it under the terms of the GNU Affero General Public License as
* published by the Free Software Foundation, either version 3 of the
* License, or(at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.See the
* GNU Affero General Public License for more details.
*
* You should have received a copy of the GNU Affero General Public License
* along with this program.If not, see < https://www.gnu.org/licenses/>.
*/

//Sudoku

#include "plateform.h"
#include "dict.h"
#include "lua.h"
#include "lauxlib.h"
#include "lualib.h"
#include "elog.h"
#include "sds.h"
#include "filesys.h"
#include "dicthelp.h"
#include "lvm.h"
#include "entityid.h"
#include "networking.h"
#include "int64.h"
#include "timesys.h"
#include "adlist.h"
#include "dict.h"
#include "transform.h"
#include "quaternion.h"
#include "vector.h"
#include "vector2.h"
#include "dicthelp.h"
#include <math.h>
#include "lua_cmsgpack.h"
#include "docker.h"
#include "int64.h"
#include "sudoku.h"

typedef struct _Entity {
	unsigned long long entityid;
	listNode* listNode;
	listNode* girdNode;//网格grid_entity的node为了快速查找和删除node
	list* entityview;//dict-list 某个时间点进入视角的玩家列表
	struct BasicTransform transform;
	struct Vector3 nowPotion;
	float velocity;//速度
	unsigned int stamp;
	unsigned int oldGird;//旧的网格id
	int update;//上一帧中是否移动过网格
}*PEntity, Entity;

//网格相当于缩小的坐标系统。例如网格和坐标比例是1比10。哪么0到10就是在网格1 * 1位置, 也就是编号为1的网格。
//但地图原点也就是begin点并不一定是0，0。也可能是-99，-99。这时负数的网格就不能用简单的乘法来表示了。
//对于任意坐标。减去begin获得相对坐标值，然后除以网格长度，再使用ceil向上取整。然后x * y获得网格id
typedef struct _Sudoku {
	list* entitiesList;//所有玩家 要保存所在网络的entity 为所有的查询工作
	dict* entitiesDict;//所有玩家 要保存所在网络的id和list entity指针为了第二步收集可见玩家，
	dict* grid_entity;//dict-list 网格内的玩家数据。
	struct Vector3 gird;//网格的长宽，是从配置文件中设置的
	struct Vector3 begin, end;//地图原点和结束点，和unity一样是从左下角开始到右上角
	unsigned int linex, maxid;
}*PSudoku, Sudoku;

unsigned int GirdId(PSudoku pSudoku, struct Vector3 position) {
	unsigned int dx = (unsigned int)ceil((position.x - pSudoku->begin.x) / pSudoku->gird.x);
	unsigned int dz = (unsigned int)floor((position.z - pSudoku->begin.z) / pSudoku->gird.z);

	return dx + pSudoku->linex * dz;
}

enum SudokuDir {
	up = 0,
	down,
	left,
	right,
	upleft,
	upright,
	downleft,
	downright,
	dircentre,
	SudokuDirError
};

unsigned int GirdDirId(PSudoku pSudoku, enum SudokuDir dir, unsigned int centre) {

	unsigned int ret = 0;
	switch (dir)
	{
	case up:
		ret = centre + pSudoku->linex;
		if (ret > pSudoku->maxid) {
			ret = MAXUINT32;
		}
		break;
	case down:
		if (centre <= pSudoku->linex)
			ret = MAXUINT32;
		else
			ret = centre - pSudoku->linex;
		break;
	case left:
		if (floor(centre / pSudoku->linex) == floor((centre - 1) / pSudoku->linex))
			ret = centre - 1;
		else
			ret = MAXUINT32;
		break;
	case right:
		if (floor(centre / pSudoku->linex) == floor((centre + 1 - 0.001) / pSudoku->linex))
			ret = centre + 1;
		else
			ret = MAXUINT32;
		break;
	case upleft:
		if (MAXUINT32 == GirdDirId(pSudoku, up, centre) || MAXUINT32 == GirdDirId(pSudoku, left, centre))
			ret = MAXUINT32;
		else
			ret = centre + pSudoku->linex - 1;
		break;
	case upright:
		if (MAXUINT32 == GirdDirId(pSudoku, up, centre) || MAXUINT32 == GirdDirId(pSudoku, right, centre))
			ret = MAXUINT32;
		else
			ret = centre + pSudoku->linex + 1;
		break;
	case downleft:
		if (MAXUINT32 == GirdDirId(pSudoku, down, centre) || MAXUINT32 == GirdDirId(pSudoku, left, centre))
			ret = MAXUINT32;
		else
			ret = centre - pSudoku->linex - 1;
		break;
	case downright:
		if (MAXUINT32 == GirdDirId(pSudoku, down, centre) || MAXUINT32 == GirdDirId(pSudoku, right, centre))
			ret = MAXUINT32;
		else
			ret = centre - pSudoku->linex + 1;
		break;
	case dircentre:
		ret = centre;
		break;

	default:
		break;
	}

	return ret;
}

int GirdDir(void* pSudoku, unsigned int entry, unsigned int centre) {

	for (int i = up; i <= downright; i++) {

		if (entry == GirdDirId(pSudoku, i, centre))
			return i;
	}

	return SudokuDirError;
}

void SudokuMove(void* pSudoku, unsigned long long id, struct Vector3 position, struct Vector3 rotation, float velocity, unsigned int stamp) {
	PSudoku s = (PSudoku)pSudoku;

	//查找对应的id
	dictEntry* entry = dictFind(s->entitiesDict, &id);

	if (entry == NULL) {
		n_error("sudoku::move error not find id %U", id);
	}

	PEntity pEntity = dictGetVal(entry);

	//position
	pEntity->transform.position.x = position.x;
	pEntity->transform.position.z = position.z;
	pEntity->nowPotion.x = position.x;
	pEntity->nowPotion.z = position.z;

	//rotation
	rotation.y = rotation.y * RADIANS_PER_DEGREE;

	quatEulerAngles(&rotation, &pEntity->transform.rotation);

	//velocity 每秒移动的速度
	pEntity->velocity = velocity;

	//stamp 秒
	pEntity->stamp = stamp;
}

static int luaB_Move(lua_State* L) {
	void* pVoid = LVMGetGlobleLightUserdata(L, "dockerHandle");
	PSudoku s = lua_touserdata(L, 1);
	unsigned long long id = luaL_tou64(L, 2);

	//position
	struct Vector3 position = { 0 };
	position.x = luaL_checknumber(L, 3);
	position.z = luaL_checknumber(L, 4);

	//rotation
	struct Vector3 rotation = { 0 };
	rotation.y = luaL_checknumber(L, 5);

	//velocity 每秒移动的速度
	float velocity = luaL_checknumber(L, 6);

	//stamp 秒
	float stamp = luaL_checkinteger(L, 7);

	SudokuMove(s, id, position, rotation, velocity, stamp);
	return 0;
}

void SudokuLeave(void* pSudoku, unsigned long long id) {
	PSudoku s = (PSudoku)pSudoku;

	//查找对应的id
	dictEntry* entry = dictFind(s->entitiesDict, &id);

	if (entry == NULL) {
		n_error("sudoku::move error not find id %U", id);
	}
	PEntity pEntity = dictGetVal(entry);

	listDelNode(s->entitiesList, pEntity->listNode);

	unsigned int gird = GirdId(s, pEntity->transform.position);
	entry = dictFind(s->grid_entity, &gird);
	list* girdlist = NULL;
	if (entry == NULL) {
		n_error("sudoku::move error not find gird id %i", gird);
	}
	else {
		girdlist = dictGetVal(entry);
	}
	listDelNode(girdlist, pEntity->girdNode);

	if (listLength(girdlist) == 0) {
		dictDelete(s->grid_entity, &gird);
	}

	listRelease(pEntity->entityview);
	free(pEntity);
}

static int luaB_Leave(lua_State* L) {
	void* pVoid = LVMGetGlobleLightUserdata(L, "dockerHandle");
	PSudoku s = lua_touserdata(L, 1);
	unsigned long long id = luaL_tou64(L, 2);

	SudokuLeave(s, id);
	return 0;
}

void SudokuEntry(void* pSudoku, unsigned long long id, struct Vector3 position, struct Vector3 rotation, float velocity, unsigned int stamp) {
	PSudoku s = (PSudoku)pSudoku;

	if (position.x < s->begin.x || position.x > s->end.x || position.z < s->begin.z || position.z > s->end.z) {

		n_error("entry out limit %U", id);
		return;
	}

	dictEntry* entry = dictFind(s->entitiesDict, &id);
	if (entry != NULL) {
		n_error("entry already in world");
		return;
	}

	PEntity pEntity = calloc(1, sizeof(Entity));

	pEntity->entityid = id;
	pEntity->entityview = listCreate();

	//position
	pEntity->transform.position.x = position.x;
	pEntity->transform.position.z = position.z;
	pEntity->nowPotion.x = position.x;
	pEntity->nowPotion.z = position.z;

	//rotation
	rotation.y = rotation.y * RADIANS_PER_DEGREE;

	quatEulerAngles(&rotation, &pEntity->transform.rotation);

	//velocity 每秒移动的速度
	pEntity->velocity = velocity;

	//stamp 秒
	pEntity->stamp = stamp;

	pEntity->update = 2;

	pEntity->oldGird = GirdId(s, pEntity->nowPotion);

	listAddNodeHead(s->entitiesList, pEntity);
	pEntity->listNode = listFirst(s->entitiesList);
	dictAddWithLonglong(s->entitiesDict, id, pEntity);

	//放入gird
	unsigned int gird = GirdId(s, pEntity->transform.position);
	entry = dictFind(s->grid_entity, &gird);
	list* girdlist = NULL;
	if (entry == NULL) {
		girdlist = listCreate();
		dictAddWithUint(s->grid_entity, gird, girdlist);
	}
	else {
		girdlist = dictGetVal(entry);
	}

	listAddNodeHead(girdlist, pEntity);
	pEntity->girdNode = listFirst(girdlist);
}

static int luaB_Entry(lua_State* L) {
	void* pVoid = LVMGetGlobleLightUserdata(L, "dockerHandle");
	PSudoku s = lua_touserdata(L, 1);
	unsigned long long id = luaL_tou64(L, 2);

	//position
	struct Vector3 position = { 0 };
	position.x = luaL_checknumber(L, 3);
	position.z = luaL_checknumber(L, 4);

	//rotation
	struct Vector3 rotation = { 0 };
	rotation.y = luaL_checknumber(L, 5);

	//velocity 每秒移动的速度
	float velocity = luaL_checknumber(L, 6);

	//stamp 秒
	float stamp = luaL_checkinteger(L, 7);

	SudokuEntry(s, id, position, rotation, velocity, stamp);
	return 0;
}

void FillView(PSudoku s, enum SudokuDir dir, unsigned int centre, PEntity pEntityEntry) {

	unsigned int viewgird = GirdDirId(s, dir, centre);
	if (MAXUINT32 == viewgird) {
		//n_error("FillView not GirdDirId gird %i", viewgird);
		return;
	}

	dictEntry* entry = dictFind(s->grid_entity, &viewgird);
	if (entry == NULL) {
		//n_error("FillView not find gird %i", viewgird);
	}
	else {
		list* newview = dictGetVal(entry);

		listIter* iter = listGetIterator(newview, AL_START_HEAD);
		listNode* node;
		while ((node = listNext(iter)) != NULL) {
			PEntity pEntity = listNodeValue(node);

			if (pEntity->entityid != pEntityEntry->entityid)
				listAddNodeTail(pEntityEntry->entityview, pEntity);

			if (pEntity->update != 0)
				continue;

			listAddNodeTail(pEntity->entityview, pEntityEntry);
		}
	}
}

void SudokuUpdate(void* pSudoku) {
	PSudoku s = (PSudoku)pSudoku;

	listIter* iter = listGetIterator(s->entitiesList, AL_START_HEAD);
	listNode* node;
	while ((node = listNext(iter)) != NULL) {
		PEntity pEntity = listNodeValue(node);

		if (pEntity->velocity == 0 || pEntity->update == 2)
			continue;

		pEntity->oldGird = GirdId(s, pEntity->nowPotion);

		struct Vector3 forward, nowPotion;
		transformForward(&pEntity->transform, &forward);
		float step = (GetCurrentSec() - pEntity->stamp) * pEntity->velocity;

		vector3Scale(&forward, &forward, step);
		vector3Add(&pEntity->transform.position, &forward, &nowPotion);

		if (nowPotion.x < s->begin.x || nowPotion.x > s->end.x || nowPotion.z < s->begin.z || nowPotion.z > s->end.z) {
			continue;
		}
		pEntity->nowPotion = nowPotion;

		unsigned int girdid = GirdId(s, pEntity->nowPotion);
		if (pEntity->oldGird == girdid) {
			pEntity->update = 0;
		}
		else {
			//更换gird
			pEntity->update = 1;

			dictEntry* entry = dictFind(s->grid_entity, &pEntity->oldGird);
			list* girdlist = NULL;
			if (entry == NULL) {
				n_error("sudoku::Update error not find gird id %i", pEntity->oldGird);
				continue;
			}
			else {
				girdlist = dictGetVal(entry);
			}

			listPickNode(girdlist, pEntity->girdNode);

			if (listLength(girdlist) == 0) {
				dictDelete(s->grid_entity, &pEntity->oldGird);
			}

			entry = dictFind(s->grid_entity, &girdid);
			if (entry == NULL) {
				girdlist = listCreate();
				dictAddWithUint(s->grid_entity, girdid, girdlist);
			}
			else {
				girdlist = dictGetVal(entry);
			}

			listAddNodeHeadForNode(girdlist, pEntity->girdNode);
			pEntity->girdNode = listFirst(girdlist);
		}
	}
	listReleaseIterator(iter);


	iter = listGetIterator(s->entitiesList, AL_START_HEAD);
	node;
	while ((node = listNext(iter)) != NULL) {
		//搜索周围生成每个用户的listview
		PEntity pEntity = listNodeValue(node);

		if (pEntity->update == 1) {
			unsigned int gird = GirdId(s, pEntity->nowPotion);
			enum SudokuDir sudokuDir = GirdDir(s, gird, pEntity->oldGird);

			switch (sudokuDir)
			{
			case up:
			{
				FillView(s, up, gird, pEntity);
				FillView(s, upleft, gird, pEntity);
				FillView(s, upright, gird, pEntity);
			}
			break;
			case down:
			{
				FillView(s, down, gird, pEntity);
				FillView(s, downleft, gird, pEntity);
				FillView(s, downright, gird, pEntity);
			}
			break;
			case left:
			{
				FillView(s, upleft, gird, pEntity);
				FillView(s, left, gird, pEntity);
				FillView(s, downleft, gird, pEntity);
			}
			break;
			case right:
			{
				FillView(s, upright, gird, pEntity);
				FillView(s, right, gird, pEntity);
				FillView(s, downright, gird, pEntity);
			}
			break;
			case upleft:
			{
				FillView(s, up, gird, pEntity);
				FillView(s, upleft, gird, pEntity);
				FillView(s, upright, gird, pEntity);
				FillView(s, left, gird, pEntity);
				FillView(s, downleft, gird, pEntity);
			}
			break;
			case upright:
			{
				FillView(s, up, gird, pEntity);
				FillView(s, upleft, gird, pEntity);
				FillView(s, upright, gird, pEntity);
				FillView(s, right, gird, pEntity);
				FillView(s, downright, gird, pEntity);
			}
			break;
			case downleft:
			{
				FillView(s, upleft, gird, pEntity);
				FillView(s, left, gird, pEntity);
				FillView(s, downleft, gird, pEntity);
				FillView(s, down, gird, pEntity);
				FillView(s, downright, gird, pEntity);
			}
			break;
			case downright:
			{
				FillView(s, upright, gird, pEntity);
				FillView(s, right, gird, pEntity);
				FillView(s, downright, gird, pEntity);
				FillView(s, down, gird, pEntity);
				FillView(s, downleft, gird, pEntity);
			}
			break;
			default:
				n_error("SudokuUpdate::step2 eid: %U, np: %i, op: %i", pEntity->entityid, gird, pEntity->oldGird);
			}
		}
		else if (pEntity->update == 2) {
			unsigned int gird = GirdId(s, pEntity->nowPotion);

			FillView(s, up, gird, pEntity);
			FillView(s, down, gird, pEntity);
			FillView(s, left, gird, pEntity);
			FillView(s, right, gird, pEntity);
			FillView(s, upleft, gird, pEntity);
			FillView(s, upright, gird, pEntity);
			FillView(s, downleft, gird, pEntity);
			FillView(s, downright, gird, pEntity);
			FillView(s, dircentre, gird, pEntity);
		}
	}
	listReleaseIterator(iter);

	iter = listGetIterator(s->entitiesList, AL_START_HEAD);
	node;
	while ((node = listNext(iter)) != NULL) {
		PEntity pEntity = listNodeValue(node);
		pEntity->update = 0;

		if (listLength(pEntity->entityview) != 0) {
			mp_buf* pmp_buf = mp_buf_new();

			mp_encode_bytes(pmp_buf, "OnAddView", sizeof("OnAddView"));
			mp_encode_array(pmp_buf, listLength(pEntity->entityview));

			listIter* viewiter = listGetIterator(pEntity->entityview, AL_START_HEAD);
			listNode* viewnode;
			while ((viewnode = listNext(viewiter)) != NULL) {
				// 发送并清空listview到客户端
				PEntity pViewEntity = listNodeValue(viewnode);

				mp_encode_array(pmp_buf, 6);
				mp_encode_double(pmp_buf, u642double(pViewEntity->entityid));
				mp_encode_double(pmp_buf, pViewEntity->transform.position.x);
				mp_encode_double(pmp_buf, pViewEntity->transform.position.z);
				mp_encode_double(pmp_buf, pViewEntity->transform.rotation.y);
				mp_encode_double(pmp_buf, pViewEntity->velocity);
				mp_encode_int(pmp_buf, pViewEntity->stamp);

				listDelNode(pEntity->entityview, viewnode);
			}
			listReleaseIterator(viewiter);

			DockerSend(NULL, pEntity->entityid, pmp_buf->b, pmp_buf->len);
			mp_buf_free(pmp_buf);
		}
	}
	listReleaseIterator(iter);
}

void PrintPoition(void* pSudoku) {
	PSudoku s = (PSudoku)pSudoku;

	listIter* iter = listGetIterator(s->entitiesList, AL_START_HEAD);
	listNode* node;
	while ((node = listNext(iter)) != NULL) {
		PEntity pEntity = listNodeValue(node);

		printf("%llu %f %f\n", pEntity->entityid, pEntity->nowPotion.x, pEntity->nowPotion.z);
	}
	listReleaseIterator(iter);
}


//这里要发送广播
static int luaB_Update(lua_State* L) {
	void* pVoid = LVMGetGlobleLightUserdata(L, "dockerHandle");
	PSudoku s = lua_touserdata(L, 1);

	SudokuUpdate(s);
	return 1;
}

static void freeValueCallback(void* privdata, void* val) {
	DICT_NOTUSED(privdata);
	listRelease(val);
}

static dictType UintDictType = {
	hashCallback,
	NULL,
	NULL,
	uintCompareCallback,
	memFreeCallback,
	freeValueCallback
};

void* SudokuCreate(struct Vector3 gird, struct Vector3 begin, struct Vector3 end) {
	PSudoku s = calloc(1, sizeof(Sudoku));

	s->entitiesList = listCreate();
	s->entitiesDict = dictCreate(DefaultLonglongPtr(), NULL);
	s->grid_entity = dictCreate(DefaultUintPtr(), NULL);

	s->gird = gird;
	s->begin = begin;
	s->end = end;

	s->end.x = s->end.x - 0.00001;
	s->end.z = s->end.z - 0.00001;

	s->linex = (unsigned int)ceil((s->end.x - s->begin.x) / s->gird.x);
	unsigned int dz = (unsigned int)floor((s->end.z - s->begin.z) / s->gird.z);
	s->maxid = s->linex + s->linex * dz;

	return s;
}

static int luaB_Create(lua_State* L) {
	void* pVoid = LVMGetGlobleLightUserdata(L, "dockerHandle");
	struct Vector3 gird;
	struct Vector3 begin;
	struct Vector3 end;
	gird.x = luaL_checknumber(L, 1);
	gird.z = luaL_checknumber(L, 2);
	begin.x = luaL_checknumber(L, 3);
	begin.z = luaL_checknumber(L, 4);
	end.x = luaL_checknumber(L, 5);
	end.z = luaL_checknumber(L, 6);

	lua_pushlightuserdata(L, SudokuCreate(gird, begin, end));
	return 1;
}

void DestroyFun(void* value) {
	PEntity pEntity = value;
	listRelease(pEntity->entityview);
	free(pEntity);
}

void SudokuDestory(void* pSudokus) {

	PSudoku s = (PSudoku)pSudokus;
	dictRelease(s->grid_entity);
	dictRelease(s->entitiesDict);

	listSetFreeMethod(s->entitiesList, DestroyFun);
	listRelease(s->entitiesList);

	free(s);
}

static int luaB_Destroy(lua_State* L) {
	void* pVoid = LVMGetGlobleLightUserdata(L, "dockerHandle");
	PSudoku s = lua_touserdata(L, 1);

	SudokuDestory(s);
	return 1;
}

static int luaB_Poition(lua_State* L) {
	void* pVoid = LVMGetGlobleLightUserdata(L, "dockerHandle");

	//position
	struct Vector3 position;
	position.x = luaL_checknumber(L, 1);
	position.y = 0;
	position.z = luaL_checknumber(L, 2);

	//rotation
	struct Vector3 rotation;
	rotation.x = luaL_checknumber(L, 3) * RADIANS_PER_DEGREE;
	rotation.y = 0;
	rotation.z = luaL_checknumber(L, 4) * RADIANS_PER_DEGREE;

	//velocity 每秒移动的速度
	float velocity = luaL_checknumber(L, 5);

	//stamp 秒
	float stamp = luaL_checkinteger(L, 6);

	struct Vector3 out;
	quatMultVector(&rotation, &gForward, &out);

	vector3Scale(&out, &out, (GetCurrentSec() - stamp) * velocity);

	lua_pushnumber(L, out.x);
	lua_pushnumber(L, out.z);

	return 2;
}

static int luaB_Dist(lua_State* L) {

	struct Vector3 position, position2;
	position.x = luaL_checknumber(L, 1);
	position.y = luaL_checknumber(L, 2);
	position.z = luaL_checknumber(L, 3);

	position2.x = luaL_checknumber(L, 1);
	position2.y = luaL_checknumber(L, 2);
	position2.z = luaL_checknumber(L, 3);

	lua_pushnumber(L, sqrtf(vector3DistSqrd(&position, &position2)));
	return 1;
}

static const luaL_Reg sudoku_funcs[] = {
	{"Create", luaB_Create},
	{"Destroy", luaB_Destroy},
	{"Update", luaB_Update},
	{"Move", luaB_Move},
	{"Leave", luaB_Leave},
	{"Entry", luaB_Entry},
	{"Poition", luaB_Poition},
	{"Dist", luaB_Dist},
	{NULL, NULL}
};

int LuaOpenSudoku(lua_State* L) {
	luaL_register(L, "sudokuapi", sudoku_funcs);
	return 1;
}