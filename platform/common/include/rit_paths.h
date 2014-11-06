#ifndef INC_COMMON_RIT_PATHS
#define INC_COMMON_RIT_PATHS

#define MAX_DESTS 1000

#define THIS_NODE "/thisNode"
#define BUNDLES THIS_NODE"/bundles"

#define GPS_BRANCH_LATITUDE THIS_NODE"/GPS/latitude"
#define GPS_BRANCH_LONGITUDE THIS_NODE"/GPS/longitude"
#define GPS_BRANCH_ALTITUDE THIS_NODE"/GPS/altitude"
#define GPS_BRANCH_TIME THIS_NODE"/GPS/timestamp"

#define HABITAT_BRANCH THIS_NODE"/hlra/habitat"
#define HABITAT_CHAR_BRANCH THIS_NODE"/hlra/habitatChar"

#define NBS_BRANCH "/nbs"
#define NBS_HABITAT_CHAR_ENC "habitatCharEnc"

// Format BUNDLES_DESTS/{0..999}/{lat,lon,alt}
#define BUNDLES_DESTS BUNDLES"/dests"
#define BUNDLE_DESTS_NUM BUNDLES_DESTS"/num"

#define NBS_INFO "/nbs"
#define ANNOUNCED_INFO "/announced"
#define LOCAL_INFO "/local"
#define MAX_RIT_PATH 2048

#endif