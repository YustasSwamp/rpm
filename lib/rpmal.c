/** \ingroup rpmdep
 * \file lib/rpmal.c
 */

#include "system.h"


#include <rpm/rpmds.h>
#include <rpm/rpmte.h>
#include <rpm/rpmfi.h>
#include <rpm/rpmstrpool.h>

#include "lib/rpmal.h"
#include "lib/misc.h"
#include "lib/rpmte_internal.h"

#include "debug.h"

typedef struct availablePackage_s * availablePackage;
typedef int rpmalNum;

/** \ingroup rpmdep
 * Info about a single package to be installed.
 */
struct availablePackage_s {
    rpmte p;                    /*!< transaction member */
    rpmds provides;		/*!< Provides: dependencies. */
    rpmfi fi;			/*!< File info set. */
};

/** \ingroup rpmdep
 * A single available item (e.g. a Provides: dependency).
 */
typedef struct availableIndexEntry_s {
    rpmalNum pkgNum;	        /*!< Containing package index. */
    unsigned int entryIx;	/*!< Dependency index. */
} * availableIndexEntry;

struct fileNameEntry_s {
    rpmsid dirName;
    rpmsid baseName;
};

#undef HASHTYPE
#undef HTKEYTYPE
#undef HTDATATYPE
#define HASHTYPE rpmalProvidesHash
#define HTKEYTYPE rpmsid
#define HTDATATYPE struct availableIndexEntry_s
#include "lib/rpmhash.H"
#include "lib/rpmhash.C"

#undef HASHTYPE
#undef HTKEYTYPE
#undef HTDATATYPE
#define HASHTYPE rpmalFileHash
#define HTKEYTYPE struct fileNameEntry_s
#define HTDATATYPE struct availableIndexEntry_s
#include "lib/rpmhash.H"
#include "lib/rpmhash.C"

/** \ingroup rpmdep
 * Set of available packages, items, and directories.
 */
struct rpmal_s {
    rpmstrPool pool;		/*!< String pool */
    availablePackage list;	/*!< Set of packages. */
    rpmalProvidesHash providesHash;
    rpmalFileHash fileHash;
    int delta;			/*!< Delta for pkg list reallocation. */
    int size;			/*!< No. of pkgs in list. */
    int alloced;		/*!< No. of pkgs allocated for list. */
    rpmtransFlags tsflags;	/*!< Transaction control flags. */
    rpm_color_t tscolor;	/*!< Transaction color. */
    rpm_color_t prefcolor;	/*!< Transaction preferred color. */
};

/**
 * Destroy available item index.
 * @param al		available list
 */
static void rpmalFreeIndex(rpmal al)
{
    al->providesHash = rpmalProvidesHashFree(al->providesHash);
    al->fileHash = rpmalFileHashFree(al->fileHash);
}

rpmal rpmalCreate(rpmstrPool pool, int delta, rpmtransFlags tsflags,
		  rpm_color_t tscolor, rpm_color_t prefcolor)
{
    rpmal al = xcalloc(1, sizeof(*al));

    /* transition time safe-guard */
    assert(pool != NULL);

    al->pool = rpmstrPoolLink(pool);
    al->delta = delta;
    al->size = 0;
    al->alloced = al->delta;
    al->list = xmalloc(sizeof(*al->list) * al->alloced);;

    al->providesHash = NULL;
    al->fileHash = NULL;
    al->tsflags = tsflags;
    al->tscolor = tscolor;
    al->prefcolor = prefcolor;

    return al;
}

rpmal rpmalFree(rpmal al)
{
    availablePackage alp;
    int i;

    if (al == NULL)
	return NULL;

    if ((alp = al->list) != NULL)
    for (i = 0; i < al->size; i++, alp++) {
	alp->provides = rpmdsFree(alp->provides);
	alp->fi = rpmfiFree(alp->fi);
    }
    al->pool = rpmstrPoolFree(al->pool);
    al->list = _free(al->list);
    al->alloced = 0;

    rpmalFreeIndex(al);
    al = _free(al);
    return NULL;
}

static unsigned int sidHash(rpmsid sid)
{
    return sid;
}

static int sidCmp(rpmsid a, rpmsid b)
{
    return (a != b);
}

static unsigned int fileHash(struct fileNameEntry_s file)
{
    return file.dirName ^ file.baseName;
}

static int fileCompare(struct fileNameEntry_s one, struct fileNameEntry_s two)
{
    int rc = (one.dirName != two.dirName);;
    if (!rc)
	rc = (one.baseName != two.baseName);
    return rc;
}

void rpmalDel(rpmal al, rpmte p)
{
    availablePackage alp;
    rpmalNum pkgNum;

    if (al == NULL || al->list == NULL)
	return;		/* XXX can't happen */

    // XXX use a search for self provide
    for (pkgNum=0; pkgNum<al->size; pkgNum++) {
	if (al->list[pkgNum].p == p) {
	    break;
	}
    }
    if (pkgNum == al->size ) return; // Not found!

    alp = al->list + pkgNum;
    // do not actually delete, just set p to NULL
    // and later filter that out of the results
    alp->p = NULL;
}

