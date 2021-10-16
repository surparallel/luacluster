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
#include "rtree.h"
#include "rtreehelp.h"
#include "3dmathapi.h"

enum entity_status {
	entity_normal = 0,
	entity_limite = 2,
	entity_outside = 4,//已经超出边界的对象不会放入任何列表，并会在定时结束后从缓存删除。
	entity_ghost = 8
};

typedef struct _Entity {
	unsigned long long entityid;
	listNode* listNode;
	listNode* girdNode;//网格grid_entity的node为了快速查找和删除node
	list* entityview;//dict-list 某个时间点进入视角的玩家列表
	struct BasicTransform transform;
	struct Vector3 nowPosition;
	float velocity;//速度
	unsigned int stamp;
	unsigned int stampStop;
	int update;//上一帧中是否移动过网格，0没有，1移动，2新进入
	unsigned int oldGird;
	unsigned int status;//当前所在位置，0普通区域，2在边界区域，4超出范围, 8鬼魂状态。 超出地图范围的对象将不会再更新数据。直到被删除。
	unsigned int outingStamp;
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

	//bigworld
	int isBigWorld;
	struct rtree* prtree;//地图数据树，来至bigworld服务
	unsigned long long spaceId;

	unsigned int outsideSec;
}*PSudoku, Sudoku;

