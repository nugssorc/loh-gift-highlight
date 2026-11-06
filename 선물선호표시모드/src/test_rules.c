#include "win.h"
#include "gift_rules.h"
#define CHECK(c) do{if(!(c))ExitProcess(__LINE__);}while(0)
void Entry(void){
    /* Deliberately shifted enum values verify these are resolved at runtime. */
    const int eqs[8]={30,31,32,33,34,35,36,37};
    CHECK(gift_preference(-1,-1,1,0,eqs)==PREF_BOOK);
    CHECK(gift_preference(31,0,0,0,eqs)==PREF_CLOTH);
    CHECK(gift_preference(31,1,0,0,eqs)==PREF_CLOTH);
    CHECK(gift_preference(31,2,0,0,eqs)==PREF_ARMOR);
    CHECK(gift_preference(34,0,0,0,eqs)==PREF_ARMOR);
    CHECK(gift_preference(34,1,0,0,eqs)==PREF_CLOTH);
    CHECK(gift_preference(34,2,0,0,eqs)==PREF_CLOTH);
    CHECK(gift_preference(30,0,0,0,eqs)==PREF_WEAPON);
    CHECK(gift_preference(33,0,0,0,eqs)==PREF_HORSE);
    CHECK(gift_preference(32,0,0,0,eqs)==PREF_TREASURE);
    CHECK(gift_preference(37,0,0,0,eqs)==PREF_ARMOR);
    CHECK(gift_preference(36,2,0,0,eqs)==-1); /* toys are not clothes */
    CHECK(gift_preference(-1,-1,0,1,eqs)==PREF_WINE);
    CHECK(gift_preference(-1,-1,0,0,eqs)==-1); /* medicine/unknown */
    CHECK(gift_liked(PREF_BOOK,(1u<<PREF_BOOK)|(1u<<PREF_CLOTH)));
    CHECK(!gift_liked(PREF_ARMOR,(1u<<PREF_BOOK)|(1u<<PREF_CLOTH)));
    CHECK(gift_liked(PREF_WINE,1u<<PREF_WINE));
    CHECK(!gift_liked(PREF_WINE,(1u<<PREF_WINE)|(1u<<PREF_HATE_WINE)));
    CHECK(!gift_liked(-1,~0u));
    CHECK(!gift_liked(PREF_HATE_WINE,~0u));
    ExitProcess(0);
}
