#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <math.h>

#include <time.h>
#include <sys/time.h>
#include "include/bundle.h"

/*
        TODO: Bundle fragmentation is not handled
 */

/* sdnv_ functions from DTN2 reference implementation */
int sdnv_encode(uint64_t val, uint8_t *bp)
{
	size_t val_len = 0;
	uint64_t tmp = val;

	do {
		tmp = tmp >> 7;
		val_len++;
	} while (tmp != 0);

	bp += val_len;
	uint8_t high_bit = 0; // for the last octet
	do {
		--bp;
		*bp = (uint8_t)(high_bit | (val & 0x7f));
		high_bit = (1 << 7); // for all but the last octet
		val = val >> 7;
	} while (val != 0);

	return val_len;
}

size_t sdnv_encoding_len(uint64_t val)
{
	size_t val_len = 0;
	uint64_t tmp = val;
	do {
		tmp = tmp >> 7;
		val_len++;
	} while (tmp != 0);

	return val_len;
}

int sdnv_decode(const uint8_t *bp, uint64_t *val)
{
	const uint8_t *start = bp;
	if (!val) {
		return -1;
	}
	size_t val_len = 0;
	*val = 0;
	do {
		*val = (*val << 7) | (*bp & 0x7f);
		++val_len;

		if ((*bp & (1 << 7)) == 0) {
			break; // all done;
		}

		++bp;
	} while (1);

	if ((val_len > MAX_SDNV_LENGTH) || ((val_len == MAX_SDNV_LENGTH) && (*start != 0x81))) {
		return -1;
	}

	return val_len;
}

size_t sdnv_len(const uint8_t *bp)
{
	size_t val_len = 1;
	for ( ; *bp++ & 0x80; ++val_len );

	return val_len;
}

// int get_dtn_time(/*out*/uint8_t **dtn_time, /*out*/ int *dtn_time_l)
// {
//  int ret = 1, sec_l, usec_l;
//  struct timeval tv;

//  if (gettimeofday(&tv, NULL) != 0)
//      goto end;

//  sec_l = sdnv_encoding_len(tv.tv_sec);
//  usec_l = sdnv_encoding_len(tv.tv_usec);

//  *dtn_time = (uint8_t *)malloc(sec_l + usec_l);

//  *dtn_time_l = sdnv_encode(tv.tv_sec, *dtn_time);
//  *dtn_time_l += sdnv_encode(tv.tv_usec, *dtn_time + *dtn_time_l);

//  if (*dtn_time_l != (sec_l + usec_l)) {
//      free(*dtn_time);
//      goto end;
//  }

//  ret = 0;
// end:
//  return ret;
// }


/**/

/* Primary Block Block RFC5050 */
bundle_s *bundle_new()
{
	bundle_s *ret = NULL;
	static int last_timestamp = 0;
	static int timestamp_seq = 0;
	time_t actual_time;

	bundle_s *new_b = (bundle_s *)calloc(1, sizeof(bundle_s));
	new_b->primary = (primary_block_s *)calloc(1, sizeof(primary_block_s));

	// Set actual timestamp avoiding duplicated timestamp/timestamp_seq pairs
	actual_time = time(NULL);
	new_b->primary->timestamp_time = actual_time - RFC_DATE_2000;
	if (new_b->primary->timestamp_time == last_timestamp)
		timestamp_seq++;
	else
		timestamp_seq = 0;
	new_b->primary->timestamp_seq = timestamp_seq;
	last_timestamp = new_b->primary->timestamp_time;

	// Defaults from RFC5050
	new_b->primary->version = PRIM_B;

	//All offsets points to the first position of the dict, wich is a NULL char.
	new_b->primary->dict_length = 1;
	new_b->primary->dict = (char *)calloc(1, new_b->primary->dict_length);

	ret = new_b;

	return ret;
}

int bundle_set_proc_flags(bundle_s *bundle, uint64_t flags)
{
	int ret = 0;

	if (!bundle) {
		ret = 1;
		goto end;
	}

	bundle->primary->proc_flags = bundle->primary->proc_flags | flags;

end:
	return ret;
}

int bundle_set_lifetime(bundle_s *bundle, uint64_t lifetime)
{
	int ret = 0;

	if (!bundle) {
		ret = 1;
		goto end;
	}

	bundle->primary->lifetime = lifetime;
end:
	return ret;
}

static int bundle_set_dict_offset(bundle_s *bundle, primary_field_e field, uint64_t offset)
{
	int ret = 0;

	if (!bundle) {
		ret = 1;
		goto end;
	}

	switch (field) {
	case DEST_SCHEME:
		bundle->primary->dest_scheme_offset = offset;
		break;
	case DEST_SSP:
		bundle->primary->dest_ssp_offset = offset;
		break;
	case SOURCE_SCHEME:
		bundle->primary->source_scheme_offset = offset;
		break;
	case SOURCE_SSP:
		bundle->primary->source_ssp_offset = offset;
		break;
	case REPORT_SCHEME:
		bundle->primary->report_scheme_offset = offset;
		break;
	case REPORT_SSP:
		bundle->primary->report_ssp_offset = offset;
		break;
	case CUST_SCHEME:
		bundle->primary->cust_scheme_offset = offset;
		break;
	case CUST_SSP:
		bundle->primary->cust_ssp_offset = offset;
		break;
	default:
		ret = 1;
	}

end:
	return ret;
}

static int bundle_get_dict_offset(bundle_s *bundle, primary_field_e field)
{
	int ret = 0;

	if (!bundle) {
		ret = -1;
		goto end;
	}

	switch (field) {
	case DEST_SCHEME:
		ret = bundle->primary->dest_scheme_offset;
		break;
	case DEST_SSP:
		ret = bundle->primary->dest_ssp_offset;
		break;
	case SOURCE_SCHEME:
		ret = bundle->primary->source_scheme_offset;
		break;
	case SOURCE_SSP:
		ret = bundle->primary->source_ssp_offset;
		break;
	case REPORT_SCHEME:
		ret = bundle->primary->report_scheme_offset;
		break;
	case REPORT_SSP:
		ret = bundle->primary->report_ssp_offset;
		break;
	case CUST_SCHEME:
		ret = bundle->primary->cust_scheme_offset;
		break;
	case CUST_SSP:
		ret = bundle->primary->cust_ssp_offset;
		break;
	default:
		ret = -1;
	}

end:
	return ret;
}

static int bundle_update_primary_offsets(bundle_s *bundle, uint64_t removed_entry_offset, int removed_entry_l)
{
	if (bundle->primary->dest_scheme_offset > removed_entry_offset)
		bundle->primary->dest_scheme_offset -= removed_entry_l;
	if (bundle->primary->dest_ssp_offset > removed_entry_offset)
		bundle->primary->dest_ssp_offset -= removed_entry_l;
	if (bundle->primary->source_scheme_offset > removed_entry_offset)
		bundle->primary->source_scheme_offset -= removed_entry_l;
	if (bundle->primary->source_ssp_offset > removed_entry_offset)
		bundle->primary->source_ssp_offset -= removed_entry_l;
	if (bundle->primary->report_scheme_offset > removed_entry_offset)
		bundle->primary->report_scheme_offset -= removed_entry_l;
	if (bundle->primary->report_ssp_offset > removed_entry_offset)
		bundle->primary->report_ssp_offset -= removed_entry_l;
	if (bundle->primary->cust_scheme_offset > removed_entry_offset)
		bundle->primary->cust_scheme_offset -= removed_entry_l;
	if (bundle->primary->cust_ssp_offset > removed_entry_offset)
		bundle->primary->cust_ssp_offset -= removed_entry_l;

	return 0;
}

