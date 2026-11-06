/* 선물·화제 취향 표시 0.5.3. No items, gift effects or shop hooks. */
#include "win.h"
#include "gift_rules.h"
#include "topic_bindings.h"
#include "runtime.inc"
#define WM_MOD (0x8000+0x6B1)
#define MODULE_MUTEX L"LOH_GiftHighlight_Loaded"
#define MODULE_LOG L"LOH_GiftHighlight.log"
#define MODULE_VERSION "0.5.3"
#define MODULE_BANNER "LOH_GiftHighlight " MODULE_VERSION
#define READY_TEXT "READY " MODULE_VERSION " — 선물·화제 취향 표시 준비 완료"
#define MODULE_INFO "LOH_ModInfo 1;id=gift;name=선물·화제 취향 표시;version=" MODULE_VERSION ";pause=0"
#define MODULE_DETAIL_STATE (topic_ready?2:5)
static P game_image;
static P M_active,M_getGO,M_getColor,M_setColor,M_getText;
static int active(P o){P go;if(!alive(o))return 0;go=invoke(M_getGO,o,0);return alive(go)&&ibool(M_active,go);}
static P F_self,F_intent,F_shown,F_rows,F_fixed,F_start,F_end,F_cols,F_index;
static P F_npc,F_interacting,F_interaction;
static P K_hero,K_equipment,K_book,K_drug,K_topic;
static P F_traits,F_traitType,F_traitDegree,F_equipKind,F_detail,F_drugKind;
static P hero_dicts[8],M_tryHero;static int ndicts;
static int pref_values[PREF_COUNT],equip_values[8],wine_value;
static const char *pref_names[PREF_COUNT]={"LikeBook","LikeCloth","LikeArmor","LikeWeapon","LikeHorse","LikeTreasure","LikeWine","HateWine"};
static const char *equip_names[8]={"Weapon","Armor","Treasure","Horse","Helmet","Mission","Present","Shield"};
/* 0.5.0 메모장 설정: LOH_GiftHighlight\취향표시_설정.ini. 없으면 기본값으로 만든다(UTF-16, 메모장으로 열림). */
W u32 API GetPrivateProfileIntW(const wch *, const wch *, int, const wch *);
static int only_revealed=1,show_topics=1;
/* 해금: 장수 +0x180 List<int> = 해금된 성격 칸 번호(+0xE8 성격 목록의 순번). 게임 성격 창(UI_HeroInfoUI·InteractOptionUI)이
   칸 i 마다 이 목록.Contains(i) 가 거짓이면 '?' 그림과 "未解锁" 를 보여 준다(2026-10-31 역어셈블 확인). 이름·형식이 다르면 해금 취향이 없는 것으로 본다. */
static P F_revealed;
static int revealed(P hero,int slot){P l=get(hero,F_revealed),a;int n,k;if(!l)return 0;n=length(l);a=*(P*)((u8*)l+0x10);
    if(!a)return 0;if((uptr)n>*(uptr*)((u8*)a+0x18))n=(int)*(uptr*)((u8*)a+0x18);
    for(k=0;k<n;k++)if(((int*)((u8*)a+0x20))[k]==slot)return 1;return 0;}
static const wch settings_text[]=L"\xFEFF; 선물·화제 취향 표시 설정 — 메모장으로 고쳐 저장한 뒤 게임을 다시 켜면 적용됩니다.\r\n"
    L"[GiftHighlight]\r\n"
    L"; 1 = 게임에서 이미 알아낸(해금한) 취향만 분홍색으로 표시(화제 포함), 0 = '?' 로 가려진 취향까지 모두 표시\r\n"
    L"OnlyRevealed=1\r\n"
    L"; 1 = 화제 고르기에서도 잘 먹히는 화제를 분홍색으로 표시, 0 = 화제는 표시 안 함\r\n"
    L"Topics=1\r\n";
