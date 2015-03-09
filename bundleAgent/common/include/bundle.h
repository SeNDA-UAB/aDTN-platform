/*
* Copyright (c) 2014 SeNDA
* 
* Licensed under the Apache License, Version 2.0 (the "License");
* you may not use this file except in compliance with the License.
* You may obtain a copy of the License at
* 
*     http://www.apache.org/licenses/LICENSE-2.0
* 
* Unless required by applicable law or agreed to in writing, software
* distributed under the License is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
* See the License for the specific language governing permissions and
* limitations under the License.
* 
*/

/*
    TODO: Functions to add EID-refs (at the moment no extension uses them)
    TODO: Fragmentation is right now ignored.
 */
#ifndef INC_COMMON_BUNDLE
#define INC_COMMON_BUNDLE

#include <stdint.h>
#include <stdio.h>
#include "constants.h"

#define CHECK_BIT(var,pos) ((var) & (1<<(pos)))

/* Administrative records */
typedef enum {
	SR_RECV    = 0x01,                      // Reporting node received bundle.
	SR_CACC    = 0x02,                      // Reporting node accepted custody of bundle.
	SR_FORW    = 0x04,                      // Reporting node forwarded the bundle. 
	SR_DELI    = 0x08,                      // Reporting node delivered the bundle. 
	SR_DEL     = 0x10,                      // Reporting node deleted the bundle.
} sr_status_flags_e;

typedef enum {
	RC_NO_ADD_INFO          = 0x0,          // No additional information.
	RC_LIFE_EXPIRED         = 0x1,          // Lifetime expired.
	RC_UNI_FORW             = 0x2,          // Forwarded over unidirectional link.
	RC_TRANS_CANCEL         = 0x3,          // Transmission canceled
	RC_DEPELTED_STOR        = 0x4,          // Depleted storage.
	RC_DEST_END_UNIN        = 0x5,          // Destination endpoint ID unintelligible.
	RC_NO_ROUTE_DEST        = 0x6,          // No known route to destination from here.
	RC_NO_TIMElY_CONT       = 0x7,          // No timely contact with next node on route.
	RC_BLOCK_UNIN           = 0x8           // Block unintelligible.
} sr_reason_codes_e;

typedef struct _bundle_sr {
	uint8_t status_flags;
	uint8_t reason_codes;
	int64_t fragment_offset;            //SDNV
	int64_t fragment_length;            //SDNV
	int64_t sec_time_of_receipt;        //SDNV
	int64_t usec_time_of_receipt;       //SDNV

	int64_t sec_time_of_custody;        //SDNV
	int64_t usec_time_of_custody;       //SDNV

	int64_t sec_time_of_forwarding;     //SDNV
	int64_t usec_time_of_forwarding;    //SDNV

	int64_t sec_time_of_delivery;       //SDNV
	int64_t usec_time_of_delivery;      //SDNV

	int64_t sec_time_of_deletion;       //SDNV
	int64_t usec_time_of_deletion;      //SDNV

	int64_t cp_creation_timestamp;      //SDNV
	int64_t cp_creation_ts_seq_num;     //SDNV
	int64_t source_EID_len;             //SDNV
	char *source_EID;
} status_report_s;

union _ar_body {
	/* Status report */
	status_report_s *sr;
	/* TODO: custody signal */
};

typedef enum {
	AR_SR = 0x01,
	AR_CS = 0x02
} ar_type_e;

typedef enum {
	AR_FRAGMENT = 0x01
} ar_flags_s;

typedef struct _adm_record_s {
	uint8_t type: 4;
	uint8_t flags: 4;
	union _ar_body body;
} adm_record_s;

/**/

/** Extension blocks **/

typedef enum {
	ROUTING_CODE = 0x01,
	PRIO_CODE = 0x02,
	LIFE_CODE = 0x03,
	DEST_CODE = 0x04,
	DATA = 0x05
} code_type_e;

__attribute__ ((unused)) static char *code_type_e_name[] = {
	"",
	"ROUTING_CODE",
	"PRIO_CODE",
	"LIFE_CODE",
	"DEST_CODE",
	"DATA"
};