static int bundle_remove_dict_entry(bundle_s *bundle, uint64_t removed_entry_offset)
{
	int removed_entry_l = 0, ret = 0, new_dict_l = 0;
	char *new_dict = NULL;

	removed_entry_l = strlen(bundle->primary->dict + removed_entry_offset) + 1;
	new_dict_l = bundle->primary->dict_length - removed_entry_l;
	new_dict = (char *)malloc(new_dict_l);

	// Before removed entry
	memcpy(new_dict, bundle->primary->dict, removed_entry_offset);
	// After removed entry
	memcpy(new_dict + removed_entry_offset, bundle->primary->dict + removed_entry_offset + removed_entry_l, bundle->primary->dict_length - removed_entry_offset - removed_entry_l);

	// Replace dictionary
	free(bundle->primary->dict);
	bundle->primary->dict = new_dict;
	bundle->primary->dict_length = new_dict_l;

	// Update offsets
	ret = bundle_update_primary_offsets(bundle, removed_entry_offset, removed_entry_l);

	return ret;
}

static int bundle_add_dict_entry(bundle_s *bundle, const char *new_dict_entry)
{
	int new_dict_entry_l = 0, entry_offset = 0, ret = 0;

	if (!bundle) {
		ret = -1;
		goto end;
	}

	new_dict_entry_l = strlen(new_dict_entry) + 1;
	bundle->primary->dict = (char *) realloc(bundle->primary->dict, bundle->primary->dict_length + new_dict_entry_l);
	memcpy(bundle->primary->dict + bundle->primary->dict_length, new_dict_entry, new_dict_entry_l);

	entry_offset = bundle->primary->dict_length;
	bundle->primary->dict_length += new_dict_entry_l;

	ret = entry_offset;
end:
	return ret;
}

int bundle_set_primary_entry(bundle_s *bundle, primary_field_e field, const char *new_dict_entry)
{
	int entry_offset = 0, ret = 0;

	if (!bundle) {
		ret = 1;
		goto end;
	}

	entry_offset = bundle_get_dict_offset(bundle, field);

	// Already set. First we will remove the entry
	if (entry_offset > 0) {
		if ((ret = bundle_remove_dict_entry(bundle, entry_offset)) != 0)
			goto end;
	}

	// Add new entry
	entry_offset = bundle_add_dict_entry(bundle, new_dict_entry);
	if (entry_offset < 0) {
		ret = 1;
		goto end;
	}
	ret = bundle_set_dict_offset(bundle, field, entry_offset);

end:
	return ret;
}

int bundle_remove_primary_entry(bundle_s *bundle, primary_field_e field)
{
	int entry_offset = 0, ret = 0;

	if (!bundle) {
		ret = 1;
		goto end;
	}

	entry_offset = bundle_get_dict_offset(bundle, field);
	if (entry_offset > 0) {
		if ((ret = bundle_remove_dict_entry(bundle, entry_offset)) != 0)
			goto end;
	}

	ret = bundle_set_dict_offset(bundle, field, 0);

end:
	return ret;
}
/**/

/* Payload Block RFC5050 */
payload_block_s *bundle_new_payload_block()
{
	payload_block_s *payload_block = (payload_block_s *)calloc(1, sizeof(payload_block_s));
	payload_block->body.payload = (payload_body_s *)calloc(1, sizeof(payload_body_s));
	payload_block->type = PAYL_B;

	return payload_block;
}

int bundle_set_payload(payload_block_s *block, uint8_t *payload, int payload_l)
{
	int ret = 0;
	if (!block) {
		ret = 1;
		goto end;
	}
	if (block->body.payload->payload)
		free(block->body.payload->payload);

	block->body.payload->payload = (uint8_t *)malloc(sizeof(uint8_t) * payload_l);
	memcpy(block->body.payload->payload, payload, payload_l);

	block->length = payload_l;

end:
	return ret;
}
/**/

/* Metadata Extension Block RFC6258 */
meb_s *bundle_new_meb()
{
	meb_s *meb = (meb_s *)calloc(1, sizeof(meb_s));
	meb->type = META_B;
	meb->body.meb = (meb_body_s *)calloc(1, sizeof(meb_body_s));

	return meb;
}

int bundle_set_metadata(meb_s *block, uint64_t meta_type, uint8_t *metadata, int metadata_l)
{
	int ret = 0;
	if (!block) {
		ret = 1;
		goto end;
	}

	block->body.meb->type = meta_type;
	if (block->body.meb->metadata.metadata)
		free(block->body.meb->metadata.metadata);
	block->body.meb->metadata.metadata = (uint8_t *)malloc(sizeof(uint8_t) * metadata_l);
	memcpy(block->body.meb->metadata.metadata, metadata, metadata_l);

	// Maybe we could directly encode the metadata as required and put the correct length in the metadata block header. At the moment, this is easier.
	block->body.meb->metadata_l = metadata_l;

end:
	return ret;
}
/**/

/* Mobile-code Metadata Extension Block (SeNDA) */
mmeb_s *bundle_new_mmeb()
{
	mmeb_s *mmeb = (mmeb_s *)bundle_new_meb();
	mmeb->body.meb->type = MMEB_B;

	return mmeb;
}



int bundle_add_mmeb_code(mmeb_s *block, uint16_t alg_type, uint16_t fwk, size_t sw_length, uint8_t *sw_code)
{
	int ret = 0;
	mmeb_code_s **new_code = NULL;
	mmeb_body_s **body = NULL;

	if (!block) {
		ret = 1;
		goto end;
	}

	// Find if we already have a <body> block
	for (body = &block->body.meb->metadata.mmeb;
	     *body != NULL && (*body)->alg_type != alg_type;
	     body = &(*body)->next);

	// If we don't, create a new one at the end of body blocks list.
	if (*body == NULL) {
		*body = (mmeb_body_s *)calloc(1, sizeof(mmeb_body_s));
		(*body)->alg_type = alg_type;
	}

	// Find last code block of the <*body> block
	for (new_code = &(*body)->code;
	     *new_code != NULL;
	     new_code = &(*new_code)->next
	    );

	// Populate code block
	(*new_code) = (mmeb_code_s *)calloc(1, sizeof(mmeb_code_s));
	(*new_code)->fwk = fwk;
	(*new_code)->sw_length = sw_length;
	(*new_code)->sw_code = (uint8_t *) strdup((char *) sw_code);

	// Update alg_length
	(*body)->alg_length += sizeof((*new_code)->fwk);
	(*body)->alg_length += sdnv_encoding_len((*new_code)->sw_length);
	(*body)->alg_length += (*new_code)->sw_length;

end:
	return ret;
}
/**/


int bundle_set_ext_block_flags(ext_block_s *ext_block, uint8_t flags)
{
	int ret = 0;

	if (!ext_block) {
		ret = 1;
		goto end;
	}

	ext_block->proc_flags |= flags;

end:
	return ret;

}

int bundle_add_ext_block(bundle_s *bundle, ext_block_s *ext_block)
{
	int ret = 0;
	ext_block_s **next = NULL;

	if (!bundle || !ext_block) {
		ret = 1;
		goto end;
	}

	next = &bundle->ext;
	while (*next) {
		next = &(*next)->next;
	}
	*next = (ext_block_s *)malloc(sizeof(ext_block_s));
	memcpy(*next, ext_block, sizeof(ext_block_s));
end:
	return ret;
}

/* Administrative records functions*/
adm_record_s *bundle_ar_new(ar_type_e type)
{
	adm_record_s *ar = (adm_record_s *)calloc(1, sizeof(adm_record_s));

	switch (type) {
	case AR_SR:
		ar->type = type;
		ar->body.sr = (status_report_s *)malloc(sizeof(status_report_s));

		// Set all fields to -1 so we could know when they are set.
		memset(ar->body.sr, -1, sizeof(*ar->body.sr));
		memset(&ar->body.sr->status_flags, 0 , sizeof(ar->body.sr->status_flags));
		memset(&ar->body.sr->reason_codes, 0 , sizeof(ar->body.sr->reason_codes));

		break;
	case AR_CS:
		fprintf(stderr, "Not implemented\n");
		free(ar);
		ar = NULL;
		break;
	}

	return ar;
}