static void load_settings(void){wch path[800];HANDLE h=GetModuleHandleW(L"LOH_GiftHighlight.dll");u32 n=h?GetModuleFileNameW(h,path,700):0,w;int i;
    static const wch name[]=L"취향표시_설정.ini";
    if(!n)return;while(n&&path[n-1]!=0x5C)n--;for(i=0;name[i];i++)path[n++]=name[i];path[n]=0;
    if(GetFileAttributesW(path)==0xFFFFFFFFu){HANDLE f=CreateFileW(path,0x40000000,1,0,1,0x80,0);      /* CREATE_NEW: 있으면 건드리지 않음 */
        if(f!=INVALID_HANDLE){WriteFile(f,settings_text,sizeof settings_text-2,&w,0);CloseHandle(f);logmsg("settings: created 취향표시_설정.ini with defaults");}}
    only_revealed=GetPrivateProfileIntW(L"GiftHighlight",L"OnlyRevealed",1,path)!=0;show_topics=GetPrivateProfileIntW(L"GiftHighlight",L"Topics",1,path)!=0;
    logmsg("settings: OnlyRevealed=%d Topics=%d",only_revealed,show_topics);
}
static P F_topicFactors,F_conditions,M_condition,M_factor,empty_string;
static u32 empty_handle;
static int hero_context,topic_ready;
/* 0.5.1 가벼운 화제(Topic.type Relax: 음주·여행·오락·재물·자녀·정의·사악·낭만)의 원본 반응 대화(10010908 등 8개)는
   첫 분기가 '9/性格/24/>=/0'(가혹 Harsh)이라, 수치 배율과 상관없이 싫은 대사·표정이 나온다(순욱 신고, 2026-10-31).
   그런 장수에게는 가벼운 화제를 분홍으로 칠하지 않는다. 성격은 대화 조건처럼 해금 여부와 상관없이 본다.
   0.5.3 고침: 음주·여행·오락·재물(10060505~10060805)은 첫 분기가 '그 화제를 좋아하는 성격'(好酒3·好游45·爱玩6·爱财46)이고
   가혹은 그 다음 else 라, 그 성격이 있으면 가혹이어도 좋은 반응이 나온다(곽가 = 가혹+好酒2, 디시 79013 신고, 2026-11-01).
   그래서 가혹+가벼운 화제는 배율을 올린 규칙이 그 네 성격 조건일 때만 칠한다. 대화 조건의 일곱째 칸(…/0/0/2/0/)은
   여섯째 칸이 0 이면 게임이 안 읽는다(BIIBOIPOFEI 9번 처리 역어셈블). */
static P F_topicType;static int relax_value=-1,harsh_value=-1;
static void resolve_relax(void){P it=0,f;
    while((f=il2cpp_class_get_fields(K_topic,&it))){P k=typeclass(f);if(!stat(f)&&field(k,"Serious")&&field(k,"Relax")){F_topicType=f;relax_value=eval(k,"Relax");}}
    logmsg("harsh relax filter: %s",F_topicType&&relax_value>=0&&harsh_value>=0?"READY":"unavailable");}
