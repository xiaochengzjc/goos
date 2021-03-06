#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "php.h"
#include "ext/standard/info.h"
#include "php_go.h"
#include "PHPCoroutine.h"
#include "Proc.h"
#include "Coroutine.h"
#include "ZendFunction.h"
ZEND_DECLARE_MODULE_GLOBALS(go)
static void go_globals_ctor(zend_go_globals *pg)
{
    ZVAL_UNDEF(&pg->_this);
    pg->free_stack = new queue<Coroutine*>();
    pg->runq       = new queue<Coroutine*>();
    pg->_g         = NULL;
    pg->_glock     = new mutex;
    pg->pid        = 0L;
    pg->signal     = 0;
    pg->resources  = NULL;
    pg->schedtick  = 0;
    pg->schedwhen  = chrono::steady_clock::now();
}
static inline int sapi_cli_deactivate(void)
{
    fflush(stdout);
    return SUCCESS;
}
PHP_MINIT_FUNCTION(go)
{
    ZEND_INIT_MODULE_GLOBALS(go, go_globals_ctor, NULL);
    if (memcmp(sapi_module.name, ZEND_STRL("cli")) == SUCCESS) {
        sapi_module.deactivate = sapi_cli_deactivate;
    }
    register_php_coroutine();
    register_php_runtime();
    return SUCCESS;
}
PHP_MSHUTDOWN_FUNCTION(go)
{
    return SUCCESS;
}
PHP_RINIT_FUNCTION(go)
{
    ZEND_TSRMLS_CACHE_UPDATE();
    zend_hash_init(&GO_ZG(resolve), 15, NULL, NULL, 0);
    zend_hash_init(&GO_ZG(filenames), 15, NULL, NULL, 0);
    GO_ZG(hard_copy_interned_strings) = 0;
    GO_ZG(local) = (void ***)TSRMLS_CACHE;
    return SUCCESS;
}
PHP_RSHUTDOWN_FUNCTION(go)
{
    delete GO_ZG(_glock);
    delete GO_ZG(free_stack);
    delete GO_ZG(runq);
    zend_hash_destroy(&GO_ZG(resolve));
    zend_hash_destroy(&GO_ZG(filenames));
    return SUCCESS;
}
PHP_MINFO_FUNCTION(go)
{
    php_info_print_table_start();
    php_info_print_table_header(2, "go support", "enabled");
    php_info_print_table_row(2, "Author", "Brewlin Team <97404667@qq.com>");
    php_info_print_table_row(2, "Version", "1.0.0");
    php_info_print_table_end();
}
ZEND_MODULE_POST_ZEND_DEACTIVATE_D(go)
{
    if (GO_ZG(resources)) {
        zend_hash_destroy(GO_ZG(resources));
        FREE_HASHTABLE(GO_ZG(resources));
        GO_ZG(resources) = NULL;
    }
    return SUCCESS;
}

static const zend_function_entry go_functions[] = {
        PHP_FALIAS(go,go_create,arg_go_create)
        PHP_FALIAS(goyield,go_yield,NULL)
        PHP_FE_END
};

zend_module_entry go_module_entry = {
        STANDARD_MODULE_HEADER,
        "go",					/* Extension name */
        go_functions,			/* zend_function_entry */
        PHP_MINIT(go),							/* PHP_MINIT - Module initialization */
        PHP_MSHUTDOWN(go),							/* PHP_MSHUTDOWN - Module shutdown */
        PHP_RINIT(go),			/* PHP_RINIT - Request initialization */
        PHP_RSHUTDOWN(go),							/* PHP_RSHUTDOWN - Request shutdown */
        PHP_MINFO(go),			/* PHP_MINFO - Module info */
        PHP_GO_VERSION,		/* Version */
//        STANDARD_MODULE_PROPERTIES
        NO_MODULE_GLOBALS,
        ZEND_MODULE_POST_ZEND_DEACTIVATE_N(go),
        STANDARD_MODULE_PROPERTIES_EX
};
#ifdef COMPILE_DL_GO
# ifdef ZTS
ZEND_TSRMLS_CACHE_DEFINE()
# endif
ZEND_GET_MODULE(go)
#endif