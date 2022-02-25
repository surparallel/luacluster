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

//
// 假设场景内有1万人在一个格子中，排队跨过格子边界。
// 从第一个开始，所发送封包的总数量为1*n，n为当前已经跨过格子的数量。
// 接收封包的数量也为1*n。哪么每帧也就是1秒，所有处理封包的总数为n+n。
// 当最有一个玩家跨过格子，所要处理的封包数量达到巅峰，为1万*1万。
// 其中1万个封包可以分摊到所有其他对象所在的服务器。
// 另1万个封包需要目标对象来进行处理。 哪么最极端情况下万人同屏每秒需要处理1万~2万的进入数据。
// 
// 这里上限的瓶颈是发送给自己的1万个可见目标。因为服务器的单核cpu处理是有上限的。
// 但这1万个发给自己的数据并不是必须的。因为人脑也不能一下处理1万个可见目标。
// 这1万个目标是可以删减优化的。除非1/4概率两边都没有发送会导致目标完全不可见。
// 
// 如果是1/4概率两边都没有发送导致不可见。哪么有没有补救的方法呢？
// 对玩家当前格子内的数据要完整处理，对于外围的8个格子可以随机处理。
// 
// 任何曲线都可以折算成对应减速的直线。所以有方向和起点速度的向量可以近似模拟任何活动路径。
//
// 总数为1万人时，当进入格子的人数为n时。进入格子人数为5千人时，发送的封包数量为5千万达到巅峰。 
// 当达到巅峰是每个玩家收到的封包数量为5千。5千万封包平摊到16线程为也要312万5千
// 2*n*(10000 - n)
// 
//

enum entity_status {
	entity_normal = 1 << 1,
	entity_limite = 1 << 2,
	entity_outside = 1 << 3,//已经超出边界的对象不会放入任何列表，并会在定时结束后从缓存删除。
	entity_ghost = 1 << 4,
	entity_stop = 1 << 5
};

enum entity_update {
	update_stable = 0,
	update_change = 1,
	update_entry = 2
};

typedef struct _Entity {
	unsigned long long entityid;
	listNode* listNode;//全局的列表node
	listNode* girdNode;//网格grid_entity的node为了快速查找和删除node
	//list* entityview;//dict-list 某个时间点进入视角的玩家列表
	struct BasicTransform transform;
	struct Vector3 nowPosition;
	unsigned int newGird;
	float velocity;//速度
	unsigned int stamp;
	unsigned int stampStop;
	int update;//entity_update上一帧中是否移动过网格，0没有，1移动，2新进入
	int oldGird;
	unsigned int status;//entity_status，0普通区域，2在边界区域，4超出范围, 8鬼魂状态。 超出地图范围的对象将不会再更新数据。直到被删除。
	unsigned int outingStamp;//超出区域的时间戳
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

	unsigned int outsideSec;//出界删除的时间
	unsigned int update_count;

	list** msgList;
}*PSudoku, Sudoku;

