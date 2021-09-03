// Copyright 2020 Joshua J Baker. All rights reserved.
// Use of this source code is governed by an MIT-style
// license that can be found in the LICENSE file.

#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdio.h>
#include <stdbool.h>
#include <time.h>
#include "rtree.h"

#define MIN(a,b) ((a) < (b) ? (a) : (b))
#define MAX(a,b) ((a) > (b) ? (a) : (b))

#define ALLOW_REINSERTS
#define MAXITEMS 32      // 32 max items per node
#define MINFILL  20      // 20% min fill

static void *(*_malloc)(size_t) = NULL;
static void (*_free)(void *) = NULL;

#define rtmalloc (_malloc?_malloc:malloc)
#define rtfree (_free?_free:free)

// rtree_set_allocator allows for configuring a custom allocator for
// all rtree library operations. This function, if needed, should be called
// only once at startup and a prior to calling rtree_new().
void rtree_set_allocator(void *(malloc)(size_t), void (*free)(void*)) {
    _malloc = malloc;
    _free = free;
}

#define panic(_msg_) { \
    fprintf(stderr, "panic: %s (%s:%d)\n", (_msg_), __FILE__, __LINE__); \
    exit(1); \
}

// node_new returns a new internal rtree node. The allocation is oversized and
// the rect and data (items and children) are allocated specifically for a
// leaf or branch.
static struct node *node_new(struct rtree *rtree, bool leaf){
    size_t elsize = leaf?rtree->elsize:sizeof(struct node*);
    size_t rectsz = 8 * rtree->dims * 2 * (rtree->max_items+1);
    size_t datasz = elsize * (rtree->max_items+1);
    size_t nodesz = sizeof(struct node) + rectsz + datasz;
    struct node *node = rtmalloc(nodesz);
    if (!node) {
        return NULL;
    }
    node->leaf = leaf;
    node->count = 0;
    node->rect = ((char*)node)+sizeof(struct node);
    node->data = ((char*)node->rect)+rectsz;
    return node;
}

// item_at returns a leaf item at index
static void *item_at(struct rtree *rtree, struct node *node, int index) {
    return ((char*)node->data) + rtree->elsize * index;
}

static void item_copy(struct rtree *rtree, void *item, void *from) {
    memcpy(item, from, rtree->elsize);
}

// rect_at returns a node rect at index
static double *rect_at(struct rtree *rtree, struct node *node, int index) {
    return (void*)(((char*)node->rect) + (8 * rtree->dims * 2) * index);
}
static void rect_copy(struct rtree *rtree, double *rect, double *from) {
    memcpy(rect, from, 8*rtree->dims*2);
}

static struct node **node_at(struct rtree *rtree, struct node *node, 
                             int index) 
{
    if (rtree){}
    return ((struct node**)node->data)+index;
}
static void node_copy(struct rtree *rtree, struct node **node, 
                      struct node **from) 
{
    if (rtree){}
    memcpy(node, from, sizeof(struct node*));
}

// node_rect_data_copy copies the rect and data from src into dst.
// It's expected that dst and src are the same type of node (leaf or branch).
static void node_rect_data_copy(struct rtree *rtree, 
                                struct node *dst, int dst_index, 
                                struct node *src, int src_index)
{
    rect_copy(rtree,
              rect_at(rtree, dst, dst_index), 
              rect_at(rtree, src, src_index));
    if (dst->leaf) {
        item_copy(rtree,
                  item_at(rtree, dst, dst_index),
                  item_at(rtree, src, src_index));
    } else {
        node_copy(rtree,
                  node_at(rtree, dst, dst_index),
                  node_at(rtree, src, src_index));
    }
}

static void rect_expand(double *rect, double *other, int dims) {
    rect[0] = MIN(rect[0], other[0]); 
    rect[dims+0] = MAX(rect[dims+0], other[dims+0]);
    for (int i = 1; i < dims; i++) {
        rect[i] = MIN(rect[i], other[i]); 
        rect[dims+i] = MAX(rect[dims+i], other[dims+i]);
    }
}

// rect_calc calculates a the node mbr and puts result into rect.
static void rect_calc(struct rtree *rtree, struct node *node, double *rect) {
    rect_copy(rtree, rect, rect_at(rtree, node, 0));
    for (int i = 1; i < node->count; i++) {
        rect_expand(rect, rect_at(rtree, node, i), rtree->dims);
    }
}
// dims dimensions Î¬¶È
struct rtree *rtree_new(size_t elsize, int dims) {
    if (dims < 1) panic("invalid dims");
    if (elsize == 0) panic("elsize is zero");
    struct rtree *rtree = rtmalloc(sizeof(struct rtree));
    if (!rtree) {
        return NULL;
    }
    memset(rtree, 0, sizeof(struct rtree));
#ifdef ALLOW_REINSERTS
    rtree->use_reinsert = true;
#endif
    rtree->elsize = elsize;
    rtree->dims = dims;
    rtree->max_items = MAXITEMS;
    rtree->min_items = (rtree->max_items*MINFILL/100)+1;
    rtree->rect = rtmalloc(8*dims*2);
    if (!rtree->rect) {
        rtfree(rtree);
        return NULL;
    }
    return rtree;
}

static struct node *gimme_node(struct group *group) {
    if (group->len == 0) panic("out of nodes");
    return group->nodes[--group->len];
}

static struct node *gimme_leaf(struct rtree *rtree) {
    return gimme_node(&rtree->pool.leaves);
}

static struct node *gimme_branch(struct rtree *rtree) {
    return gimme_node(&rtree->pool.branches);
}

static bool grow_group(struct group *group) {
    int cap = group->cap?group->cap*2:1;
    struct node **nodes = rtmalloc(sizeof(struct node*)*cap);
    if (!nodes) {
        return false;
    }
    memcpy(nodes, group->nodes, group->len*sizeof(struct node*));
    rtfree(group->nodes);
    group->nodes = nodes;
    group->cap = cap;
    return true;
}

