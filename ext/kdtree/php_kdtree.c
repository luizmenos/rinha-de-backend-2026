#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "php.h"
#include "../../lib/kdtree.h"

static int le_kdtree;

static void kdtree_resource_dtor(zend_resource *res)
{
    destroy_tree((Node *)res->ptr);
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

    RETURN_RES(zend_register_resource(root, le_kdtree));
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

    Node *root = zend_fetch_resource_ex(tree_zv, "KDTree", le_kdtree);
    if (root == NULL)
        RETURN_THROWS();

    double query[DIMENSIONS];
    size_t i = 0;
    zval *coord;
    ZEND_HASH_FOREACH_VAL(Z_ARRVAL_P(query_zv), coord)
    {
        query[i++] = zval_get_double(coord);
    }
    ZEND_HASH_FOREACH_END();

    Candidate best[K_NEIGHBORS];
    int count = search_5_nearest_neighbors(root, query, best);

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

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(
    arginfo_kdtree_ping, 0, 0, _IS_BOOL, 0)
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