static int resolve_topics(void){P rule=0,kcond,it=0,m,args;char *tn,en[96];int matches=0;
    F_topicFactors=field(K_topic,TOPIC_FACTORS_FIELD);if(!F_topicFactors)return 0;
    tn=il2cpp_type_get_name(il2cpp_field_get_type(F_topicFactors));if(has(tn,"List<")&&lastarg(tn,en))rule=klass("",en);if(tn)il2cpp_free(tn);if(!rule)return 0;
    F_conditions=field(rule,TOPIC_CONDITION_CLASS_FIELD);if(!F_conditions)return 0;
    tn=il2cpp_type_get_name(il2cpp_field_get_type(F_conditions));matches=has(tn,"List<System.String>");if(tn)il2cpp_free(tn);if(!matches)return 0;matches=0;
    M_factor=method(rule,TOPIC_FACTOR_METHOD,2);if(!M_factor||!typename_is(il2cpp_method_get_return_type(M_factor),"System.Single")||!typename_is(il2cpp_method_get_param(M_factor,1),"System.String"))return 0;
    args=il2cpp_class_from_type(il2cpp_method_get_param(M_factor,0));hero_context=eval(args,"Hero");if(hero_context<0||!field(args,"Dialog"))return 0;
    kcond=klass("",TOPIC_CONDITION_CLASS);if(!kcond)return 0;
    while((m=il2cpp_class_get_methods(kcond,&it))){if(!eq(il2cpp_method_get_name(m),TOPIC_CONDITION_METHOD)||il2cpp_method_get_param_count(m)!=4)continue;
        if(il2cpp_class_from_type(il2cpp_method_get_param(m,0))!=typeclass(F_conditions)||il2cpp_class_from_type(il2cpp_method_get_param(m,1))!=args)continue;
        if(!typename_is(il2cpp_method_get_return_type(m),"System.Boolean")||!typename_is(il2cpp_method_get_param(m,2),"System.String")||!typename_is(il2cpp_method_get_param(m,3),"System.String"))continue;
        M_condition=m;matches++;}
    if(matches!=1)return 0;empty_string=il2cpp_string_new("");empty_handle=il2cpp_gchandle_new(empty_string,0);return !!empty_handle;
}
static int resolve(void){
    uptr n=0,i;P *as=il2cpp_domain_get_assemblies(il2cpp_domain_get(),&n);
    P ku,kr,ks,ki,kbag,m,it,f,kpref=0,kequipenum=0;int nt=0;
    for(i=0;i<n&&nimages<512;i++){P im=il2cpp_assembly_get_image(as[i]);images[nimages++]=im;if(eq(il2cpp_image_get_name(im),"Assembly-CSharp.dll"))game_image=im;}
    ku=klass("","UI_SelectItemUI");kr=klass("","UI_SelectItemRowUI");ks=klass("","UI_FixedScrollRect");ki=klass("","InteractOptionUI");kbag=klass("","UI_BagUI");
    if(!ku||!kr||!ks||!ki||!kbag||!game_image)return 0;
    F_self=bytype(ku,"UI_SelectItemUI",1);F_intent=field(ku,"m_selItemIntension");F_shown=field(ku,"m_listItemShow");F_rows=bytype(ku,"List<UI_SelectItemRowUI>",0);
    F_fixed=field(ku,"m_fixedRect");F_start=field(ks,"m_startIndex");F_end=field(ks,"m_endIndex");F_cols=field(kr,"m_colContent");F_index=field(kr,"m_index");
    F_npc=field(ki,"m_npcID");F_interacting=field(ki,"m_isInteracting");F_interaction=field(ki,"m_interactionUI");
    m=method(kbag,"OpenUI",1);K_hero=m?il2cpp_class_from_type(il2cpp_method_get_param(m,0)):0;
    m=method(ku,"IsEquipmentMeetCondition",2);K_equipment=m?il2cpp_class_from_type(il2cpp_method_get_param(m,0)):0;
    m=method(ku,"IsBookMeetCondition",2);K_book=m?il2cpp_class_from_type(il2cpp_method_get_param(m,0)):0;
    m=method(ku,"IsDrugMeetCondition",2);K_drug=m?il2cpp_class_from_type(il2cpp_method_get_param(m,0)):0;
    m=method(ku,"IsTopicMeetCondition",2);K_topic=m?il2cpp_class_from_type(il2cpp_method_get_param(m,0)):0;
    if(!K_hero||!K_equipment||!K_book||!K_drug)return 0;
    it=0;while((f=il2cpp_class_get_fields(K_hero,&it))){char *tn,en[96];P el,jt=0,q,idf=0,df=0;if(stat(f))continue;tn=il2cpp_type_get_name(il2cpp_field_get_type(f));el=has(tn,"List<")&&lastarg(tn,en)?klass("",en):0;if(tn)il2cpp_free(tn);
        while(el&&(q=il2cpp_class_get_fields(el,&jt))){P ek=typeclass(q);if(stat(q))continue;if(field(ek,"LikeBook")&&field(ek,"LikeCloth")){idf=q;kpref=ek;}if(field(ek,"Light")&&field(ek,"Severe"))df=q;}
        if(idf&&df){F_traits=f;F_traitType=idf;F_traitDegree=df;nt++;}}
    if(nt!=1)return 0;
    load_settings();
    F_revealed=field(K_hero,"PKONJIKGOML");
    if(F_revealed){char *tn=il2cpp_type_get_name(il2cpp_field_get_type(F_revealed));if(stat(F_revealed)||!has(tn,"List<System.Int32>"))F_revealed=0;if(tn)il2cpp_free(tn);}
    logmsg("reveal list: %s",F_revealed?"found (hero unlocked trait slots)":only_revealed?"NOT FOUND — OnlyRevealed: no gift preference is shown":"not found (unused, OnlyRevealed=0)");
    for(i=0;i<PREF_COUNT;i++)if((pref_values[i]=eval(kpref,pref_names[i]))<0)return 0;
    harsh_value=eval(kpref,"Harsh");
    it=0;while((f=il2cpp_class_get_fields(K_equipment,&it))){P k=typeclass(f);if(!stat(f)&&field(k,"Weapon")&&field(k,"Shield")){F_equipKind=f;kequipenum=k;}}
    if(!kequipenum)return 0;for(i=0;i<8;i++)if((equip_values[i]=eval(kequipenum,equip_names[i]))<0)return 0;
    /* detailType is the next int after equipType in the loaded metadata. Validate unique and int. */
    it=0;while((f=il2cpp_class_get_fields(K_equipment,&it))){char *tn=il2cpp_type_get_name(il2cpp_field_get_type(f));if(!stat(f)&&il2cpp_field_get_offset(f)==il2cpp_field_get_offset(F_equipKind)+4&&eq(tn,"System.Int32"))F_detail=f;if(tn)il2cpp_free(tn);}
    it=0;while((f=il2cpp_class_get_fields(K_drug,&it))){P k=typeclass(f);if(!stat(f)&&field(k,"Wine")&&field(k,"Medicine")){F_drugKind=f;wine_value=eval(k,"Wine");}}
    /* Find the live data manager by its equipment/book/drug dictionaries, then its hero maps. */
    for(i=0;i<il2cpp_image_get_class_count(game_image);i++){
        P k=il2cpp_image_get_class(game_image,i),jt=0,q;int e=0,b=0,d=0;P candidates[8];int nc=0;
        while((q=il2cpp_class_get_fields(k,&jt))){char *tn,en[96];if(!stat(q))continue;tn=il2cpp_type_get_name(il2cpp_field_get_type(q));if(has(tn,"Dictionary<System.String,")&&lastarg(tn,en)){if(eq(en,il2cpp_class_get_name(K_equipment)))e++;if(eq(en,il2cpp_class_get_name(K_book)))b++;if(eq(en,il2cpp_class_get_name(K_drug)))d++;if(eq(en,il2cpp_class_get_name(K_hero))&&nc<8)candidates[nc++]=q;}if(tn)il2cpp_free(tn);}
        if(e==1&&b==1&&d==1&&nc){if(ndicts)return 0;for(int j=0;j<nc;j++)hero_dicts[ndicts++]=candidates[j];}}
    M_active=method(klass("UnityEngine","GameObject"),"get_activeInHierarchy",0);M_getGO=method(klass("UnityEngine","Component"),"get_gameObject",0);
    M_getColor=method(klass("UnityEngine.UI","Graphic"),"get_color",0);M_setColor=method(klass("UnityEngine.UI","Graphic"),"set_color",1);M_getText=method(klass("UnityEngine.UI","Text"),"get_text",0);
    logmsg("resolve: UI=%d rows=%d traits=%d maps=%d itemTypes=%d",!!F_self,!!F_rows,nt,ndicts,!!F_detail&&!!F_drugKind);
    if(!F_self||!F_intent||!F_shown||!F_rows||!F_fixed||!F_start||!F_end||!F_cols||!F_index||!F_npc||!F_interacting||!F_interaction||!F_detail||!F_drugKind||!ndicts||!M_active||!M_getGO||!M_getColor||!M_setColor||!M_getText)return 0;
    {P kg=klass("UnityEngine","GameObject"),args[1],ctor=method(kg,".ctor",1),keep=method(klass("UnityEngine","Object"),"DontDestroyOnLoad",1);if(!ctor||!keep)return 0;sentinel=il2cpp_object_new(kg);args[0]=il2cpp_string_new("LOH_GiftHighlight_Lifetime");invoke(ctor,sentinel,args);if(!alive(sentinel))return 0;args[0]=sentinel;invoke(keep,0,args);sentinel_handle=il2cpp_gchandle_new(sentinel,0);}
    topic_ready=K_topic&&resolve_topics();if(topic_ready)resolve_relax();logmsg("topic rules: %s",topic_ready?"READY — native condition and factor evaluators":"unavailable, gift highlight only");
    return errors==0;
}
static P find_hero(P id){int i;for(i=0;i<ndicts;i++){P d=sget(hero_dicts[i]),result=0,args[2],r;if(!d)continue;if(!M_tryHero)M_tryHero=method(il2cpp_object_get_class(d),"TryGetValue",2);args[0]=id;args[1]=&result;r=invoke(M_tryHero,d,args);if(r&&*(u8*)il2cpp_object_unbox(r)&&result&&il2cpp_object_get_class(result)==K_hero)return result;}return 0;}
static unsigned preferences(P hero){P l=get(hero,F_traits);unsigned mask=0;int n=length(l),i,j;if(n>256)return 0;for(i=0;i<n;i++){P t=at(l,i);int id=geti(t,F_traitType);if(!t)continue;if(only_revealed&&!revealed(hero,i))continue;for(j=0;j<PREF_COUNT;j++)if(id==pref_values[j])mask|=1u<<j;}return mask;}
static int item_pref(P item){P k;if(!item)return -1;k=il2cpp_object_get_class(item);return gift_preference(k==K_equipment?geti(item,F_equipKind):-1,k==K_equipment?geti(item,F_detail):-1,k==K_book,k==K_drug&&geti(item,F_drugKind)==wine_value,equip_values);}
typedef struct{u32 topic;int liked;} TopicCache;
static TopicCache topic_cache[128];static int ntopics;static u32 cache_time;
static void clear_topics(void){for(int i=0;i<ntopics;i++)il2cpp_gchandle_free(topic_cache[i].topic);ntopics=0;cache_time=GetTickCount();}
/* 0.5.2 OnlyRevealed 를 화제에도: 배율을 올린 규칙이 성격 조건(9/性格/성격번호/…, 원본 Topic.json 224개)을 보면
   그 성격이 '?' 칸에 있거나, 그 성격이 없는데 '?' 칸이 남아 있으면(없다는 것도 아직 모름) 칠하지 않는다.
   성격 없이 자녀 수·선악·성별만 보는 규칙(자녀·정의·사악·낭만)은 화면에서 알 수 있는 값이라 그대로 둔다(밸런스판 '?' 상태 분홍 신고, 2026-10-31). */
