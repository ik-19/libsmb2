/* -*-  mode:c; tab-width:8; c-basic-offset:8; indent-tabs-mode:nil;  -*- */
/*
   Copyright (C) 2018 by Ronnie Sahlberg <ronniesahlberg@gmail.com>

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU Lesser General Public License as published by
   the Free Software Foundation; either version 2.1 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public License
   along with this program; if not, see <http://www.gnu.org/licenses/>.
*/
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#ifdef HAVE_STDINT_H
#include <stdint.h>
#endif

#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif

#ifdef HAVE_STRING_H
#include <string.h>
#endif

#ifdef STDC_HEADERS
#include <stddef.h>
#endif

#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif

#ifdef HAVE_SYS_STAT_H
#include <sys/stat.h>
#endif

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <sys/socket.h>

#include "slist.h"
#include "smb2.h"
#include "libsmb2.h"
#include "libsmb2-raw.h"
#include "libsmb2-private.h"
#include "portable-endian.h"

#define container_of(ptr, type, member) ({                      \
        const typeof( ((type *)0)->member ) *__mptr = (ptr);    \
        (type *)( (char *)__mptr - offsetof(type,member) );})

struct dcerpc_context;

/* Encoder/Decoder for a DCERPC object */
typedef int (*dcerpc_coder)(struct dcerpc_context *ctx,
                            struct smb2_iovec *iov, int len,
                            void *ptr);

struct dcerpc_deferred_pointer {
        dcerpc_coder coder;
        void *ptr;
};

#define MAX_DEFERRED_PTR 1024

struct dcerpc_context {
        struct smb2_context *smb2;
        uint8_t tctx_id; /* 0:NDR32 1:NDR64 */
        uint64_t ptr_id;
        int top_level;

        int cur_ptr;
        int max_ptr;
        struct dcerpc_deferred_pointer ptrs[MAX_DEFERRED_PTR];
        void *memctx;
};

enum ptr_type {
        PTR_REF    = 0,
        PTR_UNIQUE = 1
};

/*
 * SRVSVC BEGIN:  DEFINITIONS FROM SRVSVC.IDL
 */
int dcerpc_decode_ptr(struct dcerpc_context *ctx, struct smb2_iovec *iov,
                      int len, void *ptr,
                      enum ptr_type type, dcerpc_coder coder);
int dcerpc_decode_32(struct dcerpc_context *ctx,
                     struct smb2_iovec *iov, int len, void *ptr);
int dcerpc_decode_3264(struct dcerpc_context *ctx,
                       struct smb2_iovec *iov, int len, void *ptr);
int dcerpc_decode_ucs2z(struct dcerpc_context *ctx,
                        struct smb2_iovec *iov, int len, void *ptr);
int dcerpc_encode_ptr(struct dcerpc_context *ctx, struct smb2_iovec *iov,
                      int len, void *ptr,
                      enum ptr_type type, dcerpc_coder coder);
int dcerpc_encode_ucs2z(struct dcerpc_context *ctx,
                        struct smb2_iovec *iov, int len, void *ptr);
int dcerpc_encode_32(struct dcerpc_context *ctx,
                     struct smb2_iovec *iov, int len, void *ptr);
int dcerpc_encode_3264(struct dcerpc_context *ctx, struct smb2_iovec *iov,
                       int len, uint64_t val);
int dcerpc_process_deferred_pointers(struct dcerpc_context *ctx,
                                     struct smb2_iovec *iov,
                                     int len);
void dcerpc_add_deferred_pointer(struct dcerpc_context *ctx,
                                 dcerpc_coder coder, void *ptr);
/*
	typedef struct {
		[string,charset(UTF16)] uint16 *name;
		srvsvc_ShareType type;
		[string,charset(UTF16)] uint16 *comment;
	} srvsvc_NetShareInfo1;

	typedef struct {
		uint32 count;
		[size_is(count)] srvsvc_NetShareInfo1 *array;
	} srvsvc_NetShareCtr1;

	typedef union {
		[case(0)] srvsvc_NetShareCtr0 *ctr0;
		[case(1)] srvsvc_NetShareCtr1 *ctr1;
		[case(2)] srvsvc_NetShareCtr2 *ctr2;
		[case(501)] srvsvc_NetShareCtr501 *ctr501;
		[case(502)] srvsvc_NetShareCtr502 *ctr502;
		[case(1004)] srvsvc_NetShareCtr1004 *ctr1004;
		[case(1005)] srvsvc_NetShareCtr1005 *ctr1005;
		[case(1006)] srvsvc_NetShareCtr1006 *ctr1006;
		[case(1007)] srvsvc_NetShareCtr1007 *ctr1007;
		[case(1501)] srvsvc_NetShareCtr1501 *ctr1501;
		[default] ;
	} srvsvc_NetShareCtr;
*/
/*****************
 * Function: 0x0f
	WERROR srvsvc_NetShareEnumAll (
		[in]   [string,charset(UTF16)] uint16 *server_unc,
		[in,out,ref]   uint32 *level,
		[in,out,switch_is(level),ref] srvsvc_NetShareCtr *ctr,
		[in]   uint32 max_buffer,
		[out,ref]  uint32 *totalentries,
		[in,out]   uint32 *resume_handle
		);
******************/
#define SRVSVC_NETSHAREENUMALL 15

struct srvsvc_netsharectr {
        uint32_t level;
        union {
                struct srvsvc_netsharectr1 ctr1;
        };
};

struct srvsvc_netshareenumall_req {
        struct ucs2 *server;
        uint32_t level;
        struct srvsvc_netsharectr *ctr;
        uint32_t max_buffer;
        uint32_t resume_handle;
};

struct srvsvc_netshareenumall_rep {
        uint32_t level;
        struct srvsvc_netsharectr *ctr;
        uint32_t total_entries;
        uint32_t resume_handle;

        uint32_t status;
};

int
srvsvc_NetShareCtr1_encoder(struct dcerpc_context *ctx,
                            struct smb2_iovec *iov, int len,
                            void *ptr)
{
        /* just encode a fake array with 0 count and no pointer */
        len = dcerpc_encode_3264(ctx, iov, len, 0);
        len = dcerpc_encode_3264(ctx, iov, len, 0);

        len = dcerpc_process_deferred_pointers(ctx, iov, len);
        return len;
}