static void takeaway(struct rtree *rtree, struct node *node) {
    const int MAXLEN = 32;
    struct group *group;
    if (node->leaf) {
        group = &rtree->pool.leaves;
    } else {
        group = &rtree->pool.branches;
    }
    if (group->len == MAXLEN) {
        rtfree(node);
        return;
    }
    if (group->len == group->cap) {
        if (!grow_group(group)) {
            rtfree(node);
            return;
        }
    }
    group->nodes[group->len++] = node;
}

// fill_pool fills the node pool prior to inserting items. This ensures there
// is enough memory before we begin doing to things like splits and tree
// rebalancing. There needs to be at least two available leaf and N branches
// where N is equal to the height of the tree plus one.
static bool fill_pool(struct rtree *rtree) {
    while (rtree->pool.leaves.len < 2) {
        if (rtree->pool.leaves.len == rtree->pool.leaves.cap) {
            if (!grow_group(&rtree->pool.leaves)) {
                return false;
            }
        }
        struct node *leaf = node_new(rtree, true);
        if (!leaf) {
            return false;
        }
        rtree->pool.leaves.nodes[rtree->pool.leaves.len++] = leaf;
    }
    while (rtree->pool.branches.len < rtree->height+1) {
        if (rtree->pool.branches.len == rtree->pool.branches.cap) {
            if (!grow_group(&rtree->pool.branches)) {
                return false;
            }
        }
        struct node *branch = node_new(rtree, false);
        if (!branch) {
            return false;
        }
        rtree->pool.branches.nodes[rtree->pool.branches.len++] = branch;
    }
    return true;
}

static int rect_largest_axis(double *rect, int dims) {
    int axis = 0;
    double size = rect[dims+0]-rect[0];
    for (int i = 1; i < dims; i++) {
        double nsize = rect[dims+i]-rect[i];
        if (nsize > size) {
            axis = i;
            size = nsize;
        }
    }
    return axis;
}

// node_split splits the node into two and returns the newly created node.
// It uses the R!Tree (rbang-tree) algorithm, which is custom to this
// R-tree implemention. 
// For more information please visit https://github.com/tidwall/rbang
static struct node *node_split(struct rtree *rtree, struct node *node, 
                               double *rect)
{
    int dims = rtree->dims;
    int axis = rect_largest_axis(rect, dims);
    double axis_min = rect[axis];
    double axis_max = rect[dims+axis];
    struct node *right = node->leaf ? gimme_leaf(rtree) : gimme_branch(rtree);
    right->count = 0;
    struct node *equals = node->leaf ? gimme_leaf(rtree) : gimme_branch(rtree);
    equals->count = 0;
    for (int i = 0; i < node->count; i++) {
        double *crect = rect_at(rtree, node, i); // child rect
        double min_dist = crect[axis] - axis_min;
        double max_dist = axis_max - crect[dims+axis];
        if (min_dist < max_dist) {
            // keep the child in the left node
            continue;
        }
        if (min_dist > max_dist) {
            // move child to right
            node_rect_data_copy(rtree, right, right->count, node, i);
            right->count++;
        } else {
            // move child to equals bucket and resolve ties below
            node_rect_data_copy(rtree, equals, equals->count, node, i);
            equals->count++;
        }
        node_rect_data_copy(rtree, node, i, node, node->count-1);
        node->count--;
        i--;
    }
    // resolve ties
    for (int i = 0; i < equals->count; i++) {
        if (node->count < right->count) {
            node_rect_data_copy(rtree, node, node->count, equals, i);
            node->count++;
        } else {
            node_rect_data_copy(rtree, right, right->count, equals, i);
            right->count++;
        }
    }
    takeaway(rtree, equals);
    return right;
}

// rect_enlarged_area calculates the enlarged area rect when unioned with other
// rect, and it also calculates the total area of rect.
static double rect_enlarged_area_d(double *rect, double *other, int dims, 
                                   double *area_out)
{
    double enlarged = MAX(other[dims+0], rect[dims+0]) - MIN(other[0], rect[0]);
    double area = rect[dims+0] - rect[0];
    for (int i = 1; i < dims; i++) {
        enlarged *= MAX(other[dims+i], rect[dims+i]) - MIN(other[i], rect[i]);
        area *= rect[dims+i] - rect[i];
    }
    *area_out = area;
    return enlarged - area;
}
static double rect_enlarged_area_2(double *rect, double *other, int dims, 
                                   double *area_out)
{
    double area = (rect[dims+0] - rect[0]) *
                  (rect[dims+1] - rect[1]);
    double enlarged = 
        (MAX(other[dims+0], rect[dims+0]) - MIN(other[0], rect[0])) *
        (MAX(other[dims+1], rect[dims+1]) - MIN(other[1], rect[1]));    
    *area_out = area;
    return enlarged - area;
}
static double rect_enlarged_area_3(double *rect, double *other, int dims, 
                                   double *area_out)
{
    double enlarged = 
        (MAX(other[dims+0], rect[dims+0]) - MIN(other[0], rect[0])) *
        (MAX(other[dims+1], rect[dims+1]) - MIN(other[1], rect[1])) *
        (MAX(other[dims+2], rect[dims+2]) - MIN(other[2], rect[2]));    
    double area = (rect[dims+0] - rect[0]) *
                  (rect[dims+1] - rect[1]) *
                  (rect[dims+2] - rect[2]);
    *area_out = area;
    return enlarged - area;
}
static double rect_enlarged_area_4(double *rect, double *other, int dims, 
                                   double *area_out)
{
    double enlarged = 
        (MAX(other[dims+0], rect[dims+0]) - MIN(other[0], rect[0])) *
        (MAX(other[dims+1], rect[dims+1]) - MIN(other[1], rect[1])) *
        (MAX(other[dims+2], rect[dims+2]) - MIN(other[2], rect[2])) *
        (MAX(other[dims+3], rect[dims+3]) - MIN(other[3], rect[3]));    
    double area = (rect[dims+0] - rect[0]) *
                  (rect[dims+1] - rect[1]) *
                  (rect[dims+2] - rect[2]) *
                  (rect[dims+3] - rect[3]);
    *area_out = area;
    return enlarged - area;
}
/*
// subtree returns the best candidate for inserting a rectangle. It
// searches for the rectangle which requires the least enlargement. When two
// candidate enlargments are equal, the one with the smallest area wins.
#define FN_SUBTREE(fn_subtree, fn_rect_enlarged_area) \
static int \
fn_subtree(double *rects, int nrects, double *rect, int dims) { \
    double *crect = rects; \
    int j = 0; \
    double j_area; \
    double j_enlargement = fn_rect_enlarged_area(crect, rect, dims, &j_area); \
    for (int i = 1; i < nrects; i++) { \
        crect += dims*2; \
        double area; \
        double enlargement = fn_rect_enlarged_area(crect, rect, dims, &area); \
        if (enlargement > j_enlargement) { \
            continue; \
        } \
        if (enlargement == j_enlargement) { \
            if (area > j_area) { \
                continue; \
            } \
        } \
        j = i; \
        j_enlargement = enlargement; \
        j_area = area; \
    } \
    return j; \
}

FN_SUBTREE(subtree_d, rect_enlarged_area_d);
FN_SUBTREE(subtree_2, rect_enlarged_area_2);
FN_SUBTREE(subtree_3, rect_enlarged_area_3);
FN_SUBTREE(subtree_4, rect_enlarged_area_4);
*/
typedef double (*Pfn_rect_enlarged_area)(double* rect, double* other, int dims, double* area_out);