int bundle_ar_free(adm_record_s *ar)
{
	int ret = 0;

	switch (ar->type) {
	case AR_SR:
		if (ar) {
			if (ar->body.sr)
				free(ar->body.sr);
			free(ar);
		}
		break;
	case AR_CS:
		fprintf(stderr, "Not implemented\n");
		free(ar);
		ar = NULL;
		break;
	}

	return ret;
}

static int bundle_ar_sr_raw(status_report_s *ar_sr, /*out*/uint8_t **ar_sr_raw)
{
	int ar_sr_raw_l, off = 0;;

	// Calculate sr length
	ar_sr_raw_l = sizeof(ar_sr->status_flags) + sizeof(ar_sr->reason_codes);

	// The fragment bit is set at the administrative record struct,
	// to know if we should include fragment information into the status report raw,
	// we check if the field has been initialized.
	if (ar_sr->fragment_offset != -1) {
		ar_sr_raw_l += sdnv_encoding_len(ar_sr->fragment_offset);
		ar_sr_raw_l += sdnv_encoding_len(ar_sr->fragment_length);
	}

	if (ar_sr->sec_time_of_receipt != -1)
		ar_sr_raw_l += sdnv_encoding_len(ar_sr->sec_time_of_receipt);
	if (ar_sr->usec_time_of_receipt != -1)
		ar_sr_raw_l += sdnv_encoding_len(ar_sr->usec_time_of_receipt);
	if (ar_sr->sec_time_of_qustody != -1)
		ar_sr_raw_l += sdnv_encoding_len(ar_sr->sec_time_of_qustody);
	if (ar_sr->usec_time_of_qustody != -1)
		ar_sr_raw_l += sdnv_encoding_len(ar_sr->usec_time_of_qustody);
	if (ar_sr->sec_time_of_forwarding != -1)
		ar_sr_raw_l += sdnv_encoding_len(ar_sr->sec_time_of_forwarding);
	if (ar_sr->usec_time_of_forwarding != -1)
		ar_sr_raw_l += sdnv_encoding_len(ar_sr->usec_time_of_forwarding);
	if (ar_sr->sec_time_of_delivery != -1)
		ar_sr_raw_l += sdnv_encoding_len(ar_sr->sec_time_of_delivery);
	if (ar_sr->usec_time_of_delivery != -1)
		ar_sr_raw_l += sdnv_encoding_len(ar_sr->usec_time_of_delivery);
	if (ar_sr->sec_time_of_deletion != -1)
		ar_sr_raw_l += sdnv_encoding_len(ar_sr->sec_time_of_deletion);
	if (ar_sr->usec_time_of_deletion != -1)
		ar_sr_raw_l += sdnv_encoding_len(ar_sr->usec_time_of_deletion);
	if (ar_sr->cp_creation_timestamp != -1)
		ar_sr_raw_l += sdnv_encoding_len(ar_sr->cp_creation_timestamp);
	if (ar_sr->cp_creation_ts_seq_num != -1)
		ar_sr_raw_l += sdnv_encoding_len(ar_sr->cp_creation_ts_seq_num);

	if (ar_sr->source_EID_len != -1) {
		ar_sr_raw_l += sdnv_encoding_len(ar_sr->source_EID_len);
		ar_sr_raw_l += ar_sr->source_EID_len;
	}

	*ar_sr_raw = (uint8_t *)malloc(ar_sr_raw_l);

	//Codify sr
	memcpy(*ar_sr_raw, &ar_sr->status_flags, sizeof(ar_sr->status_flags));
	off++;
	memcpy(*ar_sr_raw + off, &ar_sr->reason_codes, sizeof(ar_sr->reason_codes));
	off++;

	if (ar_sr->fragment_offset != -1) {
		off += sdnv_encode(ar_sr->fragment_offset, *ar_sr_raw + off);
		off += sdnv_encode(ar_sr->fragment_length, *ar_sr_raw + off);
	}
	if (ar_sr->sec_time_of_receipt != -1)
		off += sdnv_encode(ar_sr->sec_time_of_receipt, *ar_sr_raw + off);
	if (ar_sr->usec_time_of_receipt != -1)
		off += sdnv_encode(ar_sr->usec_time_of_receipt, *ar_sr_raw + off);
	if (ar_sr->sec_time_of_qustody != -1)
		off += sdnv_encode(ar_sr->sec_time_of_qustody, *ar_sr_raw + off);
	if (ar_sr->usec_time_of_qustody != -1)
		off += sdnv_encode(ar_sr->usec_time_of_qustody, *ar_sr_raw + off);
	if (ar_sr->sec_time_of_forwarding != -1)
		off += sdnv_encode(ar_sr->sec_time_of_forwarding, *ar_sr_raw + off);
	if (ar_sr->usec_time_of_forwarding != -1)
		off += sdnv_encode(ar_sr->usec_time_of_forwarding, *ar_sr_raw + off);
	if (ar_sr->sec_time_of_delivery != -1)
		off += sdnv_encode(ar_sr->sec_time_of_delivery, *ar_sr_raw + off);
	if (ar_sr->usec_time_of_delivery != -1)
		off += sdnv_encode(ar_sr->usec_time_of_delivery, *ar_sr_raw + off);
	if (ar_sr->sec_time_of_deletion != -1)
		off += sdnv_encode(ar_sr->sec_time_of_deletion, *ar_sr_raw + off);
	if (ar_sr->usec_time_of_deletion != -1)
		off += sdnv_encode(ar_sr->usec_time_of_deletion, *ar_sr_raw + off);
	if (ar_sr->cp_creation_timestamp != -1)
		off += sdnv_encode(ar_sr->cp_creation_timestamp, *ar_sr_raw + off);
	if (ar_sr->cp_creation_ts_seq_num != -1)
		off += sdnv_encode(ar_sr->cp_creation_ts_seq_num, *ar_sr_raw + off);
	if (ar_sr->source_EID_len != -1) {
		off += sdnv_encode(ar_sr->source_EID_len, *ar_sr_raw + off);
		memcpy(*ar_sr_raw + off, ar_sr->source_EID, ar_sr->source_EID_len);
	}

	return ar_sr_raw_l;
}

int bundle_ar_raw(adm_record_s *ar, /*out*/uint8_t **ar_raw )
{
	int ar_raw_l = 0, ar_body_raw_l = 0;
	uint8_t *ar_body_raw = NULL;

	ar_raw_l += 1; // Type + flags size
	switch (ar->type) {
	case AR_SR:
		ar_body_raw_l = bundle_ar_sr_raw(ar->body.sr, &ar_body_raw);
		ar_raw_l += ar_body_raw_l;
		break;
	case AR_CS:
		fprintf(stderr, "Not implemented\n");
		free(ar);
		ar = NULL;
		break;
	}

	*ar_raw = (uint8_t *)malloc(ar_raw_l);
	**ar_raw = 0;
	**ar_raw |= ar->type;
	**ar_raw |= ar->flags << 4;
	memcpy(*ar_raw + 1, ar_body_raw, ar_body_raw_l);
	free(ar_body_raw);

	return ar_raw_l;
}