int
srvsvc_NetShareCtr_encoder(struct dcerpc_context *ctx,
                           struct smb2_iovec *iov, int len,
                           void *ptr)
{
        /* just encode a fake union with case 1 */
        len = dcerpc_encode_3264(ctx, iov, len, 1);
        len = dcerpc_encode_ptr(ctx, iov, len, "dummy pointer",
                                PTR_UNIQUE, srvsvc_NetShareCtr1_encoder);

        len = dcerpc_process_deferred_pointers(ctx, iov, len);
        return len;
}

int
srvsvc_NetShareCtr1_array_decoder(struct dcerpc_context *ctx,
                            struct smb2_iovec *iov, int len,
                            void *ptr)
{
        struct srvsvc_netshareinfo1 *nsi1 = ptr;
        uint64_t p;

        len = dcerpc_decode_3264(ctx, iov, len, &p);

        while (p--) {
                len = dcerpc_decode_ptr(ctx, iov, len, &nsi1->name,
                                        PTR_UNIQUE,
                                        dcerpc_decode_ucs2z);
                len = dcerpc_decode_32(ctx, iov, len, &nsi1->type);
                len = dcerpc_decode_ptr(ctx, iov, len, &nsi1->comment,
                                        PTR_UNIQUE,
                                        dcerpc_decode_ucs2z);
                nsi1++;
        }

        len = dcerpc_process_deferred_pointers(ctx, iov, len);
        return len;
}

int
srvsvc_NetShareCtr1_decoder(struct dcerpc_context *ctx,
                            struct smb2_iovec *iov, int len,
                            void *ptr)
{
        struct srvsvc_netsharectr1 *ctr1 = ptr;

        len = dcerpc_decode_32(ctx, iov, len, &ctr1->count);

        ctr1->array = smb2_alloc_data(ctx->smb2, ctx->memctx,
                     ctr1->count * sizeof(struct srvsvc_netshareinfo1));
        if (ctr1->array == NULL) {
                return -1;
        }

        len = dcerpc_decode_ptr(ctx, iov, len, ctr1->array,
                                PTR_UNIQUE,
                                srvsvc_NetShareCtr1_array_decoder);

        len = dcerpc_process_deferred_pointers(ctx, iov, len);
        return len;
}

int
srvsvc_NetShareCtr_decoder(struct dcerpc_context *ctx,
                           struct smb2_iovec *iov, int len,
                           void *ptr)
{
        struct srvsvc_netsharectr *ctr = ptr;
        uint64_t p;

        len = dcerpc_decode_3264(ctx, iov, len, &p);
        ctr->level = (uint32_t)p;

        switch (ctr->level) {
        case 1:
                len = dcerpc_decode_ptr(ctx, iov, len, &ctr->ctr1,
                                        PTR_UNIQUE,
                                        srvsvc_NetShareCtr1_decoder);
                break;
        };

        len = dcerpc_process_deferred_pointers(ctx, iov, len);
        return len;
}

int
srvsvc_netshareenumall_encoder(struct dcerpc_context *ctx,
                               struct smb2_iovec *iov, int len,
                               void *ptr)
{
        struct srvsvc_netshareenumall_req *req = ptr;

        len = dcerpc_encode_ptr(ctx, iov, len, req->server,
                                PTR_UNIQUE, dcerpc_encode_ucs2z);
        len = dcerpc_encode_ptr(ctx, iov, len, &req->level,
                                PTR_REF, dcerpc_encode_32);
        len = dcerpc_encode_ptr(ctx, iov, len, "dummy pointer",
                                PTR_REF, srvsvc_NetShareCtr_encoder);
        len = dcerpc_encode_ptr(ctx, iov, len, &req->max_buffer,
                                PTR_REF, dcerpc_encode_32);
        len = dcerpc_encode_ptr(ctx, iov, len, &req->resume_handle,
                                PTR_UNIQUE, dcerpc_encode_32);

        len = dcerpc_process_deferred_pointers(ctx, iov, len);
        return len;
}

int
srvsvc_netshareenumall_decoder(struct dcerpc_context *ctx,
                               struct smb2_iovec *iov, int len,
                               void *ptr)
{
        struct srvsvc_netshareenumall_rep *rep = ptr;
        struct srvsvc_netsharectr *ctr;

        len = dcerpc_decode_ptr(ctx, iov, len, &rep->level,
                                PTR_REF, dcerpc_decode_32);

        ctr = smb2_alloc_data(ctx->smb2, ctx->memctx,
                              sizeof(struct srvsvc_netsharectr));
        if (ctr == NULL) {
                return -1;
        }

        rep->ctr = ctr;
        len = dcerpc_decode_ptr(ctx, iov, len, rep->ctr,
                                PTR_REF, srvsvc_NetShareCtr_decoder);

        len = dcerpc_decode_ptr(ctx, iov, len, &rep->total_entries,
                                PTR_REF, dcerpc_decode_32);
        
        len = dcerpc_decode_ptr(ctx, iov, len, &rep->resume_handle,
                                PTR_UNIQUE, dcerpc_decode_32);

        len = dcerpc_decode_32(ctx, iov, len, &rep->status);
        
        return len;
}

//static void
//srvsvc_netshareenumall_req_free(struct srvsvc_netshareenumall_req *req);
/*
 * SRVSVC END
 */

struct dcerpc_uuid {
        uint32_t v1;
        uint16_t v2;
        uint16_t v3;
        uint64_t v4;
};

typedef struct p_syntax_id {
        struct dcerpc_uuid uuid;
        uint16_t vers;
        uint16_t vers_minor;
} p_syntax_id_t;

struct dcerpc_transfer_syntax {
        struct dcerpc_uuid uuid;
        uint16_t vers;
};

#define SRVSVC_UUID    0x4b324fc8, 0x1670, 0x01d3, 0x12785a47bf6ee188
#define NDR32_UUID     0x8a885d04, 0x1ceb, 0x11c9, 0x9fe808002b104860
#define NDR64_UUID     0x71710533, 0xbeba, 0x4937, 0x8319b5dbef9ccc36

p_syntax_id_t srvsvc_interface = {
        {SRVSVC_UUID}, 3, 0
};

p_syntax_id_t ndr32_syntax = {
        {NDR32_UUID}, 2
};

p_syntax_id_t ndr64_syntax = {
        {NDR64_UUID}, 1
};

struct dcerpc_header {
        uint8_t  rpc_vers;
        uint8_t  rpc_vers_minor;
        uint8_t  PTYPE;
        uint8_t  pfc_flags;
        uint8_t  packed_drep[4];
        uint16_t frag_length;
        uint16_t auth_length;
        uint32_t call_id;
};

