/*********************************************************************
 * Generic Skip List - 支持重复键（插入到链表）
 * - CamelCase
 * - void* key/value + compare function
 * - 重复 key 按插入顺序排在同一层
 *********************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <stdint.h>

 /* ==================== Linux Kernel List API ==================== */
struct ListHead {
    struct ListHead* next, * prev;
};

#define INIT_LIST_HEAD(ptr) do { \
    (ptr)->next = (ptr); (ptr)->prev = (ptr); \
} while (0)

static inline void listAdd(struct ListHead* newEntry, struct ListHead* head)
{
    head->next->prev = newEntry;
    newEntry->next = head->next;
    newEntry->prev = head;
    head->next = newEntry;
}

static inline void listDel(struct ListHead* entry)
{
    entry->next->prev = entry->prev;
    entry->prev->next = entry->next;
    entry->next = entry->prev = NULL;
}

#define listEntry(ptr, type, member) \
    ((type *)((char *)(ptr) - (uintptr_t)(&((type *)0)->member)))

#define listForEach(pos, head) \
    for (pos = (head)->next; pos != (head); pos = pos->next)

#define listForEachSafe(pos, n, head) \
    for (pos = (head)->next, n = pos->next; pos != (head); \
         pos = n, n = pos->next)

/* ==================== Skip List Config ==================== */
#define MAX_LEVEL 16
#define SENTINEL_KEY ((void*)0x1)

typedef int (*CompareFn)(const void* a, const void* b);

/* ==================== Data Structures ==================== */
typedef struct SkipNode {
    void* key;
    void* value;
    int          level;
    struct ListHead forward[0];
} SkipNode;

typedef struct SkipList {
    int          level;
    SkipNode* header;
    CompareFn    compare;
} SkipList;

/* ==================== Helpers ==================== */
static int randomLevel(void)
{
    int lvl = 1;
    while ((rand() & 1) && lvl < MAX_LEVEL) lvl++;
    return lvl;
}

static SkipNode* createNode(void* key, void* value, int level)
{
    SkipNode* node = malloc(sizeof(SkipNode) + level * sizeof(struct ListHead));
    if (!node) return NULL;
    node->key = key;
    node->value = value;
    node->level = level;
    for (int i = 0; i < level; i++) {
        INIT_LIST_HEAD(&node->forward[i]);
    }
    return node;
}

/* ==================== API ==================== */
SkipList* skipListCreate(CompareFn cmp)
{
    SkipList* sl = malloc(sizeof(SkipList));
    if (!sl) return NULL;
    sl->level = 1;
    sl->compare = cmp;
    sl->header = createNode(SENTINEL_KEY, NULL, MAX_LEVEL);
    if (!sl->header) { free(sl); return NULL; }
    for (int i = 0; i < MAX_LEVEL; i++) {
        INIT_LIST_HEAD(&sl->header->forward[i]);
    }
    return sl;
}

void skipListDestroy(SkipList* sl)
{
    if (!sl) return;
    struct ListHead* pos, * n;
    listForEachSafe(pos, n, &sl->header->forward[0]) {
        SkipNode* node = listEntry(pos, SkipNode, forward[0]);
        listDel(pos);
        free(node->key);
        free(node->value);
        free(node);
    }
    free(sl->header);
    free(sl);
}

/* 查找：返回第一个匹配 key 的节点 */
SkipNode* skipListSearch(SkipList* sl, const void* key)
{
    SkipNode* x = sl->header;
    for (int i = sl->level - 1; i >= 0; i--) {
        while (x->forward[i].next != &sl->header->forward[i]) {
            SkipNode* next = listEntry(x->forward[i].next, SkipNode, forward[i]);
            if (sl->compare(next->key, key) > 0) break;
            if (sl->compare(next->key, key) == 0) {
                // 找到第一个等于 key 的节点
                x = next;
                break;
            }
            x = next;
        }
    }
    if (x->forward[0].next != &sl->header->forward[0]) {
        SkipNode* next = listEntry(x->forward[0].next, SkipNode, forward[0]);
        if (sl->compare(next->key, key) == 0) return next;
    }
    return NULL;
}

/* 插入：重复 key 也插入（按顺序排在后面） */
void skipListInsert(SkipList* sl, void* key, void* value)
{
    SkipNode* update[MAX_LEVEL];
    SkipNode* x = sl->header;
    memset(update, 0, sizeof(update));

    for (int i = sl->level - 1; i >= 0; i--) {
        while (x->forward[i].next != &sl->header->forward[i]) {
            SkipNode* next = listEntry(x->forward[i].next, SkipNode, forward[i]);
            if (sl->compare(next->key, key) > 0) break;
            x = next;
        }
        update[i] = x;
    }

    int lvl = randomLevel();
    if (lvl > sl->level) {
        for (int i = sl->level; i < lvl; i++) {
            update[i] = sl->header;
        }
        sl->level = lvl;
    }

    SkipNode* newNode = createNode(key, value, lvl);
    if (!newNode) { free(key); free(value); return; }

    for (int i = 0; i < lvl; i++) {
        listAdd(&newNode->forward[i], &update[i]->forward[i]);
    }
}