bundle_s *bundle_new_sr(
	const sr_status_flags_e sr_status_flag, const uint8_t reason_codes, 
	const char *source_eid, struct timeval reception_time, const uint8_t *orig_bundle_raw)
{
	int err = 1, ar_raw_l = 0;
	char *report_to;
	adm_record_s *ar = NULL;
	uint8_t *ar_raw = NULL;
	uint64_t timestamp_time, timestamp_seq, lifetime;
	bundle_s *sr_bundle = NULL;
	payload_block_s *payload = NULL;

	/* Get required information for creating the status report */

	// Get report-to to set it as destination
	if ((bundle_raw_get_primary_field(orig_bundle_raw, REPORT_SCHEME, &report_to) != 0) || report_to == NULL || *report_to == '\0')
		goto end;
	if (bunlde_raw_get_timestamp_and_seq(orig_bundle_raw, &timestamp_time, &timestamp_seq) != 0)
		goto end;
	if (bundle_raw_get_lifetime(orig_bundle_raw, &lifetime))
		goto end;
	/**/

	// Prepare sr
	ar = bundle_ar_new(AR_SR);

	if (!reason_codes)
		ar->body.sr->reason_codes = RC_NO_ADD_INFO;
	else
		ar->body.sr->reason_codes = reason_codes;
	
	switch(sr_status_flag){
		case SR_RECV:
			ar->body.sr->status_flags = SR_RECV;
			ar->body.sr->sec_time_of_receipt = reception_time.tv_sec - RFC_DATE_2000;
			ar->body.sr->usec_time_of_receipt = reception_time.tv_usec;
			break;
		case SR_CACC:
			ar->body.sr->status_flags = SR_CACC;
			ar->body.sr->sec_time_of_qustody = reception_time.tv_sec - RFC_DATE_2000;
			ar->body.sr->usec_time_of_qustody = reception_time.tv_usec;
			break;
		case SR_FORW:
			ar->body.sr->status_flags = SR_FORW;
			ar->body.sr->sec_time_of_qustody = reception_time.tv_sec - RFC_DATE_2000;
			ar->body.sr->usec_time_of_qustody = reception_time.tv_usec;
			break;
		case SR_DELI:
			ar->body.sr->status_flags = SR_DELI;
			ar->body.sr->sec_time_of_qustody = reception_time.tv_sec - RFC_DATE_2000;
			ar->body.sr->usec_time_of_qustody = reception_time.tv_usec;
			break;
		case SR_DEL:
			ar->body.sr->status_flags = SR_DEL;
			ar->body.sr->sec_time_of_qustody = reception_time.tv_sec - RFC_DATE_2000;
			ar->body.sr->usec_time_of_qustody = reception_time.tv_usec;
			break;
	}

	ar->body.sr->cp_creation_timestamp = timestamp_time;
	ar->body.sr->cp_creation_ts_seq_num = timestamp_seq;
	ar->body.sr->source_EID_len = strlen(source_eid);
	ar->body.sr->source_EID = (char *)malloc(ar->body.sr->source_EID_len);
	strncpy(ar->body.sr->source_EID, source_eid, ar->body.sr->source_EID_len);
	ar_raw_l = bundle_ar_raw(ar, &ar_raw);
	if (ar_raw_l <= 0)
		goto end;

	// Put sr into a payload block
	payload = bundle_new_payload_block();
	if (bundle_set_payload(payload, ar_raw, ar_raw_l) != 0)
		goto end;

	// Put payload into bundle
	sr_bundle = bundle_new();
	if (bundle_add_ext_block(sr_bundle, (ext_block_s *)payload) != 0)
		goto end;
	if (bundle_set_lifetime(sr_bundle, lifetime) != 0)
		goto end;
	if (bundle_set_source(sr_bundle, source_eid) != 0)
		goto end;
	if (bundle_set_destination(sr_bundle, report_to) != 0)
		goto end;

	err = 0;
end:
	if (ar)
		bundle_ar_free(ar);
	if (ar_raw)
		free(ar_raw);
	if (payload)
		free(payload);

	if (err) {
		if (sr_bundle) {
			free(sr_bundle);
			sr_bundle = NULL;
		}
	}

	return sr_bundle;
}
/**/


/* Bundle raw creation functions */
static int bundle_primary_raw(primary_block_s *primary_block, uint8_t **raw)
{
	int ret = 0, primary_block_l = 0, off = 0, max_raw_l = 0;
	size_t header_length = 0;

	// SDNV adds an overhead of 1:7 (one bit of overhead for each 7 bits of data to be encoded)
	primary_block_l = sizeof(*primary_block) + primary_block->dict_length;
	max_raw_l = ceil(primary_block_l * 8 / 7);
	*raw = (uint8_t *)malloc(max_raw_l);

	// Primary header length
	header_length += sdnv_encoding_len(primary_block->dest_scheme_offset);
	header_length += sdnv_encoding_len(primary_block->dest_ssp_offset);
	header_length += sdnv_encoding_len(primary_block->source_scheme_offset);
	header_length += sdnv_encoding_len(primary_block->source_ssp_offset);
	header_length += sdnv_encoding_len(primary_block->report_scheme_offset);
	header_length += sdnv_encoding_len(primary_block->report_ssp_offset);
	header_length += sdnv_encoding_len(primary_block->cust_scheme_offset);
	header_length += sdnv_encoding_len(primary_block->cust_ssp_offset);
	header_length += sdnv_encoding_len(primary_block->timestamp_time);
	header_length += sdnv_encoding_len(primary_block->timestamp_seq);
	header_length += sdnv_encoding_len(primary_block->lifetime);
	header_length += sdnv_encoding_len(primary_block->dict_length);
	header_length += primary_block->dict_length;
	if (primary_block->fragment_offset) {
		header_length += sdnv_encoding_len(primary_block->fragment_offset);
		header_length += sdnv_encoding_len(primary_block->total_length);
	}
	primary_block->length = header_length;

	// Encode bundle
	memcpy(*raw, &primary_block->version, sizeof(primary_block->version));
	off++;
	off += sdnv_encode(primary_block->proc_flags, *raw + off);
	off += sdnv_encode(primary_block->length, *raw + off);
	off += sdnv_encode(primary_block->dest_scheme_offset, *raw + off);
	off += sdnv_encode(primary_block->dest_ssp_offset, *raw + off);
	off += sdnv_encode(primary_block->source_scheme_offset, *raw + off);
	off += sdnv_encode(primary_block->source_ssp_offset, *raw + off);
	off += sdnv_encode(primary_block->report_scheme_offset, *raw + off);
	off += sdnv_encode(primary_block->report_ssp_offset, *raw + off);
	off += sdnv_encode(primary_block->cust_scheme_offset, *raw + off);
	off += sdnv_encode(primary_block->cust_ssp_offset, *raw + off);
	off += sdnv_encode(primary_block->timestamp_time, *raw + off);
	off += sdnv_encode(primary_block->timestamp_seq, *raw + off);
	off += sdnv_encode(primary_block->lifetime, *raw + off);
	off += sdnv_encode(primary_block->dict_length, *raw + off);
	memcpy(*raw + off, primary_block->dict, primary_block->dict_length);
	off += primary_block->dict_length;
	if (primary_block->fragment_offset) {
		off += sdnv_encoding_len(primary_block->fragment_offset);
		off += sdnv_encoding_len(primary_block->total_length);
	}

	ret = off;

	return ret;
}

// Length must be correctly set!!
static int bundle_ext_block_header_raw(ext_block_s *ext_block, uint8_t **raw)
{
	int ret = 0, max_raw_header_l = 0, off = 0;

	// Max header length considering SDNV encodings
	max_raw_header_l = 1 + ceil((1 + 1 + ext_block->EID_ref_count * 2 + 1) * 8 / 7);
	*raw = (uint8_t *)calloc(1, max_raw_header_l * sizeof(uint8_t));

	memcpy(*raw, &ext_block->type, sizeof(ext_block->type));
	off++;
	off += sdnv_encode(ext_block->proc_flags, *raw + off);

	// Only if 'block contains an EID-reference field' bit is set
	if ((ext_block->proc_flags & B_EID_RE) == B_EID_RE) {
		off += sdnv_encode(ext_block->EID_ref_count, *raw + off);
		if (ext_block->EID_ref_count > 0 && ext_block->eid_ref) {
			eid_ref_s *next_eid_ref = ext_block->eid_ref;
			do {
				off += sdnv_encode(next_eid_ref->scheme, *raw + off);
				off += sdnv_encode(next_eid_ref->ssp, *raw + off);
				next_eid_ref = next_eid_ref->next;
			} while (next_eid_ref == NULL);
		}
	}

	off += sdnv_encode(ext_block->length, *raw + off);
	ret = off;

	return ret;
}

