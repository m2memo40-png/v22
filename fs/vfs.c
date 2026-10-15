#include "fs/vfs.h"
#include "mm/heap.h"
#include "lib/mem.h"
#include "kernel/errno.h"
#include <stdint.h>

static vfs_node_t *root_node;

static size_t str_len(const char *s) { size_t n = 0; if (s) while (s[n]) ++n; return n; }
static int str_eq_n(const char *a, const char *b, size_t n) {
    for (size_t i = 0; i < n; ++i) if (a[i] != b[i]) return 0;
    return 1;
}
static vfs_node_t *node_new(const char *name, size_t length, vfs_node_type_t type) {
    if (!name || !length || length >= sizeof(((vfs_node_t *)0)->name)) return 0;
    vfs_node_t *n = (vfs_node_t *)kmalloc(sizeof(*n));
    if (!n) return 0;
    memset(n, 0, sizeof(*n));
    memcpy(n->name, name, length); n->name[length] = '\0'; n->type = type;
    return n;
}
static vfs_node_t *child_find(vfs_node_t *parent, const char *name, size_t length) {
    if (!parent || parent->type != VFS_NODE_DIRECTORY) return 0;
    for (vfs_node_t *n = parent->first_child; n; n = n->next_sibling)
        if (str_len(n->name) == length && str_eq_n(n->name, name, length)) return n;
    return 0;
}
static int next_component(const char **cursor, const char **start, size_t *length) {
    const char *p = *cursor;
    while (*p == '/') ++p;
    if (!*p) { *cursor = p; return 0; }
    *start = p;
    while (*p && *p != '/') ++p;
    *length = (size_t)(p - *start);
    *cursor = p;
    return 1;
}

int vfs_init(void) {
    if (root_node) return 0;
    root_node = node_new("/", 1, VFS_NODE_DIRECTORY);
    return root_node ? 0 : -T_ENOMEM;
}
vfs_node_t *vfs_root(void) { return root_node; }

vfs_node_t *vfs_lookup(const char *path) {
    if (!root_node || !path || path[0] != '/') return 0;
    vfs_node_t *current = root_node;
    const char *cursor = path, *part; size_t length;
    while (next_component(&cursor, &part, &length)) {
        if (length == 1 && part[0] == '.') continue;
        if (length == 2 && part[0] == '.' && part[1] == '.') {
            if (current->parent) current = current->parent;
            continue;
        }
        if (length >= sizeof(current->name)) return 0;
        current = child_find(current, part, length);
        if (!current) return 0;
    }
    return current;
}

vfs_node_t *vfs_create(const char *path, vfs_node_type_t type) {
    if (!root_node || !path || path[0] != '/' || (type != VFS_NODE_FILE && type != VFS_NODE_DIRECTORY)) return 0;
    size_t full = str_len(path);
    if (full < 2 || path[full - 1] == '/') return 0;
    const char *last = path + full;
    while (last > path && last[-1] != '/') --last;
    size_t name_len = (size_t)((path + full) - last);
    if (!name_len || name_len >= 64 || (name_len == 1 && last[0] == '.') ||
        (name_len == 2 && last[0] == '.' && last[1] == '.')) return 0;
    vfs_node_t *parent;
    if (last == path + 1) parent = root_node;
    else {
        size_t parent_len = (size_t)(last - path);
        char parent_path[256];
        if (parent_len >= sizeof(parent_path)) return 0;
        memcpy(parent_path, path, parent_len);
        while (parent_len > 1 && parent_path[parent_len - 1] == '/') --parent_len;
        parent_path[parent_len] = '\0';
        parent = vfs_lookup(parent_path);
    }
    if (!parent || parent->type != VFS_NODE_DIRECTORY || child_find(parent, last, name_len)) return 0;
    vfs_node_t *created = node_new(last, name_len, type);
    if (!created) return 0;
    created->parent = parent;
    if (!parent->first_child) parent->first_child = created;
    else { vfs_node_t *tail = parent->first_child; while (tail->next_sibling) tail = tail->next_sibling; tail->next_sibling = created; }
    return created;
}
int vfs_mkdir(const char *path) { return vfs_create(path, VFS_NODE_DIRECTORY) ? 0 : -T_EINVAL; }

int vfs_unlink(const char *path) {
    vfs_node_t *n = vfs_lookup(path);
    if (!n || n == root_node || !n->parent || n->first_child) return -T_EINVAL;
    vfs_node_t *p = n->parent;
    vfs_node_t **link = &p->first_child;
    while (*link && *link != n) link = &(*link)->next_sibling;
    if (*link != n) return -T_ENOENT;
    *link = n->next_sibling;
    if (n->data) kfree(n->data);
    n->size = n->capacity = 0; n->data = 0;
    kfree(n);
    return 0;
}

int64_t vfs_read(vfs_node_t *node, uint64_t offset, void *buffer, size_t length) {
    if (!node || (!buffer && length) || node->type != VFS_NODE_FILE) return -T_EINVAL;
    if (offset >= node->size || !length) return 0;
    uint64_t left = node->size - offset;
    if ((uint64_t)length > left) length = (size_t)left;
    memcpy(buffer, node->data + (size_t)offset, length);
    return (int64_t)length;
}

int64_t vfs_write(vfs_node_t *node, uint64_t offset, const void *buffer, size_t length) {
    if (!node || (!buffer && length) || node->type != VFS_NODE_FILE) return -T_EINVAL;
    if (!length) return 0;
    if (offset > UINT64_MAX - (uint64_t)length) return -T_EINVAL;
    uint64_t end64 = offset + length;
    if (end64 > 1024u * 1024u) return -T_ENOMEM; /* Per-file safety cap for this RAM filesystem. */
    size_t end = (size_t)end64;
    if (end > node->capacity) {
        size_t capacity = node->capacity ? (size_t)node->capacity : 64u;
        while (capacity < end) {
            if (capacity > (1024u * 1024u) / 2u) { capacity = 1024u * 1024u; break; }
            capacity *= 2u;
        }
        uint8_t *new_data = (uint8_t *)kmalloc(capacity);
        if (!new_data) return -T_ENOMEM;
        memset(new_data, 0, capacity);
        if (node->data && node->size) memcpy(new_data, node->data, (size_t)node->size);
        if (node->data) kfree(node->data);
        node->data = new_data; node->capacity = capacity;
    }
    if (offset > node->size) memset(node->data + (size_t)node->size, 0, (size_t)(offset - node->size));
    memcpy(node->data + (size_t)offset, buffer, length);
    if (end64 > node->size) node->size = end64;
    return (int64_t)length;
}

int vfs_list(vfs_node_t *directory, vfs_list_fn visit, void *context) {
    if (!directory || directory->type != VFS_NODE_DIRECTORY || !visit) return -T_EINVAL;
    for (vfs_node_t *n = directory->first_child; n; n = n->next_sibling) {
        int result = visit(n, context);
        if (result) return result;
    }
    return 0;
}
