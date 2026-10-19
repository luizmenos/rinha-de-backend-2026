#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "php.h"
#include "../../lib/kdtree.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int le_kdtree;

typedef struct
{
    Node *root;
    unsigned char *records;
    uint32_t count;
} KDTreeHandle;

typedef struct
{
    uint64_t distance;
    unsigned char label;
} CompactCandidate;

static int16_t compact_coord(const unsigned char *records, uint32_t row, int axis)
{
    int16_t value;
    memcpy(&value, records + (size_t)row * 29 + axis * 2, sizeof(value));
    return value;
}

static void swap_records(unsigned char *records, uint32_t a, uint32_t b)
{
    if (a == b)
        return;
    unsigned char temp[29];
    unsigned char *left = records + (size_t)a * 29;
    unsigned char *right = records + (size_t)b * 29;
    memcpy(temp, left, sizeof(temp));
    memcpy(left, right, sizeof(temp));
    memcpy(right, temp, sizeof(temp));
}

/* Three-way quickselect handles repeated feature values without quadratic partitions. */
static void select_median(unsigned char *records, int64_t low, int64_t high,
                          int64_t target, int axis)
{
    while (low <= high)
    {
        int16_t pivot = compact_coord(records, (uint32_t)(low + (high - low) / 2), axis);
        int64_t less = low;
        int64_t current = low;
        int64_t greater = high;
        while (current <= greater)
        {
            int16_t value = compact_coord(records, (uint32_t)current, axis);
            if (value < pivot)
            {
                swap_records(records, (uint32_t)current, (uint32_t)less);
                current++;
                less++;
            }
            else if (value > pivot)
            {
                swap_records(records, (uint32_t)current, (uint32_t)greater);
                greater--;
            }
            else
            {
                current++;
            }
        }
        if (target < less)
            high = less - 1;
        else if (target > greater)
            low = greater + 1;
        else
            return;
    }
}

static void build_compact_tree(unsigned char *records, uint32_t start,
                               uint32_t end, int depth)
{
    if (end - start <= 1)
        return;
    uint32_t middle = start + (end - start) / 2;
    select_median(records, start, (int64_t)end - 1, middle,
                  depth % DIMENSIONS);
    build_compact_tree(records, start, middle, depth + 1);
    build_compact_tree(records, middle + 1, end, depth + 1);
}

static uint64_t compact_distance(const unsigned char *records, uint32_t row,
                                 const int16_t query[DIMENSIONS])
{
    uint64_t distance = 0;
    for (int axis = 0; axis < DIMENSIONS; axis++)
    {
        int32_t delta = (int32_t)compact_coord(records, row, axis) - query[axis];
        distance += (uint64_t)((int64_t)delta * delta);
    }
    return distance;
}

static void add_compact_candidate(CompactCandidate best[K_NEIGHBORS], int *count,
                                  uint64_t distance, unsigned char label)
{
    if (*count == K_NEIGHBORS && distance >= best[K_NEIGHBORS - 1].distance)
        return;
    int pos = *count < K_NEIGHBORS ? (*count)++ : K_NEIGHBORS - 1;
    while (pos > 0 && distance < best[pos - 1].distance)
    {
        best[pos] = best[pos - 1];
        pos--;
    }
    best[pos].distance = distance;
    best[pos].label = label;
}

static void search_compact_tree(const unsigned char *records, uint32_t start,
                                uint32_t end, int depth,
                                const int16_t query[DIMENSIONS],
                                CompactCandidate best[K_NEIGHBORS], int *count)
{
    if (start >= end)
        return;
    uint32_t middle = start + (end - start) / 2;
    const unsigned char *record = records + (size_t)middle * 29;
    add_compact_candidate(best, count, compact_distance(records, middle, query), record[28]);

    int axis = depth % DIMENSIONS;
    int32_t delta = (int32_t)query[axis] - compact_coord(records, middle, axis);
    if (delta < 0)
    {
        search_compact_tree(records, start, middle, depth + 1, query, best, count);
        uint64_t worst = *count < K_NEIGHBORS ? UINT64_MAX : best[K_NEIGHBORS - 1].distance;
        if ((uint64_t)((int64_t)delta * delta) <= worst)
            search_compact_tree(records, middle + 1, end, depth + 1, query, best, count);
    }
    else
    {
        search_compact_tree(records, middle + 1, end, depth + 1, query, best, count);
        uint64_t worst = *count < K_NEIGHBORS ? UINT64_MAX : best[K_NEIGHBORS - 1].distance;
        if ((uint64_t)((int64_t)delta * delta) <= worst)
            search_compact_tree(records, start, middle, depth + 1, query, best, count);
    }
}

