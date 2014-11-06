#include "paillier.h"

#define MAX_HABITAT_LENGTH 4096

typedef struct _coord {
        double lat, lon, alt;
} coord;

typedef struct _coordEnc {
        BIGNUM *lat, *lon, *alt;
} coordEnc;

typedef struct _habitat {
        coord F1, F2;           // Focal points
        double R;               // Radius
} habitat;

typedef struct _habitatEnc {
        coordEnc F1, F2;
        BIGNUM *R;
} habitatEnc;

typedef struct _habitatChar {
        double Fx_max, Fx_min, Fy_max, Fy_min, EE, WE;
} habitatChar;

typedef struct _habitatCharEnc {
        BIGNUM *Fx_max, *Fx_min, *Fy_max, *Fy_min, *EE, *WE;
} habitatCharEnc;

typedef habitatCharEnc habitatCharEncCmp;

double normalize_coord(const double dist, int mod);

double dist(const coord E, const coord S);
int initializeHabitat(habitat *h, const coord new_coord);
int recalculateHabitat(habitat *h, const coord new_coord, const double alpha, const double beta);
int getHabitat(habitat *h);
int setHabitat(const habitat *h);

int decryptHabitatCharEnc(/*out*/ habitatChar *hc, const habitatCharEnc *hc_enc, const privKey *key, BN_CTX *ctx);