// Extension body lenght is already the block->length (there aren't SDNV values)
static int bundle_payload_raw(payload_block_s *block, uint8_t **raw)
{
	int ret = 0, header_raw_l = 0, payload_raw_l = 0;

	// Create raw header
	header_raw_l = bundle_ext_block_header_raw((ext_block_s *)block, raw);

	// Total length
	payload_raw_l = header_raw_l + block->length;

	// Concatenate raw header and raw body.
	*raw = (uint8_t *) realloc(*raw, payload_raw_l);
	memcpy(*raw + header_raw_l, block->body.payload->payload, block->length);

	ret = payload_raw_l;

	return ret;
}


static int bundle_meb_raw(meb_s *block, uint8_t **raw)
{
	int ret = 0, header_raw_l = 0, meb_body_raw_l = 0, meb_raw_l = 0, off = 0;

	meb_body_raw_l = sdnv_encoding_len(block->body.meb->type) + block->body.meb->metadata_l;
	block->length = meb_body_raw_l;
	header_raw_l = bundle_ext_block_header_raw((ext_block_s *)block, raw);

	meb_raw_l = header_raw_l + meb_body_raw_l;

	*raw = (uint8_t *) realloc(*raw, meb_raw_l);
	off = header_raw_l;

	off += sdnv_encode(block->body.meb->type, *raw + off);
	memcpy(*raw + off, block->body.meb->metadata.metadata, block->body.meb->metadata_l);

	ret = meb_raw_l;

	return ret;
}

static int bundle_mmeb_raw(mmeb_s *block, uint8_t **raw)
{
	int ret = 0, mmeb_body_raw_l = 0, header_raw_l = 0, mmeb_raw_l = 0, off = 0;
	mmeb_body_s *next_mmeb_body = NULL;
	mmeb_code_s *next_mmeb_code = NULL;

	if (!block) {
		ret = 1;
		goto end;
	}

	// Calculate mmeb body lenght
	mmeb_body_raw_l = sdnv_encoding_len(block->body.meb->type);
	for (next_mmeb_body = block->body.meb->metadata.mmeb;
	     next_mmeb_body != NULL;
	     next_mmeb_body = next_mmeb_body->next) {
		block->body.meb->metadata_l += sizeof(next_mmeb_body->alg_type);
		block->body.meb->metadata_l += sdnv_encoding_len(next_mmeb_body->alg_length);
		block->body.meb->metadata_l += next_mmeb_body->alg_length;
	}
	mmeb_body_raw_l += block->body.meb->metadata_l;

	// Raw header
	block->length = mmeb_body_raw_l;
	header_raw_l = bundle_ext_block_header_raw((ext_block_s *)block, raw);

	mmeb_raw_l = header_raw_l + mmeb_body_raw_l;
	*raw = (uint8_t *) realloc(*raw, mmeb_raw_l);

	// Raw MMEB body
	off = header_raw_l;
	off += sdnv_encode(block->body.meb->type, *raw + off);
	for (next_mmeb_body = block->body.meb->metadata.mmeb;
	     next_mmeb_body != NULL;
	     next_mmeb_body = next_mmeb_body->next) {
		memcpy(*raw + off, &next_mmeb_body->alg_type, sizeof(next_mmeb_body->alg_type));
		off += sizeof(next_mmeb_body->alg_type);
		off += sdnv_encode(next_mmeb_body->alg_length, *raw + off);

		for (next_mmeb_code =  next_mmeb_body->code;
		     next_mmeb_code != NULL;
		     next_mmeb_code = next_mmeb_code->next) {
			memcpy(*raw + off, &next_mmeb_code->fwk, sizeof(next_mmeb_code->fwk));
			off += sizeof(next_mmeb_code->fwk);
			off += sdnv_encode(next_mmeb_code->sw_length, *raw + off);

			memcpy(*raw + off, next_mmeb_code->sw_code, next_mmeb_code->sw_length);
			off += next_mmeb_code->sw_length;
		}

	}

	if (mmeb_raw_l != off) {
		ret = -1;
		free(*raw);
		goto end;
	} else {
		ret = mmeb_raw_l;
	}

end:
	return ret;
}

int bundle_create_raw(const bundle_s *bundle, /*out*/uint8_t **bundle_raw)
{
	int ret = -1, ext_raw_l = 0, bundle_raw_l = 0;
	uint8_t *ext_raw = NULL;
	ext_block_s *next_ext = NULL;

	// Header
	bundle_raw_l = bundle_primary_raw(bundle->primary, bundle_raw);
	if (!bundle->ext) {
		ret = bundle_raw_l;
		goto end;
	}

	// Extensions
	next_ext = bundle->ext;
	do {
		// Set last block flag
		if (next_ext->next == NULL)
			next_ext->proc_flags |= B_LAST;

		ext_raw_l = 0;
		switch (next_ext->type) {
		case PAYL_B:
			ext_raw_l = bundle_payload_raw((payload_block_s *) next_ext, &ext_raw);
			break;
		case META_B:
			if (next_ext->body.meb->type == MMEB_B)
				ext_raw_l = bundle_mmeb_raw((mmeb_s *) next_ext, &ext_raw);
			else // Generic Metadata Extension
				ext_raw_l = bundle_meb_raw((meb_s *) next_ext, &ext_raw);
			break;
		}

		if (ext_raw_l != 0) {
			bundle_raw_l += ext_raw_l;
			*bundle_raw = (uint8_t *) realloc(*bundle_raw, bundle_raw_l);
			memcpy(*bundle_raw + (bundle_raw_l - ext_raw_l), ext_raw, ext_raw_l);
			free(ext_raw);
		}

		next_ext = next_ext->next;
	} while (next_ext);

	ret = bundle_raw_l;
end:
	return ret;
}

int bundle_free_mmeb(mmeb_body_s *mmeb)
{
	mmeb_body_s *next_mmeb = NULL, *curr_mmeb = NULL;
	mmeb_code_s *next_code = NULL, *curr_code = NULL;


	next_mmeb = mmeb;
	while (next_mmeb != NULL) {
		// Free all codes
		next_code = next_mmeb->code;
		while (next_code != NULL) {
			free(next_code->sw_code);
			curr_code = next_code;
			next_code = curr_code->next;
			free(curr_code);
		}
		curr_mmeb = next_mmeb;
		next_mmeb = curr_mmeb->next;
		free(curr_mmeb);
	}


	return 0;
}

int bundle_free(bundle_s *b)
{
	int ret = 0;

	ext_block_s *next_ext = NULL, *old_ext = NULL;
	eid_ref_s *next_eid_ref = NULL, *old_eid_ref = NULL;

	if (b == NULL)
		goto end;

	if (b->primary != NULL) {
		if (b->primary->dict != NULL)
			free(b->primary->dict);
		free(b->primary);
	}

	next_ext = b->ext;
	while (next_ext != NULL) {
		switch (next_ext->type) {
		case PAYL_B:
			free(next_ext->body.payload->payload);
			next_ext->body.payload->payload = NULL;
			free(next_ext->body.payload);
			next_ext->body.payload = NULL;
			break;
		case META_B:
			if (next_ext->body.meb->type != MMEB_B) {
				free(next_ext->body.meb->metadata.metadata);
				next_ext->body.meb->metadata.metadata = NULL;
			} else {
				ret |= bundle_free_mmeb(next_ext->body.meb->metadata.mmeb);
			}
			next_eid_ref = next_ext->eid_ref;
			if (next_eid_ref != NULL) {
				old_eid_ref = next_eid_ref;
				next_eid_ref = next_eid_ref->next;
				free(old_eid_ref);
				old_eid_ref = NULL;
			}
			free(next_ext->body.meb);
			next_ext->body.meb = NULL;
		}
		old_ext = next_ext;
		next_ext = next_ext->next;
		free(old_ext);
		old_ext = NULL;
	}

	free(b);

end:
	return ret;
}