#define NO_TRAIT (-0x7fffffff)
/* '9/…/성격번호/…' 의 세 번째 칸. 성격 조건이 아니면 NO_TRAIT, 숫자가 아니면 NO_TRAIT-1 */
static int cond_trait(P cond){char s[192],*q=s;int k,id,neg;utf8(cond,s,sizeof s);
    if(s[0]!='9'||s[1]!='/')return NO_TRAIT;
    for(k=0;k<2&&*q;q++)if(*q=='/')k++;
    neg=*q=='-';if(neg)q++;if(*q<'0'||*q>'9')return NO_TRAIT-1;
    for(id=0;*q>='0'&&*q<='9';q++)id=id*10+*q-'0';return neg?-id:id;}
static int hidden_trait(P hero,P conds){P l=get(hero,F_traits);int n=length(l),i,j,id,found,hidden=0;
    if(n>256)return 1;for(j=0;j<n;j++)if(!revealed(hero,j)){hidden=1;break;}
    for(i=0;i<length(conds)&&i<32;i++){
        if((id=cond_trait(at(conds,i)))==NO_TRAIT)continue;
        if(id==NO_TRAIT-1)return 1;                        /* 모르는 꼴이면 가려진 것으로 본다 */
        for(found=0,j=0;j<n;j++){P t=at(l,j);if(t&&geti(t,F_traitType)==id){found=1;if(!revealed(hero,j))return 1;}}
        if(!found&&hidden)return 1;}
    return 0;}