static int fn_subtree(double *rects, int nrects, double *rect, int dims, Pfn_rect_enlarged_area fn_rect_enlarged_area) {
    double *crect = rects;
    int j = 0;
    double j_area;
    double j_enlargement = fn_rect_enlarged_area(crect, rect, dims, &j_area);
    for (int i = 1; i < nrects; i++) {
        crect += dims*2;
        double area;
        double enlargement = fn_rect_enlarged_area(crect, rect, dims, &area);
        if (enlargement > j_enlargement) {
            continue;
        }
        if (enlargement == j_enlargement) {
            if (area > j_area) {
                continue;
            }
        }
        j = i;
        j_enlargement = enlargement;
        j_area = area;
    }
    return j; 
}

// node_insert inserts an item into the node. If the node is a branch, then
// a child node (subtree) is chosen until a leaf node is found.
static void node_insert(struct rtree *rtree, struct node *node, double *rect, 
                        void *item)
{
    if (node->leaf) {
        rect_copy(rtree, rect_at(rtree, node, node->count), rect);
        item_copy(rtree, item_at(rtree, node, node->count), item);
        node->count++;
        return;
    }
    int dims = rtree->dims;
    int index;
    switch (rtree->dims) {
    case 2:  index = fn_subtree(node->rect, node->count, rect, dims, rect_enlarged_area_2); break;
    case 3:  index = fn_subtree(node->rect, node->count, rect, dims, rect_enlarged_area_3); break;
    case 4:  index = fn_subtree(node->rect, node->count, rect, dims, rect_enlarged_area_4); break;
    default: index = fn_subtree(node->rect, node->count, rect, dims, rect_enlarged_area_d);
    }
    struct node *child = *node_at(rtree, node, index);
    double *child_rect = rect_at(rtree, node, index);
    node_insert(rtree, child, rect, item);
    rect_expand(child_rect, rect, rtree->dims);
    if (child->count == rtree->max_items+1) {
        struct node *right = node_split(rtree, child, child_rect);
        rect_calc(rtree, child, child_rect);
        rect_calc(rtree, right, rect_at(rtree, node, node->count));
        node_copy(rtree, node_at(rtree, node, node->count), &right);
        node->count++;
    }
}

static bool rtree_insert_x(struct rtree *rtree, double *rect, void *item) {
    if (!fill_pool(rtree)) {
        return false;
    }
    if (!rtree->root) {
        rtree->root = gimme_leaf(rtree);
        rtree->root->count = 1;
        rtree->height = 1;
        rtree->count = 1;
        rect_copy(rtree, rect_at(rtree, rtree->root, 0), rect);
        item_copy(rtree, item_at(rtree, rtree->root, 0), item);
        rect_copy(rtree, rtree->rect, rect);
        return true;
    }
    node_insert(rtree, rtree->root, rect, item);
    rect_expand(rtree->rect, rect, rtree->dims);
    if (rtree->root->count == rtree->max_items+1) {
        // overflow. split root into two and calculate their rects
        //double right_rect[8*rtree->dims*2]; // VLA
        double* right_rect = malloc(8 * rtree->dims * 2);
        struct node *right = node_split(rtree, rtree->root, rtree->rect);
        rect_calc(rtree, rtree->root, rtree->rect);
        rect_calc(rtree, right, right_rect);

        // create a new root and place the old root and new node into new root
        // at positions 0 and 1.
        struct node *new_root = gimme_branch(rtree);
        rect_copy(rtree, rect_at(rtree, new_root, 0), rtree->rect);
        node_copy(rtree, node_at(rtree, new_root, 0), &rtree->root);
        rect_copy(rtree, rect_at(rtree, new_root, 1), right_rect);
        node_copy(rtree, node_at(rtree, new_root, 1), &right);

        // assign the new root
        rtree->root = new_root;
        rtree->root->count = 2;
        
        // finally, recalculate the root
        rect_calc(rtree, rtree->root, rtree->rect);
        rtree->height++;
        free(right_rect);
    }
    rtree->count++;
    return true;
}