/**/

/*  Returns a printable string containing bundle information from a bundle_s structure. */

int bundle_get_printable(const bundle_s *bundle, /*out*/ char **buff)
{
	int ret = 1, eb_counter = 0, eid_counter = 0, mmeb_counter = 0, mcode_counter = 0, len = 0;
	int pl_counter = 0, meta_counter = 0, i = 0;
	char *message, *dict;
	time_t real_time;
	primary_block_s *pb = NULL;
	ext_block_s *eb = NULL;
	eid_ref_s *eid = NULL;
	meb_body_s *meb = NULL;
	mmeb_body_s *mmeb = NULL;
	mmeb_code_s *mcode = NULL;

	message = (char *) calloc(MAX_BUNDLE_MSG, sizeof(char));

	if (!bundle) {
		strcpy(message + len, "Error: The bundle is NULL");
		goto end;
	}
	pb = bundle->primary;
	dict = (char *) malloc (pb->dict_length);

	len += snprintf(message + len, MAX_BUNDLE_MSG - len, "PRIMARY block:\n");
	len += snprintf(message + len, MAX_BUNDLE_MSG - len, "%s\tversion: %d\n", message + len, pb->version);
	len += snprintf(message + len, MAX_BUNDLE_MSG - len, "%s\tproc_flags: 0x%02X\n", message + len, pb->proc_flags);
	len += snprintf(message + len, MAX_BUNDLE_MSG - len, "%s\tdest_scheme_offset: %ld\n", message + len, pb->dest_scheme_offset);
	len += snprintf(message + len, MAX_BUNDLE_MSG - len, "%s\tdest_ssp_offset: %ld\n", message + len, pb->dest_ssp_offset);
	len += snprintf(message + len, MAX_BUNDLE_MSG - len, "%s\tsource_scheme_offset: %ld\n", message + len, pb->source_scheme_offset);
	len += snprintf(message + len, MAX_BUNDLE_MSG - len, "%s\tsource_ssp_offset: %ld\n", message + len, pb->source_ssp_offset);
	len += snprintf(message + len, MAX_BUNDLE_MSG - len, "%s\treport_scheme_offset: %ld\n", message + len, pb->report_scheme_offset);
	len += snprintf(message + len, MAX_BUNDLE_MSG - len, "%s\treport_ssp_offset: %ld\n", message + len, pb->report_ssp_offset);
	len += snprintf(message + len, MAX_BUNDLE_MSG - len, "%s\tcust_scheme_offset: %ld\n", message + len, pb->cust_scheme_offset);
	len += snprintf(message + len, MAX_BUNDLE_MSG - len, "%s\tcust_ssp_offset: %ld\n", message + len, pb->cust_ssp_offset);
	real_time = (time_t) (pb->timestamp_time + RFC_DATE_2000);
	len += snprintf(message + len, MAX_BUNDLE_MSG - len, "%s\ttimestamp_time: %s", message + len, ctime(&(real_time)));
	len += snprintf(message + len, MAX_BUNDLE_MSG - len, "%s\ttimestamp_seq: %ld\n", message + len, pb->timestamp_seq);
	len += snprintf(message + len, MAX_BUNDLE_MSG - len, "%s\tlifetime: %ld\n", message + len, pb->lifetime);
	len += snprintf(message + len, MAX_BUNDLE_MSG - len, "%s\tdict_length: %ld\n", message + len, pb->dict_length);
	for (i = 0; i < pb->dict_length - 1; ++i) { //Be able to print dict avoiding '\0' special characters
		if (pb->dict[i] == '\0')
			dict[i] = ' ';
		else
			dict[i] = pb->dict[i];
	}
	dict[i] = '\0';
	len += snprintf(message + len, MAX_BUNDLE_MSG - len, "%s\tdict: %s\n", message + len, dict);
	len += snprintf(message + len, MAX_BUNDLE_MSG - len, "%s\t(opt) fragment_offset: %ld\n", message + len, pb->fragment_offset);
	len += snprintf(message + len, MAX_BUNDLE_MSG - len, "%s\t(opt) total_length : %ld\n", message + len, pb->total_length);
	eb = bundle->ext;
	while (eb != NULL) {
		len += snprintf(message + len, MAX_BUNDLE_MSG - len, "%sExtension block %d:\n", message + len, ++eb_counter);
		len += snprintf(message + len, MAX_BUNDLE_MSG - len, "%s\ttype: 0x%02X\n", message + len, eb->type);
		len += snprintf(message + len, MAX_BUNDLE_MSG - len, "%s\tproc_flags: 0x%02X\n", message + len, eb->proc_flags);
		len += snprintf(message + len, MAX_BUNDLE_MSG - len, "%s\tEID_ref_count: %ld\n", message + len, eb->EID_ref_count);
		eid_counter = 0;
		eid = eb->eid_ref;
		while (eid != NULL) {
			len += snprintf(message + len, MAX_BUNDLE_MSG - len, "%s\tEID struct %d:\n", message + len, ++eid_counter);
			len += snprintf(message + len, MAX_BUNDLE_MSG - len, "%s\t\tscheme: %ld\n", message + len, eid->scheme);
			len += snprintf(message + len, MAX_BUNDLE_MSG - len, "%s\t\tssp: %ld\n", message + len, eid->ssp);
			eid = eid->next;
		}
		if (eb->type == PAYL_B) {
			len += snprintf(message + len, MAX_BUNDLE_MSG - len, "%s\tPAYLOAD block %d:\n", message + len, ++pl_counter);
			len += snprintf(message + len, MAX_BUNDLE_MSG - len, "%s\t\tbody: %s\n", message + len, eb->body.payload->payload);
		} else if (eb->type == META_B) {
			len += snprintf(message + len, MAX_BUNDLE_MSG - len, "%s\tMETA block %d:\n", message + len, ++meta_counter);
			len += snprintf(message + len, MAX_BUNDLE_MSG - len, "%s\t\ttype: 0x%02lX\n", message + len, eb->body.meb->type);
			len += snprintf(message + len, MAX_BUNDLE_MSG - len, "%s\t\tmetadata_l: %d\n", message + len, eb->body.meb->metadata_l);
			meb = eb->body.meb;
			if (meb->type == MMEB_B) {
				mmeb_counter = 0;
				mmeb = meb->metadata.mmeb;
				while (mmeb != NULL) {
					len += snprintf(message + len, MAX_BUNDLE_MSG - len, "%s\t\tMMEB block: %d:\n", message + len, ++mmeb_counter);
					len += snprintf(message + len, MAX_BUNDLE_MSG - len, "%s\t\t\talg_type: 0x%02X\n", message + len, mmeb->alg_type);
					len += snprintf(message + len, MAX_BUNDLE_MSG - len, "%s\t\t\talg_length: %ld\n", message + len, mmeb->alg_length);
					mcode_counter = 0;
					mcode = mmeb->code;
					while (mcode != NULL) {
						len += snprintf(message + len, MAX_BUNDLE_MSG - len, "%s\t\t\tMobile-code Metadata Extension Block %d:\n", message + len, ++mcode_counter);
						len += snprintf(message + len, MAX_BUNDLE_MSG - len, "%s\t\t\t\tSoftware architecture: 0x%02X\n", message + len, mcode->fwk);
						len += snprintf(message + len, MAX_BUNDLE_MSG - len, "%s\t\t\t\tsw_length: %d\n", message + len, (int) mcode->sw_length);
						len += snprintf(message + len, MAX_BUNDLE_MSG - len, "%s\t\t\t\tsw_code: %s\n", message + len, mcode->sw_code);
						mcode = mcode->next;
					}
					mmeb = mmeb->next;
				}
			} else {
				len += snprintf(message + len, MAX_BUNDLE_MSG - len, "%s\tERROR: Unrecognized metadata extension block (%ld), skipping...\n", message + len, meb->type);
			}
		} else {
			len += snprintf(message + len, MAX_BUNDLE_MSG - len, "%s\tERROR: Unrecognized block type (%d), skipping...\n", message + len, eb->type);
		}

		eb = eb->next;
	}
	ret = 0;
	free(dict);
end:
	*buff = message;
	return ret;
}