static void kdtree_resource_dtor(zend_resource *res)
{
    KDTreeHandle *tree = (KDTreeHandle *)res->ptr;
    if (tree != NULL)
    {
        destroy_tree(tree->root);
        free(tree->records);
        free(tree);
    }
}

PHP_MINIT_FUNCTION(kdtree)
{
    le_kdtree = zend_register_list_destructors_ex(
        kdtree_resource_dtor,
        NULL,
        "KDTree",
        module_number);

    return SUCCESS;
}

PHP_FUNCTION(kdtree_ping)
{
    ZEND_PARSE_PARAMETERS_NONE();
    RETURN_TRUE;
}

PHP_FUNCTION(kdtree_build)
{
    zval *points;
    zend_long num_points;
    zend_long depth;

    ZEND_PARSE_PARAMETERS_START(3, 3)
    Z_PARAM_ARRAY(points)
    Z_PARAM_LONG(num_points)
    Z_PARAM_LONG(depth)
    ZEND_PARSE_PARAMETERS_END();

    if (num_points <= 0)
    {
        zend_value_error("num_points must be positive");
        RETURN_THROWS();
    }

    Reference *references = safe_emalloc(
        (size_t)num_points, sizeof(*references), 0);

    HashTable *ht_points = Z_ARRVAL_P(points);
    zval *sub_array;
    size_t i = 0;
    zend_bool invalid = 0;

    ZEND_HASH_FOREACH_VAL(ht_points, sub_array)
    {
        if (i >= (size_t)num_points)
        {
            invalid = 1;
            break;
        }
        if (Z_TYPE_P(sub_array) != IS_ARRAY)
        {
            invalid = 1;
            break;
        }

        zval *vector = zend_hash_str_find(Z_ARRVAL_P(sub_array), "vector", sizeof("vector") - 1);
        zval *label = zend_hash_str_find(Z_ARRVAL_P(sub_array), "label", sizeof("label") - 1);

        if (vector == NULL || Z_TYPE_P(vector) != IS_ARRAY ||
            label == NULL || Z_TYPE_P(label) != IS_STRING)
        {
            invalid = 1;
            break;
        }

        zend_string *label_str = Z_STR_P(label);
        zend_bool is_fraud = zend_string_equals_literal(label_str, "fraud");

        if (!is_fraud && !zend_string_equals_literal(label_str, "legit"))
        {
            invalid = 1;
            break;
        }

        HashTable *ht_vector = Z_ARRVAL_P(vector);
        if (zend_hash_num_elements(ht_vector) != DIMENSIONS)
        {
            invalid = 1;
            break;
        }

        size_t j = 0;
        zval *coord;

        ZEND_HASH_FOREACH_VAL(ht_vector, coord)
        {
            references[i].point[j++] = zval_get_double(coord);
        }
        ZEND_HASH_FOREACH_END();

        references[i].is_fraud = is_fraud;
        i++;
    }
    ZEND_HASH_FOREACH_END();

    if (invalid || i != (size_t)num_points)
    {
        efree(references);
        zend_value_error("points must contain num_points items with a 14-value vector and fraud/legit label");
        RETURN_THROWS();
    }

    Node *root = build_tree(references, (int)num_points, (int)depth);
    efree(references);

    if (root == NULL)
    {
        zend_throw_error(NULL, "failed to build KD tree");
        RETURN_THROWS();
    }

    KDTreeHandle *tree = calloc(1, sizeof(*tree));
    if (tree == NULL)
    {
        destroy_tree(root);
        zend_throw_error(NULL, "failed to allocate KD tree handle");
        RETURN_THROWS();
    }
    tree->root = root;
    RETURN_RES(zend_register_resource(tree, le_kdtree));
}