struct dcerpc_bind_pdu {
        uint16_t max_xmit_frag;
        uint16_t max_recv_frag;
        uint32_t assoc_group_id;

        /* presentation context list */
        p_syntax_id_t *abstract_syntax;
};

#define ACK_RESULT_ACCEPTANCE         0
#define ACK_RESULT_USER_REJECTION     1
#define ACK_RESULT_PROVIDER_REJECTION 2

#define ACK_REASON_REASON_NOT_SPECIFIED                     0
#define ACK_REASON_ABSTRACT_SYNTAX_NOT_SUPPORTED            1
#define ACK_REASON_PROPOSED_TRANSFER_SYNTAXES_NOT_SUPPORTED 2
#define ACK_REASON_PROTOCOL_VERSION_NOT_SUPPORTED           4

struct dcerpc_bind_context_results {
        uint16_t ack_result;
        uint16_t ack_reason;
        struct dcerpc_uuid uuid;
        uint32_t syntax_version;
};

#define MAX_ACK_RESULTS 4
struct dcerpc_bind_ack_pdu {
        uint16_t max_xmit_frag;
        uint16_t max_recv_frag;
        uint32_t assoc_group_id;

        /* presentation context list */
        uint8_t num_results;
        struct dcerpc_bind_context_results results[MAX_ACK_RESULTS];
};

struct dcerpc_request_pdu {
        uint32_t alloc_hint;
        uint16_t context_id;
        uint16_t opnum;

      /* optional field for request, only present if the PFC_OBJECT_UUID
         * field is non-zero */
      /*  uuid_t  object;              24:16 object UID */

      /* stub data, 8-octet aligned 
                   .
                   .
                   .                 */
};

struct dcerpc_response_pdu {
        uint32_t alloc_hint;
        uint16_t context_id;
        uint8_t cancel_count;
        uint8_t reserverd;
      /* stub data, 8-octet aligned 
                   .
                   .
                   .                 */
};

/* PDU Types */
#define PDU_TYPE_REQUEST             0
#define PDU_TYPE_PING                1
#define PDU_TYPE_RESPONSE            2
#define PDU_TYPE_FAULT               3
#define PDU_TYPE_WORKING             4
#define PDU_TYPE_NOCALL              5
#define PDU_TYPE_REJECT              6
#define PDU_TYPE_ACK                 7
#define PDU_TYPE_CL_CANCEL           8
#define PDU_TYPE_FACK                9
#define PDU_TYPE_CANCEL_ACK         10
#define PDU_TYPE_BIND               11
#define PDU_TYPE_BIND_ACK           12
#define PDU_TYPE_BIND_NAK           13
#define PDU_TYPE_ALTER_CONTEXT      14
#define PDU_TYPE_ALTER_CONTEXT_RESP 15
#define PDU_TYPE_SHUTDOWN           17
#define PDU_TYPE_CO_CANCEL          18
#define PDU_TYPE_ORPHANED           19


/* Flags */
#define PFC_FIRST_FRAG      0x01
#define PFC_LAST_FRAG       0x02
#define PFC_PENDING_CANCEL  0x04
#define PFC_RESERVED_1      0x08
#define PFC_CONC_MPX        0x10
#define PFC_DID_NOT_EXECUTE 0x20
#define PFC_MAYBE           0x40
#define PFC_OBJECT_UUID     0x80

/* Data representation */
/* Integer */
#define DCERPC_DR_BYTE_ORDER_BIG_ENDIAN         0x00
#define DCERPC_DR_BYTE_ORDER_LITTLE_ENDIAN      0x10
/* Character */
#define DCERPC_DR_CHARACTER_ASCII               0x00
#define DCERPC_DR_CHARACTER_EBCDIC              0x01

#define NSE_BUF_SIZE 1024

struct dcerpc_pdu {
        struct dcerpc_header hdr;

        union {
                struct dcerpc_bind_pdu bind;
                struct dcerpc_bind_ack_pdu bind_ack;
                struct dcerpc_request_pdu req;
                struct dcerpc_response_pdu rsp;
        };

        /* optional authentication verifier */
        /* following fields present iff auth_length != 0 */
        /*auth_verifier_co_t   auth_verifier; */


        dcerpc_coder coder;
        void *payload;
};

struct smb2nse {
        smb2_command_cb cb;
        void *cb_data;

        smb2_file_id file_id;
        uint8_t buf[NSE_BUF_SIZE];
        struct dcerpc_context ctx;
};

static void
dcerpc_free_pdu(struct dcerpc_context *ctx, struct dcerpc_pdu *pdu)
{
        if (pdu == NULL) {
                return;
        }

        smb2_free_data(ctx->smb2, pdu->payload);
        free(pdu);
}

static struct dcerpc_pdu *
dcerpc_allocate_pdu(struct dcerpc_context *ctx, int size)
{
        struct dcerpc_pdu *pdu;

        pdu = malloc(sizeof(struct dcerpc_pdu));
        if (pdu == NULL) {
                smb2_set_error(ctx->smb2, "Failed to allocate DCERPC PDU");
                return NULL;
        }

        memset(pdu, 0, sizeof(struct dcerpc_pdu));

        if (size) {
                pdu->payload = smb2_alloc_init(ctx->smb2, size);
                ctx->memctx = pdu->payload;
                if (pdu->payload == NULL) {
                        smb2_set_error(ctx->smb2, "Failed to allocate "
                                       "dcerpc payload context");
                        dcerpc_free_pdu(ctx, pdu);
                        return NULL;
                }
        }

        return pdu;
}

void
dcerpc_add_deferred_pointer(struct dcerpc_context *ctx,
                            dcerpc_coder coder, void *ptr)
{
        ctx->ptrs[ctx->max_ptr].coder = coder;
        ctx->ptrs[ctx->max_ptr].ptr = ptr;
        ctx->max_ptr++;
}

int
dcerpc_process_deferred_pointers(struct dcerpc_context *ctx,
                                 struct smb2_iovec *iov,
                                 int len)
{
        struct dcerpc_deferred_pointer *dp;
        int idx;

        while (ctx->cur_ptr != ctx->max_ptr) {
                idx = ctx->cur_ptr++;
                dp = &ctx->ptrs[idx];
                len = dp->coder(ctx, iov, len, dp->ptr);
        }
        return len;
}

int
dcerpc_decode_32(struct dcerpc_context *ctx, struct smb2_iovec *iov,
                 int len, void *ptr)
{
        smb2_get_uint32(iov, len, ptr);
        len += 4;

        return len;
}