// attempt_reinsert tries to reinsert all items in the reinsert list. Because
// an insert can fail due to the system being out of memory, this operation is
// only an attempt. It's ok if we fail here. We'll automatically try again 
// later.
static void attempt_reinsert(struct rtree *rtree) {
    // resinsert items from leaf nodes
    while (rtree->reinsert) {
        struct node *node = rtree->reinsert;
        while (node->count > 0) {
            void *item = item_at(rtree, node, node->count-1);
            double *rect = rect_at(rtree, node, node->count-1);
            if (!rtree_insert_x(rtree, rect, item)) {
                return; // out of memory, that's ok
            }
            rtree->reinsert_count--;
            node->count--;
        }
        rtree->reinsert = node->next;
        takeaway(rtree, node);
    }
}

// rtree_insert inserts an item into the rtree. This operation performs a copy
// of the data that is pointed to in the second and third arguments. The R-tree
// expects a rectangle, which is an array of doubles, that has the first N
// values as the minimum corner of the rect, and the next N values as the
// maximum corner of the rect, where N is the number of dimensions provided
// to rtree_new().
// Returns false if the system is out of memory.
bool rtree_insert(struct rtree *rtree, double *rect, void *item) {
    if (rtree->reinsert) {
        attempt_reinsert(rtree);
    }
    return rtree_insert_x(rtree, rect, item);
}

static void node_free(struct rtree *rtree, struct node *node) {
    if (!node->leaf) {
        for (int i=0;i<node->count;i++) {
            node_free(rtree, *node_at(rtree, node, i));
        }
    }
    rtfree(node);
}

static void release_pool(struct rtree *rtree) {
    for (int i = 0; i < rtree->pool.leaves.len; i++) {
        rtfree(rtree->pool.leaves.nodes[i]);
    }
    rtfree(rtree->pool.leaves.nodes);
    for (int i = 0; i < rtree->pool.branches.len; i++) {
        rtfree(rtree->pool.branches.nodes[i]);
    }
    rtfree(rtree->pool.branches.nodes);
    memset(&rtree->pool, 0, sizeof(struct pool));
}

// rtree_free frees the rtree.
void rtree_free(struct rtree *rtree) {
    if (!rtree) {
        return;
    }
    if (rtree->root) {
        node_free(rtree, rtree->root);
    }
    release_pool(rtree);
    while (rtree->reinsert) {
        rtfree(rtree->reinsert);
        rtree->reinsert = rtree->reinsert->next;
    }
    rtfree(rtree->rect);
    rtfree(rtree);
}

// rtree_count returns the number of items in the rtree.
size_t rtree_count(struct rtree *rtree) {
    return rtree->count + rtree->reinsert_count;
}

static bool inter_d(double *rect, double *other, int dims) {
    if (rect[dims+0] < other[0] || rect[0] > other[dims+0]) {
        return false;
    }
    for (int i = 1; i < dims; i++) {
        if (rect[dims+i] < other[i] || rect[i] > other[dims+i]) {
            return false;
        }
    }
    return true;
}
static bool inter_2(double *rect, double *other, int dims) {
    return !(rect[dims+0] < other[0] || rect[0] > other[dims+0] ||
             rect[dims+1] < other[1] || rect[1] > other[dims+1]);
}
static bool inter_3(double *rect, double *other, int dims) {
    return !(rect[dims+0] < other[0] || rect[0] > other[dims+0] ||
             rect[dims+1] < other[1] || rect[1] > other[dims+1] ||
             rect[dims+2] < other[2] || rect[2] > other[dims+2]);
}
static bool inter_4(double *rect, double *other, int dims) {
    return !(rect[dims+0] < other[0] || rect[0] > other[dims+0] ||
             rect[dims+1] < other[1] || rect[1] > other[dims+1] ||
             rect[dims+2] < other[2] || rect[2] > other[dims+2] ||
             rect[dims+3] < other[3] || rect[3] > other[dims+3]);
}

/*
// FN_SEARCH is a template macro that will generate a custom search function
// for a specific dimensions.
#define FN_SEARCH(fn_search, fn_inter) \
static bool \
fn_search(struct rtree *rtree, struct node *node, double *rect, \
       bool (*iter)(const double *rect, const void *item, void *udata), \
       void *udata) \
{ \
    int dims = rtree->dims; \
    double *crect = node->rect; \
    if (node->leaf) { \
        for (int i = 0; i < node->count; i++) { \
            if (fn_inter(rect, crect, dims)) { \
                void *citem = item_at(rtree, node, i); \
                if (!iter(crect, citem, udata)) { \
                    return false; \
                } \
            } \
            crect += dims*2; \
        } \
    } else { \
        for (int i = 0; i < node->count; i++) { \
            if (fn_inter(rect, crect, dims)) { \
                struct node *cnode = *node_at(rtree, node, i); \
                if (!fn_search(rtree, cnode, rect, iter, udata)) { \
                    return false; \
                } \
            } \
            crect += dims*2; \
        } \
    } \
    return true; \
}

FN_SEARCH(search_d, inter_d);
FN_SEARCH(search_2, inter_2);
FN_SEARCH(search_3, inter_3);
FN_SEARCH(search_4, inter_4);
*/

typedef bool (*Pfn_inter)(double* rect, double* other, int dims);

static bool fn_search(struct rtree *rtree, struct node *node, double *rect,
       bool (*iter)(const double *rect, const void *item, void *udata),
       void *udata, Pfn_inter fn_inter)
{
    int dims = rtree->dims;
    double *crect = node->rect;
    if (node->leaf) {
        for (int i = 0; i < node->count; i++) {
            if (fn_inter(rect, crect, dims)) {
                void *citem = item_at(rtree, node, i);
                if (!iter(crect, citem, udata)) {
                    return false;
                }
            }
            crect += dims*2;
        }
    } else {
        for (int i = 0; i < node->count; i++) {
            if (fn_inter(rect, crect, dims)) {
                struct node *cnode = *node_at(rtree, node, i);
                if (!fn_search(rtree, cnode, rect, iter, udata, fn_inter)) {
                    return false;
                }
            }
            crect += dims*2;
        }
    }
    return true;
}
// search_reinsert searches the reinsert list.
static bool search_reinsert(struct rtree *rtree, double *rect, 
                            bool (*iter)(const double *rect, const void *item, 
                                         void *udata), 
                            void *udata)
{
    struct node *node = rtree->reinsert;
    while (node) {
        for (int i = 0; i < node->count; i++) {
            double *crect = rect_at(rtree, node, i);
            if (inter_d(rect, crect, rtree->dims)) {
                void *citem = item_at(rtree, node, i);
                if (!iter(crect, citem, udata)) {
                    return false;
                }
            }
        }
        node = node->next;
    }
    return true;
}