typedef enum {
	PAYL_B = 0x01,
	PRIM_B = 0x06,
	META_B = 0x08,
} block_type_e;

typedef enum {
	MMEB_B = 0x10
} meta_block_type;

/* Metadata extension blocks */

// Mobile-code Metadata Extension Block (SeNDA)
typedef struct _mmeb_code_s {
	uint16_t fwk;
	uint64_t sw_length;               // SDNV
	uint8_t *sw_code;
	struct _mmeb_code_s *next;
} mmeb_code_s;

typedef struct _mmeb_body_s {
	uint16_t alg_type;              //
	uint64_t alg_length;            // SDNV
	mmeb_code_s *code;
	struct _mmeb_body_s *next;
} mmeb_body_s;

union _meta_block {
	// Mobile-code Metadata Extension Block (SeNDA)
	mmeb_body_s *mmeb;
	// Metadata extension block RFC6258
	uint8_t *metadata;
};

/* Metadata extension block RFC6258 */
typedef struct _meb_body_s {
	uint64_t type;
	int metadata_l;
	union _meta_block metadata;                             // Links to a different struct depending of the meta_type.
} meb_body_s;
/**/

/* Payload extension block RFC5050 */
typedef struct _payload_body_s {
	uint8_t *payload;
} payload_body_s;
/**/

/****/

/* Canonical extension block */

// Suported extension blocks
union _ext_block {
	/* Payload extension block RFC5050 */
	payload_body_s *payload;
	/* Metadata extension block RFC6258*/
	meb_body_s *meb;
};

// Block proc. flags for any other bundle block (RFC5050)
typedef enum {
	B_REP_FR = 0x01,                        // Must be replicated in each fragment
	B_ERR_NP = 0x02,                        // Transmit status error if block can't be processed
	B_DEL_NP = 0x04,                        // Delete Bundle if block can't be processed
	B_LAST   = 0x08,                        // Is the last block
	B_DIS_NP = 0x10,                        // Discard block if it can't be processed
	B_WFW_NP = 0x20,                        // Block was forwarded without being processed
	B_EID_RE = 0x40                         // Block contains EID-reference field
} block_flags_t;

// EID references struct (RFC5050 under Canonical Block format)
typedef struct _eid_ref_s {
	uint64_t scheme;
	uint64_t ssp;
	struct _eid_ref_s *next;
} eid_ref_s;

typedef struct _ext_block_s {
	uint8_t type;
	uint64_t proc_flags: 7;                  // SDNV
	uint64_t EID_ref_count;                  // SDNV
	eid_ref_s *eid_ref;
	uint64_t length;                         // SDNV
	union _ext_block body;
	struct _ext_block_s *next;
} ext_block_s;

typedef ext_block_s payload_block_s;
typedef ext_block_s meb_s;
typedef ext_block_s mmeb_s;

/**/

/* Primary Block RFC5050 */
// Primary proc. flags for Primary Bundle Block header
typedef enum {
	H_FRAG = 0x01,                      // Bundle is a fragment
	H_ADMR = 0x02,                      // Administrative record
	H_NOTF = 0x04,                      // Must not be fragmented
	H_CSTR = 0x08,                      // Custody transfer requested
	H_DESS = 0x10,                      // Destination is singleton
	H_ACKR = 0x20,                      // Acknowledgment is requested

	H_COS_BULK = 0x80,                  // bundle’s priority == bulk
	H_COS_NORM = 0x100,                 // bundle’s priority == normal
	H_COS_EXP  = 0x200,                 // bundle’s priority == expedited

	H_SR_BREC = 0x4000,                 // Request reporting of bundle reception
	H_SR_CACC = 0x8000,                 // Request reporting of custody acceptance
	H_SR_BFRW = 0x10000,                // Request reporting of bundle forwarding
	H_SR_BDEL = 0x20000,                // Request reporting of bundle delivery
	H_SR_BDLT = 0x40000                 // Request reporting of bundle deletion
} primary_flag_e;