int
dcerpc_encode_32(struct dcerpc_context *ctx, struct smb2_iovec *iov,
                 int len, void *ptr)
{
        uint32_t val = *((uint32_t *)ptr);

        len = (len + 3) & ~3;

        smb2_set_uint32(iov, len, val);
        len += 4;

        return len;
}

int
dcerpc_encode_16(struct dcerpc_context *ctx, struct smb2_iovec *iov,
                 int len, void *ptr)
{
        uint16_t val = *((uint16_t *)ptr);

        len = (len + 1) & ~1;

        smb2_set_uint16(iov, len, val);
        len += 2;

        return len;
}

/* Encode words that vary in size depending on the tranport syntax */
int
dcerpc_encode_3264(struct dcerpc_context *ctx, struct smb2_iovec *iov,
                   int len, uint64_t val)
{
        if (ctx->tctx_id) {
                len = (len + 7) & ~7;

                /* NDR64 */
                smb2_set_uint64(iov, len, val);
                len += 8;
        } else {
                len = (len + 3) & ~3;

                /* NDR32 */
                smb2_set_uint32(iov, len, val);
                len += 4;
        }
        return len;
}

int
dcerpc_decode_3264(struct dcerpc_context *ctx, struct smb2_iovec *iov,
                   int len, void *ptr)
{
        uint32_t u32;
        uint64_t u64;

        if (ctx->tctx_id) {
                len = (len + 7) & ~7;

                /* NDR64 */
                smb2_get_uint64(iov, len, &u64);
                *(uint64_t *)ptr = u64;
                len += 8;
        } else {
                len = (len + 3) & ~3;

                /* NDR32 */
                smb2_get_uint32(iov, len, &u32);
                *(uint64_t *)ptr = u32;
                len += 4;
        }
        return len;
}

int
dcerpc_encode_ptr(struct dcerpc_context *ctx, struct smb2_iovec *iov,
                  int len, void *ptr,
                  enum ptr_type type, dcerpc_coder coder)
{
        int top_level = ctx->top_level;

        if (ctx->tctx_id) {
                len = (len + 7) & ~7;
        } else {
                len = (len + 3) & ~3;
        }

        switch (type) {
        case PTR_REF:
                if (ctx->top_level) {
                        ctx->top_level = 0;
                        len = coder(ctx, iov, len, ptr);
                        ctx->top_level = top_level;
                        return len;
                }
                
                ctx->ptr_id++;
                len = dcerpc_encode_3264(ctx, iov, len, ctx->ptr_id);
                dcerpc_add_deferred_pointer(ctx, coder, ptr);
                break;
        case PTR_UNIQUE:
                if (ptr == NULL) {
                        len = dcerpc_encode_3264(ctx, iov, len, 0);
                        return len;
                }
                
                ctx->ptr_id++;
                len = dcerpc_encode_3264(ctx, iov, len, ctx->ptr_id);
                if (ctx->top_level) {
                        ctx->top_level = 0;
                        len = coder(ctx, iov, len, ptr);
                        ctx->top_level = top_level;
                } else {
                        dcerpc_add_deferred_pointer(ctx, coder, ptr);
                }
                break;
        }

        return len;
}

int
dcerpc_decode_ptr(struct dcerpc_context *ctx, struct smb2_iovec *iov,
                  int len, void *ptr,
                  enum ptr_type type, dcerpc_coder coder)
{
        int top_level = ctx->top_level;
        uint64_t p;

        if (ctx->tctx_id) {
                len = (len + 7) & ~7;
        } else {
                len = (len + 3) & ~3;
        }

        switch (type) {
        case PTR_REF:
                if (ctx->top_level) {
                        ctx->top_level = 0;
                        len = coder(ctx, iov, len, ptr);
                        ctx->top_level = top_level;
                        return len;
                }
                
                len = dcerpc_decode_3264(ctx, iov, len, &p);
                dcerpc_add_deferred_pointer(ctx, coder, ptr);
                break;
        case PTR_UNIQUE:
                len = dcerpc_decode_3264(ctx, iov, len, &p);
                if (p == 0) {
                        return len;
                }
                
                if (ctx->top_level) {
                        ctx->top_level = 0;
                        len = coder(ctx, iov, len, ptr);
                        ctx->top_level = top_level;
                } else {
                        dcerpc_add_deferred_pointer(ctx, coder, ptr);
                }
                break;
        }

        return len;
}

/* Encode a \0 terminated ucs2 string */
int dcerpc_encode_ucs2z(struct dcerpc_context *ctx,
                        struct smb2_iovec *iov, int len,
                        void *ptr)
{
        struct ucs2 *str = ptr;
        uint16_t zero = 0;
        int i;

        len = dcerpc_encode_3264(ctx, iov, len, str->len + 1);
        len = dcerpc_encode_3264(ctx, iov, len, 0);
        len = dcerpc_encode_3264(ctx, iov, len, str->len + 1);
        for (i = 0; i < str->len; i++) {
                len = dcerpc_encode_16(ctx, iov, len, &str->val[i]);
        }
        len = dcerpc_encode_16(ctx, iov, len, &zero);
        return len;
}

int dcerpc_decode_ucs2z(struct dcerpc_context *ctx,
                        struct smb2_iovec *iov, int len,
                        void *ptr)
{
        uint64_t max, offset, actual;
        char *str, **dst;
        const char *tmp;

        len = dcerpc_decode_3264(ctx, iov, len, &max);
        len = dcerpc_decode_3264(ctx, iov, len, &offset);
        len = dcerpc_decode_3264(ctx, iov, len, &actual);

        if (len + actual * 2 > iov->len) {
                return -1;
        }
        tmp = ucs2_to_utf8((uint16_t *)(&iov->buf[len]), actual);
        len += actual * 2;

        str = smb2_alloc_data(ctx->smb2, ctx->memctx, strlen(tmp) + 1);
        if (str == NULL) {
                free(discard_const(tmp));
                return -1;
        }
        strcat(str, tmp);
        free(discard_const(tmp));

        dst = ptr;
        *dst = str;

        return len;
}