// push_reinsert add the leaves belonging to node to the rtree reinsert list
static void push_reinsert(struct rtree *rtree, struct node *node) {
    if (node->leaf) {
        node->next = rtree->reinsert;
        rtree->reinsert = node;
        rtree->reinsert_count += node->count;
        rtree->count -= node->count;
    } else {
        for (int i = 0; i < node->count; i++) {
             push_reinsert(rtree, *node_at(rtree, node, i));
        }
        takeaway(rtree, node);
    }
}

bool rtree_search(struct rtree *rtree, double *rect, 
                  bool (*iter)(const double *rect, const void *item, 
                               void *udata), 
                  void *udata)
{
    if (rtree->reinsert) {
        if (!search_reinsert(rtree, rect, iter, udata)) {
            return false;
        }
    }
    if (rtree->root) {
        switch (rtree->dims) {
        case 2:  return fn_search(rtree, rtree->root, rect, iter, udata, inter_2);
        case 3:  return fn_search(rtree, rtree->root, rect, iter, udata, inter_3);
        case 4:  return fn_search(rtree, rtree->root, rect, iter, udata, inter_4);
        default: return fn_search(rtree, rtree->root, rect, iter, udata, inter_d);
        }
    }
    return true;
}
/*
#define FN_NODE_DELETE(fn_node_delete, fn_inter) \
static bool \
fn_node_delete(struct rtree *rtree, struct node *node, double *rect, \
               void *item) \
{\
    int dims = rtree->dims;\
    double *crect = node->rect;\
    if (node->leaf) {\
        for (int i = 0; i < node->count; i++) {\
            if (fn_inter(rect, crect, dims)) {\
                void *citem = item_at(rtree, node, i);\
                if (memcmp(citem, item, rtree->elsize) == 0) {\
                    // found the target item, delete it now \
                    rect_copy(rtree, crect, \
                            rect_at(rtree, node, node->count-1));\
                    item_copy(rtree, citem, \
                            item_at(rtree, node, node->count-1));\
                    node->count--;\
                    return true;\
                }\
            }\
            crect += dims*2;\
        }        \
    } else {\
        for (int i = 0; i < node->count; i++) {\
            if (fn_inter(rect, crect, dims)) {\
                struct node *cnode = *node_at(rtree, node, i);\
                if (fn_node_delete(rtree, cnode, rect, item)) {\
                    if (rtree->use_reinsert &&\
                        cnode->count < rtree->min_items) \
                    {\
                        // underflow  \
                        push_reinsert(rtree, cnode);\
                        node_rect_data_copy(rtree, node, i, node, \
                                            node->count-1);\
                        node->count--;\
                    } else if (!rtree->use_reinsert && cnode->count == 0) {\
                        takeaway(rtree, cnode);\
                        node_rect_data_copy(rtree, node, i, node, \
                                            node->count-1);\
                        node->count--;\
                    } else {\
                        rect_calc(rtree, cnode, crect);\
                    } \
                    return true; \
                } \
            } \
            crect += dims*2; \
        } \
    } \
    return false; \
}

FN_NODE_DELETE(node_delete_d, inter_d);
FN_NODE_DELETE(node_delete_2, inter_2);
FN_NODE_DELETE(node_delete_3, inter_3);
FN_NODE_DELETE(node_delete_4, inter_4);
*/
typedef bool (*Pfn_inter)(double* rect, double* other, int dims);

static bool fn_node_delete(struct rtree *rtree, struct node *node, double *rect, void *item, Pfn_inter fn_inter)
{
    int dims = rtree->dims;
    double *crect = node->rect;
    if (node->leaf) {
        for (int i = 0; i < node->count; i++) {
            if (fn_inter(rect, crect, dims)) {
                void *citem = item_at(rtree, node, i);
                if (memcmp(citem, item, rtree->elsize) == 0) {
                    // found the target item, delete it now
                    rect_copy(rtree, crect,
                            rect_at(rtree, node, node->count-1));
                    item_copy(rtree, citem,
                            item_at(rtree, node, node->count-1));
                    node->count--;
                    return true;
                }
            }
            crect += dims*2;
        }        
    } else {
        for (int i = 0; i < node->count; i++) {
            if (fn_inter(rect, crect, dims)) {
                struct node *cnode = *node_at(rtree, node, i);
                if (fn_node_delete(rtree, cnode, rect, item, fn_inter)) {
                    if (rtree->use_reinsert &&
                        cnode->count < rtree->min_items) 
                    {
                        // underflow
                        push_reinsert(rtree, cnode);
                        node_rect_data_copy(rtree, node, i, node,
                                            node->count-1);
                        node->count--;
                    } else if (!rtree->use_reinsert && cnode->count == 0) {
                        takeaway(rtree, cnode);
                        node_rect_data_copy(rtree, node, i, node,
                                            node->count-1);
                        node->count--;
                    } else {
                        rect_calc(rtree, cnode, crect);
                    }
                    return true;
                }
            }
            crect += dims*2;
        }
    }
    return false;
}

// delete_from_reinsert searches the reinsert list for the target item.
// Returns true if the item was found and deleted.
static bool delete_from_reinsert(struct rtree *rtree, double *rect, 
                                 void *item) 
{
    struct node *node = rtree->reinsert;
    while (node) {
        for (int i = 0; i < node->count; i++) {
            double *crect = rect_at(rtree, node, i);
            if (inter_d(rect, crect, rtree->dims)) { 
                void *citem = item_at(rtree, node, i);
                if (memcmp(item, citem, rtree->elsize) == 0) {
                    node_rect_data_copy(rtree, node, i, node, node->count-1);
                    rtree->reinsert_count--;
                    node->count--;
                    return true;
                }
            }
        }
        node = node->next;    
    }
    return false;
}