/* Bundle raw parsing functions */

static int bundle_raw_check_block_requirements(const uint8_t *bundle_raw, int last_block)
{
	int ret = 1;
	const uint8_t *b_pos = NULL;
	uint64_t block_flags = 0;
	uint64_t block_length = 0;

	b_pos = bundle_raw;
	if (bundle_raw_get_proc_flags(b_pos, &block_flags) == 0) {
		// (True && False || False && True) : last_block missmatch --> Fail
		if ((last_block && (block_flags & B_LAST) != B_LAST) || (last_block == 0 && (block_flags & B_LAST) == B_LAST))
			goto end;
	}
	b_pos += 2; // Skip type + proc. flags
	if (b_pos += sdnv_decode(b_pos, &block_length) > 0) {
		if (block_length == 0)
			goto end;
	} else {
		goto end;
	}

	ret = 0;
end:
	return ret;
}

int bundle_raw_check(const uint8_t *bundle_raw, int length)
{
	int ret = 1, prim = 0, payl = 0, last_block = 0, off = 0;
	const uint8_t *b_pos = NULL;

	if (!bundle_raw || *bundle_raw == 0 || length <= 0 || length > MAX_BUNDLE_SIZE)        //Bundle params incorrect
		goto end;
	b_pos = bundle_raw;
	while (b_pos - bundle_raw <= length) {
		if ((b_pos + bundle_raw_next_block_off(b_pos)) - bundle_raw == length)// Last block bit must be set
			last_block = 1;
		if (*b_pos == PRIM_B) {
			if (prim != 0)          // More than one Primary block
				goto end;
			if (b_pos != bundle_raw)// Primary block is not the first one
				goto end;
			prim = 1;
			uint8_t *dest;
			uint8_t *src;
			if (bundle_get_destination(bundle_raw, &dest) != 0)
				goto end;
			if (bundle_get_source(bundle_raw, &src) != 0)
				goto end;
			int dest_l = strlen((const char *) dest);
			int src_l = strlen((const char *) src);
			free(dest);
			free(src);
			if (dest_l < MIN_ID_LEN || dest_l > MAX_ID_LEN || src_l < MIN_ID_LEN || src_l > MAX_ID_LEN)
				goto end;
		} else if (*b_pos == PAYL_B) {
			if (payl != 0)          // More than one Payload
				goto end;
			if (bundle_raw_check_block_requirements(b_pos, last_block) != 0)
				goto end;
			payl = 1;
		} else {
			if (bundle_raw_check_block_requirements(b_pos, last_block) != 0)
				goto end;
		}
		off = bundle_raw_next_block_off(b_pos);
		b_pos += off;
		if (last_block)
			break;
		if (off == 0)
			goto end;
	}
	if (prim == 0 || payl == 0)  // Madatory blocks missing
		goto end;
	if (b_pos - bundle_raw != length) // Value of length field of any ext incorrect
		goto end;

	ret = 0;
end:
	return ret;
}

//Returns the offset of the <field_num> SDNV field
int bundle_raw_get_sdnv_off(const uint8_t *raw, const int field_num)
{
	int i = 0, off = 0;

	for (i = 0; i < field_num; ++i) {
		off += sdnv_len(raw + off);
	}

	return off;
}

//Returns length of the ext block body
static inline int bundle_raw_get_body_off(const uint8_t *raw, /*out*/ int *ext_block_body_off)
{
	uint64_t EID_ref_count = 0, body_length = 0, proc_flags = 0;
	const uint8_t *b_pos = NULL;

	b_pos = raw;
	b_pos++; // Skip version / block type
	b_pos += sdnv_decode(b_pos, &proc_flags); // Decode proc_flags
	if (*raw != PRIM_B) { // If it is an ext. block
		if (CHECK_BIT(proc_flags, 6)) {
			b_pos += sdnv_decode(b_pos, &EID_ref_count); // Skip EID-ref-count
			if (EID_ref_count)
				b_pos += bundle_raw_get_sdnv_off(b_pos, EID_ref_count * 2); // Skip EID-refs
		}
	}
	b_pos += sdnv_decode(b_pos, &body_length); // Skip body length

	*ext_block_body_off = b_pos - raw;

	return body_length;
}


//Returns length of the ext block body
static inline int bundle_raw_ext_get_block_body(const uint8_t *bundle_raw, block_type_e block_type, /*out*/uint8_t **body)
{
	size_t body_l = 0;
	int ret = 0, block_off = 0, body_off = 0;

	// Find block
	if ((block_off = bundle_raw_find_block_off(bundle_raw, block_type)) < 0) {
		ret = -1;
		goto end;
	}

	// Get body length and offset
	body_l = bundle_raw_get_body_off(bundle_raw + block_off, &body_off);
	*body = (uint8_t *)malloc(body_l * sizeof(uint8_t));

	// Get body content
	memcpy(*body, bundle_raw + block_off + body_off, body_l);
	ret = body_l;
end:
	return ret;
}

// Returns offset to next block
int bundle_raw_next_block_off(const uint8_t *raw)
{
	int body_l = 0, body_off = 0;

	body_l = bundle_raw_get_body_off(raw, &body_off);
	return (body_l + body_off); // Skip to next block
}


int bundle_raw_get_proc_flags(const uint8_t *primary_raw, /*out*/uint64_t *flags)
{
	int off = 0, ret = 0;

	off++; //Skyp version
	if (sdnv_decode(primary_raw + off, flags) == 0)
		ret = 1;

	return ret;
}

int bundle_raw_set_proc_flags(uint8_t *primary_raw, /*out*/uint64_t new_flags)
{
	int off = 0, ret = 0, old_flags_l = 0, new_flags_l = 0;
	uint64_t old_flags = 0;

	off++;
	old_flags_l = sdnv_decode(primary_raw + off, &old_flags);
	new_flags_l = sdnv_encoding_len(new_flags);
	if (old_flags_l ==  new_flags_l) {
		sdnv_encode(new_flags, primary_raw + off);
	} else {
		fprintf(stderr, "TODO: We need to reorganize bundle raw structure\n");
		ret = 1;
	}

	return ret;
}

int bundle_raw_ext_get_proc_flags(const uint8_t *ext_raw, /*out*/uint8_t *flags)
{
	uint64_t flags_32 = 0, ret = 0;

	ret = bundle_raw_get_proc_flags(ext_raw, &flags_32);
	*flags = flags_32;

	return ret;
}

int bundle_raw_ext_set_proc_flags(uint8_t *ext_raw, uint8_t new_flags)
{
	uint64_t flags_32 = 0;
	flags_32 = new_flags;

	return bundle_raw_set_proc_flags(ext_raw, flags_32);
}

//Returs 1 if current block is the last one 0 otherwise
int bundle_raw_ext_is_last_block(const uint8_t *raw)
{
	uint8_t block_flags = 0;
	int ret = 0;

	if (*raw != PRIM_B && bundle_raw_ext_get_proc_flags(raw, &block_flags) == 0) {
		if ((block_flags & B_LAST) == B_LAST)
			ret = 1;
	}

	return ret;
}

