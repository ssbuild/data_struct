#if 1

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define ORDER 4
#define MIN_KEYS ((ORDER + 1) / 2 - 1)  // 最小键数要求（ORDER=4 → 1）

typedef struct BPlusNode {
    int keys[ORDER - 1];
    struct BPlusNode* children[ORDER];
    struct BPlusNode* next;         // 只在叶子节点使用
    int num_keys;
    int is_leaf;
} BPlusNode;

typedef struct BPlusTree {
    BPlusNode* root;
} BPlusTree;

// 创建节点
BPlusNode* create_node(int is_leaf) {
    BPlusNode* node = (BPlusNode*)malloc(sizeof(BPlusNode));
    if (!node) return NULL;
    memset(node, 0, sizeof(BPlusNode));
    node->is_leaf = is_leaf;
    node->num_keys = 0;
    node->next = NULL;
    return node;
}

BPlusTree* create_tree() {
    BPlusTree* tree = (BPlusTree*)malloc(sizeof(BPlusTree));
    if (!tree) return NULL;
    tree->root = create_node(1);
    return tree;
}

// 查找叶子节点并记录路径
BPlusNode* find_leaf(BPlusTree* tree, int key, BPlusNode*** path, int* depth) {
    BPlusNode* cur = tree->root;
    *depth = 0;
    *path = (BPlusNode**)malloc(sizeof(BPlusNode*) * 64);

    while (!cur->is_leaf) {
        (*path)[(*depth)++] = cur;
        int i = 0;
        while (i < cur->num_keys && key >= cur->keys[i]) i++;
        cur = cur->children[i];
    }
    return cur;
}

int find_pos(BPlusNode* node, int key) {
    int i = 0;
    while (i < node->num_keys && key >= node->keys[i]) i++;
    return i;
}

// 分裂叶子，返回上升的键（新节点最小键）
int split_leaf(BPlusNode* leaf, BPlusNode** new_leaf) {
    *new_leaf = create_node(1);
    int mid = (ORDER - 1) / 2;  // 1 for ORDER=4

    for (int i = mid; i < ORDER - 1; i++) {
        (*new_leaf)->keys[(*new_leaf)->num_keys++] = leaf->keys[i];
    }

    leaf->num_keys = mid;

    (*new_leaf)->next = leaf->next;
    leaf->next = *new_leaf;

    return (*new_leaf)->keys[0];
}

// 分裂内部节点，返回上升的键
int split_internal(BPlusNode* node, BPlusNode** new_node) {
    *new_node = create_node(0);
    int mid = (ORDER - 1) / 2;

    int up_key = node->keys[mid];

    for (int i = mid + 1; i < ORDER - 1; i++) {
        (*new_node)->keys[(*new_node)->num_keys++] = node->keys[i];
    }

    for (int i = mid + 1; i < ORDER; i++) {
        (*new_node)->children[i - mid - 1] = node->children[i];
        node->children[i] = NULL;
    }

    node->num_keys = mid;

    return up_key;
}

void insert_into_parent(BPlusTree* tree, BPlusNode* parent, int up_key, BPlusNode* right) {
    if (!parent) {
        BPlusNode* new_root = create_node(0);
        new_root->keys[0] = up_key;
        new_root->num_keys = 1;
        new_root->children[0] = tree->root;
        new_root->children[1] = right;
        tree->root = new_root;
        return;
    }

    int pos = find_pos(parent, up_key);

    for (int i = parent->num_keys; i > pos; i--)
        parent->keys[i] = parent->keys[i - 1];
    parent->keys[pos] = up_key;
    parent->num_keys++;

    for (int i = parent->num_keys; i > pos + 1; i--)
        parent->children[i] = parent->children[i - 1];
    parent->children[pos + 1] = right;
}

// 插入
void insert(BPlusTree* tree, int key) {
    if (!tree || !tree->root) return;

    int depth;
    BPlusNode** path;
    BPlusNode* leaf = find_leaf(tree, key, &path, &depth);

    int pos = find_pos(leaf, key);
    for (int i = leaf->num_keys; i > pos; i--)
        leaf->keys[i] = leaf->keys[i - 1];
    leaf->keys[pos] = key;
    leaf->num_keys++;

    BPlusNode* current = leaf;
    int cur_depth = depth;

    while (current->num_keys == ORDER - 1) {
        BPlusNode* new_sibling;
        int up_key;

        if (current->is_leaf)
            up_key = split_leaf(current, &new_sibling);
        else
            up_key = split_internal(current, &new_sibling);

        if (cur_depth == 0) {
            insert_into_parent(tree, NULL, up_key, new_sibling);
            break;
        }

        BPlusNode* parent = path[--cur_depth];
        insert_into_parent(tree, parent, up_key, new_sibling);
        current = parent;
    }

    free(path);
}