PHP_FUNCTION(kdtree_load)
{
    zend_string *path;
    ZEND_PARSE_PARAMETERS_START(1, 1)
    Z_PARAM_PATH_STR(path)
    ZEND_PARSE_PARAMETERS_END();

    FILE *file = fopen(ZSTR_VAL(path), "rb");
    if (file == NULL)
    {
        zend_value_error("could not open KD index file");
        RETURN_THROWS();
    }

    unsigned char header[16];
    if (fread(header, 1, sizeof(header), file) != sizeof(header) ||
        memcmp(header, "RINHAIDX", 8) != 0 ||
        header[8] != 1 || header[9] != 0 || header[10] != 0 || header[11] != 0)
    {
        fclose(file);
        zend_value_error("invalid KD index header or unsupported version");
        RETURN_THROWS();
    }

    uint32_t count = (uint32_t)header[12] | ((uint32_t)header[13] << 8) |
                     ((uint32_t)header[14] << 16) | ((uint32_t)header[15] << 24);
    if (count == 0 || (uint64_t)count * 29 > SIZE_MAX)
    {
        fclose(file);
        zend_value_error("invalid KD index reference count");
        RETURN_THROWS();
    }

    KDTreeHandle *tree = calloc(1, sizeof(*tree));
    if (tree == NULL)
    {
        fclose(file);
        zend_throw_error(NULL, "failed to allocate KD tree handle");
        RETURN_THROWS();
    }
    tree->records = malloc((size_t)count * 29);
    tree->count = count;
    if (tree->records == NULL || fread(tree->records, 29, count, file) != count || fgetc(file) != EOF)
    {
        free(tree->records);
        free(tree);
        fclose(file);
        zend_value_error("truncated or malformed KD index data");
        RETURN_THROWS();
    }
    fclose(file);
    for (uint32_t i = 0; i < count; i++)
    {
        if (tree->records[(size_t)i * 29 + 28] > 1)
        {
            free(tree->records);
            free(tree);
            zend_value_error("invalid label in KD index data");
            RETURN_THROWS();
        }
    }
    build_compact_tree(tree->records, 0, count, 0);
    RETURN_RES(zend_register_resource(tree, le_kdtree));
}

PHP_FUNCTION(kdtree_search)
{
    zval *tree_zv;
    zval *query_zv;

    ZEND_PARSE_PARAMETERS_START(2, 2)
    Z_PARAM_RESOURCE(tree_zv)
    Z_PARAM_ARRAY(query_zv)
    ZEND_PARSE_PARAMETERS_END();

    if (zend_hash_num_elements(Z_ARRVAL_P(query_zv)) != DIMENSIONS)
    {
        zend_value_error("query must contain exactly 14 dimensions");
        RETURN_THROWS();
    }

    KDTreeHandle *tree = zend_fetch_resource_ex(tree_zv, "KDTree", le_kdtree);
    if (tree == NULL)
        RETURN_THROWS();
    if (tree->root == NULL)
    {
        zend_value_error("kdtree_search requires a tree created by kdtree_build");
        RETURN_THROWS();
    }

    double query[DIMENSIONS];
    size_t i = 0;
    zval *coord;
    ZEND_HASH_FOREACH_VAL(Z_ARRVAL_P(query_zv), coord)
    {
        query[i++] = zval_get_double(coord);
    }
    ZEND_HASH_FOREACH_END();

    Candidate best[K_NEIGHBORS];
    int count = search_5_nearest_neighbors(tree->root, query, best);

    array_init(return_value);
    for (int j = 0; j < count; j++)
    {
        zval neighbor;
        array_init(&neighbor);
        add_assoc_string(&neighbor, "label", best[j].node->is_fraud ? "fraud" : "legit");
        add_assoc_double(&neighbor, "distance_squared", best[j].distance);
        add_next_index_zval(return_value, &neighbor);
    }
}