//Returns the offset start of the block with id <block_id>.
int bundle_raw_find_block_off(const uint8_t *raw, const uint8_t block_id)
{
	int next_block_off = 0;
	const uint8_t *b_pos = NULL;

	b_pos = raw;
	for (;;) {
		uint8_t actual_block_id = 0;
		//Check block type
		actual_block_id = *b_pos;
		if (actual_block_id == block_id) {
			next_block_off = b_pos - raw;
			break;
		}
		if (bundle_raw_ext_is_last_block(b_pos) == 1) {
			next_block_off = -1;
			break;
		}
		b_pos += bundle_raw_next_block_off(b_pos);
	}

	return next_block_off;
}


int bundle_raw_get_primary_field(const uint8_t *bundle_raw, const primary_field_e field_id, /*out*/char **field)
{
	int ret = 0, sdnv_field_off = 0, dict_off = 0;
	const uint8_t *b_pos = NULL;
	uint64_t field_off = 0;

	b_pos = bundle_raw;
	if (*b_pos != PRIM_B) {
		fputs("Can't find primary block.\n", stderr);
		ret = 1;
		goto end;
	}
	b_pos++; // Skip Version

	sdnv_field_off = bundle_raw_get_sdnv_off(b_pos, 2 + field_id); // Skip proc_flags, block_length and fields before target field
	b_pos += sdnv_field_off;

	if (sdnv_len(b_pos) <= sizeof(field_off)) {
		sdnv_decode(b_pos, &field_off);
	} else { // Something went wrong
		ret = 1;
		goto end;
	}

	// Get string from dict
	dict_off = bundle_raw_get_sdnv_off(b_pos, 12 - field_id);
	b_pos += dict_off;
	*field = strdup((char *)(b_pos + field_off));

end:
	return ret;
}

int bunlde_raw_get_timestamp_and_seq(const uint8_t *bundle_raw, /*out*/uint64_t *timestamp_time, /*out*/uint64_t *timestamp_seq)
{
	unsigned off = 0;

	off++; // Skip version
	off += bundle_raw_get_sdnv_off(bundle_raw + off, 10); // Get timestamp_time offset
	off += sdnv_decode(bundle_raw + off, timestamp_time);
	off += sdnv_decode(bundle_raw + off, timestamp_seq);

	return 0;
}

int bundle_raw_get_lifetime(const uint8_t *bundle_raw, /*out*/uint64_t *lifetime)
{
	unsigned off = 0;

	off++; //Skip version
	off += bundle_raw_get_sdnv_off(bundle_raw + off, 12);
	off += sdnv_decode(bundle_raw + off, lifetime);

	return 0;
}

int bundle_raw_get_payload(const uint8_t *bundle_raw, /*out*/uint8_t **payload)
{
	return bundle_raw_ext_get_block_body(bundle_raw, PAYL_B, payload);
}

int bundle_raw_get_metadata(const uint8_t *bundle_raw, /*out*/uint64_t *metadata_type, /*out*/uint8_t **metadata)
{
	uint8_t *metadata_raw = NULL;
	size_t metadata_l = 0;

	metadata_l = bundle_raw_ext_get_block_body(bundle_raw, META_B, &metadata_raw);

	sdnv_decode(metadata_raw, metadata_type);
	*metadata = (uint8_t *)malloc((metadata_l - 1) * sizeof(uint8_t));
	memcpy(*metadata, metadata_raw + 1, metadata_l - 1);

	free(metadata_raw);

	return metadata_l - 1;
}

static inline int bundle_raw_get_mmeb_codes(const uint8_t *mmeb_codes_raw, const int mmeb_codes_raw_l, /*out*/mmeb_code_s **mmeb_codes)
{
	int ret = 0, off = 0;
	mmeb_code_s **next_code = mmeb_codes;

	do {
		(*next_code) = (mmeb_code_s *)calloc(1, sizeof(mmeb_code_s));

		memcpy(&(*next_code)->fwk, mmeb_codes_raw + off, sizeof((*next_code)->fwk));
		off += sizeof((*next_code)->fwk);
		off += sdnv_decode(mmeb_codes_raw + off, (uint64_t *) & (*next_code)->sw_length);
		(*next_code)->sw_code = (uint8_t *)malloc((*next_code)->sw_length);
		memcpy((*next_code)->sw_code, mmeb_codes_raw + off, (*next_code)->sw_length);
		off += (*next_code)->sw_length;

		next_code = &(*next_code)->next;
	} while (off < mmeb_codes_raw_l);

	ret = off;

	return ret;
}

int bundle_raw_get_mmeb(const uint8_t *bundle_raw, /*out*/mmeb_body_s *mmeb)
{
	int ret = 0, total_block_off = 0, block_off = 0, body_off = 0, mmeb_off = 0;
	size_t body_l = 0, mmeb_body_l = 0;
	const uint8_t *mmeb_raw = NULL;
	uint64_t metadata_type = 0;
	mmeb_body_s **next_mmeb;

	for (;;) {
		block_off = bundle_raw_find_block_off(bundle_raw + block_off, META_B);
		if (block_off == -1) {
			ret = 1;
			goto end;
		}
		total_block_off += block_off;

		body_l = bundle_raw_get_body_off(bundle_raw + total_block_off, &body_off);
		sdnv_decode(bundle_raw + total_block_off + body_off, &metadata_type);
		if (metadata_type == MMEB_B) {
			mmeb_body_l = body_l - 1;
			mmeb_raw = bundle_raw + total_block_off + body_off + 1;
			break;
		}
	}

	//Find block
	// for (;;) {
	//  if (*(bundle_raw + block_off) == META_B) {
	//      body_l = bundle_raw_get_body_off(bundle_raw + block_off, &body_off);
	//      sdnv_decode(bundle_raw + block_off + body_off, &metadata_type);
	//      if (metadata_type == MMEB_B) {
	//          mmeb_body_l = body_l - 1;
	//          mmeb_raw = bundle_raw + block_off + body_off + 1;
	//          break;
	//      }
	//  }
	//  block_off += bundle_raw_next_block_off(bundle_raw + block_off);
	// }

	next_mmeb = &mmeb;
	for (;;) {
		(*next_mmeb)->alg_type = *(mmeb_raw + mmeb_off);
		mmeb_off += sizeof((*next_mmeb)->alg_type);

		mmeb_off += sdnv_decode(mmeb_raw + mmeb_off, &(*next_mmeb)->alg_length);

		if ((*next_mmeb)->alg_length > 0)
			mmeb_off += bundle_raw_get_mmeb_codes(mmeb_raw + mmeb_off, (*next_mmeb)->alg_length, &(*next_mmeb)->code);

		if (mmeb_off < mmeb_body_l) {
			next_mmeb = &(*next_mmeb)->next;
			(*next_mmeb) = (mmeb_body_s *)calloc(1, sizeof(mmeb_body_s));
		} else {
			break;
		}
	}

end:

	return ret;
}
/**/

/************* Wrappers *************/
int bundle_get_destination(const uint8_t *bundle_raw, /*out*/uint8_t **dest)
{
	return bundle_raw_get_primary_field(bundle_raw, DEST_SCHEME, (char **)dest);
}
int bundle_set_destination(bundle_s *bundle, const char *destination)
{
	return bundle_set_primary_entry(bundle, DEST_SCHEME, destination);
}
int bundle_get_source(const uint8_t *bundle_raw, /*out*/uint8_t **source)
{
	return bundle_raw_get_primary_field(bundle_raw, SOURCE_SCHEME, (char **)source);
}
int bundle_set_source(bundle_s *bundle, const char *source)
{
	return bundle_set_primary_entry(bundle, SOURCE_SCHEME, source);
}
/**************************/