static int
dcerpc_encode_header(struct smb2_iovec *iov, struct dcerpc_header *hdr)
{
        /* Major Version */
        smb2_set_uint8(iov, 0, hdr->rpc_vers);
        /* Minor Version */
        smb2_set_uint8(iov, 1, hdr->rpc_vers_minor);
        /* Packet Type */
        smb2_set_uint8(iov, 2, hdr->PTYPE);
        /* Flags */
        smb2_set_uint8(iov, 3, hdr->pfc_flags);

        /* Data Representation */
        smb2_set_uint8(iov, 4, hdr->packed_drep[0]);
        smb2_set_uint8(iov, 5, hdr->packed_drep[1]);
        smb2_set_uint8(iov, 6, hdr->packed_drep[2]);
        smb2_set_uint8(iov, 7, hdr->packed_drep[3]);

        /* Fragment len */
        smb2_set_uint16(iov, 8, hdr->frag_length);

        /* Auth len */
        smb2_set_uint16(iov, 10, hdr->auth_length);

        /* Call id */
        smb2_set_uint32(iov, 12, hdr->call_id);

        return 16;
}

static int
dcerpc_decode_header(struct smb2_iovec *iov, struct dcerpc_header *hdr)
{
        /* Major Version */
        smb2_get_uint8(iov, 0, &hdr->rpc_vers);
        /* Minor Version */
        smb2_get_uint8(iov, 1, &hdr->rpc_vers_minor);
        /* Packet Type */
        smb2_get_uint8(iov, 2, &hdr->PTYPE);
        /* Flags */
        smb2_get_uint8(iov, 3, &hdr->pfc_flags);

        /* Data Representation */
        smb2_get_uint8(iov, 4, &hdr->packed_drep[0]);
        smb2_get_uint8(iov, 5, &hdr->packed_drep[1]);
        smb2_get_uint8(iov, 6, &hdr->packed_drep[2]);
        smb2_get_uint8(iov, 7, &hdr->packed_drep[3]);

        /* Fragment len */
        smb2_get_uint16(iov, 8, &hdr->frag_length);

        /* Auth len */
        smb2_get_uint16(iov, 10, &hdr->auth_length);

        /* Call id */
        smb2_get_uint32(iov, 12, &hdr->call_id);

        return 16;
}

static int
dcerpc_encode_uuid(struct dcerpc_context *ctx,
                   struct smb2_iovec *iov, int len,
                   struct dcerpc_uuid *uuid)
{
        smb2_set_uint32(iov, len, uuid->v1);
        len += 4;
        smb2_set_uint16(iov, len, uuid->v2);
        len += 2;
        smb2_set_uint16(iov, len, uuid->v3);
        len += 2;

        smb2_set_uint64(iov, len, (uuid->v4 >> 56) & 0xff);
        len += 1;
        smb2_set_uint64(iov, len, (uuid->v4 >> 48) & 0xff);
        len += 1;
        smb2_set_uint64(iov, len, (uuid->v4 >> 40) & 0xff);
        len += 1;
        smb2_set_uint64(iov, len, (uuid->v4 >> 32) & 0xff);
        len += 1;
        smb2_set_uint64(iov, len, (uuid->v4 >> 24) & 0xff);
        len += 1;
        smb2_set_uint64(iov, len, (uuid->v4 >> 16) & 0xff);
        len += 1;
        smb2_set_uint64(iov, len, (uuid->v4 >>  8) & 0xff);
        len += 1;
        smb2_set_uint64(iov, len, (uuid->v4      ) & 0xff);
        len += 1;

        return len;
}

static int
dcerpc_decode_uuid(struct dcerpc_context *ctx,
                   struct smb2_iovec *iov, int len,
                   struct dcerpc_uuid *uuid)
{
        uint8_t ch;
        int i;

        smb2_get_uint32(iov, len, &uuid->v1);
        len += 4;
        smb2_get_uint16(iov, len, &uuid->v2);
        len += 2;
        smb2_get_uint16(iov, len, &uuid->v3);
        len += 2;

        uuid->v4 = 0;
        for (i = 0; i < 8; i++) {
                smb2_get_uint8(iov, len, &ch);
                uuid->v4 = (uuid->v4 << 8) | ch;
                len += 1;
        }

        return len;
}

static int
dcerpc_encode_bind(struct dcerpc_context *ctx,
                   struct dcerpc_bind_pdu *bind,
                   struct smb2_iovec *iov, int len)
{
        /* Max Xmit Frag */
        smb2_set_uint16(iov, len, bind->max_xmit_frag);
        len += 2;

        /* Max Recv Frag */
        smb2_set_uint16(iov, len, bind->max_recv_frag);
        len += 2;

        /* Association Group */
        smb2_set_uint32(iov, len, bind->assoc_group_id);
        len += 4;

        /* Number Of Context Items */
        smb2_set_uint8(iov, len, 2);
        len += 4;

        /* Context Id[0]: NDR32 */
        smb2_set_uint16(iov, len, 0);
        len += 2;
        /* Num Trans Items */
        smb2_set_uint8(iov, len, 1);
        len += 2;
        /* Abstract Syntax */
        len = dcerpc_encode_uuid(ctx, iov, len, &bind->abstract_syntax->uuid);
        smb2_set_uint16(iov, len, bind->abstract_syntax->vers);
        len += 2;
        smb2_set_uint16(iov, len, bind->abstract_syntax->vers_minor);
        len += 2;
        /* Transport Syntax */
        len = dcerpc_encode_uuid(ctx, iov, len, &ndr32_syntax.uuid);
        smb2_set_uint16(iov, len, ndr32_syntax.vers);
        len += 4;
        /* Context Id[1]: NDR64 */
        smb2_set_uint16(iov, len, 1);
        len += 2;
        /* Num Trans Items */
        smb2_set_uint8(iov, len, 1);
        len += 2;
        /* Abstract Syntax */
        len = dcerpc_encode_uuid(ctx, iov, len, &bind->abstract_syntax->uuid);
        smb2_set_uint16(iov, len, bind->abstract_syntax->vers);
        len += 2;
        smb2_set_uint16(iov, len, bind->abstract_syntax->vers_minor);
        len += 2;
        /* Transport Syntax */
        len = dcerpc_encode_uuid(ctx, iov, len, &ndr64_syntax.uuid);
        smb2_set_uint16(iov, len, ndr64_syntax.vers);
        len += 4;
        
        /* Fixup fragment length */
        smb2_set_uint16(iov, 8, len);
        
        return len;
}

static int
dcerpc_encode_request(struct dcerpc_context *ctx,
                      struct dcerpc_request_pdu *req,
                      struct smb2_iovec *iov, int len)
{
        /* Alloc Hint */
        smb2_set_uint32(iov, len, req->alloc_hint);
        len += 4;

        /* Context ID */
        smb2_set_uint16(iov, len, req->context_id);
        len += 2;
        
        /* Opnum */
        smb2_set_uint16(iov, len, req->opnum);
        len += 2;

