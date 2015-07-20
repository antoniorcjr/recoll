/*
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the
 *   Free Software Foundation, Inc.,
 *   59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */
/*
 * First Draft 2010/01/22 
 * Wenqiang Song
 * wsong.cn@gmail.com
 */
#include "php_recoll.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/stat.h>
#include <limits.h>

#include <iostream>

#include "rcldb.h"
#include "rclquery.h"
#include "rclconfig.h"
#include "pathut.h"
#include "rclinit.h"
#include "debuglog.h"
#include "wasastringtoquery.h"
#include "wasatorcl.h"
#include "internfile.h"
#include "wipedir.h"
#include "transcode.h"

using namespace std;

/*
 * Class Query Definition
 */
zend_object_handlers query_object_handlers;
zend_class_entry *query_ce;

struct query_object {
    zend_object std;
    Rcl::Query *pRclQuery;
    Rcl::Db *pRclDb;
};

void query_free_storage(void *object TSRMLS_DC)
{
    query_object *obj = (query_object *)object;

    delete obj->pRclQuery;
    delete obj->pRclDb;

    zend_hash_destroy(obj->std.properties);
    FREE_HASHTABLE(obj->std.properties);

    efree(obj);
}

zend_object_value query_create_handler(zend_class_entry *type TSRMLS_DC)
{
    zval *tmp;
    zend_object_value retval;

    query_object *obj = (query_object *)emalloc(sizeof(query_object));
    memset(obj, 0, sizeof(query_object));
    obj->std.ce = type;

    ALLOC_HASHTABLE(obj->std.properties);
    zend_hash_init(obj->std.properties, 0, NULL, ZVAL_PTR_DTOR, 0);
    zend_hash_copy(obj->std.properties, &type->default_properties,
        (copy_ctor_func_t)zval_add_ref, (void *)&tmp, sizeof(zval *));

    retval.handle = zend_objects_store_put(obj, NULL,
        query_free_storage, NULL TSRMLS_CC);
    retval.handlers = &query_object_handlers;

    return retval;
}

PHP_METHOD(Query, query)
{
    string reason;
    string a_config;
    char *qs_c;
    int qs_len;
    long maxchars;
    long ctxwords;

    if (zend_parse_parameters(3 TSRMLS_CC, "sll", &qs_c, &qs_len, &maxchars, &ctxwords) == FAILURE) {
	printf("failed to get parameters\n");
	RETURN_BOOL(false);
    }
    string qs = qs_c;

    RclConfig *rclconfig = recollinit(0, 0, reason, &a_config);
    if (!rclconfig || !rclconfig->ok()) {
	fprintf(stderr, "Recoll init failed: %s\n", reason.c_str());
	RETURN_BOOL(false);
    }

    Rcl::Db *pRclDb = new Rcl::Db(rclconfig);
    if (!pRclDb->open(Rcl::Db::DbRO)) {
	cerr << "Cant open database in " << rclconfig->getDbDir() << 
	    " reason: " << pRclDb->getReason() << endl;
	RETURN_BOOL(false);
    }

    pRclDb->setAbstractParams(-1, maxchars, ctxwords);
    Rcl::SearchData *sd = 0;

    // jf: the original implementation built an AND clause. It would
    // be nice to offer an option, but the next best thing is to
    // default to the query language
    sd = wasaStringToRcl(rclconfig, "english", qs, reason);

    if (!sd) {
	cerr << "Query string interpretation failed: " << reason << endl;
	RETURN_BOOL(false);
    }

    RefCntr<Rcl::SearchData> rq(sd);
    Rcl::Query *pRclQuery = new Rcl::Query(pRclDb);
    pRclQuery->setQuery(rq);

    query_object *obj = (query_object *)zend_object_store_get_object(getThis() TSRMLS_CC);
    obj->pRclQuery = pRclQuery;
    obj->pRclDb = pRclDb;

    int rescnt = pRclQuery->getResCnt();
    RETURN_LONG(rescnt); /* -1 means no result */
}

PHP_METHOD(Query, get_doc)
{
    Rcl::Query *pRclQuery;
    query_object *obj = (query_object *)zend_object_store_get_object(
        getThis() TSRMLS_CC);
    pRclQuery = obj->pRclQuery;
    if(NULL == pRclQuery)
    {
	printf("error, NULL pointer pRclQuery\n");
	RETURN_BOOL(false);
    }

    long index;
    if (zend_parse_parameters(1 TSRMLS_CC, "l", &index) == FAILURE) {
	    RETURN_BOOL(false);
    }

    Rcl::Doc doc;
    if (!pRclQuery->getDoc(index, doc))
    {
	RETURN_BOOL(false);
    }

    string abs;
    pRclQuery->makeDocAbstract(doc, abs);

    char splitter[] = {7,8,1,2,0};
    char ret_string[1000];
    snprintf(ret_string, 1000, "mime:%s%surl:%s%stitle:%s%sabs:%s",
	    doc.mimetype.c_str(),splitter,
	    doc.url.c_str(),splitter,
	    doc.meta[Rcl::Doc::keytt].c_str(), splitter,
	    abs.c_str());
    RETURN_STRING(ret_string, 1);
}

function_entry query_methods[] = {
    PHP_ME(Query,  query,      NULL, ZEND_ACC_PUBLIC)
    PHP_ME(Query,  get_doc,    NULL, ZEND_ACC_PUBLIC)
    {NULL, NULL, NULL}
};
/* End of Class Query Definition */

PHP_FUNCTION(recoll_connect)
{
    zval *object;

    ALLOC_INIT_ZVAL(object);
    object_init_ex(object, query_ce);

    query_object *obj = (query_object *)zend_object_store_get_object(object TSRMLS_CC);
    obj->pRclQuery = NULL;

    RETURN_ZVAL(object,NULL,NULL);
}

function_entry recoll_functions[] = {
	PHP_FE(recoll_connect,		NULL)
	{NULL, NULL, NULL}
};

PHP_MINIT_FUNCTION(recoll)
{
    zend_class_entry ce;
    INIT_CLASS_ENTRY(ce, "Query", query_methods);
    query_ce = zend_register_internal_class(&ce TSRMLS_CC);
    query_ce->create_object = query_create_handler;
    memcpy(&query_object_handlers,
        zend_get_std_object_handlers(), sizeof(zend_object_handlers));
    query_object_handlers.clone_obj = NULL;
    return SUCCESS;
}

zend_module_entry recoll_module_entry = {
#if ZEND_MODULE_API_NO >= 20010901
    STANDARD_MODULE_HEADER,
#endif
    PHP_RECOLL_EXTNAME,
    recoll_functions,        /* Functions */
    PHP_MINIT(recoll),        /* MINIT */
    NULL,        /* MSHUTDOWN */
    NULL,        /* RINIT */
    NULL,        /* RSHUTDOWN */
    NULL,        /* MINFO */
#if ZEND_MODULE_API_NO >= 20010901
    PHP_RECOLL_EXTVER,
#endif
    STANDARD_MODULE_PROPERTIES
};

#ifdef COMPILE_DL_RECOLL
extern "C" {
ZEND_GET_MODULE(recoll)
}
#endif
