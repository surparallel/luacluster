/* entity.c
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
* 
* 1）NAN - Not A Number。意思是不是一个数值。VS调试显示类似”1.#QNAN00000000000“。产生原因：无意义的算术计算如负数开方。判断方法：包含float.h头文件，调用_isnan()。指数位全为1，尾数位为空。
* 2）IND - Indeterminate Number。意思是不确定数值。VS调试显示类似“1.#IND000000000000”。是NAN的一种特殊情况。产生原因：0除0，或无限大除无限大。判断方法通NAN。指数位全为1，尾数位不为空。
* 3）INF - Infinity。意思是无限大。VS调试显示类似“1.#INF000000000000”。产生原因：如1/0.0的计算结果。判断方法：_finite()。 7ff开头其余位为0。
* 4）DEN - Denormalized。意思是非规格化数值。VS调试显示类似“4.940656458421e-324#DEN”。指数位全为零，尾数位不为空。
*/

#include "plateform.h"
#include "entityid.h"