        return len;
}

static int
dcerpc_encode_pdu(struct dcerpc_context *ctx, struct dcerpc_pdu *pdu,
                  struct smb2_iovec *iov)
{
        int len = 0;

        len = dcerpc_encode_header(iov, &pdu->hdr);

        switch (pdu->hdr.PTYPE) {
        case PDU_TYPE_BIND:
                len = dcerpc_encode_bind(ctx, &pdu->bind, iov, len);
                break;
        case PDU_TYPE_REQUEST:
                len = dcerpc_encode_request(ctx, &pdu->req, iov, len);
                break;
        default:
                smb2_set_error(ctx->smb2, "DCERPC No encoder for PDU type %d",
                               pdu->hdr.PTYPE);
                return -1;
        }

        return len;
}

static int
dcerpc_decode_bind_ack(struct dcerpc_context *ctx,
                       struct dcerpc_bind_ack_pdu *bind_ack,
                       struct smb2_iovec *iov, int len)
{
        int i;
        uint16_t sec_addr_len;

        /* Max Xmit Frag */
        smb2_get_uint16(iov, len, &bind_ack->max_xmit_frag);
        len += 2;

        /* Max Recv Frag */
        smb2_get_uint16(iov, len, &bind_ack->max_recv_frag);
        len += 2;

        /* Association Group */
        smb2_get_uint32(iov, len, &bind_ack->assoc_group_id);
        len += 4;

        /* Secondary Addres Length */
        smb2_get_uint16(iov, len, &sec_addr_len);
        len += 2;

        /* Skip the secondary address and realign to 32bit */
        len += sec_addr_len;
        len = (len + 3) & ~3;

        /* Number Of Results */
        smb2_get_uint8(iov, len, &bind_ack->num_results);
        len += 4;

        for (i = 0; i < bind_ack->num_results; i++) {
                smb2_get_uint16(iov, len, &bind_ack->results[i].ack_result);
                len += 2;

                smb2_get_uint16(iov, len, &bind_ack->results[i].ack_reason);
                len += 2;

                len = dcerpc_decode_uuid(ctx, iov, len,
                                         &bind_ack->results[i].uuid);

                smb2_get_uint32(iov, len,
                                &bind_ack->results[i].syntax_version);
                len += 4;
        }

        return len;
}

static int
dcerpc_decode_response(struct dcerpc_context *ctx,
                       struct dcerpc_response_pdu *rsp,
                       struct smb2_iovec *iov, int len)
{
        struct dcerpc_pdu *pdu = container_of(rsp, struct dcerpc_pdu, rsp);

        /* Alloc Hint */
        smb2_get_uint32(iov, len, &rsp->alloc_hint);
        len += 4;

        if (rsp->alloc_hint < 0 || rsp->alloc_hint > 16*1024*1024) {
                smb2_set_error(ctx->smb2, "DCERPC RESPONSE alloc_hint out "
                               "of range.");
                return -1;
        }

        /* Context Id */
        smb2_get_uint16(iov, len, &rsp->context_id);
        len += 2;
        
        /* Cancel Count */
        smb2_get_uint8(iov, len, &rsp->cancel_count);
        len += 2;


        /* decode the blob */
        ctx->top_level = 1;
        ctx->cur_ptr = 0;
        ctx->max_ptr = 0;
        if (pdu->coder(ctx, iov, len, pdu->payload) < 0) {
                return -1;
        }

        len += rsp->alloc_hint;

        return len;
}

static int
dcerpc_decode_pdu(struct dcerpc_context *ctx, struct dcerpc_pdu *pdu,
                  struct smb2_iovec *iov)
{
        int len = 0;

        len = dcerpc_decode_header(iov, &pdu->hdr);

        switch (pdu->hdr.PTYPE) {
        case PDU_TYPE_BIND_ACK:
                len = dcerpc_decode_bind_ack(ctx, &pdu->bind_ack, iov, len);
                break;
        case PDU_TYPE_RESPONSE:
                len = dcerpc_decode_response(ctx, &pdu->rsp, iov, len);
                break;
        default:
                smb2_set_error(ctx->smb2, "DCERPC No decoder for PDU type %d",
                               pdu->hdr.PTYPE);
                dcerpc_free_pdu(ctx, pdu);
                return -1;
        }

        return len;
}

static int
encode_request(struct dcerpc_context *ctx, uint8_t *buf, int max,
               uint8_t opnum, dcerpc_coder coder, void *ptr)
{
        int len = 0;
        struct smb2_iovec iov = {buf, max, NULL};
        struct dcerpc_pdu pdu;

        memset(buf, 0, max);
        memset(&pdu, 0, sizeof(struct dcerpc_pdu));
        pdu.hdr.rpc_vers = 5;
        pdu.hdr.rpc_vers_minor = 0;
        pdu.hdr.PTYPE = PDU_TYPE_REQUEST;
        pdu.hdr.pfc_flags = PFC_FIRST_FRAG | PFC_LAST_FRAG;
        pdu.hdr.packed_drep[0] = DCERPC_DR_BYTE_ORDER_LITTLE_ENDIAN |
                DCERPC_DR_CHARACTER_ASCII;
        pdu.hdr.frag_length = 0;
        pdu.hdr.auth_length = 0;
        pdu.hdr.call_id = 2;
        pdu.req.alloc_hint = 0;
        pdu.req.context_id = ctx->tctx_id;
        pdu.req.opnum = opnum;
        
        len = dcerpc_encode_pdu(ctx, &pdu, &iov);

        /* encode the blob */
        ctx->top_level = 1;
        ctx->cur_ptr = 0;
        ctx->max_ptr = 0;
        len = coder(ctx, &iov, len, ptr);

        /* Fixup frag_length and alloc_hint */
        smb2_set_uint16(&iov,  8, len);
        smb2_set_uint32(&iov, 16, len - 24);

        return len;
}