unsigned int GirdId(void* pVoid, struct Vector3* position) {
	PSudoku pSudoku = pVoid;
	unsigned int dx = (unsigned int)floor((position->x - pSudoku->begin.x) / pSudoku->gird.x);
	unsigned int dz = (unsigned int)floor((position->z - pSudoku->begin.z) / pSudoku->gird.z);

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

	unsigned int ret = UINT_MAX;
	switch (dir)
	{
	case up:
		ret = centre + pSudoku->linex;
		if (ret > pSudoku->maxid) {
			ret = UINT_MAX;
		}
		break;
	case down:
		if (centre < pSudoku->linex)
			ret = UINT_MAX;
		else
			ret = centre - pSudoku->linex;
		break;
	case left:
		if (floor(centre / pSudoku->linex) == floor((centre - 1) / pSudoku->linex))
			ret = centre - 1;
		else
			ret = UINT_MAX;
		break;
	case right:
		if (floor(centre / pSudoku->linex) == floor((centre + 1) / pSudoku->linex))
			ret = centre + 1;
		else
			ret = UINT_MAX;
		break;
	case upleft:
		if (UINT_MAX == GirdDirId(pSudoku, up, centre) || UINT_MAX == GirdDirId(pSudoku, left, centre))
			ret = UINT_MAX;
		else
			ret = centre + pSudoku->linex - 1;
		break;
	case upright:
		if (UINT_MAX == GirdDirId(pSudoku, up, centre) || UINT_MAX == GirdDirId(pSudoku, right, centre))
			ret = UINT_MAX;
		else
			ret = centre + pSudoku->linex + 1;
		break;
	case downleft:
		if (UINT_MAX == GirdDirId(pSudoku, down, centre) || UINT_MAX == GirdDirId(pSudoku, left, centre))
			ret = UINT_MAX;
		else
			ret = centre - pSudoku->linex - 1;
		break;
	case downright:
		if (UINT_MAX == GirdDirId(pSudoku, down, centre) || UINT_MAX == GirdDirId(pSudoku, right, centre))
			ret = UINT_MAX;
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

	for (int i = up; i <= dircentre; i++) {

		if (entry == GirdDirId(pSudoku, i, centre))
			return i;
	}

	return SudokuDirError;
}

int GirdLimite(void* pSudoku, struct Vector3* position) {
	PSudoku s = (PSudoku)pSudoku;

	if (s->begin.x <= position->x && position->x < s->end.x && s->begin.z <= position->z && position->z < s->end.z)
		if ((position->x >= (s->begin.x + s->gird.x * 2) && position->x < (s->end.x - s->gird.x * 2))
			&& (position->z >= (s->begin.z + s->gird.z * 2) && position->z < (s->end.z - s->gird.z * 2)))
			return false;
		else
			return true;
	else
		return false;
}

void SudokuMove(void* pSudoku, unsigned long long id, struct Vector3 position, 
	struct Vector3 rotation, float velocity, unsigned int stamp, unsigned int stampStop) {

	PSudoku s = (PSudoku)pSudoku;
	//查找对应的id
	dictEntry* entry = dictFind(s->entitiesDict, &id);

	if (entry == NULL) {
		s_error("sudoku(%U)::move error not find id %U", s->spaceId, id);
		return;
	}

	//s_fun("sudoku(%U)::SudokuMove id %U", s->spaceId, id);

	PEntity pEntity = dictGetVal(entry);

	//position
	pEntity->transform.position.x = position.x;
	pEntity->transform.position.y = position.y;
	pEntity->transform.position.z = position.z;
	quatEulerAngles(&rotation, &pEntity->transform.rotation);

	s_details("Sudoku(%U)::SudokuMove id:%U Euler(%f, %f, %f) pos(%f, %f, %f) rotation(%f, %f, %f, %f)"
		, s->spaceId, pEntity->entityid, rotation.x, rotation.y, rotation.z, pEntity->transform.position.x, pEntity->transform.position.y, pEntity->transform.position.z
		, pEntity->transform.rotation.w, pEntity->transform.rotation.x, pEntity->transform.rotation.y, pEntity->transform.rotation.z);

	//velocity 每秒移动的速度
	pEntity->velocity = velocity;

	//stamp 秒
	pEntity->stamp = stamp;
	pEntity->stampStop = stampStop;
	pEntity->status &= ~entity_stop;
}

static int luaB_Move(lua_State* L) {

	PSudoku s = lua_touserdata(L, 1);
	unsigned long long id = luaL_tou64(L, 2);
	//s_fun("sudoku(%U)::Move id:%U", s->spaceId, id);

	//position
	struct Vector3 position = { 0 };
	position.x = luaL_checknumber(L, 3);
	position.y = luaL_checknumber(L, 4);
	position.z = luaL_checknumber(L, 5);

	//rotation
	struct Vector3 rotation = { 0 };
	rotation.x = luaL_checknumber(L, 6);
	rotation.y = luaL_checknumber(L, 7);
	rotation.z = luaL_checknumber(L, 8);

	//velocity 每秒移动的速度
	float velocity = luaL_checknumber(L, 9);

	//stamp 秒
	unsigned int stamp = luaL_checkinteger(L, 10);
	unsigned int stampStop = luaL_checkinteger(L, 11);

	SudokuMove(s, id, position, rotation, velocity, stamp, stampStop);
	return 0;
}

void SudokuLeave(void* pSudoku, unsigned long long id) {

	PSudoku s = (PSudoku)pSudoku;
	s_fun("Sudoku(%U)::SudokuLeave id:%U", s->spaceId, id);

	//查找对应的id
	dictEntry* entry = dictFind(s->entitiesDict, &id);

	if (entry == NULL) {
		s_error("sudoku(%U)::SudokuLeave error not find id %U", s->spaceId, id);
	}
	PEntity pEntity = dictGetVal(entry);
	listDelNode(s->entitiesList, pEntity->listNode);

	entry = dictFind(s->grid_entity, &pEntity->newGird);
	list* girdlist = NULL;
	if (entry == NULL) {
		s_error("sudoku(%U)::SudokuLeave::leave error not find gird id %i", s->spaceId, pEntity->newGird);
		return;
	}
	else {
		girdlist = dictGetVal(entry);
	}

	listDelNode(girdlist, pEntity->girdNode);
	s_details("sudoku(%U)::SudokuLeave::remove from gird id:%U to gird:%u", s->spaceId, pEntity->entityid, pEntity->newGird);

	if (listLength(girdlist) == 0) {
		dictDelete(s->grid_entity, &pEntity->newGird);
	}

	dictDelete(s->entitiesDict, &id);
	free(pEntity);
}

static int luaB_Leave(lua_State* L) {

	PSudoku s = lua_touserdata(L, 1);
	unsigned long long id = luaL_tou64(L, 2);

	s_fun("sudoku(%U)::Leave id:%U",s->spaceId, id);

	SudokuLeave(s, id);
	return 0;
}

void SendAddView(PSudoku s, PEntity pEntity, PEntity pViewEntity) {
	mp_buf* pmp_buf = mp_buf_new();
	MP_ENCODE_CONST(pmp_buf, "OnAddView");

	mp_encode_array(pmp_buf, 10);
	mp_encode_double(pmp_buf, u642double(pViewEntity->entityid));
	mp_encode_double(pmp_buf, pViewEntity->transform.position.x);
	mp_encode_double(pmp_buf, pViewEntity->transform.position.y);
	mp_encode_double(pmp_buf, pViewEntity->transform.position.z);

	struct Vector3 Euler = { 0 };
	quatToEulerAngles(&pViewEntity->transform.rotation, &Euler);
	mp_encode_double(pmp_buf, Euler.x);
	mp_encode_double(pmp_buf, Euler.y);
	mp_encode_double(pmp_buf, Euler.z);
	mp_encode_double(pmp_buf, pViewEntity->velocity);
	mp_encode_int(pmp_buf, pViewEntity->stamp);
	mp_encode_int(pmp_buf, pViewEntity->stampStop);

	DockerSendWithList(pEntity->entityid, pmp_buf->b, pmp_buf->len, s->msgList);
	mp_buf_free(pmp_buf);
}


int GirdExchang(PSudoku s, PEntity pEntity, int newGird) {

	dictEntry* entry = dictFind(s->grid_entity, &pEntity->oldGird);
	list* girdlist = NULL;
	if (entry == NULL) {
		s_error("Sudoku(%U)::GirdExchang error not find old gird id %i", s->spaceId, pEntity->oldGird);
		return 0;
	}
	else {
		girdlist = dictGetVal(entry);
	}

	listPickNode(girdlist, pEntity->girdNode);
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

	s_details("Sudoku(%U)::GirdExchang from gird to gird id:%U to oldGird:%u newGird:%u", s->spaceId, pEntity->entityid, pEntity->oldGird, newGird);
	return 1;
}

void GirdPut(PSudoku s, PEntity pEntity) {

	dictEntry* entry = dictFind(s->grid_entity, &pEntity->newGird);
	list* girdlist = NULL;
	if (entry == NULL) {
		girdlist = listCreate();
		dictAddWithUint(s->grid_entity, pEntity->newGird, girdlist);
	}
	else {
		girdlist = dictGetVal(entry);
	}

	listAddNodeHead(girdlist, pEntity);
	pEntity->girdNode = listFirst(girdlist);
}

void SudokuEntry(void* pSudoku, unsigned long long id, struct Vector3 position, 
	struct Vector3 rotation, float velocity, 
	unsigned int stamp, unsigned int stampStop, unsigned int isGhost) {
	
	PSudoku s = (PSudoku)pSudoku;
	//兼容重复进入空间，如果重复进入空间就相当于瞬间移动。
	PEntity pEntity;
	dictEntry* entry = dictFind(s->entitiesDict, &id);

	if (entry != NULL) {
		pEntity = dictGetVal(entry);
		s_details("sudoku(%U)::Entry find is %U", s->spaceId, id);
		pEntity->entityid = id;
		//position
		pEntity->transform.position.x = position.x;
		pEntity->transform.position.y = position.y;
		pEntity->transform.position.z = position.z;
		quatEulerAngles(&rotation, &pEntity->transform.rotation);

		//velocity 每秒移动的速度
		pEntity->velocity = velocity;

		//stamp 秒
		pEntity->stamp = stamp;
		pEntity->stampStop = stampStop;
		pEntity->status &= ~entity_stop;
	}
	else {
		pEntity = calloc(1, sizeof(Entity));
		s_details("sudoku(%U)::Entry calloc new is %U", s->spaceId, id);

		pEntity->entityid = id;
		//position
		pEntity->transform.position.x = position.x;
		pEntity->transform.position.y = position.y;
		pEntity->transform.position.z = position.z;
		quatEulerAngles(&rotation, &pEntity->transform.rotation);

		//velocity 每秒移动的速度
		pEntity->velocity = velocity;

		//stamp 秒
		pEntity->stamp = stamp;
		pEntity->stampStop = stampStop;
		pEntity->update = update_entry;
		pEntity->newGird = UINT_MAX;
		pEntity->oldGird = UINT_MAX;

		listAddNodeHead(s->entitiesList, pEntity);
		pEntity->listNode = listFirst(s->entitiesList);
		dictAddWithLonglong(s->entitiesDict, id, pEntity);
	}

	s_details("Sudoku(%U)::SudokuEntry id:%U Euler(%f, %f, %f) pos(%f, %f, %f) rotation(%f, %f, %f, %f)"
		, s->spaceId, pEntity->entityid, rotation.x, rotation.y, rotation.z, pEntity->transform.position.x, pEntity->transform.position.y, pEntity->transform.position.z
		, pEntity->transform.rotation.w, pEntity->transform.rotation.x, pEntity->transform.rotation.y, pEntity->transform.rotation.z);

}

static int luaB_Entry(lua_State* L) {

	PSudoku s = lua_touserdata(L, 1);
	unsigned long long id = luaL_tou64(L, 2);

	//position
	struct Vector3 position = { 0 };
	position.x = luaL_checknumber(L, 3);
	position.y = luaL_checknumber(L, 4);
	position.z = luaL_checknumber(L, 5);

	//rotation
	struct Vector3 rotation = { 0 };
	rotation.x = luaL_checknumber(L, 6);
	rotation.y = luaL_checknumber(L, 7);
	rotation.z = luaL_checknumber(L, 8);

	//velocity 每秒移动的速度
	float velocity = luaL_checknumber(L, 9);

	//stamp 秒
	unsigned int stamp = luaL_checkinteger(L, 10);
	unsigned int stampStop = luaL_checkinteger(L, 11);
	int isGhost = luaL_checkinteger(L, 12);
	
	//s_fun("sudoku(%U)::Entry %U ",s->spaceId, id);
	SudokuEntry(s, id, position, rotation, velocity, stamp, stampStop, isGhost);
	return 0;
}

void FillView(PSudoku s, enum SudokuDir dir, PEntity pEntityEntry) {

	unsigned int viewgird = GirdDirId(s, dir, pEntityEntry->newGird);
	if (UINT_MAX == viewgird) {
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
				if (!(s->isBigWorld && (pEntityEntry->status & entity_ghost) && (pEntity->status & entity_limite) || (pEntity->status & entity_outside) || (pEntityEntry->status & entity_outside))) {
					//a发起移动更新，b是否被动更新分为3种情况。1，从原来a可见移动过来的，两边都不更新。2，从其他非可见移动过来(包括新进入)，不用处理各自更新。3，没动要更新。
					//a发起进入更新，b是否被动更新分为2种情况。1，从其他非可见移动过来(包括新进入)，不用处理各自更新。2，没动要更新。

					//a发起进入更新
					if (pEntityEntry->update == update_entry) {				
						SendAddView(s, pEntityEntry, pEntity);
						//b没动要更新。
						if (pEntity->update == update_stable) {
							SendAddView(s, pEntity, pEntityEntry);
						}
					}
					else //a发起移动更新
					{
						//两边原来是非可见
						if (pEntity->update == update_entry ||
							(pEntity->oldGird != UINT_MAX && pEntityEntry->oldGird != UINT_MAX && pEntity->oldGird != pEntityEntry->oldGird
							&& GirdDir(s, pEntityEntry->oldGird, pEntity->oldGird) == SudokuDirError)) {		
							SendAddView(s, pEntityEntry, pEntity);
							//b没动要更新。
							if (pEntity->update == update_stable) {
								SendAddView(s, pEntity, pEntityEntry);
							}
						}
					}
				}
			}
		}
	}
}

