package gotoc

/*
#include "../../../../include/sigar.h"
#include "../../../../include/sigar_ptql.h"
#include <stdio.h>
#include <stdlib.h>
*/
import "C"

import (
	"fmt"
	"unsafe"
)

func ExecutePtql(ptql string, infoTypesMask ProcInfoType) (result []*ProcessInfo, err error) {
	defer Panic2Error(&err)

	var query *C.sigar_ptql_query_t
	var queryError *C.sigar_ptql_error_t
	var proclist C.sigar_proc_list_t
	var ptqlFunc C.sigar_ptql_re_impl_t
	var data unsafe.Pointer
	sigar := GetSigarHandle()
	ptqlC := C.CString(ptql)
	//defer C.sigar_ptql_query_destroy(query)
	defer C.sigar_proc_list_destroy(sigar, &proclist)
	defer C.sigar_ptql_re_impl_set(sigar, data, ptqlFunc)
	defer Free(ptqlC)

	if status := C.sigar_ptql_query_create(&query, ptqlC, queryError); status != SIGAR_OK {
		err = fmt.Errorf("Failed to create sigar ptql query for: %v with errorcode: %v", ptql, status)
		return nil, err
	}

	C.sigar_ptql_query_find(sigar, query, &proclist)
	C.sigar_ptql_query_destroy(query)

	/*
		noOfProcesses := int(proclist.number)
		result = make([]*ProcessInfo, noOfProcesses)
		pids :=  *(*[]C.sigar_pid_t) (CArr2SlicePtr(noOfProcesses, proclist.data))
		for i,pid := range pids {
			procInfo,err := GetProcInfo(uint64(pid), infoTypesMask)
			if err != nil {
				return nil,err
			}
			result[i] =  procInfo
		}

		return result,nil*/

	processes, err := getProcInfos(&proclist, infoTypesMask, sigar)
	return processes.Processes, err
}