static void rpmalAddFiles(rpmal al, rpmalNum pkgNum, rpmfi fi)
{
    struct fileNameEntry_s fileName;
    struct availableIndexEntry_s fileEntry;
    int i;
    rpm_color_t ficolor;
    int skipdoc = (al->tsflags & RPMTRANS_FLAG_NODOCS);
    int skipconf = (al->tsflags & RPMTRANS_FLAG_NOCONFIGS);

    fileEntry.pkgNum = pkgNum;

    fi = rpmfiInit(fi, 0);
    while ((i = rpmfiNext(fi)) >= 0) {
	/* Ignore colored provides not in our rainbow. */
        ficolor = rpmfiFColor(fi);
        if (al->tscolor && ficolor && !(al->tscolor & ficolor))
            continue;

	/* Ignore files that wont be installed */
	if (skipdoc && (rpmfiFFlags(fi) & RPMFILE_DOC))
	    continue;
	if (skipconf && (rpmfiFFlags(fi) & RPMFILE_CONFIG))
	    continue;

	fileName.dirName = rpmfiDNId(fi);
	fileName.baseName = rpmfiBNId(fi);

	fileEntry.entryIx = i;

	rpmalFileHashAddEntry(al->fileHash, fileName, fileEntry);
    }
}

static void rpmalAddProvides(rpmal al, rpmalNum pkgNum, rpmds provides)
{
    struct availableIndexEntry_s indexEntry;
    rpm_color_t dscolor;
    int skipconf = (al->tsflags & RPMTRANS_FLAG_NOCONFIGS);

    indexEntry.pkgNum = pkgNum;

    if (rpmdsInit(provides) != NULL)
    while (rpmdsNext(provides) >= 0) {
        /* Ignore colored provides not in our rainbow. */
        dscolor = rpmdsColor(provides);
        if (al->tscolor && dscolor && !(al->tscolor & dscolor))
            continue;

	/* Ignore config() provides if the files wont be installed */
	if (skipconf & (rpmdsFlags(provides) & RPMSENSE_CONFIG))
	    continue;

	indexEntry.entryIx = rpmdsIx(provides);
	rpmalProvidesHashAddEntry(al->providesHash, rpmdsNId(provides), indexEntry);
    }
}

void rpmalAdd(rpmal al, rpmte p)
{
    rpmalNum pkgNum;
    availablePackage alp;

    if (al->size == al->alloced) {
	al->alloced += al->delta;
	al->list = xrealloc(al->list, sizeof(*al->list) * al->alloced);
    }
    pkgNum = al->size++;

    alp = al->list + pkgNum;

    alp->p = p;

    alp->provides = rpmdsLink(rpmteDS(p, RPMTAG_PROVIDENAME));
    alp->fi = rpmfiLink(rpmteFI(p));

    /*
     * Transition-time safe-guard to catch private-pool uses.
     * File sets with no files have NULL pool, that's fine. But WTF is up
     * with the provides: every single package should have at least its
     * own name as a provide, and thus never NULL, and normal use matches
     * this expectation. However the test-suite is tripping up on NULL
     * NULL pool from NULL alp->provides in numerous cases?
     */
    {
	rpmstrPool fipool = rpmfiPool(alp->fi);
	rpmstrPool dspool = rpmdsPool(alp->provides);
	
	assert(fipool == NULL || fipool == al->pool);
	assert(dspool == NULL || dspool == al->pool);
    }

    if (al->providesHash != NULL) { // index is already created
	rpmalAddProvides(al, pkgNum, alp->provides);
	rpmalAddFiles(al, pkgNum, alp->fi);
    }

    assert(((rpmalNum)(alp - al->list)) == pkgNum);
}

static void rpmalMakeIndex(rpmal al)
{
    availablePackage alp;
    int i;
    int providesCnt = 0;
    int fileCnt = 0;

    if (al == NULL || al->list == NULL) return;
    if (al->providesHash != NULL || al->fileHash != NULL)
	return;
    for (i = 0; i < al->size; i++) {
	alp = al->list + i;
	if (alp->provides != NULL)
	    providesCnt += rpmdsCount(alp->provides);
	if (alp->fi != NULL)
	    fileCnt += rpmfiFC(alp->fi);
    }

    al->providesHash = rpmalProvidesHashCreate(providesCnt/4+128, sidHash,
					       sidCmp, NULL, NULL);
    al->fileHash = rpmalFileHashCreate(fileCnt/4+128, fileHash, fileCompare,
				       NULL, NULL);

    for (i = 0; i < al->size; i++) {
	alp = al->list + i;
	rpmalAddProvides(al, i, alp->provides);
	rpmalAddFiles(al, i, alp->fi);
    }
}

