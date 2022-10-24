#include "runtime/DecimalRuntime.h"
const int64_t powersOfTen[19] = {
   1ll, 10ll, 100ll, 1000ll, 10000ll, 100000ll, 1000000ll, 10000000ll, 100000000ll,
   1000000000ll, 10000000000ll, 100000000000ll, 1000000000000ll, 10000000000000ll,
   100000000000000ll, 1000000000000000ll, 10000000000000000ll,
   100000000000000000ll, 1000000000000000000ll};

__int128 runtime::DecimalRuntime::round(__int128 value, int64_t roundByScale) {
   if (roundByScale == 0) {
      return value; //no rounding necessary
   }
   auto shift = powersOfTen[roundByScale];
   auto divided = value / shift;
   auto remainder = value % shift;
   auto res = divided;
   if (value > 0) {
      res += (remainder > (shift / 2)) ? 1 : 0;
   } else {
      res -= (-remainder > (shift / 2)) ? 1 : 0;
   }
   return res * shift;
}