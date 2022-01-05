#include <eospowerupio.hpp>

#if defined(DEBUG)
  ACTION eospowerupio::clrwhitelist(){
    require_auth(get_self());
    cleanTable<tknwhitelist_table>(get_self(),get_self().value, 100);
  };
#endif