static void
nse_enum_read_cb(struct smb2_context *smb2, int status,
                 void *command_data, void *private_data)
{
        struct smb2nse *nse = private_data;
        struct smb2_read_reply *rep = command_data;
        struct dcerpc_pdu *dce_pdu;
        struct smb2_iovec iov;
        struct srvsvc_netshareenumall_rep *nse_rep;
        int ret;

        if (status != SMB2_STATUS_SUCCESS) {
                nse->cb(smb2, -nterror_to_errno(status),
                       NULL, nse->cb_data);
                free(nse);
                return;
        }

        dce_pdu = dcerpc_allocate_pdu(&nse->ctx,
                      sizeof(struct srvsvc_netshareenumall_rep));
        if (dce_pdu == NULL) {
                nse->cb(smb2, -ENOMEM, NULL, nse->cb_data);
                free(nse);
                return;
        }
        dce_pdu->coder = srvsvc_netshareenumall_decoder;

        iov.buf = &nse->buf[0];
        iov.len = rep->data_length;
        iov.free = NULL;
        ret = dcerpc_decode_pdu(&nse->ctx, dce_pdu, &iov);
        if (ret < 0) {
                nse->cb(smb2, -nterror_to_errno(status),
                       NULL, nse->cb_data);
                dcerpc_free_pdu(&nse->ctx, dce_pdu);
                free(nse);
                return;
        }

        if (dce_pdu->hdr.PTYPE != PDU_TYPE_RESPONSE) {
                smb2_set_error(smb2, "DCERPC response was not a RESPONSE");
                nse->cb(smb2, -EINVAL, NULL, nse->cb_data);
                dcerpc_free_pdu(&nse->ctx, dce_pdu);
                free(nse);
                return;
        }

        nse_rep = dce_pdu->payload;
        nse->cb(smb2, nse_rep->status, &nse_rep->ctr->ctr1, nse->cb_data);
        
        dcerpc_free_pdu(&nse->ctx, dce_pdu);
}

static void
nse_enum_write_cb(struct smb2_context *smb2, int status,
                  void *command_data, void *private_data)
{
        struct smb2nse *nse = private_data;
        struct smb2_read_request req;
        struct smb2_pdu *pdu;

        if (status != SMB2_STATUS_SUCCESS) {
                nse->cb(smb2, -nterror_to_errno(status),
                       NULL, nse->cb_data);
                free(nse);
                return;
        }

        memset(&req, 0, sizeof(struct smb2_read_request));
        req.flags = 0;
        req.length = NSE_BUF_SIZE;
        req.offset = 0;
        req.buf = &nse->buf[0];
        memcpy(req.file_id, nse->file_id, SMB2_FD_SIZE);
        req.minimum_count = 0;
        req.channel = SMB2_CHANNEL_NONE;
        req.remaining_bytes = 0;

        pdu = smb2_cmd_read_async(smb2, &req, nse_enum_read_cb, nse);
        if (pdu == NULL) {
                nse->cb(smb2, -ENOMEM,
                       NULL, nse->cb_data);
                free(nse);
                return;
        }

        smb2_queue_pdu(smb2, pdu);
}

#define MAX_SERVER_NAME 64
static void
nse_bind_read_cb(struct smb2_context *smb2, int status,
                 void *command_data, void *private_data)
{
        struct smb2nse *nse = private_data;
        struct smb2_read_reply *rep = command_data;
        struct dcerpc_pdu *dce_pdu;
        struct smb2_write_request req;
        struct smb2_pdu *pdu;
        struct srvsvc_netshareenumall_req ea_req;
        int i, len;
        char server[MAX_SERVER_NAME];
        struct ucs2 *ucs2_unc = NULL;
        struct smb2_iovec iov;

        if (status != SMB2_STATUS_SUCCESS) {
                nse->cb(smb2, -nterror_to_errno(status),
                       NULL, nse->cb_data);
                free(nse);
                return;
        }

        dce_pdu = dcerpc_allocate_pdu(&nse->ctx, 0);
        if (dce_pdu == NULL) {
                nse->cb(smb2, -ENOMEM, NULL, nse->cb_data);
                free(nse);
                return;
        }

        iov.buf = &nse->buf[0];
        iov.len = rep->data_length;
        iov.free = NULL;
        if (dcerpc_decode_pdu(&nse->ctx, dce_pdu, &iov) < 0) {
                nse->cb(smb2, -nterror_to_errno(status),
                       NULL, nse->cb_data);
                dcerpc_free_pdu(&nse->ctx, dce_pdu);
                free(nse);
                return;
        }

        if (dce_pdu->hdr.PTYPE != PDU_TYPE_BIND_ACK) {
                smb2_set_error(smb2, "DCERPC response was not a BIND_ACK");
                nse->cb(smb2, -EINVAL, NULL, nse->cb_data);
                dcerpc_free_pdu(&nse->ctx, dce_pdu);
                free(nse);
                return;
        }

        if (dce_pdu->bind_ack.num_results < 1) {
                smb2_set_error(smb2, "No results in BIND ACK");
                nse->cb(smb2, -EINVAL, NULL, nse->cb_data);
                dcerpc_free_pdu(&nse->ctx, dce_pdu);
                free(nse);
                return;
        }
        for (i = 0; i < dce_pdu->bind_ack.num_results; i++) {
                if (dce_pdu->bind_ack.results[i].ack_result !=
                    ACK_RESULT_ACCEPTANCE) {
                        continue;
                }

                nse->ctx.tctx_id = i;
                break;
        }
        if (i == dce_pdu->bind_ack.num_results) {
                smb2_set_error(smb2, "Bind rejected all contexts");
                nse->cb(smb2, -EINVAL, NULL, nse->cb_data);
                dcerpc_free_pdu(&nse->ctx, dce_pdu);
                free(nse);
                return;
        }
        dcerpc_free_pdu(&nse->ctx, dce_pdu);

        memset(&ea_req, 0, sizeof(struct srvsvc_netshareenumall_req ));
        if (strlen(smb2->server) > MAX_SERVER_NAME - 3) {
                smb2_set_error(smb2, "Server name too long");
                nse->cb(smb2, -EINVAL, NULL, nse->cb_data);
                free(nse);
                return;
        }
        snprintf(server, MAX_SERVER_NAME, "\\\\%s", smb2->server);
        ucs2_unc = utf8_to_ucs2(server);
        if (ucs2_unc == NULL) {
                smb2_set_error(smb2, "Failed to convert server unc to ucs2");
                nse->cb(smb2, -ENOMEM, NULL, nse->cb_data);
                free(nse);
                return;
        }
                
        ea_req.server = ucs2_unc;
        ea_req.level = 1;
        ea_req.ctr = NULL;
        ea_req.max_buffer = 0xffffffff;
        ea_req.resume_handle = 0;

        len = encode_request(&nse->ctx, &nse->buf[0], 1024,
                             SRVSVC_NETSHAREENUMALL,
                             srvsvc_netshareenumall_encoder,
                             &ea_req);
        free(ucs2_unc);
        if (len < 0) {
                nse->cb(smb2, -ENOMEM, NULL, nse->cb_data);
                free(nse);
                return;
        }

        memset(&req, 0, sizeof(struct smb2_write_request));
        req.length = len;
        req.offset = 0;
        req.buf = &nse->buf[0];
        memcpy(req.file_id, nse->file_id, SMB2_FD_SIZE);
        req.channel = SMB2_CHANNEL_NONE;
        req.remaining_bytes = 0;
        req.flags = 0;

        pdu = smb2_cmd_write_async(smb2, &req, nse_enum_write_cb, nse);
        if (pdu == NULL) {
                nse->cb(smb2, -ENOMEM, NULL, nse->cb_data);
                free(nse);
                return;
        }
        smb2_queue_pdu(smb2, pdu);
}