static void DestroyRtreeFun(void* value) {
	free(value);
}

void OnRedirectToSpace(PSudoku s, PEntity pEntity) {
	double rect[4] = { 0 };
	rect[0] = pEntity->nowPosition.x;
	rect[1] = pEntity->nowPosition.z;
	rect[2] = pEntity->nowPosition.x;
	rect[3] = pEntity->nowPosition.z;

	list* result = listCreate();
	rtree_search(s->prtree, rect, MultipleIter, result);

	if (listLength(result) > 0) {
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
	}

	listSetFreeMethod(result, DestroyRtreeFun);
	listRelease(result);
}

//每次切换格子时，进入或移动，要检查是否在边界，是否为ghost，是否超出地图范围
void SudokuUpdateLimite(PSudoku s, PEntity pEntity) {

	//进入边界地区，通知用户登陆其他空间
	if (s->isBigWorld && GirdLimite(s, &pEntity->nowPosition)) {

		if (!(pEntity->status & entity_limite)) {
			pEntity->status |= entity_limite;
			s_details("Sudoku(%U)::SudokuUpdateLimite::entity_limite id:%U x:%f y:%f z:%f", s->spaceId, pEntity->entityid, pEntity->nowPosition.x, pEntity->nowPosition.y, pEntity->nowPosition.z);
		}
		else {
			s_details("Sudoku(%U)::SudokuUpdateLimite::entity_limite_across id:%U x:%f y:%f z:%f", s->spaceId, pEntity->entityid, pEntity->nowPosition.x, pEntity->nowPosition.y, pEntity->nowPosition.z);
		}

		if (pEntity->status & entity_ghost)
			return;

		OnRedirectToSpace(s, pEntity);
	}
	else if (s->isBigWorld && !GirdLimite(s, &pEntity->nowPosition)) {
		if (pEntity->status & entity_limite) {

			if (pEntity->nowPosition.x < s->begin.x || pEntity->nowPosition.x > s->end.x || pEntity->nowPosition.z < s->begin.z || pEntity->nowPosition.z > s->end.z) {
				
				pEntity->status &= ~entity_limite;
				pEntity->status |= entity_outside;
				pEntity->outingStamp = GetCurrentSec();
				
				s_details("Sudoku(%U)::SudokuUpdateLimite::entity_outside id:%U x:%f y:%f z:%f", s->spaceId, pEntity->entityid, pEntity->nowPosition.x, pEntity->nowPosition.y, pEntity->nowPosition.z);
			}
			else {

				pEntity->status &= ~entity_limite;
				pEntity->status &= ~entity_ghost;

				//离开边界状态，通知更换主空间
				const char ptr[] = "OnDelGhost";
				mp_buf* pmp_buf = mp_buf_new();
				mp_encode_bytes(pmp_buf, ptr, sizeof(ptr) - 1);
				mp_encode_double(pmp_buf, u642double(s->spaceId));

				DockerSend(pEntity->entityid, pmp_buf->b, pmp_buf->len);
				mp_buf_free(pmp_buf);

				s_details("Sudoku(%U)::SudokuUpdateLimite::~entity_limite id:%U x:%f y:%f z:%f", s->spaceId, pEntity->entityid, pEntity->nowPosition.x, pEntity->nowPosition.y, pEntity->nowPosition.z);
			}
		}
	}
}