PHP_FUNCTION(kdtree_fraud_count)
{
    zval *tree_zv;
    zval *query_zv;
    ZEND_PARSE_PARAMETERS_START(2, 2)
    Z_PARAM_RESOURCE(tree_zv)
    Z_PARAM_ARRAY(query_zv)
    ZEND_PARSE_PARAMETERS_END();

    if (zend_hash_num_elements(Z_ARRVAL_P(query_zv)) != DIMENSIONS)
    {
        zend_value_error("query must contain exactly 14 dimensions");
        RETURN_THROWS();
    }
    KDTreeHandle *tree = zend_fetch_resource_ex(tree_zv, "KDTree", le_kdtree);
    if (tree == NULL)
        RETURN_THROWS();
    if (tree->records == NULL)
    {
        zend_value_error("kdtree_fraud_count requires an index loaded by kdtree_load");
        RETURN_THROWS();
    }

    int16_t query[DIMENSIONS];
    size_t dim = 0;
    zval *coord;
    ZEND_HASH_FOREACH_VAL(Z_ARRVAL_P(query_zv), coord)
    {
        double scaled = zval_get_double(coord) * 10000.0;
        long quantized = (long)(scaled + (scaled >= 0.0 ? 0.5 : -0.5));
        if (quantized < INT16_MIN)
            quantized = INT16_MIN;
        if (quantized > INT16_MAX)
            quantized = INT16_MAX;
        query[dim++] = (int16_t)quantized;
    }
    ZEND_HASH_FOREACH_END();

    CompactCandidate best[K_NEIGHBORS] = {{0}};
    int count = 0;
    search_compact_tree(tree->records, 0, tree->count, 0, query, best, &count);
    int fraud_count = 0;
    for (int i = 0; i < count; i++)
        fraud_count += best[i].label == 1;
    RETURN_LONG(fraud_count);
}

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(
    arginfo_kdtree_ping, 0, 0, _IS_BOOL, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_kdtree_load, 0, 1, IS_RESOURCE, 0)
ZEND_ARG_TYPE_INFO(0, path, IS_STRING, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_kdtree_fraud_count, 0, 2, IS_LONG, 0)
ZEND_ARG_TYPE_INFO(0, tree, IS_RESOURCE, 0)
ZEND_ARG_TYPE_INFO(0, query, IS_ARRAY, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(
    arginfo_kdtree_build, 0, 3, IS_RESOURCE, 0)
ZEND_ARG_TYPE_INFO(0, points, IS_ARRAY, 0)
ZEND_ARG_TYPE_INFO(0, num_points, IS_LONG, 0)
ZEND_ARG_TYPE_INFO(0, depth, IS_LONG, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(
    arginfo_kdtree_search, 0, 2, IS_ARRAY, 0)
ZEND_ARG_TYPE_INFO(0, tree, IS_RESOURCE, 0)
ZEND_ARG_TYPE_INFO(0, query, IS_ARRAY, 0)
ZEND_END_ARG_INFO()

static const zend_function_entry kdtree_functions[] = {
    PHP_FE(kdtree_ping, arginfo_kdtree_ping)
        PHP_FE(kdtree_build, arginfo_kdtree_build)
            PHP_FE(kdtree_search, arginfo_kdtree_search)
                PHP_FE(kdtree_load, arginfo_kdtree_load)
                    PHP_FE(kdtree_fraud_count, arginfo_kdtree_fraud_count)
                        PHP_FE_END};

zend_module_entry kdtree_module_entry = {
    STANDARD_MODULE_HEADER,
    "kdtree",          /* Extension name */
    kdtree_functions,  /* zend_function_entry */
    PHP_MINIT(kdtree), /* PHP_MINIT - Module initialization */
    NULL,              /* PHP_MSHUTDOWN - Module shutdown */
    NULL,              /* PHP_RINIT - Request initialization */
    NULL,              /* PHP_RSHUTDOWN - Request shutdown */
    NULL,              /* PHP_MINFO - Module info */
    "0.4.0",           /* Version */
    STANDARD_MODULE_PROPERTIES};

#ifdef COMPILE_DL_KDTREE
ZEND_GET_MODULE(kdtree)
#endif
