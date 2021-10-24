#include <stdio.h>
#include <stdlib.h>
#include <sigar.h>

void checkStatus(int status, const char* msg) {
    if(status != SIGAR_OK){
        printf("%s\n",msg);
        exit(-1);
    }
}

void checkStatusWithClose(sigar_t* sig, int status, const char* msg) {
    if(status != SIGAR_OK){
        sigar_close(sig);
        printf("%s\n",msg);
        exit(-1);
    }
}

void printMemInfo(sigar_mem_t* memInfo) {
    printf("Memory usage:\n");
    printf("\tRAM: %ld MB\n\n", memInfo->ram);

    printf("\tTotal: %ld KB\n", (memInfo->total / 1024));
    printf("\tFree: %ld KB\n", (memInfo->free / 1024));
    printf("\tUsed: %ld KB\n\n", (memInfo->used / 1024));

    printf("\tActual free: %ld KB\n", (memInfo->actual_free / 1024));
    printf("\tActual used: %ld KB\n\n", (memInfo->actual_used / 1024));

    printf("\tFree percent: %.2lf\n", memInfo->free_percent);
    printf("\tUsed percent: %.2lf\n\n", memInfo->used_percent);
}

void printSwapInfo(sigar_swap_t* swapInfo) {
    printf("Swap usage:\n");
    printf("\tTotal: %ld KB\n", (swapInfo->total / 1024));
    printf("\tFree: %ld KB\n", (swapInfo->free / 1024));
    printf("\tUsed: %ld KB\n\n", (swapInfo->used / 1024));

    printf("\tPage in: %ld KB\n", (swapInfo->page_in / 1024));
    printf("\tPage out: %ld KB\n\n", (swapInfo->page_out / 1024));
}

void printCpu(sigar_cpu_t* cpu){
    printf("Cpu:\n");
    printf("\tUser: %ld\n", cpu->user);
    printf("\tSys: %ld\n", cpu->sys);
    printf("\tNice: %ld\n", cpu->nice);
    printf("\tIdle: %ld\n", cpu->idle);
    printf("\tWait: %ld\n", cpu->wait);
    printf("\tIrq: %ld\n", cpu->irq);
    printf("\tSoft Irq: %ld\n", cpu->soft_irq);
    printf("\tStolen: %ld\n", cpu->stolen);
    printf("\tTotal: %ld\n\n", cpu->total);
}

void printCpuList(sigar_cpu_list_t* cpuList) {
    sigar_cpu_t* cpu = cpuList->data;
    for(int i = 0; i < cpuList->number; i++) {
        printCpu(cpu);
        cpu = cpu + 1;
    }
}

void printCpuInfo(sigar_cpu_info_t* cpuInfo) {
    printf("Cpu info:\n");
    printf("\tVendor: %s\n", cpuInfo->vendor);
    printf("\tModel: %s\n\n", cpuInfo->model);
    printf("\tMhz: %d\n", cpuInfo->mhz);
    printf("\tMaximum Mhz: %d\n", cpuInfo->mhz_max);
    printf("\tMinimal Mhz: %d\n", cpuInfo->mhz_min);
    printf("\tCache size: %ld\n", cpuInfo->cache_size);
    printf("\tTotal sockets: %d\n", cpuInfo->total_sockets);
    printf("\tTotal cores: %d\n", cpuInfo->total_cores);
    printf("\tCores per socket: %d\n\n", cpuInfo->cores_per_socket);
}

void printCpuInfoList(sigar_cpu_info_list_t* cpuInfoList) {
    sigar_cpu_info_t* cpuInfo = cpuInfoList->data;
    for(int i = 0; i < cpuInfoList->number; i++){
        printCpuInfo(cpuInfo);
        cpuInfo = cpuInfo + 1;
    }
}

int main() {
    sigar_t* sig;
    int status = sigar_open(&sig);
    checkStatus(status,"Cannot open Sigar");

    sigar_mem_t memInfo;
    status = sigar_mem_get(sig,&memInfo);
    checkStatusWithClose(sig,status,"Cannot get memory info");
    printMemInfo(&memInfo);

    sigar_swap_t swapInfo;
    status = sigar_swap_get(sig,&swapInfo);
    checkStatusWithClose(sig,status,"Cannot get swap info");
    printSwapInfo(&swapInfo);

    sigar_cpu_list_t cpuList;
    status = sigar_cpu_list_get(sig,&cpuList);
    checkStatusWithClose(sig,status,"Cannot get cpu list");
    printCpuList(&cpuList);
    sigar_cpu_list_destroy(sig,&cpuList);

    sigar_cpu_info_list_t cpuInfoList;
    status = sigar_cpu_info_list_get(sig,&cpuInfoList);
    checkStatusWithClose(sig, status, "Cannot get cpu info list");
    printCpuInfoList(&cpuInfoList);
    sigar_cpu_info_list_destroy(sig,&cpuInfoList);

    sigar_uptime_t uptime;
    status = sigar_uptime_get(sig,&uptime);
    checkStatusWithClose(sig, status, "Cannot get uptime");
    printf("Uptime: %.2lf\n\n", uptime.uptime);

    sigar_loadavg_t loadAvarage;
    status = sigar_loadavg_get(sig,&loadAvarage);
    checkStatusWithClose(sig, status, "Cannot get load avarage");
    printf("Load avarage: %lf %lf %lf\n\n", loadAvarage.loadavg[0], loadAvarage.loadavg[1], loadAvarage.loadavg[2]);

    sigar_close(sig);
}