void SudokuUpdate(void* pSudoku, unsigned int count, float deltaTime) {

	PSudoku s = (PSudoku)pSudoku;
	s->update_count = count;

	listIter* iter = listGetIterator(s->entitiesList, AL_START_HEAD);
	listNode* node;
	while ((node = listNext(iter)) != NULL) {
		PEntity pEntity = listNodeValue(node);

		if (pEntity->status & entity_stop || pEntity->status & entity_outside) {
			continue;
		}

		if (pEntity->stampStop && GetCurrentSec() >= pEntity->stampStop) {
			pEntity->status |= entity_stop;
			s_details("Sudoku(%U)::update::stop id:%U x:%f y:%f z:%f", s->spaceId, pEntity->entityid, pEntity->nowPosition.x, pEntity->nowPosition.y, pEntity->nowPosition.z);
		}

		if (pEntity->velocity == 0 || pEntity->stamp == 0) {
			pEntity->status |= entity_stop;
			s_details("Sudoku(%U)::update::stop id:%U x:%f y:%f z:%f", s->spaceId, pEntity->entityid, pEntity->nowPosition.x, pEntity->nowPosition.y, pEntity->nowPosition.z);
		}

		struct Vector3 nowPosition = { 0 };
		struct Vector3 Euler = { 0 };
		quatToEulerAngles(&pEntity->transform.rotation, &Euler);
		CurrentPosition(&pEntity->transform.position, &Euler, pEntity->velocity, pEntity->stamp, pEntity->stampStop, &nowPosition, 0);
		//s_details("Sudoku(%U)::SudokuUpdate::nowPosition id:%U Euler(%f, %f, %f) pos(%f, %f, %f)", s->spaceId, pEntity->entityid, Euler.x, Euler.y, Euler.z, nowPosition.x, nowPosition.y, nowPosition.z);

		pEntity->nowPosition = nowPosition;
		pEntity->oldGird = pEntity->newGird;
		pEntity->newGird = GirdId(s, &pEntity->nowPosition);

		if (pEntity->update == update_stable || pEntity->update == update_change)
		{
			if (pEntity->newGird == pEntity->oldGird) {
				pEntity->update = update_stable;
			}
			else {
				s_details("Sudoku(%U)::SudokuUpdate::nowPosition id:%U Euler(%f, %f, %f) pos(%f, %f, %f)", s->spaceId, pEntity->entityid, Euler.x, Euler.y, Euler.z, nowPosition.x, nowPosition.y, nowPosition.z);
				if (GirdExchang(s, pEntity, pEntity->newGird)) {
					pEntity->update = update_change;
				}
			}
		}else if (pEntity->update == update_entry) {
			s_details("Sudoku(%U)::SudokuUpdate::GirdPut::nowPosition id:%U Euler(%f, %f, %f) pos(%f, %f, %f)", s->spaceId, pEntity->entityid, Euler.x, Euler.y, Euler.z, nowPosition.x, nowPosition.y, nowPosition.z);
			GirdPut(s, pEntity);
		}
	}
	listReleaseIterator(iter);


	iter = listGetIterator(s->entitiesList, AL_START_HEAD);
	while ((node = listNext(iter)) != NULL) {
		//搜索周围,生成每个用户的listview
		PEntity pEntity = listNodeValue(node);

		//判断游出边界
		if (pEntity->status & entity_outside) {
			continue;
		}
			
		if (pEntity->update == update_change) {

			//s_details("Sudoku(%U)::step2::normalupdate id:%U x:%f y:%f z:%f", s->spaceId, pEntity->entityid, pEntity->nowPosition.x, pEntity->nowPosition.y, pEntity->nowPosition.z);
			enum SudokuDir sudokuDir = GirdDir(s, pEntity->newGird, pEntity->oldGird);

			switch (sudokuDir)
			{
			case up:
			{
				FillView(s, up, pEntity);
				FillView(s, upleft, pEntity);
				FillView(s, upright, pEntity);
			}
			break;
			case down:
			{
				FillView(s, down, pEntity);
				FillView(s, downleft, pEntity);
				FillView(s, downright, pEntity);
			}
			break;
			case left:
			{
				FillView(s, upleft, pEntity);
				FillView(s, left, pEntity);
				FillView(s, downleft, pEntity);
			}
			break;
			case right:
			{
				FillView(s, upright, pEntity);
				FillView(s, right, pEntity);
				FillView(s, downright, pEntity);
			}
			break;
			case upleft:
			{
				FillView(s, up, pEntity);
				FillView(s, upleft, pEntity);
				FillView(s, upright, pEntity);
				FillView(s, left, pEntity);
				FillView(s, downleft, pEntity);
			}
			break;
			case upright:
			{
				FillView(s, up, pEntity);
				FillView(s, upleft, pEntity);
				FillView(s, upright, pEntity);
				FillView(s, right, pEntity);
				FillView(s, downright, pEntity);
			}
			break;
			case downleft:
			{
				FillView(s, upleft, pEntity);
				FillView(s, left, pEntity);
				FillView(s, downleft, pEntity);
				FillView(s, down, pEntity);
				FillView(s, downright, pEntity);
			}
			break;
			case downright:
			{
				FillView(s, upright, pEntity);
				FillView(s, right, pEntity);
				FillView(s, downright, pEntity);
				FillView(s, down, pEntity);
				FillView(s, downleft, pEntity);
			}
			break;
			default: {
				pEntity->update = update_entry;
				s_warn("Sudoku(%U)::SudokuUpdate::step2::Flash event in space eid: %U, newGrid: %i, oldGird: %i x:%f y:%f z:%f", s->spaceId, pEntity->entityid, pEntity->newGird, pEntity->oldGird, pEntity->nowPosition.x, pEntity->nowPosition.y, pEntity->nowPosition.z);
				}
			}

			if (pEntity->update == update_change) {
				SudokuUpdateLimite(s, pEntity);
			}
		}
		
		if (pEntity->update == update_entry) {

			s_details("Sudoku(%U)::step2::all update id:%U x:%f y:%f z:%f", s->spaceId, pEntity->entityid, pEntity->nowPosition.x, pEntity->nowPosition.y, pEntity->nowPosition.z);

			FillView(s, up, pEntity);
			FillView(s, down, pEntity);
			FillView(s, left, pEntity);
			FillView(s, right, pEntity);
			FillView(s, upleft, pEntity);
			FillView(s, upright, pEntity);
			FillView(s, downleft, pEntity);
			FillView(s, downright, pEntity);
			FillView(s, dircentre, pEntity);
			SudokuUpdateLimite(s, pEntity);
		}

		
	}
	listReleaseIterator(iter);

	iter = listGetIterator(s->entitiesList, AL_START_HEAD);
	while ((node = listNext(iter)) != NULL) {
		PEntity pEntity = listNodeValue(node);
		
		pEntity->update = update_stable;

		//删除已经超过60秒离开边界的对象
		if (pEntity->status & entity_outside && pEntity->outingStamp && (GetCurrentSec() - pEntity->outingStamp) > s->outsideSec) {
			SudokuLeave(s, pEntity->entityid);

			mp_buf* pmp_buf = mp_buf_new();
			MP_ENCODE_CONST(pmp_buf, "OnLeaveSpace");
			mp_encode_double(pmp_buf, u642double(pEntity->entityid));

			DockerSend(pEntity->entityid, pmp_buf->b, pmp_buf->len);
			mp_buf_free(pmp_buf);
		}
	}
	listReleaseIterator(iter);

	DockerPushAllMsgList(s->msgList);
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
	unsigned int count = luaL_checkinteger(L, 2);
	float deltaTime = luaL_checknumber(L, 3);
	//s_fun("sudoku(%U)::Update", s->spaceId);
	SudokuUpdate(s, count, deltaTime);
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

	s->linex = (unsigned int)floor((s->end.x - s->begin.x) / s->gird.x);
	unsigned int dx = (unsigned int)floor((s->end.x - 0.001 - s->begin.x) / s->gird.x);
	unsigned int dz = (unsigned int)floor((s->end.z - 0.001 - s->begin.z) / s->gird.z);
	s->maxid = dx + s->linex * dz;

	s_fun("sudoku(%U)::Create isBigworld:%i gx:%f gz:%f bx:%f bz:%f ex:%f ez:%f linex:%i maxid:%i"
		, spaceId, isBigWorld, gird.x, gird.z, begin.x, begin.z, end.x, end.z, s->linex, s->maxid);

	s->isBigWorld = isBigWorld;
	s->spaceId = spaceId;
	s->prtree = rtree_new(sizeof(unsigned long long), 2);
	s->outsideSec = outsideSec;

	s->msgList = DockerCreateMsgList();

	double rect[4] = { 0 };
	rect[0] = begin.x;
	rect[1] = begin.z;
	rect[2] = end.x;
	rect[3] = end.z;

	rtree_insert(s->prtree, rect, &s->spaceId);
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

	lua_pushlightuserdata(L, SudokuCreate(gird, begin, end, isBigWorld, spaceId, outsideSec));
	return 1;
}