// -------------------------- 删除相关辅助函数 --------------------------

// 从叶子删除指定位置的键
void remove_from_leaf(BPlusNode* leaf, int pos) {
    for (int i = pos; i < leaf->num_keys - 1; i++)
        leaf->keys[i] = leaf->keys[i + 1];
    leaf->num_keys--;
}

// 从内部节点删除指定位置的键，并调整孩子指针
void remove_from_internal(BPlusNode* node, int pos) {
    for (int i = pos; i < node->num_keys - 1; i++)
        node->keys[i] = node->keys[i + 1];
    node->num_keys--;

    // 孩子指针向左移动（删除的是第pos个分隔符，右边的孩子要前移）
    for (int i = pos + 1; i <= node->num_keys; i++)
        node->children[i] = node->children[i + 1];
}

// 查找兄弟节点并返回可借键的兄弟（左优先）
BPlusNode* get_sibling(BPlusNode** path, int depth, int* sibling_idx) {
    if (depth <= 0) return NULL;
    BPlusNode* parent = path[depth - 1];
    int child_idx = -1;

    for (int i = 0; i <= parent->num_keys; i++) {
        if (parent->children[i] == path[depth]) {
            child_idx = i;
            break;
        }
    }

    if (child_idx == -1) return NULL;

    // 优先左兄弟
    if (child_idx > 0) {
        *sibling_idx = child_idx - 1;
        return parent->children[child_idx - 1];
    }
    // 否则右兄弟
    if (child_idx < parent->num_keys) {
        *sibling_idx = child_idx + 1;
        return parent->children[child_idx + 1];
    }

    return NULL;
}

// 从兄弟节点借一个键（叶子或内部）
void borrow_from_sibling(BPlusNode* parent, int child_idx, BPlusNode* child, BPlusNode* sibling, int sibling_idx) {
    if (sibling_idx < child_idx) {
        // 左兄弟借 → 右移
        if (child->is_leaf) {
            // 叶子：把左兄弟最大键移到当前节点最前面
            for (int i = child->num_keys; i > 0; i--)
                child->keys[i] = child->keys[i - 1];
            child->keys[0] = sibling->keys[sibling->num_keys - 1];
            child->num_keys++;
            sibling->num_keys--;

            // 更新父节点分隔符（左兄弟最大键）
            parent->keys[child_idx - 1] = child->keys[0];
        }
        else {
            // 内部节点：把左兄弟最大键 + 右孩子移过来
            child->keys[0] = parent->keys[child_idx - 1];
            child->num_keys++;

            for (int i = child->num_keys; i > 1; i--)
                child->children[i] = child->children[i - 1];
            child->children[1] = child->children[0];
            child->children[0] = sibling->children[sibling->num_keys + 1];
            sibling->children[sibling->num_keys + 1] = NULL;

            parent->keys[child_idx - 1] = sibling->keys[sibling->num_keys - 1];
            sibling->num_keys--;
        }
    }
    else {
        // 右兄弟借 → 左移
        if (child->is_leaf) {
            child->keys[child->num_keys] = sibling->keys[0];
            child->num_keys++;
            for (int i = 0; i < sibling->num_keys - 1; i++)
                sibling->keys[i] = sibling->keys[i + 1];
            sibling->num_keys--;

            parent->keys[child_idx] = sibling->keys[0];
        }
        else {
            child->keys[child->num_keys] = parent->keys[child_idx];
            child->num_keys++;
            child->children[child->num_keys] = sibling->children[0];

            parent->keys[child_idx] = sibling->keys[0];

            for (int i = 0; i < sibling->num_keys - 1; i++)
                sibling->keys[i] = sibling->keys[i + 1];
            for (int i = 0; i < sibling->num_keys; i++)
                sibling->children[i] = sibling->children[i + 1];
            sibling->num_keys--;
        }
    }
}

