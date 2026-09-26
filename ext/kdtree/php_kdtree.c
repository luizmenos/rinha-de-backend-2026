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

    double (*c_points)[DIMENSIONS] = emalloc(num_points * sizeof(*c_points));

    HashTable *ht_points = Z_ARRVAL_P(points);
    zval *sub_array;
    int i = 0;

    ZEND_HASH_FOREACH_VAL(ht_points, sub_array)
    {
        if (i >= num_points)
            break;
        if (Z_TYPE_P(sub_array) == IS_ARRAY)
        {
            HashTable *ht_sub = Z_ARRVAL_P(sub_array);
            zval *coord;
            int j = 0;

            ZEND_HASH_FOREACH_VAL(ht_sub, coord)
            {
                if (j >= DIMENSIONS)
                    break;
                c_points[i][j] = zval_get_double(coord);
                j++;
            }
            ZEND_HASH_FOREACH_END();
        }
        i++;
    }
    ZEND_HASH_FOREACH_END();

    Node *root = build_tree(c_points, num_points, depth);

    efree(c_points);

    RETURN_RES(zend_register_resource(root, le_kdtree));
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

static const zend_function_entry kdtree_functions[] = {
    PHP_FE(kdtree_ping, arginfo_kdtree_ping)
        PHP_FE(kdtree_build, arginfo_kdtree_build)
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
    "0.3.0",           /* Version */
    STANDARD_MODULE_PROPERTIES};

#ifdef COMPILE_DL_KDTREE
ZEND_GET_MODULE(kdtree)
#endif