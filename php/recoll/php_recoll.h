#ifndef PHP_RECOLL_H
#define PHP_RECOLL_H

#define PHP_RECOLL_EXTNAME  "recoll"
#define PHP_RECOLL_EXTVER   "0.1"

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif 

extern "C" {
#include "php.h"
}

extern zend_module_entry recoll_module_entry;
#define phpext_recoll_ptr &recoll_module_entry;

#endif /* PHP_RECOLL_H */
