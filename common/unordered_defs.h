
#include "autoconfig.h"

#ifdef  HAVE_CXX0X_UNORDERED
#  include <unordered_map>
#  include <unordered_set>
#  define STD_UNORDERED_MAP std::unordered_map
#  define STD_UNORDERED_SET std::unordered_set
#elif defined(HAVE_TR1_UNORDERED)
#  include <tr1/unordered_map>
#  include <tr1/unordered_set>
#  define STD_UNORDERED_MAP std::tr1::unordered_map
#  define STD_UNORDERED_SET std::tr1::unordered_set
#else
#  include <map>
#  include <set>
#  define STD_UNORDERED_MAP std::map
#  define STD_UNORDERED_SET std::set
#endif