// rtree_delete deletes an item from the rtree. 
// Returns true if the item was deleted or false if the item was not found.
bool rtree_delete(struct rtree *rtree, double *rect, void *item) {
    // search the reinsert list
    if (rtree->reinsert) {
        bool deleted = delete_from_reinsert(rtree, rect, item);
        attempt_reinsert(rtree);
        if (deleted) {
            return true;
        }
    }
    // search the tree
    if (!rtree->root) {
        return false;
    }
    bool deleted;
    switch (rtree->dims) {
    case 2:  deleted = fn_node_delete(rtree, rtree->root, rect, item, inter_2); break;
    case 3:  deleted = fn_node_delete(rtree, rtree->root, rect, item, inter_3); break;
    case 4:  deleted = fn_node_delete(rtree, rtree->root, rect, item, inter_4); break;
    default: deleted = fn_node_delete(rtree, rtree->root, rect, item, inter_d);
    }
    if (!deleted) {
        return false;
    }
    rtree->count--;
    if (rtree->count == 0) {
        // tree is empty, remove the root
        takeaway(rtree, rtree->root);
        rtree->root = NULL;
        rtree->height = 0;
    } else {
        if (!rtree->root->leaf && rtree->root->count == 1) {
            // drop the height
            struct node *new_root = *node_at(rtree, rtree->root, 0);
            takeaway(rtree, rtree->root);
            rtree->root = new_root;
            rtree->height--;
        }
        rect_calc(rtree, rtree->root, rtree->rect);
    }
    // attempt to reinsert items from deleted leaves. 
    if (rtree->reinsert) {
        attempt_reinsert(rtree);
    }
    return true;
}

//==============================================================================
// TESTS AND BENCHMARKS
// $ cc -DRTREE_TEST rtree.c && ./a.out              # run tests
// $ cc -DRTREE_TEST -O3 rtree.c && BENCH=1 ./a.out  # run benchmarks
//==============================================================================
#ifdef RTREE_TEST

#ifndef _WIN32
#pragma GCC diagnostic ignored "-Wextra"
#endif // _WIN32

#ifdef CITIES
#include "cities.xh"
#endif
#include <assert.h>
#include <time.h>

static const double svg_scale = 20.0;
static const char *strokes[] = { "black", "red", "green", "purple" };
static const int nstrokes = 4;

static void node_write_svg(struct rtree *rtree, struct node *node, double *rect,
                           FILE *f, int height, int depth) 
{
    double *min = rect;
    double *max = &rect[rtree->dims];
    bool point = min[0] == max[0] && min[1] == max[1];
    if (node) {
        if (!node->leaf) {
            for (int i = 0; i < node->count; i++) {
                node_write_svg(rtree, *node_at(rtree, node, i), 
                               rect_at(rtree, node, i), f, height-1, depth+1);
            }
        } else {
            for (int i = 0; i < node->count; i++) {
                node_write_svg(rtree, NULL, 
                            rect_at(rtree, node, i), f, height-1, depth+1);
            }
        }
    }
    if (point) {
        double w = (max[0]-min[0]+1/svg_scale)*svg_scale*10;
        fprintf(f, 
            "<rect x=\"%f\" y=\"%f\" width=\"%f\" height=\"%f\" "
                "fill=\"%s\" fill-opacity=\"1\" "
                "rx=\"3\" ry=\"3\"/>\n",
            (min[0])*svg_scale-w/2, 
            (min[1])*svg_scale-w/2,
            w, w, 
            strokes[depth%nstrokes]);
    } else {
        fprintf(f, 
            "<rect x=\"%f\" y=\"%f\" width=\"%f\" height=\"%f\" "
                "stroke=\"%s\" fill=\"%s\" "
                "stroke-width=\"%d\" "
                "fill-opacity=\"0\" stroke-opacity=\"1\"/>\n",
            (min[0])*svg_scale,
            (min[1])*svg_scale,
            (max[0]-min[0]+1/svg_scale)*svg_scale,
            (max[1]-min[1]+1/svg_scale)*svg_scale,
            strokes[depth%nstrokes],
            strokes[depth%nstrokes],
            height+1);
    }
}

// rtree_write_svg draws the R-tree to an SVG file. This is only useful on
// small geospatial dataset.
void rtree_write_svg(struct rtree *rtree, const char *path) {
    FILE *f = fopen(path, "wb+");
    fprintf(f, "<svg viewBox=\"%.0f %.0f %.0f %.0f\" " 
        "xmlns =\"http://www.w3.org/2000/svg\">\n",
        -190.0*svg_scale, -100.0*svg_scale,
        380.0*svg_scale, 190.0*svg_scale);
    fprintf(f, "<g transform=\"scale(1,-1)\">\n");

    if (rtree->root && rtree->dims > 1) {
        node_write_svg(rtree, rtree->root, rtree->rect, f, rtree->height, 0);
    }
    fprintf(f, "</g>\n");
    fprintf(f, "</svg>\n");
    fclose(f);
}


static void node_deep_check(struct rtree *rtree, struct node *node, 
                            double *rect, int height, int depth)
{
    assert(node->count);
    assert(height);
    double* rect2 = malloc(8*rtree->dims*2); // VLA
    for (int i = 0; i < node->count; i++) {
        double *child_rect = rect_at(rtree, node, i);
        assert((node->leaf && height == 1) || (!node->leaf && height > 1));
        // for (int j = 0; j < depth; j++) { printf("  "); }
        // printf("%d: ", i); print_rect(rtree, child_rect); printf("\n");
        if (!node->leaf) {
            struct node *child = *node_at(rtree, node, i);
            node_deep_check(rtree, child, child_rect, height-1, depth + 1);
        }
        if (i == 0) {
            rect_copy(rtree, rect2, child_rect);
        } else {
            rect_expand(rect2, child_rect, rtree->dims);
        }
    }
    assert(memcmp(rect, rect2, 8*rtree->dims*2) == 0);
    free(rect2);
}