static void DestroyFun(void* value) {

	PEntity pEntity = value;
	free(pEntity);
}

void SudokuDestory(void* pSudokus) {

	PSudoku s = (PSudoku)pSudokus;
	dictRelease(s->grid_entity);
	dictRelease(s->entitiesDict);

	listSetFreeMethod(s->entitiesList, DestroyFun);
	listRelease(s->entitiesList);

	DockerDestoryMsgList(s->msgList);

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
		unsigned int dx = (unsigned int)floor((s->end.x - 0.001 - s->begin.x) / s->gird.x);
		unsigned int dz = (unsigned int)floor((s->end.z - 0.001 - s->begin.z) / s->gird.z);
		s->maxid = dx + s->linex * dz;
		s_details("sudoku(%U)::Config bx:%f bz:%f ex:%f ez:%f", s->spaceId, s->begin.x, s->begin.z, s->end.x, s->end.z);
	}

	rtree_delete(s->prtree, rect, &id);
	rtree_insert(s->prtree, rect, &id);
	return 0;
}

static int luaB_SetGhost(lua_State* L) {

	PSudoku s = lua_touserdata(L, 1);
	unsigned long long id = luaL_tou64(L, 2);

	//s_fun("sudoku(%U)::SetGhost %U", s->spaceId, id);

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