/* 删除：删除第一个匹配 key 的节点 */
void skipListDelete(SkipList* sl, const void* key)
{
    SkipNode* update[MAX_LEVEL];
    SkipNode* x = sl->header;
    memset(update, 0, sizeof(update));

    for (int i = sl->level - 1; i >= 0; i--) {
        while (x->forward[i].next != &sl->header->forward[i]) {
            SkipNode* next = listEntry(x->forward[i].next, SkipNode, forward[i]);
            if (sl->compare(next->key, key) > 0) break;
            if (sl->compare(next->key, key) == 0) {
                x = next;
                break;
            }
            x = next;
        }
        update[i] = x;
    }

    if (x->forward[0].next == &sl->header->forward[0]) return;
    SkipNode* target = listEntry(x->forward[0].next, SkipNode, forward[0]);
    if (sl->compare(target->key, key) != 0) return;

    int nodeLevel = target->level;
    for (int i = 0; i < nodeLevel; i++) {
        if (update[i]->forward[i].next == &target->forward[i]) {
            listDel(&target->forward[i]);
        }
    }

    free(target->key);
    free(target->value);
    free(target);

    while (sl->level > 1 &&
        sl->header->forward[sl->level - 1].next == &sl->header->forward[sl->level - 1]) {
        sl->level--;
    }
}

/* ==================== 新增 API ==================== */

/* 查找所有 key 相同的节点，返回底层链表的第一个 list_head，count 返回数量 */
struct ListHead* skipListFindAll(SkipList* sl, const void* key, int* count)
{
    *count = 0;
    SkipNode* x = sl->header;

    /* 1. 快速定位到第一个 >= key 的位置 */
    for (int i = sl->level - 1; i >= 0; i--) {
        while (x->forward[i].next != &sl->header->forward[i]) {
            SkipNode* next = listEntry(x->forward[i].next, SkipNode, forward[i]);
            if (sl->compare(next->key, key) > 0) break;
            x = next;
        }
    }

    /* 2. 在底层从 x->forward[0] 开始，找第一个 == key 的节点 */
    struct ListHead* start = x->forward[0].next;
    if (start == &sl->header->forward[0]) return NULL;

    SkipNode* first = listEntry(start, SkipNode, forward[0]);
    if (sl->compare(first->key, key) != 0) return NULL;

    /* 3. 统计连续 == key 的节点数量 */
    struct ListHead* pos = start;
    while (pos != &sl->header->forward[0]) {
        SkipNode* node = listEntry(pos, SkipNode, forward[0]);
        if (sl->compare(node->key, key) != 0) break;
        (*count)++;
        pos = pos->next;
    }

    return start;  // 返回第一个匹配节点的 list_head
}

/* 删除所有 key 相同的节点 */
void skipListDeleteAll(SkipList* sl, const void* key)
{
    int count = 0;
    struct ListHead* start = skipListFindAll(sl, key, &count);
    if (!start || count == 0) return;

    /* 1. 收集所有要删除的节点（因为删除时不能边删边遍历） */
    SkipNode* toDelete[MAX_LEVEL * 32] = { 0 };  // 最多删除这么多
    int delCount = 0;

    struct ListHead* pos = start;
    for (int i = 0; i < count; i++) {
        SkipNode* node = listEntry(pos, SkipNode, forward[0]);
        toDelete[delCount++] = node;
        pos = pos->next;
    }

    /* 2. 对每个要删除的节点，逐层删除 */
    for (int i = 0; i < delCount; i++) {
        SkipNode* target = toDelete[i];
        int nodeLevel = target->level;

        /* 重新构建 update 数组（因为节点可能跨层） */
        SkipNode* update[MAX_LEVEL] = { 0 };
        SkipNode* x = sl->header;
        for (int j = sl->level - 1; j >= 0; j--) {
            while (x->forward[j].next != &sl->header->forward[j]) {
                SkipNode* next = listEntry(x->forward[j].next, SkipNode, forward[j]);
                if (next == target) break;
                if (sl->compare(next->key, key) > 0) break;
                x = next;
            }
            update[j] = x;
        }

        for (int j = 0; j < nodeLevel; j++) {
            if (update[j]->forward[j].next == &target->forward[j]) {
                listDel(&target->forward[j]);
            }
        }

        free(target->key);
        free(target->value);
        free(target);
    }

    /* 3. 缩减空层 */
    while (sl->level > 1 &&
        sl->header->forward[sl->level - 1].next == &sl->header->forward[sl->level - 1]) {
        sl->level--;
    }
}
void skipListPrint(SkipList* sl, void (*printKey)(const void*))
{
    for (int i = sl->level - 1; i >= 0; i--) {
        printf("Level %2d: ", i);
        struct ListHead* pos;
        listForEach(pos, &sl->header->forward[i]) {
            SkipNode* node = listEntry(pos, SkipNode, forward[i]);
            if (node->key != SENTINEL_KEY) {
                printKey(node->key);
                printf(" ");
            }
        }
        printf("\n");
    }
    printf("Max level: %d\n\n", sl->level);
}

/* ==================== Test: Integer with Duplicates ==================== */
static int intCmp(const void* a, const void* b)
{
    return *(int*)a - *(int*)b;
}

static void printInt(const void* p)
{
    printf("%d", *(int*)p);
}

int main2(void)
{
    srand(time(NULL));
    SkipList* sl = skipListCreate(intCmp);

    // 插入重复 key
    int keys[] = { 3, 1, 4, 1, 5, 9, 2, 6, 5, 3, 5 };
    for (int i = 0; i < 11; i++) {
        int* k = malloc(sizeof(int)), * v = malloc(sizeof(int));
        *k = keys[i]; *v = keys[i] * 100 + i;  // value 带序号
        skipListInsert(sl, k, v);
    }

    printf("=== After Insert (Duplicates Allowed) ===\n");
    skipListPrint(sl, printInt);

    int searchKey = 5;
    SkipNode* found = skipListSearch(sl, &searchKey);
    printf("First 5: value = %d\n", found ? *(int*)found->value : -1);

    skipListDelete(sl, &searchKey);
    printf("=== After Delete First 5 ===\n");
    skipListPrint(sl, printInt);

    skipListDestroy(sl);
    return 0;
}