static void rtree_deep_check(struct rtree *rtree) {
    // printf(">>>> DEEP CHECK <<<<\n");
    if (rtree->root) {
        // print_rect(rtree, rtree->rect); printf(" ((root)) \n");
        node_deep_check(rtree, rtree->root, rtree->rect, rtree->height, 0);
    } else {
        assert(rtree->height == 0);
        assert(rtree->count == 0);
    }
}


void print_rect(struct rtree *rtree, double *rect) {
    printf("[[");
    for (int i = 0; i < rtree->dims; i++) {
        if (i > 0) {
            printf(",");
        }
        printf("%f",rect[i]);
    }
    printf("],[");
    for (int i = 0; i < rtree->dims; i++) {
        if (i > 0) {
            printf(",");
        }
        printf("%f",rect[rtree->dims+i]);
    }
    printf("]]");
}

static bool rand_alloc_fail = false;
static int rand_alloc_fail_odds = 3; // 1 in 3 chance malloc will fail.
static uintptr_t total_allocs = 0;
static uintptr_t total_mem = 0;

static void *xmalloc(size_t size) {
    if (rand_alloc_fail && rand()%rand_alloc_fail_odds == 0) {
        return NULL;
    }
    void *mem = malloc(sizeof(uintptr_t)+size);
    assert(mem);
    *(uintptr_t*)mem = size;
    total_allocs++;
    total_mem += size;
    return (char*)mem+sizeof(uintptr_t);
}

static void xfree(void *ptr) {
    if (ptr) {
        total_mem -= *(uintptr_t*)((char*)ptr-sizeof(uintptr_t));
        free((char*)ptr-sizeof(uintptr_t));
        total_allocs--;
    }
}

static void shuffle(void *array, int numels, int elsize) {
    char* tmp = malloc(elsize);
    char *arr = array;
    for (int i = 0; i < numels - 1; i++) {
        int j = i + rand() / (RAND_MAX / (numels - i) + 1);
        memcpy(tmp, arr + j * elsize, elsize);
        memcpy(arr + j * elsize, arr + i * elsize, elsize);
        memcpy(arr + i * elsize, tmp, elsize);
    }
    free(tmp);
}

static void copy_rand_point(int dims, double *point) {
    for (int i = 0; i < dims; i++) {
        if (i == 0) {
            point[i] = ((double)rand()/(double)RAND_MAX) * 360.0 - 180.0;
        } else if (i == 1) {
            point[i] = ((double)rand()/(double)RAND_MAX) * 180.0 - 90.0;
        } else {
            point[i] = ((double)rand()/(double)RAND_MAX) * 100;
        }
        point[dims+i] = point[i];
    }
}

static bool search_iter(const double *rect, const void *item, void *udata) {
    (*(int*)udata)++;
    return true;
}

struct single_ctx {
    int dims;
    double *rect;
    int index;
    bool found;
    struct rtree *rtree;
};

static bool single_iter(const double *rect, const void *item, void *udata) {
    struct single_ctx *ctx = udata;
    if (memcmp(rect, ctx->rect, 8*ctx->dims*2) == 0 && 
        *(int*)item == ctx->index) 
    {
        ctx->found = true;
    }
    return true;
}


static bool tsearch(struct rtree *rtree, int dims, double *rect, int index) {
    struct single_ctx ctx = {
        .dims = dims,
        .rect = rect,
        .index = index,
        .rtree = rtree,
    };
    rtree_search(rtree, rect, single_iter, &ctx);
    return ctx.found;
}

static void test(int N, int dims) {
    double *rects = 0;
    bool use_cities = false;
#ifdef CITIES
    if (N ==0 && dims == 0) {
        use_cities = true;
        N = sizeof(cities)/sizeof(int[2]);
        dims = 2;
        while(!(rects = xmalloc(8*dims*2*N))){}
        assert(rects);
        for (int i = 0; i < N; i++) {
            double *point = &rects[dims*2*i];
            point[0] = (double)cities[i*2+0] / 1000.0 - 180.0;
            point[1] = (double)cities[i*2+1] / 1000.0 - 90.0;
            point[2] = (double)cities[i*2+0] / 1000.0 - 180.0;
            point[3] = (double)cities[i*2+1] / 1000.0 - 90.0;
        }
    }
#endif
    if (!use_cities) {
        while(!(rects = xmalloc(8*dims*2*N))){}
        assert(rects);
        for (int i = 0; i < N; i++) {
            double *rect = &rects[dims*2*i];
            for (int j = 0; j < dims; j++) {
                rect[j] = (double)rand()/RAND_MAX * 100;
                rect[dims+j] = rect[j] + (double)rand()/RAND_MAX;
            }
        }
    }
    int *vals;
    while(!(vals = xmalloc(sizeof(int)*N))){}
    for (int i = 0; i < N; i++) {
        vals[i] = i;
    }  

    struct rtree *rtree;
    while(!(rtree = rtree_new(sizeof(int), dims))){}
    rtree_deep_check(rtree);

    shuffle(vals, N, sizeof(int));
    for (int i = 0; i < N; i++) {
        int index = vals[i];
        double *rect = &rects[dims*2*index];
        assert(!rtree_delete(rtree, rect, &index));
        assert(!tsearch(rtree, dims, rect, index));
        while (!rtree_insert(rtree, rect, &index)){}
        assert(rtree_count(rtree) == i+1);
        assert(tsearch(rtree, dims, rect, index));
        assert(rtree_delete(rtree, rect, &index));
        assert(rtree_count(rtree) == i);
        assert(!tsearch(rtree, dims, rect, index));
        assert(!rtree_delete(rtree, rect, &index));
        while(!rtree_insert(rtree, rect, &index)){}
        assert(rtree_count(rtree) == i+1);
        assert(tsearch(rtree, dims, rect, index));
        rtree_deep_check(rtree);
    }
    rtree_deep_check(rtree);
    
    if (use_cities) {
        rtree_write_svg(rtree, "cities.svg");
    } else if (dims == 2) {
        rtree_write_svg(rtree, "dim2.svg");
    }


    shuffle(vals, N, sizeof(int));
    double del = 0.50;
    for (int i = 0; i < N*del; i++) {
        int index = vals[i];
        double *rect = &rects[dims*2*index];
        assert(rtree_delete(rtree, rect, &index));
    }
    if (use_cities) {
        rtree_write_svg(rtree, "cities-2.svg");
    } else if (dims == 2) {
        rtree_write_svg(rtree, "dim2-2.svg");
    }

    for (int i = 0; i < N*del; i++) {
        int index = vals[i];
        double *rect = &rects[dims*2*index];
        while (!rtree_insert(rtree, rect, &index)){}
    }

    if (use_cities) {
        rtree_write_svg(rtree, "cities-3.svg");
    } else if (dims == 2) {
        rtree_write_svg(rtree, "dim2-3.svg");
    }

    shuffle(vals, N, sizeof(int));
    for (int i = 0; i < N; i++) {
        int index = vals[i];
        double *rect = &rects[dims*2*index];
        assert(tsearch(rtree, dims, rect, index));
    }


    shuffle(vals, N, sizeof(int));
    for (int i = 0; i < N; i++) {
        int index = vals[i];
        double *rect = &rects[dims*2*index];
        assert(tsearch(rtree, dims, rect, index));
        assert(rtree_count(rtree) == N-i);
        rtree_delete(rtree, rect, &index);
        assert(!tsearch(rtree, dims, rect, index));
        assert(rtree_count(rtree) == N-i-1);
        rtree_deep_check(rtree);
    }
    rtree_deep_check(rtree);




    rtree_free(rtree);
    xfree(vals);
    xfree(rects);
    if (total_allocs != 0) {
        fprintf(stderr, "total_allocs: expected 0, got %llu\n", total_allocs);
        exit(1);
    }
}

