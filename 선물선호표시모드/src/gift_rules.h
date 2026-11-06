/* UI hints only: no gift rewards, inventory or character data are changed. */
#ifndef LOH_GIFT_RULES_H
#define LOH_GIFT_RULES_H
enum { PREF_BOOK, PREF_CLOTH, PREF_ARMOR, PREF_WEAPON, PREF_HORSE,
       PREF_TREASURE, PREF_WINE, PREF_HATE_WINE, PREF_COUNT };
/* Resolved enum values are supplied at runtime; there are no localized names here. */
static int gift_preference(int kind, int detail, int book, int wine,
                           const int equipment[8]) {
    if (book) return PREF_BOOK;
    if (wine) return PREF_WINE;
    if (kind == equipment[0]) return PREF_WEAPON;
    if (kind == equipment[1]) return detail == 2 ? PREF_ARMOR :
                                     (detail == 0 || detail == 1) ? PREF_CLOTH : -1;
    if (kind == equipment[2]) return PREF_TREASURE;
    if (kind == equipment[3]) return PREF_HORSE;
    if (kind == equipment[4]) return detail == 0 ? PREF_ARMOR :
                                     (detail == 1 || detail == 2) ? PREF_CLOTH : -1;
    if (kind == equipment[7]) return PREF_ARMOR;
    return -1;
}
static int gift_liked(int pref, unsigned mask) {
    return pref >= 0 && pref < PREF_HATE_WINE && (mask & (1u << pref)) &&
           !(pref == PREF_WINE && (mask & (1u << PREF_HATE_WINE)));
}
#endif