// 合并两个节点（把 right 合并到 left）
void merge_nodes(BPlusTree* tree, BPlusNode* parent, int left_idx, BPlusNode* left, BPlusNode* right) {
    if (left->is_leaf) {
        // 叶子合并
        for (int i = 0; i < right->num_keys; i++)
            left->keys[left->num_keys++] = right->keys[i];

        left->next = right->next;

        // 释放 right
        free(right);
    }
    else {
        // 内部节点合并：中间要带上父节点的下降键
        left->keys[left->num_keys++] = parent->keys[left_idx];

        for (int i = 0; i < right->num_keys; i++)
            left->keys[left->num_keys++] = right->keys[i];

        for (int i = 0; i <= right->num_keys; i++)
            left->children[left->num_keys - i] = right->children[i];  // 注意顺序

        free(right);
    }

    // 从父节点删除分隔符
    remove_from_internal(parent, left_idx);
}

// 删除实现
void delete_key(BPlusTree* tree, int key) {
    if (!tree || !tree->root) return;

    int depth;
    BPlusNode** path;
    BPlusNode* leaf = find_leaf(tree, key, &path, &depth);

    // 找到要删除的位置
    int pos = 0;
    while (pos < leaf->num_keys && leaf->keys[pos] < key) pos++;
    if (pos >= leaf->num_keys || leaf->keys[pos] != key) {
        free(path);
        return;  // 不存在
    }

    remove_from_leaf(leaf, pos);

    BPlusNode* current = leaf;
    int cur_depth = depth;

    // 处理 underflow
    while (current->num_keys < MIN_KEYS && cur_depth >= 0) {
        if (cur_depth == 0) {
            // 根节点 underflow → 如果只有一个孩子，降层
            if (tree->root->num_keys == 0 && !tree->root->is_leaf) {
                BPlusNode* new_root = tree->root->children[0];
                free(tree->root);
                tree->root = new_root;
            }
            break;
        }

        BPlusNode* parent = path[cur_depth - 1];
        int child_idx = -1;
        for (int i = 0; i <= parent->num_keys; i++) {
            if (parent->children[i] == current) {
                child_idx = i;
                break;
            }
        }

        int sibling_idx;
        BPlusNode* sibling = get_sibling(path, cur_depth, &sibling_idx);

        if (sibling && sibling->num_keys > MIN_KEYS) {
            // 可以借用
            borrow_from_sibling(parent, child_idx, current, sibling, sibling_idx);
            break;  // 借完结束
        }
        else {
            // 需要合并
            BPlusNode* left, * right;
            int merge_idx;

            if (sibling_idx < child_idx) {
                left = sibling;
                right = current;
                merge_idx = sibling_idx;
            }
            else {
                left = current;
                right = sibling ? sibling : current;  // 若无兄弟则不处理
                merge_idx = child_idx;
            }

            if (right == current && !sibling) {
                // 特殊情况：只有一个孩子且 underflow（根降层已处理）
                break;
            }

            merge_nodes(tree, parent, merge_idx, left, right);

            current = left;
            cur_depth--;
        }
    }

    free(path);
}

// 打印树
void print_tree(BPlusNode* node, int level) {
    if (!node) return;
    for (int i = 0; i < level; i++) printf("  ");
    printf(node->is_leaf ? "L " : "I ");
    printf("[");
    for (int i = 0; i < node->num_keys; i++) {
        printf("%d", node->keys[i]);
        if (i < node->num_keys - 1) printf(",");
    }
    printf("]\n");

    if (!node->is_leaf) {
        for (int i = 0; i <= node->num_keys; i++)
            print_tree(node->children[i], level + 1);
    }
}

// 测试
int main() {
    BPlusTree* tree = create_tree();

    int vals[] = { 1, 3, 5, 7, 10, 12, 15, 18, 20, 22, 25, 28, 30, 33, 35, 40, 45, 50 };
    for (int v : vals) {
        insert(tree, v);
    }

    printf("初始树:\n");
    print_tree(tree->root, 0);
    printf("\n");

    int to_delete[] = { 12, 20, 5, 30, 1, 40, 18 };
    for (int d : to_delete) {
        printf("删除 %d 后:\n", d);
        delete_key(tree, d);
        print_tree(tree->root, 0);
        printf("\n");
    }

    return 0;
}

#endif