static void all() {
    unsigned int seed = (unsigned int)(getenv("SEED")?atoi(getenv("SEED")):time(NULL));
    srand(seed);
    printf("seed=%d\n", seed);

    int counts[] = { 0, 1, 2, 3, 4, 5, 8, 16, 32, 64, 128, 256, 512, 1024 };

    rand_alloc_fail = true;

    for (int dims = 1; dims <= 8; dims++) {
        printf("(%d) => ", dims);
        for (int i = 0; i < sizeof(counts)/sizeof(int); i++) {
            printf("%d ", counts[i]);
            fflush(stdout);
            test(counts[i], dims);
        }
        printf("\n");
    }
#ifdef CITIES
    printf("testings cities...\n");
    test(0, 0);
#endif
    rand_alloc_fail = false;
}

#define bench(name, N, code) {{ \
    if (strlen(name) > 0) { \
        printf("%-14s ", name); \
    } \
    size_t tmem = total_mem; \
    size_t tallocs = total_allocs; \
    unsigned long long bytes = 0; \
    clock_t begin = clock(); \
    for (int i = 0; i < N; i++) { \
        code; \
    } \
    clock_t end = clock(); \
    double elapsed_secs = (double)(end - begin) / CLOCKS_PER_SEC; \
    double bytes_sec = (double)bytes/elapsed_secs; \
    printf("%d ops in %.3f secs, %.0f ns/op, %.0f op/sec", \
        N, elapsed_secs, \
        elapsed_secs/(double)N*1e9, \
        (double)N/elapsed_secs \
    ); \
    if (bytes > 0) { \
        printf(", %.1f GB/sec", bytes_sec/1024/1024/1024); \
    } \
    if (total_mem > tmem) { \
        size_t used_mem = total_mem-tmem; \
        printf(", %.2f bytes/op", (double)used_mem/N); \
    } \
    if (total_allocs > tallocs) { \
        size_t used_allocs = total_allocs-tallocs; \
        printf(", %.2f allocs/op", (double)used_allocs/N); \
    } \
    printf("\n"); \
}}


static void benchmarks() {
    int seed = (int)(getenv("SEED")?atoi(getenv("SEED")):time(NULL));
    int N = getenv("N")?atoi(getenv("N")):1000000;
    int dims = 2;
    printf("seed=%d count=%d\n", seed, N);
    srand(seed);

    double *coords = xmalloc(8*dims*2*N);
    for (int i = 0; i < N; i++) {
        double *point = &coords[dims*2*i];
        copy_rand_point(dims, point);
    }

    struct rtree *rtree = rtree_new(sizeof(int), dims);

    bench("insert", N, {
        double *point = &coords[dims*2*i];
        rtree_insert(rtree, point, &(int){i});
    });

    rtree_write_svg(rtree, "out.svg");

    
    bench("search", N, {
        double *point = &coords[dims*2*i];
        int res = 0;
        rtree_search(rtree, point, search_iter, &res);
        //assert(res == 1);
    });

    bench("replace", N, {
        double *point = &coords[dims*2*i];
        rtree_delete(rtree, point, &(int){i});
        rtree_insert(rtree, point, &(int){i});
    });


    bench("delete", N, {
        double *point = &coords[dims*2*i];
        rtree_delete(rtree, point, &(int){i});
    });

    rtree_free(rtree);

    xfree(coords);
    if (total_allocs != 0) {
        fprintf(stderr, "total_allocs: expected 0, got %llu\n", total_allocs);
        exit(1);
    }

}

int main() {
    rtree_set_allocator(xmalloc, xfree);
    if (getenv("BENCH")) {
        printf("Running rtree.c benchmarks...\n");
        benchmarks();
    } else {
        printf("Running rtree.c tests...\n");
        all();
        printf("PASSED\n");
    }
}
#endif
