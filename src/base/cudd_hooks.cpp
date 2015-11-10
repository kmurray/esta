#include "cudd_hooks.hpp"
#include "util.h" //From CUDD

int PreReorderHook( DdManager *dd, const char *str, void *data) {
    int retval;

    retval = fprintf(dd->out,"%s reordering", str);
    if (retval == EOF) return(0);

    retval = fprintf(dd->out,": from %ld to ... ", strcmp(str, "BDD") == 0 ?
		     Cudd_ReadNodeCount(dd) : Cudd_zddReadNodeCount(dd));
    if (retval == EOF) return(0);
    fflush(dd->out);
    return(1);

}

int PostReorderHook( DdManager *dd, const char *str, void *data) {
    unsigned long initialTime = (long) data;
    int retval;
    unsigned long finalTime = util_cpu_time();
    double totalTimeSec = (double)(finalTime - initialTime) / 1000.0;

    auto node_cnt = strcmp(str, "BDD") == 0 ? Cudd_ReadNodeCount(dd) : Cudd_zddReadNodeCount(dd);

    retval = fprintf(dd->out,"%ld nodes in %g sec", node_cnt,
		     totalTimeSec);
    //Override default reorder size to be factor of two
    auto next_reorder = std::max(2*node_cnt, (long) 4096) + dd->constants.keys;
    Cudd_SetNextReordering(dd, next_reorder);
    retval = fprintf(dd->out," (next reorder %u nodes)\n", dd->nextDyn - dd->constants.keys);
    if (retval == EOF) return(0);
    retval = fflush(dd->out);
    if (retval == EOF) return(0);
    return(1);
}

int PreGarbageCollectHook(DdManager* dd, const char* str, void* data) {
    fprintf(dd->out,"%s gc %u dead nodes ...", str, dd->dead);
    return 1;
}

int PostGarbageCollectHook(DdManager* dd, const char* str, void* data) {
    fprintf(dd->out,"gc finished\n");
    return 1;
}