/* 가혹보다 먼저 보는 '좋아하는 성격'(好酒3·好游45·爱玩6·爱财46) 조건이 있는 규칙인가 */
static int harsh_exempt(P conds){int i,id;
    for(i=0;i<length(conds)&&i<32;i++){id=cond_trait(at(conds,i));if(id==3||id==45||id==6||id==46)return 1;}
    return 0;}
static int revealed_count(P hero){int n=length(get(hero,F_traits)),i,c=0;if(n>256)return 0;for(i=0;i<n;i++)c+=revealed(hero,i);return c;}
static int has_trait(P hero,int value){P l=get(hero,F_traits);int n=length(l),i;if(value<0||n>256)return 0;for(i=0;i<n;i++){P t=at(l,i);if(t&&geti(t,F_traitType)==value)return 1;}return 0;}
static int topic_liked(P topic,P id,P hero,int harsh){P rules=get(topic,F_topicFactors);int i,result=0,harsh_relax;
    for(i=0;i<ntopics;i++)if(il2cpp_gchandle_get_target(topic_cache[i].topic)==topic)return topic_cache[i].liked;
    harsh_relax=harsh&&F_topicType&&relax_value>=0&&geti(topic,F_topicType)==relax_value;
    /* Same first-matching-rule semantics as the game's Topic.Use. Read only.
       This includes children, alignment and custom mod formulas automatically. */
    for(i=0;i<length(rules)&&i<64;i++){
        P rule=at(rules,i),conds=get(rule,F_conditions),a[4],r;float factor;
        if(!rule||!conds)continue;
        a[0]=conds;a[1]=&hero_context;a[2]=id;a[3]=empty_string;r=invoke(M_condition,0,a);
        if(!r||!*(u8*)il2cpp_object_unbox(r))continue;
        a[0]=&hero_context;a[1]=id;r=invoke(M_factor,rule,a);if(!r)break;
        factor=*(float*)il2cpp_object_unbox(r);result=factor>1.0001f&&!(harsh_relax&&!harsh_exempt(conds))&&!(only_revealed&&hidden_trait(hero,conds));break;
    }
    if(ntopics<128){topic_cache[ntopics].topic=il2cpp_gchandle_new(topic,0);topic_cache[ntopics++].liked=result;}return result;
}
typedef struct{float r,g,b,a;} Color;
static const Color pink={1.0f,0.47f,0.73f,1.0f};
typedef struct{u32 text,original;Color color;int changed,seen;} Mark;
static Mark marks[384];static int nmarks;
static int samecolor(Color a,Color b){return a.r==b.r&&a.g==b.g&&a.b==b.b&&a.a==b.a;}
static Color color(P t){Color c={1,1,1,1};P r=invoke(M_getColor,t,0);if(r)memcpy(&c,il2cpp_object_unbox(r),sizeof c);return c;}
static void setcolor(P t,Color c){P a[1]={&c};invoke(M_setColor,t,a);}
/* Row recycling updates text without necessarily resetting Graphic.color.
 * Undo our pink even after text changes; preserve a color changed by the game. */