typedef enum {
	DEST_SCHEME,
	DEST_SSP,
	SOURCE_SCHEME,
	SOURCE_SSP,
	REPORT_SCHEME,
	REPORT_SSP,
	CUST_SCHEME,
	CUST_SSP
} primary_field_e;

typedef struct _primary_block_s {
	uint8_t version;                         // 0x06
	uint64_t proc_flags: 20;                 // SDNV
	uint64_t length;                         // SDNV
	uint64_t dest_scheme_offset;             // SDNV
	uint64_t dest_ssp_offset;                // SDNV
	uint64_t source_scheme_offset;           // SDNV
	uint64_t source_ssp_offset;              // SDNV
	uint64_t report_scheme_offset;           // SDNV
	uint64_t report_ssp_offset;              // SDNV
	uint64_t cust_scheme_offset;             // SDNV
	uint64_t cust_ssp_offset;                // SDNV
	uint64_t timestamp_time;                 // SDNV
	uint64_t timestamp_seq;                  // SDNV
	uint64_t lifetime;                       // SDNV
	uint64_t dict_length;                    // SDNV
	char *dict;
	/* Optionals. Only if fragmentation is enabled in proc_flgas */
	uint64_t fragment_offset;
	uint64_t total_length;
	/**/
} primary_block_s;
/**/

/* Bundle */
typedef struct _bundle_s {
	primary_block_s *primary;
	ext_block_s *ext;                             // All blocks must have a next pointer to the next block. At least there should be a payload block.
} bundle_s;
/**/

/************* Bundle creation *************/

bundle_s *bundle_new(); // Initializes a new bundle and a primary block with default values
int bundle_free_mmeb(mmeb_body_s *mmeb);
int bundle_free(bundle_s *b);

int bundle_set_proc_flags(bundle_s *bundle, uint64_t flags);
int bundle_set_lifetime(bundle_s *bundle, uint64_t lifetime);

// static int bundle_set_dict_offset(bundle_s *bundle, primary_field_e field, uint64_t offset);
// static int bundle_get_dict_offset(bundle_s *bundle, primary_field_e field); // Returns entry offset or < 0 on error.
// static int bundle_update_primary_offsets(bundle_s *bundle, uint64_t removed_entry_offset, int removed_entry_l);
// static int bundle_remove_dict_entry(bundle_s *bundle, uint64_t removed_entry_offset);
// static int bundle_add_dict_entry(bundle_s *bundle, const char *new_dict_entry); // Returns entry offset or < 0 on error.
int bundle_set_primary_entry(bundle_s *bundle, primary_field_e field, const char *new_entry);
int bundle_remove_primary_entry(bundle_s *bundle, primary_field_e field);

payload_block_s *bundle_new_payload_block();
int bundle_set_payload(payload_block_s *block, uint8_t *payload, int payload_l);

meb_s *bundle_new_meb();
int bundle_set_metadata(meb_s *block, uint64_t meta_type, uint8_t *metadata, int metadata_l);

mmeb_s *bundle_new_mmeb();
int bundle_add_mmeb_code(mmeb_s *block, uint16_t alg_type, uint16_t fwk, size_t sw_length, uint8_t *sw_code);

// General block functions (meb_s, mmeb_s and payload_block_s can be cast to ext_block_s)
int bundle_set_ext_block_flags(ext_block_s *ext_block, uint8_t flags);
int bundle_add_ext_block(bundle_s *bundle, ext_block_s *ext_block);

// Return raw lenght
// static int bundle_primary_raw(primary_block_s *primary_block, uint8_t **raw);

// static int bundle_ext_block_header_raw(ext_block_s *ext_block, uint8_t **raw);
// static int bundle_payload_raw(payload_block_s *block, uint8_t **raw);
// static int bundle_meb_raw(meb_s *block, uint8_t **raw);
// static int bundle_mmeb_raw(mmeb_s *block, uint8_t **raw);
int bundle_create_raw(const bundle_s *bundle, /*out*/uint8_t **bundle_raw); // Returns bundle length or <=0 on error

/**************************/