static rpmte * rpmalAllFileSatisfiesDepend(const rpmal al, const rpmds ds)
{
    rpmsid fnId = rpmdsNId(ds);
    const char *fileName = rpmstrPoolStr(al->pool, fnId);
    const char *slash; 
    rpmte * ret = NULL;

    if (al == NULL || fileName == NULL || *fileName != '/')
	return NULL;

    /* Split path into dirname and basename components for lookup */
    if ((slash = strrchr(fileName, '/')) != NULL) {
	availableIndexEntry result;
	int resultCnt = 0;
	size_t bnStart = (slash - fileName) + 1;
	struct fileNameEntry_s fne;

	fne.baseName = rpmstrPoolId(al->pool, fileName + bnStart, 0);
	fne.dirName = rpmstrPoolIdn(al->pool, fileName, bnStart, 0);

	rpmalFileHashGetEntry(al->fileHash, fne, &result, &resultCnt, NULL);

	if (resultCnt > 0) {
	    int i, found;
	    ret = xmalloc((resultCnt+1) * sizeof(*ret));

	    for (found = i = 0; i < resultCnt; i++) {
		availablePackage alp = al->list + result[i].pkgNum;
		if (alp->p == NULL) // deleted
		    continue;

		rpmdsNotify(ds, "(added files)", 0);

		ret[found] = alp->p;
		found++;
	    }
	    ret[found] = NULL;
	}
    }

    return ret;
}

static rpmte * rpmalAllSatisfiesDepend(const rpmal al, const rpmds ds)
{
    rpmte * ret = NULL;
    int i, found;
    rpmsid nameId;
    const char *name;
    availableIndexEntry result;
    int resultCnt;
    int obsolete;

    availablePackage alp;
    int rc;

    if (al == NULL || ds == NULL || (nameId = rpmdsNId(ds)) == 0)
	return ret;

    if (al->providesHash == NULL && al->fileHash == NULL)
	rpmalMakeIndex(al);

    obsolete = (rpmdsTagN(ds) == RPMTAG_OBSOLETENAME);
    name = rpmstrPoolStr(al->pool, nameId);
    if (!obsolete && *name == '/') {
	/* First, look for files "contained" in package ... */
	ret = rpmalAllFileSatisfiesDepend(al, ds);
	if (ret != NULL && *ret != NULL)
	    return ret;
	/* ... then, look for files "provided" by package. */
	ret = _free(ret);
    }

    rpmalProvidesHashGetEntry(al->providesHash, nameId, &result,
			      &resultCnt, NULL);

    if (resultCnt==0) return NULL;

    ret = xmalloc((resultCnt+1) * sizeof(*ret));

    for (found=i=0; i<resultCnt; i++) {
	alp = al->list + result[i].pkgNum;
	if (alp->p == NULL) // deleted
	    continue;
	(void) rpmdsSetIx(alp->provides, result[i].entryIx);

	/* Obsoletes are on package name, filter out other provide matches */
	if (obsolete && !rstreq(rpmdsN(alp->provides), rpmteN(alp->p)))
	    continue;

	rc = 0;
	if (rpmdsIx(alp->provides) >= 0)
	    rc = rpmdsCompare(alp->provides, ds);

	if (rc) {
	    rpmdsNotify(ds, "(added provide)", 0);
	    ret[found] = alp->p;
	    found++;
	}
    }
    ret[found] = NULL;

    return ret;
}

rpmte
rpmalSatisfiesDepend(const rpmal al, const rpmds ds)
{
    rpmte *providers = rpmalAllSatisfiesDepend(al, ds);
    rpmte best = NULL;

    if (providers) {
	if (al->tscolor) {
	    /*
	     * For colored dependencies, try to find a matching provider.
	     * Otherwise prefer provider of ts preferred color.
	     */
	    rpm_color_t dscolor = rpmdsColor(ds);
	    for (rpmte *p = providers; *p; p++) {
		rpm_color_t tecolor = rpmteColor(*p);
		if (dscolor) {
		    if (dscolor == tecolor) best = *p;
		} else if (al->prefcolor) {
		    if (al->prefcolor == tecolor) best = *p;
		}
		if (best) break;
	    }
	}
	/* if not decided by now, just pick first match */
	if (!best) best = providers[0];
	free(providers);
    }
    return best;
}

rpmte *
rpmalAllInCollection(const rpmal al, const char *collname)
{
    rpmte *ret = NULL;
    int found = 0;
    rpmalNum pkgNum;

    if (!al || !al->list || !collname)
	return NULL;

    for (pkgNum = 0; pkgNum < al->size; pkgNum++) {
	rpmte p = al->list[pkgNum].p;
	if (rpmteHasCollection(p, collname)) {
	    ret = xrealloc(ret, sizeof(*ret) * (found + 1 + 1));
	    ret[found] = p;
	    found++;
	}
    }
    if (ret) {
	ret[found] = NULL;
    }

    return ret;
}
