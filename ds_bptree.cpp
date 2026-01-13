#if 1
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define ORDER 4
#define MIN_KEYS ((ORDER + 1) / 2 - 1)  // ORDER=4 → 1

typedef struct BPlusNode {
    int keys[ORDER - 1];
    struct BPlusNode* children[ORDER];
    struct BPlusNode* next;     // 仅叶子使用
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

// 查找叶子节点，path 只记录非叶子节点
BPlusNode* find_leaf(BPlusTree* tree, int key, BPlusNode*** path, int* depth) {
    BPlusNode* cur = tree->root;
    *depth = 0;
    *path = (BPlusNode**)malloc(sizeof(BPlusNode*) * 64);
    if (!*path) return NULL;

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

// 分裂叶子节点，返回新节点的最小键（用于插入父节点）
int split_leaf(BPlusNode* leaf, BPlusNode** new_leaf) {
    *new_leaf = create_node(1);
    int mid = (ORDER - 1) / 2;
    for (int i = mid; i < ORDER - 1; i++) {
        (*new_leaf)->keys[(*new_leaf)->num_keys++] = leaf->keys[i];
    }
    leaf->num_keys = mid;
    (*new_leaf)->next = leaf->next;
    leaf->next = *new_leaf;
    return (*new_leaf)->keys[0];
}

// 分裂内部节点，返回中间键（上升键）
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

// ──────────────── 删除相关 ────────────────

void remove_from_leaf(BPlusNode* leaf, int pos) {
    for (int i = pos; i < leaf->num_keys - 1; i++)
        leaf->keys[i] = leaf->keys[i + 1];
    leaf->num_keys--;
}

void remove_from_internal(BPlusNode* node, int pos) {
    for (int i = pos; i < node->num_keys - 1; i++)
        node->keys[i] = node->keys[i + 1];
    node->num_keys--;
    for (int i = pos + 1; i <= node->num_keys; i++)
        node->children[i] = node->children[i + 1];
}

// 查找可借用的兄弟（键数 > MIN_KEYS），优先左兄弟
BPlusNode* get_borrowable_sibling(BPlusNode** path, int depth, BPlusNode* current, int* sibling_idx) {
    if (depth <= 0) {
        *sibling_idx = -1;
        return NULL;
    }
    BPlusNode* parent = path[depth - 1];
    int child_idx = -1;
    for (int i = 0; i <= parent->num_keys; i++) {
        if (parent->children[i] == current) {
            child_idx = i;
            break;
        }
    }
    if (child_idx == -1) {
        *sibling_idx = -1;
        return NULL;
    }

    // 左兄弟
    if (child_idx > 0) {
        BPlusNode* sib = parent->children[child_idx - 1];
        if (sib->num_keys > MIN_KEYS) {
            *sibling_idx = child_idx - 1;
            return sib;
        }
    }
    // 右兄弟
    if (child_idx < parent->num_keys) {
        BPlusNode* sib = parent->children[child_idx + 1];
        if (sib->num_keys > MIN_KEYS) {
            *sibling_idx = child_idx + 1;
            return sib;
        }
    }
    *sibling_idx = -1;
    return NULL;
}

// 从兄弟借键
void borrow_from_sibling(BPlusNode* parent, int child_idx, BPlusNode* child, BPlusNode* sibling, int sibling_idx) {
    if (sibling_idx < child_idx) {
        // 左兄弟 → 借最大键
        if (child->is_leaf) {
            for (int i = child->num_keys; i > 0; i--)
                child->keys[i] = child->keys[i - 1];
            child->keys[0] = sibling->keys[sibling->num_keys - 1];
            sibling->num_keys--;
            child->num_keys++;
            parent->keys[child_idx - 1] = child->keys[0];
        }
        else {
            // 内部节点：下降父键到 child 最前，左兄弟最右孩子接过来
            for (int i = child->num_keys; i > 0; i--)
                child->keys[i] = child->keys[i - 1];
            for (int i = child->num_keys + 1; i > 0; i--)
                child->children[i] = child->children[i - 1];

            child->keys[0] = parent->keys[child_idx - 1];
            child->children[0] = sibling->children[sibling->num_keys];
            sibling->children[sibling->num_keys] = NULL;

            parent->keys[child_idx - 1] = sibling->keys[sibling->num_keys - 1];
            sibling->num_keys--;
            child->num_keys++;
        }
    }
    else {
        // 右兄弟 → 借最小键
        if (child->is_leaf) {
            child->keys[child->num_keys++] = sibling->keys[0];
            for (int i = 0; i < sibling->num_keys - 1; i++)
                sibling->keys[i] = sibling->keys[i + 1];
            sibling->num_keys--;
            parent->keys[child_idx] = sibling->keys[0];
        }
        else {
            child->keys[child->num_keys] = parent->keys[child_idx];
            child->children[child->num_keys + 1] = sibling->children[0];

            parent->keys[child_idx] = sibling->keys[0];
            for (int i = 0; i < sibling->num_keys - 1; i++)
                sibling->keys[i] = sibling->keys[i + 1];
            for (int i = 0; i < sibling->num_keys; i++)
                sibling->children[i] = sibling->children[i + 1];
            sibling->children[sibling->num_keys] = NULL;

            sibling->num_keys--;
            child->num_keys++;
        }
    }
}

// 合并 right 到 left
void merge_nodes(BPlusTree* tree, BPlusNode* parent, int merge_idx, BPlusNode* left, BPlusNode* right) {
    if (left->is_leaf) {
        for (int i = 0; i < right->num_keys; i++)
            left->keys[left->num_keys++] = right->keys[i];
        left->next = right->next;
    }
    else {
        int old = left->num_keys;
        left->keys[old] = parent->keys[merge_idx];
        left->num_keys = old + 1 + right->num_keys;

        for (int i = 0; i < right->num_keys; i++)
            left->keys[old + 1 + i] = right->keys[i];

        for (int i = 0; i <= right->num_keys; i++)
            left->children[old + 1 + i] = right->children[i];
    }
    free(right);
    remove_from_internal(parent, merge_idx);
}

// 删除
void delete_key(BPlusTree* tree, int key) {
    if (!tree || !tree->root) return;

    int depth;
    BPlusNode** path;
    BPlusNode* leaf = find_leaf(tree, key, &path, &depth);

    int pos = 0;
    while (pos < leaf->num_keys && leaf->keys[pos] < key) pos++;
    if (pos >= leaf->num_keys || leaf->keys[pos] != key) {
        free(path);
        return;
    }
    remove_from_leaf(leaf, pos);

    BPlusNode* current = leaf;
    int cur_depth = depth;

    while (current->num_keys < MIN_KEYS && cur_depth > 0) {
        BPlusNode* parent = path[cur_depth - 1];
        int child_idx = -1;
        for (int i = 0; i <= parent->num_keys; i++) {
            if (parent->children[i] == current) {
                child_idx = i;
                break;
            }
        }
        if (child_idx == -1) break;

        int sib_idx = -1;
        BPlusNode* sibling = get_borrowable_sibling(path, cur_depth, current, &sib_idx);

        if (sibling) {
            borrow_from_sibling(parent, child_idx, current, sibling, sib_idx);
            break;
        }

        // 合并
        BPlusNode* left_node = NULL, * right_node = NULL;
        int merge_idx = -1;

        if (child_idx > 0) {
            left_node = parent->children[child_idx - 1];
            right_node = current;
            merge_idx = child_idx - 1;
        }
        else if (child_idx < parent->num_keys) {
            left_node = current;
            right_node = parent->children[child_idx + 1];
            merge_idx = child_idx;
        }

        if (left_node && right_node) {
            merge_nodes(tree, parent, merge_idx, left_node, right_node);
            current = left_node;
            cur_depth--;
        }
        else {
            break;
        }
    }

    // 处理根节点
    if (tree->root && tree->root->num_keys == 0 && !tree->root->is_leaf) {
        BPlusNode* old_root = tree->root;
        BPlusNode* new_root = old_root->children[0];
        if (new_root) {
            tree->root = new_root;
            free(old_root);
        }
        else {
            // 极端情况：树变空
            free(old_root);
            tree->root = NULL;
        }
    }

    free(path);
}

// 查找（返回包含 key 的叶子节点，或 NULL）
BPlusNode* find_key(BPlusTree* tree, int key) {
    if (!tree || !tree->root) return NULL;
    int depth;
    BPlusNode** path;
    BPlusNode* leaf = find_leaf(tree, key, &path, &depth);
    int pos = 0;
    while (pos < leaf->num_keys && leaf->keys[pos] < key) pos++;
    BPlusNode* result = (pos < leaf->num_keys && leaf->keys[pos] == key) ? leaf : NULL;
    free(path);
    return result;
}

// 打印（改进版，更清晰）
void print_tree(BPlusNode* node, int level) {
    if (!node) return;
    for (int i = 0; i < level * 4; i++) printf(" ");
    printf("%s %d keys: ", node->is_leaf ? "Leaf" : "Internal", node->num_keys);
    for (int i = 0; i < node->num_keys; i++) {
        printf("%d ", node->keys[i]);
    }
    printf("\n");
    if (!node->is_leaf) {
        for (int i = 0; i <= node->num_keys; i++) {
            print_tree(node->children[i], level + 1);
        }
    }
}

int main() {
    BPlusTree* tree = create_tree();
    int vals[] = { 1,3,5,7,10,12,15,18,20,22,25,28,30,33,35,40,45,50 };
    for (int i = 0; i < (int)(sizeof(vals) / sizeof(vals[0])); i++) {
        insert(tree, vals[i]);
    }

    printf("初始树:\n");
    print_tree(tree->root, 0);
    printf("\n");

    int deletes[] = { 12, 20, 5, 30, 1, 40, 18 };
    for (int i = 0; i < (int)(sizeof(deletes) / sizeof(deletes[0])); i++) {
        int d = deletes[i];
        printf("删除 %d 前查找: %p\n", d, find_key(tree, d));
        delete_key(tree, d);
        printf("删除 %d 后查找: %p\n", d, find_key(tree, d));
        print_tree(tree->root, 0);
        printf("───────────────────────\n");
    }

    return 0;
}
#endif