static unsigned int GirdId(PSudoku pSudoku, struct Vector3* position) {
	unsigned int dx = (unsigned int)floor((position->x - pSudoku->begin.x) / pSudoku->gird.x);
	unsigned int dz = (unsigned int)floor((position->z - pSudoku->begin.z) / pSudoku->gird.z) - 1;

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

int GirdLimite(void* pSudoku, struct Vector3* position) {
	PSudoku s = (PSudoku)pSudoku;

	if((s->begin.x <= position->x && position->x <= s->begin.x + s->gird.x * 2) || 
		(s->end.x >= position->x && position->x >= s->end.x - s->gird.x * 2) ||
		(s->begin.z <= position->z && position->z <= s->begin.z + s->gird.z * 2) || 
		(s->end.z >= position->z && position->z >= s->end.z - s->gird.z * 2))
		return true;
	else
		return false;
}

void SudokuMove(void* pSudoku, unsigned long long id, struct Vector3 position, 
	struct Vector3 rotation, float velocity, unsigned int stamp, unsigned int stampStop) {

	PSudoku s = (PSudoku)pSudoku;
	struct Vector3 nowPosition;
	Position(&position, &rotation, velocity, stamp, stampStop, &nowPosition);
	if (nowPosition.x < s->begin.x || nowPosition.x > s->end.x || nowPosition.z < s->begin.z || nowPosition.z > s->end.z) {

		s_error("sudoku::move entry out limit %U x:%f z:%f  bx:%f bz:%f  ex:%f ez:%f", id, nowPosition.x, nowPosition.z, s->begin.x, s->begin.z, s->end.x, s->end.z);
		return;
	}

	if (stamp == 0) {
		s_error("sudoku::move The start time is zero %U", id);
		return;
	}

	if (stamp && stampStop && stampStop <= stamp) {
		s_error("sudoku::move The end time is less than the start time %U %u %u", id, stamp, stampStop);
		return;
	}

	//查找对应的id
	dictEntry* entry = dictFind(s->entitiesDict, &id);

	if (entry == NULL) {
		s_error("sudoku::move error not find id %U", id);
		return;
	}

	PEntity pEntity = dictGetVal(entry);

	if (GirdId(s, &pEntity->transform.position) != GirdId(s, &position)) {
		SudokuEntry(s, id, position, rotation, velocity, stamp, (pEntity->status & entity_ghost), stampStop);
		return;
	}

	//position
	pEntity->transform.position.x = position.x;
	pEntity->transform.position.z = position.z;
	pEntity->nowPosition.x = position.x;
	pEntity->nowPosition.z = position.z;

	quatEulerAngles(&rotation, &pEntity->transform.rotation);

	//velocity 每秒移动的速度
	pEntity->velocity = velocity;

	//stamp 秒
	pEntity->stamp = stamp;
	pEntity->stampStop = stampStop;

	if (pEntity->status & entity_outside) {
		pEntity->status &= ~entity_outside;
		pEntity->outingStamp = 0;
	}
}

static int luaB_Move(lua_State* L) {

	PSudoku s = lua_touserdata(L, 1);
	unsigned long long id = luaL_tou64(L, 2);
	s_fun("sudoku(%U)::Move id:%U", s->spaceId, id);

	//position
	struct Vector3 position = { 0 };
	position.x = luaL_checknumber(L, 3);
	position.z = luaL_checknumber(L, 4);

	//rotation
	struct Vector3 rotation = { 0 };
	rotation.y = luaL_checknumber(L, 5) * RADIANS_PER_DEGREE;

	//velocity 每秒移动的速度
	float velocity = luaL_checknumber(L, 6);

	//stamp 秒
	unsigned int stamp = luaL_checkinteger(L, 7);
	unsigned int stampStop = luaL_checkinteger(L, 8);

	SudokuMove(s, id, position, rotation, velocity, stamp, stampStop);
	return 0;
}

void SudokuLeave(void* pSudoku, unsigned long long id) {

	PSudoku s = (PSudoku)pSudoku;
	//查找对应的id
	dictEntry* entry = dictFind(s->entitiesDict, &id);

	if (entry == NULL) {
		s_error("sudoku::SudokuLeave error not find id %U", id);
	}
	PEntity pEntity = dictGetVal(entry);
	listDelNode(s->entitiesList, pEntity->listNode);

	entry = dictFind(s->grid_entity, &pEntity->oldGird);
	list* girdlist = NULL;
	if (entry == NULL) {
		s_error("sudoku::SudokuLeave::move error not find gird id %i", pEntity->oldGird);
	}
	else {
		girdlist = dictGetVal(entry);
	}
	listDelNode(girdlist, pEntity->girdNode);
	s_details("sudoku::SudokuLeave::remove from gird id:%U to gird:%u", pEntity->entityid, pEntity->oldGird);

	if (listLength(girdlist) == 0) {
		dictDelete(s->grid_entity, &pEntity->oldGird);
	}

	listRelease(pEntity->entityview);
	free(pEntity);
}

static int luaB_Leave(lua_State* L) {

	PSudoku s = lua_touserdata(L, 1);
	unsigned long long id = luaL_tou64(L, 2);

	s_fun("sudoku(%U)::Leave id:%U",s->spaceId, id);

	SudokuLeave(s, id);
	return 0;
}

void SudokuEntry(void* pSudoku, unsigned long long id, struct Vector3 position, 
	struct Vector3 rotation, float velocity, 
	unsigned int stamp, unsigned int stampStop, unsigned int isGhost) {
	
	PSudoku s = (PSudoku)pSudoku;
	struct Vector3 nowPosition = {0};
	Position(&position, &rotation, velocity, stamp, stampStop, &nowPosition);
	if (nowPosition.x < s->begin.x || nowPosition.x > s->end.x || nowPosition.z < s->begin.z || nowPosition.z > s->end.z) {

		s_error("sudoku::Entry entry out limit %U x:%f z:%f  bx:%f bz:%f  ex:%f ez:%f", id, nowPosition.x, nowPosition.z, s->begin.x, s->begin.z, s->end.x, s->end.z);
		return;
	}

	if (stamp == 0) {
		s_error("sudoku::Entry The start time is zero %U", id);
		return;
	}


	if (stamp && stampStop && stampStop <= stamp) {
		s_error("sudoku::Entry The end time is less than the start time  %U %u %u", id, stamp, stampStop);
		return;
	}

	//兼容重复进入空间，如果重复进入空间就相当于瞬间移动。
	PEntity pEntity;
	dictEntry* entry = dictFind(s->entitiesDict, &id);
	if (entry != NULL) {
		pEntity = dictGetVal(entry);
	}
	else {
		pEntity = calloc(1, sizeof(Entity));
	}

	pEntity->entityid = id;
	pEntity->entityview = listCreate();

	//position
	pEntity->transform.position.x = position.x;
	pEntity->transform.position.z = position.z;
	pEntity->nowPosition = nowPosition;
	quatEulerAngles(&rotation, &pEntity->transform.rotation);

	//velocity 每秒移动的速度
	pEntity->velocity = velocity;

	//stamp 秒
	pEntity->stamp = stamp;
	pEntity->stampStop = stampStop;
	pEntity->update = 2;
	pEntity->oldGird = GirdId(s, &pEntity->nowPosition);

	listAddNodeHead(s->entitiesList, pEntity);
	pEntity->listNode = listFirst(s->entitiesList);
	dictAddWithLonglong(s->entitiesDict, id, pEntity);

	//放入gird
	entry = dictFind(s->grid_entity, &pEntity->oldGird);
	list* girdlist = NULL;
	if (entry == NULL) {
		girdlist = listCreate();
		dictAddWithUint(s->grid_entity, pEntity->oldGird, girdlist);
	}
	else {
		girdlist = dictGetVal(entry);
	}

	listAddNodeHead(girdlist, pEntity);
	pEntity->girdNode = listFirst(girdlist);
	s_details("sudoku::SudokuEntry::move to gird id:%U to gird:%u x:%f z:%f", pEntity->entityid, pEntity->oldGird, pEntity->transform.position.x, pEntity->transform.position.z);

	if (s->isBigWorld) {
		if (isGhost) {
			pEntity->status |= entity_ghost | entity_limite;
		}
		else if (GirdLimite(s, &position)) {
			pEntity->status |= entity_limite;
		}
	}
}

static int luaB_Entry(lua_State* L) {

	PSudoku s = lua_touserdata(L, 1);
	unsigned long long id = luaL_tou64(L, 2);

	//position
	struct Vector3 position = { 0 };
	position.x = luaL_checknumber(L, 3);
	position.z = luaL_checknumber(L, 4);

	//rotation
	struct Vector3 rotation = { 0 };
	rotation.y = luaL_checknumber(L, 5) * RADIANS_PER_DEGREE;

	//velocity 每秒移动的速度
	float velocity = luaL_checknumber(L, 6);

	//stamp 秒
	unsigned int stamp = luaL_checkinteger(L, 7);
	unsigned int stampStop = luaL_checkinteger(L, 8);
	int isGhost = luaL_checkinteger(L, 9);
	
	s_fun("sudoku(%U)::Entry %U ",s->spaceId, id);
	SudokuEntry(s, id, position, rotation, velocity, stamp, stampStop, isGhost);
	return 0;
}

void FillView(PSudoku s, enum SudokuDir dir, unsigned int centre, PEntity pEntityEntry) {

	unsigned int viewgird = GirdDirId(s, dir, centre);
	if (MAXUINT32 == viewgird) {
		//s_error("FillView not GirdDirId gird %i", viewgird);
		return;
	}

	dictEntry* entry = dictFind(s->grid_entity, &viewgird);
	if (entry == NULL) {
		//s_error("FillView not find gird %i", viewgird);
	}
	else {
		list* newview = dictGetVal(entry);

		listIter* iter = listGetIterator(newview, AL_START_HEAD);
		listNode* node;
		while ((node = listNext(iter)) != NULL) {
			PEntity pEntity = listNodeValue(node);

			if (pEntity->entityid != pEntityEntry->entityid) {
				if(!(s->isBigWorld && (pEntityEntry->status & entity_ghost) && (pEntity->status & entity_limite) || (pEntity->status & entity_outside) || (pEntityEntry->status & entity_outside)))
					listAddNodeTail(pEntityEntry->entityview, pEntity);
			}
				
			if (pEntity->update != 0)
				continue;

			if (pEntity->entityid != pEntityEntry->entityid) {
				if (!(s->isBigWorld && (pEntity->status & entity_ghost) && (pEntityEntry->status & entity_limite) || (pEntity->status & entity_outside) || (pEntityEntry->status & entity_outside)))
					listAddNodeTail(pEntity->entityview, pEntityEntry);
			}
		}
	}
}

static void DestroyRtreeFun(void* value) {
	free(value);
}

//每次切换格子时，进入或移动，要检查是否在边界，是否为ghost，是否超出地图范围
void SudokuUpdateLimite(PSudoku s, PEntity pEntity) {

	//进入边界地区，通知用户登陆其他空间
	if (s->isBigWorld && GirdLimite(s, &pEntity->nowPosition)) {

		if (!(pEntity->status & entity_limite)) {
			pEntity->status |= entity_limite;

			if (pEntity->status & entity_ghost)
				return;

			s_details("Sudoku(%U)::SudokuUpdateLimite::entity_limite id:%U x:%f y:%f z:%f", s->spaceId, pEntity->entityid, pEntity->nowPosition.x, pEntity->nowPosition.y, pEntity->nowPosition.z);
		
			double rect[4] = { 0 };
			rect[0] = pEntity->nowPosition.x;
			rect[1] = pEntity->nowPosition.z;
			rect[2] = pEntity->nowPosition.x;
			rect[3] = pEntity->nowPosition.z;

			list* result = listCreate();
			rtree_search(s->prtree, rect, MultipleIter, result);

			//通知登录其他空间
			mp_buf* pmp_buf = mp_buf_new();
			const char ptr[] = "OnRedirectToSpace";
			mp_encode_bytes(pmp_buf, ptr, sizeof(ptr) - 1);

			mp_encode_array(pmp_buf, listLength(result));
			listIter* iter = listGetIterator(result, AL_START_HEAD);
			listNode* node;
			while ((node = listNext(iter)) != NULL) {
				struct Iter* iter = (struct Iter*)listNodeValue(node);
				mp_encode_double(pmp_buf, u642double(*(unsigned long long*)iter->item));
			}

			DockerSend(pEntity->entityid, pmp_buf->b, pmp_buf->len);
			mp_buf_free(pmp_buf);

			listSetFreeMethod(result, DestroyRtreeFun);
			listRelease(result);
		}
	}
	else if (s->isBigWorld && (pEntity->status & entity_limite)) {
		pEntity->status &= ~entity_limite;
		pEntity->status &= ~entity_ghost;

		//删除其他空间的ghost状态
		const char ptr[] = "OnDelGhost";
		mp_buf* pmp_buf = mp_buf_new();
		mp_encode_bytes(pmp_buf, ptr, sizeof(ptr) - 1);
		mp_encode_double(pmp_buf, u642double(s->spaceId));

		DockerSend(pEntity->entityid, pmp_buf->b, pmp_buf->len);
		mp_buf_free(pmp_buf);

		s_details("Sudoku(%U)::SudokuUpdateLimite::~entity_limite id:%U x:%f y:%f z:%f", s->spaceId, pEntity->entityid, pEntity->nowPosition.x, pEntity->nowPosition.y, pEntity->nowPosition.z);
	}

	if ((pEntity->status & entity_limite) || (pEntity->status & entity_outside)) {

		//游出边界设置删除时间戳
		if (pEntity->status & entity_outside) {
			s_details("Sudoku(%U)::SudokuUpdateLimite::entity_outside id:%U x:%f y:%f z:%f", s->spaceId, pEntity->entityid, pEntity->nowPosition.x, pEntity->nowPosition.y, pEntity->nowPosition.z);
			pEntity->outingStamp = GetCurrentSec();
		}
	}
}

void SudokuUpdate(void* pSudoku) {

	PSudoku s = (PSudoku)pSudoku;

	listIter* iter = listGetIterator(s->entitiesList, AL_START_HEAD);
	listNode* node;
	while ((node = listNext(iter)) != NULL) {
		PEntity pEntity = listNodeValue(node);

		if (pEntity->velocity == 0.0f || (pEntity->status & entity_outside)) {

			if (pEntity->velocity == 0 && GetCurrentSec() - pEntity->stamp > s->outsideSec) {
				pEntity->status |= entity_outside;
				pEntity->outingStamp = GetCurrentSec();
			}
			continue;
		}

		struct Vector3 nowPosition = {0};
		struct Vector3 Euler = { 0 };
		quatToEulerAngles(&pEntity->transform.rotation, &Euler);
		Position(&pEntity->transform.position, &Euler, pEntity->velocity, pEntity->stamp, pEntity->stampStop, &nowPosition);
		unsigned int newGird = GirdId(s, &nowPosition);
		pEntity->nowPosition = nowPosition;

		if (pEntity->stampStop && GetCurrentSec() >= pEntity->stampStop) {
			pEntity->velocity = 0;
			s_details("Sudoku(%U)::update::stop id:%U", s->spaceId, pEntity->entityid);
		}

		if (nowPosition.x < s->begin.x || nowPosition.x > s->end.x || nowPosition.z < s->begin.z || nowPosition.z > s->end.z) {
			pEntity->status |= entity_outside;
		}

		if (newGird == pEntity->oldGird) {
			pEntity->update = 0;
		}
		else {
			s_details("Sudoku(%U)::SudokuUpdate::nowPosition id:%U x:%f y:%f z:%f", s->spaceId, pEntity->entityid, nowPosition.x, nowPosition.y, nowPosition.z);

			dictEntry* entry = dictFind(s->grid_entity, &pEntity->oldGird);
			list* girdlist = NULL;
			if (entry == NULL) {
				s_error("sudoku::update error not find old gird id %i", pEntity->oldGird);
				continue;
			}
			else {
				girdlist = dictGetVal(entry);
			}

			//更换gird
			if (pEntity->update == 0)
				pEntity->update = 1;

			listPickNode(girdlist, pEntity->girdNode);
			s_details("sudoku::update::remove from gird to gird id:%U to oldGird:%u newGird:%u", pEntity->entityid, pEntity->oldGird, newGird);

			if (listLength(girdlist) == 0) {
				dictDelete(s->grid_entity, &pEntity->oldGird);
			}

			entry = dictFind(s->grid_entity, &newGird);
			if (entry == NULL) {
				girdlist = listCreate();
				dictAddWithUint(s->grid_entity, newGird, girdlist);
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
	while ((node = listNext(iter)) != NULL) {
		//搜索周围,生成每个用户的listview
		PEntity pEntity = listNodeValue(node);

		//判断游出边界，并且已经设置删除时间戳的
		if ((pEntity->status & entity_outside) && pEntity->outingStamp != 0) {
			unsigned int newGird = GirdId(s, &pEntity->nowPosition);
			pEntity->oldGird = newGird;
			continue;
		}
			
		if (pEntity->update == 1) {

			s_details("Sudoku(%U)::step2::normalupdate id:%U x:%f y:%f z:%f", s->spaceId, pEntity->entityid, pEntity->nowPosition.x, pEntity->nowPosition.y, pEntity->nowPosition.z);
			unsigned int newGird = GirdId(s, &pEntity->nowPosition);
			enum SudokuDir sudokuDir = GirdDir(s, newGird, pEntity->oldGird);

			switch (sudokuDir)
			{
			case up:
			{
				FillView(s, up, newGird, pEntity);
				FillView(s, upleft, newGird, pEntity);
				FillView(s, upright, newGird, pEntity);
			}
			break;
			case down:
			{
				FillView(s, down, newGird, pEntity);
				FillView(s, downleft, newGird, pEntity);
				FillView(s, downright, newGird, pEntity);
			}
			break;
			case left:
			{
				FillView(s, upleft, newGird, pEntity);
				FillView(s, left, newGird, pEntity);
				FillView(s, downleft, newGird, pEntity);
			}
			break;
			case right:
			{
				FillView(s, upright, newGird, pEntity);
				FillView(s, right, newGird, pEntity);
				FillView(s, downright, newGird, pEntity);
			}
			break;
			case upleft:
			{
				FillView(s, up, newGird, pEntity);
				FillView(s, upleft, newGird, pEntity);
				FillView(s, upright, newGird, pEntity);
				FillView(s, left, newGird, pEntity);
				FillView(s, downleft, newGird, pEntity);
			}
			break;
			case upright:
			{
				FillView(s, up, newGird, pEntity);
				FillView(s, upleft, newGird, pEntity);
				FillView(s, upright, newGird, pEntity);
				FillView(s, right, newGird, pEntity);
				FillView(s, downright, newGird, pEntity);
			}
			break;
			case downleft:
			{
				FillView(s, upleft, newGird, pEntity);
				FillView(s, left, newGird, pEntity);
				FillView(s, downleft, newGird, pEntity);
				FillView(s, down, newGird, pEntity);
				FillView(s, downright, newGird, pEntity);
			}
			break;
			case downright:
			{
				FillView(s, upright, newGird, pEntity);
				FillView(s, right, newGird, pEntity);
				FillView(s, downright, newGird, pEntity);
				FillView(s, down, newGird, pEntity);
				FillView(s, downleft, newGird, pEntity);
			}
			break;
			default: {
				pEntity->update = 2;
				s_warn("SudokuUpdate::step2::Flash event in space eid: %U, np: %i, op: %i", pEntity->entityid, newGird, pEntity->oldGird);
			}
			}

			if (pEntity->update == 1) {
				SudokuUpdateLimite(s, pEntity);
				pEntity->oldGird = newGird;
			}
		}
		
		if (pEntity->update == 2) {

			s_details("Sudoku(%U)::step2::all update id:%U x:%f y:%f z:%f", s->spaceId, pEntity->entityid, pEntity->nowPosition.x, pEntity->nowPosition.y, pEntity->nowPosition.z);
			unsigned int newGird = GirdId(s, &pEntity->nowPosition);

			FillView(s, up, newGird, pEntity);
			FillView(s, down, newGird, pEntity);
			FillView(s, left, newGird, pEntity);
			FillView(s, right, newGird, pEntity);
			FillView(s, upleft, newGird, pEntity);
			FillView(s, upright, newGird, pEntity);
			FillView(s, downleft, newGird, pEntity);
			FillView(s, downright, newGird, pEntity);
			FillView(s, dircentre, newGird, pEntity);

			SudokuUpdateLimite(s, pEntity);
			pEntity->oldGird = newGird;
		}

		pEntity->update = 0;
	}
	listReleaseIterator(iter);

	iter = listGetIterator(s->entitiesList, AL_START_HEAD);
	while ((node = listNext(iter)) != NULL) {
		PEntity pEntity = listNodeValue(node);
		if (listLength(pEntity->entityview) != 0) {
			mp_buf* pmp_buf = mp_buf_new();
			const char ptr[] = "OnAddView";
			mp_encode_bytes(pmp_buf, ptr, sizeof(ptr));
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
				mp_encode_int(pmp_buf, pViewEntity->stampStop);

				listDelNode(pEntity->entityview, viewnode);
			}
			listReleaseIterator(viewiter);

			DockerSend(pEntity->entityid, pmp_buf->b, pmp_buf->len);
			mp_buf_free(pmp_buf);
		}

		//删除已经超过60秒离开边界的对象
		if (pEntity->status & entity_outside && pEntity->outingStamp && (GetCurrentSec() - pEntity->outingStamp) > s->outsideSec) {
			SudokuLeave(s, pEntity->entityid);

			mp_buf* pmp_buf = mp_buf_new();
			const char ptr[] = "OnLeaveSpace";
			mp_encode_bytes(pmp_buf, ptr, sizeof(ptr)-1);
			mp_encode_double(pmp_buf, u642double(pEntity->entityid));

			DockerSend(pEntity->entityid, pmp_buf->b, pmp_buf->len);
			mp_buf_free(pmp_buf);
		}
	}
	listReleaseIterator(iter);
}

void PrintAllPoition(void* pSudoku) {

	PSudoku s = (PSudoku)pSudoku;

	listIter* iter = listGetIterator(s->entitiesList, AL_START_HEAD);
	listNode* node;
	while ((node = listNext(iter)) != NULL) {
		PEntity pEntity = listNodeValue(node);

		printf("%llu %f %f\n", pEntity->entityid, pEntity->nowPosition.x, pEntity->nowPosition.z);
	}
	listReleaseIterator(iter);
}

//这里要发送广播
static int luaB_Update(lua_State* L) {

	PSudoku s = lua_touserdata(L, 1);
	//s_fun("sudoku(%U)::Update", s->spaceId);
	SudokuUpdate(s);
	return 0;
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

void* SudokuCreate(struct Vector3 gird, struct Vector3 begin, struct Vector3 end, int isBigWorld, unsigned long long spaceId, unsigned int outsideSec) 
{

	PSudoku s = calloc(1, sizeof(Sudoku));
	s->entitiesList = listCreate();
	s->entitiesDict = dictCreate(DefaultLonglongPtr(), NULL);
	s->grid_entity = dictCreate(&UintDictType, NULL);

	s->gird = gird;
	s->begin = begin;
	s->end = end;

	s->end.x = s->end.x - 0.00001;
	s->end.z = s->end.z - 0.00001;

	s->linex = (unsigned int)floor((s->end.x - s->begin.x) / s->gird.x);
	unsigned int dz = (unsigned int)floor((s->end.z - s->begin.z) / s->gird.z);
	s->maxid = s->linex * dz;

	s->isBigWorld = isBigWorld;
	s->spaceId = spaceId;
	s->prtree = rtree_new(sizeof(unsigned long long), 2);
	s->outsideSec = outsideSec;

	return s;
}

static int luaB_Create(lua_State* L) {

	struct Vector3 gird;
	struct Vector3 begin;
	struct Vector3 end;
	gird.x = luaL_checknumber(L, 1);
	gird.z = luaL_checknumber(L, 2);
	begin.x = luaL_checknumber(L, 3);
	begin.z = luaL_checknumber(L, 4);
	end.x = luaL_checknumber(L, 5);
	end.z = luaL_checknumber(L, 6);
	int isBigWorld = luaL_checkint(L, 7);
	unsigned long long spaceId = luaL_tou64(L, 8);
	unsigned int outsideSec = luaL_checkinteger(L, 9);

	s_fun("sudoku(%U)::Create isBigworld:%i gx:%f gz:%f bx:%f bz:%f ex:%f ez:%f", spaceId, isBigWorld, gird.x, gird.z, begin.x, begin.z, end.x, end.z);
	lua_pushlightuserdata(L, SudokuCreate(gird, begin, end, isBigWorld, spaceId, outsideSec));
	return 1;
}

static void DestroyFun(void* value) {

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

	PSudoku s = lua_touserdata(L, 1);
	s_fun("sudoku(%U)::Destroy", s->spaceId);
	SudokuDestory(s);
	return 1;
}

static int luaB_Insert(lua_State* L) {

	PSudoku s = lua_touserdata(L, 1);
	unsigned long long id = luaL_tou64(L, 2);
	s_fun("sudoku(%U)::Insert id:%U", s->spaceId, id);

	double rect[4] = { 0 };
	rect[0] = luaL_checknumber(L, 3);
	rect[1] = luaL_checknumber(L, 4);
	rect[2] = luaL_checknumber(L, 5);
	rect[3] = luaL_checknumber(L, 6);

	if(id != s->spaceId)
		rtree_insert(s->prtree, rect, &id);

	return 0;
}

static int luaB_Alter(lua_State* L) {

	PSudoku s = lua_touserdata(L, 1);
	unsigned long long id = luaL_tou64(L, 2);
	s_fun("sudoku(%U)::Alter id:%U", s->spaceId, id);

	double rect[4] = { 0 };
	rect[0] = luaL_checknumber(L, 3);
	rect[1] = luaL_checknumber(L, 4);
	rect[2] = luaL_checknumber(L, 5);
	rect[3] = luaL_checknumber(L, 6);

	if (s->isBigWorld && s->spaceId == id) {
		s->begin.x = rect[0];
		s->begin.z = rect[1];
		s->end.x = rect[2];
		s->end.z = rect[3];

		s->linex = (unsigned int)floor((s->end.x - s->begin.x) / s->gird.x);
		unsigned int dz = (unsigned int)floor((s->end.z - s->begin.z) / s->gird.z);
		s->maxid = s->linex * dz;
		s_details("sudoku(%U)::Config bx:%f bz:%f ex:%f ez:%f", s->spaceId, s->begin.x, s->begin.z, s->end.x, s->end.z);
	}
	else {
		rtree_delete(s->prtree, rect, &id);
		rtree_insert(s->prtree, rect, &id);
	}

	return 0;
}

static int luaB_SetGhost(lua_State* L) {

	PSudoku s = lua_touserdata(L, 1);
	unsigned long long id = luaL_tou64(L, 2);

	s_fun("sudoku(%U)::SetGhost", s->spaceId)

	PEntity pEntity;
	dictEntry* entry = dictFind(s->entitiesDict, &id);
	if (entry != NULL) {
		pEntity = dictGetVal(entry);
	}
	else {
		s_error("sudoku::SetGhost not finde entity %U", id);
		return 0;
	}

	pEntity->status |= entity_ghost;

	return 0;
}

static const luaL_Reg sudoku_funcs[] = {
	{"Create", luaB_Create},
	{"Destroy", luaB_Destroy},
	{"Update", luaB_Update},
	{"Move", luaB_Move},
	{"Leave", luaB_Leave},
	{"Entry", luaB_Entry},
	{"Insert", luaB_Insert},
	{"Alter", luaB_Alter},
	{"SetGhost", luaB_SetGhost},
	{NULL, NULL}
};

int LuaOpenSudoku(lua_State* L) {
	luaL_register(L, "sudokuapi", sudoku_funcs);
	return 1;
}