static void
nse_bind_write_cb(struct smb2_context *smb2, int status,
                  void *command_data, void *private_data)
{
        struct smb2nse *nse = private_data;
        struct smb2_read_request req;
        struct smb2_pdu *pdu;

        if (status != SMB2_STATUS_SUCCESS) {
                nse->cb(smb2, -nterror_to_errno(status),
                       NULL, nse->cb_data);
                free(nse);
                return;
        }

        memset(&req, 0, sizeof(struct smb2_read_request));
        req.flags = 0;
        req.length = NSE_BUF_SIZE;
        req.offset = 0;
        req.buf = &nse->buf[0];
        memcpy(req.file_id, nse->file_id, SMB2_FD_SIZE);
        req.minimum_count = 0;
        req.channel = SMB2_CHANNEL_NONE;
        req.remaining_bytes = 0;

        pdu = smb2_cmd_read_async(smb2, &req, nse_bind_read_cb, nse);
        if (pdu == NULL) {
                nse->cb(smb2, -ENOMEM,
                       NULL, nse->cb_data);
                free(nse);
                return;
        }

        smb2_queue_pdu(smb2, pdu);
}

static void
nse_open_cb(struct smb2_context *smb2, int status,
            void *command_data, void *private_data)
{
        struct smb2nse *nse = private_data;
        struct smb2_create_reply *rep = command_data;
        struct dcerpc_pdu dce_pdu;
        struct smb2_write_request req;
        struct smb2_pdu *pdu;
        struct smb2_iovec iov;
        int len;

        if (status != SMB2_STATUS_SUCCESS) {
                nse->cb(smb2, -nterror_to_errno(status),
                       NULL, nse->cb_data);
                free(nse);
                return;
        }

        memcpy(nse->file_id, rep->file_id, SMB2_FD_SIZE);

        memset(&dce_pdu, 0, sizeof(struct dcerpc_pdu));
        dce_pdu.hdr.rpc_vers = 5;
        dce_pdu.hdr.rpc_vers_minor = 0;
        dce_pdu.hdr.PTYPE = PDU_TYPE_BIND;
        dce_pdu.hdr.pfc_flags = PFC_FIRST_FRAG | PFC_LAST_FRAG;
        dce_pdu.hdr.packed_drep[0] = DCERPC_DR_BYTE_ORDER_LITTLE_ENDIAN |
                DCERPC_DR_CHARACTER_ASCII;
        dce_pdu.hdr.frag_length = 0;
        dce_pdu.hdr.auth_length = 0;
        dce_pdu.hdr.call_id = 2;
        dce_pdu.bind.max_xmit_frag = 4280;
        dce_pdu.bind.max_recv_frag = 4280;
        dce_pdu.bind.assoc_group_id = 0;

        dce_pdu.bind.abstract_syntax = &srvsvc_interface;

        iov.buf = &nse->buf[0];
        iov.len = NSE_BUF_SIZE;
        iov.free = NULL;
        len = dcerpc_encode_pdu(&nse->ctx, &dce_pdu, &iov);
        if (len < 0) {
                nse->cb(smb2, -ENOMEM, NULL, nse->cb_data);
                free(nse);
                return;
        }

        memset(&req, 0, sizeof(struct smb2_write_request));
        req.length = len;
        req.offset = 0;
        req.buf = &nse->buf[0];
        memcpy(req.file_id, nse->file_id, SMB2_FD_SIZE);
        req.channel = SMB2_CHANNEL_NONE;
        req.remaining_bytes = 0;
        req.flags = 0;

        pdu = smb2_cmd_write_async(smb2, &req, nse_bind_write_cb, nse);
        if (pdu == NULL) {
                nse->cb(smb2, -ENOMEM, NULL, nse->cb_data);
                free(nse);
                return;
        }
        smb2_queue_pdu(smb2, pdu);
}

int
smb2_share_enum_async(struct smb2_context *smb2, const char *server,
                      smb2_command_cb cb, void *cb_data)
{
        struct smb2nse *nse;
        struct smb2_create_request req;
        struct smb2_pdu *pdu;

        if (server == NULL) {
                smb2_set_error(smb2, "No server name specified.");
                return -1;
        }

        nse = malloc(sizeof(struct smb2nse));
        if (nse == NULL) {
                smb2_set_error(smb2, "Failed to allocate nse");
                return -ENOMEM;
        }
        memset(nse, 0, sizeof(struct smb2nse));
        nse->ctx.smb2 = smb2;
        nse->cb = cb;
        nse->cb_data = cb_data;

        memset(&req, 0, sizeof(struct smb2_create_request));
        req.requested_oplock_level = SMB2_OPLOCK_LEVEL_NONE;
        req.impersonation_level = SMB2_IMPERSONATION_IMPERSONATION;
        req.desired_access = SMB2_FILE_READ_DATA |
                SMB2_FILE_WRITE_DATA |
                SMB2_FILE_APPEND_DATA |
                SMB2_FILE_READ_EA |
                SMB2_FILE_READ_ATTRIBUTES |
                SMB2_FILE_WRITE_EA |
                SMB2_FILE_WRITE_ATTRIBUTES |
                SMB2_READ_CONTROL |
                SMB2_SYNCHRONIZE;
        req.file_attributes = 0;
        req.share_access = SMB2_FILE_SHARE_READ |
                SMB2_FILE_SHARE_WRITE |
                SMB2_FILE_SHARE_DELETE;
        req.create_disposition = SMB2_FILE_OPEN;
        req.create_options = 0;
        req.name = "srvsvc";

        pdu = smb2_cmd_create_async(smb2, &req, nse_open_cb, nse);
        if (pdu == NULL) {
                smb2_set_error(smb2, "Failed to create create command");
                return -ENOMEM;
        }
        smb2_queue_pdu(smb2, pdu);

        return 0;
}
