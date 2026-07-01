/* x509_decoder_libmail.h -- standalone _libmail X.509 decoder.
 *
 * Extracted from bearssl_x509.h so it survives when the core Tasmota BearSSL
 * (t_bearssl_x509.h) owns the shared BR_BEARSSL_X509_H__ guard and thereby skips
 * the ESP-Mail bearssl_x509.h copy (e.g. any build that links the core BearSSL,
 * such as TESLA_POWERWALL). Only the x509 DECODER is renamed to _libmail (to avoid
 * the link clash with the core's br_x509_decoder_*), so only it needs to live
 * outside that guard. Everything else is shared with whichever BearSSL is present.
 *
 * MUST be #included AFTER a bearssl x509 header (core t_bearssl_x509.h in the main
 * TU, or ESP-Mail bearssl_x509.h in the library TUs) so the shared types it uses
 * (br_x509_pkey, BR_X509_BUFSIZE_KEY, BR_ERR_X509_*) are already in scope.
 */
#ifndef BR_X509_DECODER_LIBMAIL_H__
#define BR_X509_DECODER_LIBMAIL_H__

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * \brief X.509 decoder context.
 *
 * This structure is _not_ for X.509 validation, but for extracting
 * names and public keys from encoded certificates. Intended usage is
 * to use (self-signed) certificates as trust anchors.
 *
 * Contents are opaque and shall not be accessed directly.
 */
typedef struct {

#ifndef BR_DOXYGEN_IGNORE
	/* Structure for returning the public key. */
	br_x509_pkey pkey;

	/* CPU for the T0 virtual machine. */
	struct {
		uint32_t *dp;
		uint32_t *rp;
		const unsigned char *ip;
	} cpu;
	uint32_t dp_stack[32];
	uint32_t rp_stack[32];
	int err;

	/* The pad serves as destination for various operations. */
	unsigned char pad[256];

	/* Flag set when decoding succeeds. */
	unsigned char decoded;

	/* Validity dates. */
	uint32_t notbefore_days, notbefore_seconds;
	uint32_t notafter_days, notafter_seconds;

	/* The "CA" flag. This is set to true if the certificate contains
	   a Basic Constraints extension that asserts CA status. */
	unsigned char isCA;

	/* DN processing: the subject DN is extracted and pushed to the
	   provided callback. */
	unsigned char copy_dn;
	void *append_dn_ctx;
	void (*append_dn)(void *ctx, const void *buf, size_t len);

	/* Certificate data chunk. */
	const unsigned char *hbuf;
	size_t hlen;

	/* Buffer for decoded public key. */
	unsigned char pkey_data[BR_X509_BUFSIZE_KEY];

	/* Type of key and hash function used in the certificate signature. */
	unsigned char signer_key_type;
	unsigned char signer_hash_id;
#endif

} br_x509_decoder_context_libmail;

/**
 * \brief Initialise an X.509 decoder context for processing a new
 * certificate.
 *
 * The `append_dn()` callback (with opaque context `append_dn_ctx`)
 * will be invoked to receive, chunk by chunk, the certificate's
 * subject DN. If `append_dn` is `0` then the subject DN will be
 * ignored.
 *
 * \param ctx             X.509 decoder context to initialise.
 * \param append_dn       DN receiver callback (or `0`).
 * \param append_dn_ctx   context for the DN receiver callback.
 */
void br_x509_decoder_init_libmail(br_x509_decoder_context_libmail *ctx,
	void (*append_dn)(void *ctx, const void *buf, size_t len),
	void *append_dn_ctx);

/**
 * \brief Push some certificate bytes into a decoder context.
 *
 * If `len` is non-zero, then that many bytes are pushed, from address
 * `data`, into the provided decoder context.
 *
 * \param ctx    X.509 decoder context.
 * \param data   certificate data chunk.
 * \param len    certificate data chunk length (in bytes).
 */
void br_x509_decoder_push_libmail(br_x509_decoder_context_libmail *ctx,
	const void *data, size_t len);

/**
 * \brief Obtain the decoded public key.
 *
 * Returned value is a pointer to a structure internal to the decoder
 * context; releasing or reusing the decoder context invalidates that
 * structure.
 *
 * If decoding was not finished, or failed, then `NULL` is returned.
 *
 * \param ctx   X.509 decoder context.
 * \return  the public key, or `NULL` on unfinished/error.
 */
static inline br_x509_pkey *
br_x509_decoder_get_pkey_libmail(br_x509_decoder_context_libmail *ctx)
{
	if (ctx->decoded && ctx->err == 0) {
		return &ctx->pkey;
	} else {
		return NULL;
	}
}

/**
 * \brief Get decoder error status.
 *
 * If no error was reported yet but the certificate decoding is not
 * finished, then the error code is `BR_ERR_X509_TRUNCATED`. If decoding
 * was successful, then 0 is returned.
 *
 * \param ctx   X.509 decoder context.
 * \return  0 on successful decoding, or a non-zero error code.
 */
static inline int
br_x509_decoder_last_error_libmail(br_x509_decoder_context_libmail *ctx)
{
	if (ctx->err != 0) {
		return ctx->err;
	}
	if (!ctx->decoded) {
		return BR_ERR_X509_TRUNCATED;
	}
	return 0;
}

/**
 * \brief Get the "isCA" flag from an X.509 decoder context.
 *
 * This flag is set if the decoded certificate claims to be a CA through
 * a Basic Constraints extension. This flag should not be read before
 * decoding completed successfully.
 *
 * \param ctx   X.509 decoder context.
 * \return  the "isCA" flag.
 */
static inline int
br_x509_decoder_isCA_libmail(br_x509_decoder_context_libmail *ctx)
{
	return ctx->isCA;
}

/**
 * \brief Get the issuing CA key type (type of algorithm used to sign the
 * decoded certificate).
 *
 * This is `BR_KEYTYPE_RSA` or `BR_KEYTYPE_EC`. The value 0 is returned
 * if the signature type was not recognised.
 *
 * \param ctx   X.509 decoder context.
 * \return  the issuing CA key type.
 */
static inline int
br_x509_decoder_get_signer_key_type_libmail(br_x509_decoder_context_libmail *ctx)
{
	return ctx->signer_key_type;
}

/**
 * \brief Get the identifier for the hash function used to sign the decoded
 * certificate.
 *
 * This is 0 if the hash function was not recognised.
 *
 * \param ctx   X.509 decoder context.
 * \return  the signature hash function identifier.
 */
static inline int
br_x509_decoder_get_signer_hash_id_libmail(br_x509_decoder_context_libmail *ctx)
{
	return ctx->signer_hash_id;
}

#ifdef __cplusplus
}
#endif

#endif /* BR_X509_DECODER_LIBMAIL_H__ */
