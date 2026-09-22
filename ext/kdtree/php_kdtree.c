#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "php.h"

PHP_FUNCTION(kdtree_ping)
{
    ZEND_PARSE_PARAMETERS_NONE();
    RETURN_TRUE;
}

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(
    arginfo_kdtree_ping, 0, 0, _IS_BOOL, 0)
ZEND_END_ARG_INFO()

static const zend_function_entry kdtree_functions[] = {
    PHP_FE(kdtree_ping, arginfo_kdtree_ping)
        PHP_FE_END};

zend_module_entry kdtree_module_entry = {
    STANDARD_MODULE_HEADER,
    "kdtree",         /* Extension name */
    kdtree_functions, /* zend_function_entry */
    NULL,             /* PHP_MINIT - Module initialization */
    NULL,             /* PHP_MSHUTDOWN - Module shutdown */
    NULL,             /* PHP_RINIT - Request initialization */
    NULL,             /* PHP_RSHUTDOWN - Request shutdown */
    NULL,             /* PHP_MINFO - Module info */
    "0.1.0",          /* Version */
    STANDARD_MODULE_PROPERTIES};

#ifdef COMPILE_DL_KDTREE
ZEND_GET_MODULE(kdtree)
#endif