static void unmark(Mark *m){P t=il2cpp_gchandle_get_target(m->text);if(m->changed&&alive(t)&&samecolor(color(t),pink))setcolor(t,m->color);il2cpp_gchandle_free(m->text);il2cpp_gchandle_free(m->original);}
static void restore(void){int i;for(i=0;i<nmarks;i++)unmark(&marks[i]);nmarks=0;}
static void tint(P text){P str;Color c;int i;Mark *m=0;if(!alive(text))return;str=invoke(M_getText,text,0);if(!str)return;c=color(text);
    for(i=0;i<nmarks;i++)if(il2cpp_gchandle_get_target(marks[i].text)==text){m=&marks[i];break;}
    if(m){m->seen=1;if(stringeq(str,il2cpp_gchandle_get_target(m->original))&&samecolor(c,pink))return;il2cpp_gchandle_free(m->original);m->original=il2cpp_gchandle_new(str,0);if(!samecolor(c,pink))m->color=c;}
    else{if(nmarks>=384)return;m=&marks[nmarks++];m->color=c;m->text=il2cpp_gchandle_new(text,0);m->original=il2cpp_gchandle_new(str,0);m->changed=1;m->seen=1;}
    setcolor(text,pink);
}
static void sweep(void){int i;for(i=nmarks-1;i>=0;i--)if(!marks[i].seen){unmark(&marks[i]);marks[i]=marks[--nmarks];}}
static u32 target_handle;static unsigned last_mask;static int was_open,last_start=-1,last_count=-1;
static void tick(void){P ui,shown,rows,scroll,id,hero;int start,end,i,likes=0,topics,changed,harsh;unsigned mask;
    if(!alive(sentinel))return;
    ui=sget(F_self);
    shown=sget(F_shown);topics=topic_ready&&show_topics&&at(shown,0)&&il2cpp_object_get_class(at(shown,0))==K_topic;
    if(!active(ui)||(sint(F_intent)!=1&&!topics)){if(nmarks)restore();if(was_open){logmsg("selection list closed");was_open=0;}if(target_handle){il2cpp_gchandle_free(target_handle);target_handle=0;}if(ntopics)clear_topics();return;}
    id=sget(F_npc);
    /* CloseUI clears m_isInteracting before gift dialogue runs. m_npcID retains
       the recipient; gift intent (1) is the scope guard, not UI visibility. */
    if(!id||*(int*)((u8*)id+0x10)<=0){if(nmarks)restore();return;}
    /* Always get current data on the main thread; no hero retained across save loads. */
    hero=find_hero(id);if(!hero){if(nmarks)restore();return;}mask=preferences(hero);
    changed=!was_open||!target_handle||!stringeq(id,il2cpp_gchandle_get_target(target_handle))||last_mask!=mask;
    if(changed||GetTickCount()-cache_time>1000)clear_topics();
    harsh=topics&&has_trait(hero,harsh_value);
    rows=get(ui,F_rows);scroll=get(ui,F_fixed);start=geti(scroll,F_start);end=geti(scroll,F_end);
    if(start<0||end<start||end>length(shown)||length(rows)>128){if(nmarks)restore();return;}
    for(i=0;i<nmarks;i++)marks[i].seen=0;
    for(i=0;i<length(rows)&&start+i<end;i++){
        P row=at(rows,i),item=at(shown,start+i),cols;
        if(!active(row)||!item)continue;
        if(!(topics?(il2cpp_object_get_class(item)==K_topic&&topic_liked(item,id,hero,harsh)):gift_liked(item_pref(item),mask)))continue;
        cols=get(row,F_cols);tint(arrayat(cols,0));tint(arrayat(cols,1));tint(arrayat(cols,2));likes++;
    }
    sweep();
    if(changed){char name[128];utf8(id,name,sizeof name);logmsg("target=%s mode=%s intent=%d preferences=0x%x harsh=%d revealed=%d/%d visibleMatches=%d",name,topics?"topic":"gift",sint(F_intent),mask,harsh,revealed_count(hero),length(get(hero,F_traits)),likes);if(target_handle)il2cpp_gchandle_free(target_handle);target_handle=il2cpp_gchandle_new(id,0);last_mask=mask;}
    if(!was_open||last_start!=start||last_count!=length(shown)){logmsg("list start=%d end=%d count=%d pinkRows=%d",start,end,length(shown),likes);last_start=start;last_count=length(shown);}
    was_open=1;
}
#include "lifecycle.inc"
