// Copyright 2020 Joshua J Baker. All rights reserved.
// Use of this source code is governed by an MIT-style
// license that can be found in the LICENSE file.

#ifndef RTREE_H
#define RTREE_H

#include <stdbool.h>
#include <stddef.h>

struct node {
    bool leaf;
    int count;
    void* rect;
    void* data;
    struct node* next; // used in split reinsertions
};

struct group {
    struct node** nodes;
    int len, cap;
};

struct pool {
    struct group leaves;
    struct group branches;
};

struct rtree {
    size_t elsize;
    int dims;
    int max_items;
    int min_items;
    int height;
    struct pool pool;
    size_t count;
    struct node* root;
    double* rect;

    bool use_reinsert;
    struct node* reinsert;
    size_t reinsert_count;
};

struct rtree *rtree_new(size_t elsize, int dims);
void rtree_free(struct rtree *rtree);
size_t rtree_count(struct rtree *rtree);
bool rtree_insert(struct rtree *rtree, double *rect, void *item);
bool rtree_delete(struct rtree *rtree, double *rect, void *item);
bool rtree_search(struct rtree *rtree, double *rect, 
                  bool (*iter)(const double *rect, const void *item, 
                               void *udata), 
                  void *udata);

void rtree_set_allocator(void *(malloc)(size_t), void (*free)(void*));

#endif