/************* Bundle status reports creation *************/
adm_record_s *bundle_ar_new(ar_type_e type);
int bundle_ar_free(adm_record_s *ar);
int bundle_ar_raw(adm_record_s *ar, /*out*/uint8_t **ar_raw );

bundle_s *bundle_new_sr(
	const sr_status_flags_e sr_status_flag, const uint8_t reason_codes, 
	const char *source_eid, struct timeval reception_time, const uint8_t *orig_bundle_raw);

int ar_sr_extract_cp_timestamp(uint8_t *ar,  uint64_t *timestamp);
int ar_sr_extract_time_of(uint8_t *ar,  struct timeval *tv);

/**************************/


/************* Bundle procesing *************/

int bundle_raw_check(const uint8_t *bundle_raw, int length);
// static int bundle_raw_check_block_requirements(const uint8_t *bundle_raw, int last_block);
// Jump between blocks
int bundle_raw_ext_is_last_block(const uint8_t *raw);
int bundle_raw_next_block_off(const uint8_t *raw);
int bundle_raw_find_block_off(const uint8_t *raw, const uint8_t block_id);

// static inline int bundle_raw_get_sdnv_off(const uint8_t *raw, const int field_num);
// static inline int bundle_raw_get_body_off(const uint8_t *raw, /*out*/ int *ext_block_body_off);
// static inline int bundle_raw_ext_get_block_body(const uint8_t *bundle_raw, block_type_e block_type, /*out*/uint8_t **body);

// Get/set primary and extension block flags
int bundle_raw_get_proc_flags(const uint8_t *primary_raw, /*out*/uint64_t *flags);
int bundle_raw_set_proc_flags(uint8_t *primary_raw, uint64_t new_flags);
int bundle_raw_ext_get_proc_flags(const uint8_t *ext_raw, /*out*/uint8_t *flags);
int bundle_raw_ext_set_proc_flags(uint8_t *ext_raw, uint8_t new_flags);

// Get specific block contents
int bundle_raw_get_primary_field(const uint8_t *bundle_raw, const primary_field_e field_id, /*out*/char **field);
int bunlde_raw_get_timestamp_and_seq(const uint8_t *bundle_raw, /*out*/uint64_t *timestamp_time, /*out*/uint64_t *timestamp_seq);
int bundle_raw_get_lifetime(const uint8_t *bundle_raw, /*out*/uint64_t *lifetime);
int bundle_raw_get_payload(const uint8_t *bundle_raw, /*out*/uint8_t **payload);
int bundle_raw_get_metadata(const uint8_t *bundle_raw, /*out*/uint64_t *metadata_type, /*out*/uint8_t **metadata);
// static inline int bundle_raw_get_mmeb_codes(const uint8_t *mmeb_codes_raw, const int mmeb_codes_raw_l, /*out*/mmeb_code_s *mmeb_codes);
int bundle_raw_get_mmeb(const uint8_t *bundle_raw, /*out*/mmeb_body_s *mmeb);

// Not necessary at the moment but planned (it should populate all the fields in a bundle_s struct)
//int bundle_raw_parse(static uint8_t *bundle_raw, bundle_s *bundle);

/**************************/


/************* Utils *************/

int sdnv_decode(const uint8_t *bp, uint64_t *val);
int bundle_raw_get_sdnv_off(const uint8_t *raw, const int field_num);
int bundle_get_printable(const bundle_s *bundle, char **message);

/**************************/


/************* Wrappers *************/

int bundle_get_destination(const uint8_t *bundle_raw, /*out*/uint8_t **dest);
int bundle_set_destination(bundle_s *bundle, const char *destination);
int bundle_get_source(const uint8_t *bundle_raw, /*out*/uint8_t **source);
int bundle_set_source(bundle_s *bundle, const char *source);
int bundle_get_report(const uint8_t *bundle_raw, /*out*/uint8_t **report);
int bundle_set_report(bundle_s *bundle, const char *report);
int bundle_get_custom(const uint8_t *bundle_raw, /*out*/uint8_t **cust);
int bundle_set_custom(bundle_s *bundle, const char *custom);


/**************************/

#endif
