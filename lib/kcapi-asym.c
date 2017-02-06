/* Kernel crypto API AF_ALG Asymmetric Cipher API
 *
 * Copyright (C) 2016, Stephan Mueller <smueller@chronox.de>
 *
 * License: see COPYING file in root directory
 *
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE, ALL OF
 * WHICH ARE HEREBY DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT
 * OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE
 * USE OF THIS SOFTWARE, EVEN IF NOT ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 */

#include "internal.h"
#include "kcapi.h"

DSO_PUBLIC
int kcapi_akcipher_init(struct kcapi_handle **handle, const char *ciphername,
			uint32_t flags)
{
	return _kcapi_handle_init(handle, "akcipher", ciphername, flags);
}

DSO_PUBLIC
void kcapi_akcipher_destroy(struct kcapi_handle *handle)
{
	_kcapi_handle_destroy(handle);
}

DSO_PUBLIC
int kcapi_akcipher_setkey(struct kcapi_handle *handle,
			  const uint8_t *key, uint32_t keylen)
{
	return _kcapi_common_setkey(handle, key, keylen);
}

DSO_PUBLIC
int kcapi_akcipher_setpubkey(struct kcapi_handle *handle,
			     const uint8_t *key, uint32_t keylen)
{
	int ret = 0;

	ret = setsockopt(handle->tfmfd, SOL_ALG, ALG_SET_PUBKEY, key, keylen);
	return (ret >= 0) ? ret : -errno;
}

DSO_PUBLIC
int32_t kcapi_akcipher_encrypt(struct kcapi_handle *handle,
			       const uint8_t *in, uint32_t inlen,
			       uint8_t *out, uint32_t outlen, int access)
{
	return _kcapi_cipher_crypt(handle, in, inlen, out, outlen, access,
				   ALG_OP_ENCRYPT);
}

DSO_PUBLIC
int32_t kcapi_akcipher_decrypt(struct kcapi_handle *handle,
			       const uint8_t *in, uint32_t inlen,
			       uint8_t *out, uint32_t outlen, int access)
{
	return _kcapi_cipher_crypt(handle, in, inlen, out, outlen, access,
				   ALG_OP_DECRYPT);
}

DSO_PUBLIC
int32_t kcapi_akcipher_sign(struct kcapi_handle *handle,
			    const uint8_t *in, uint32_t inlen,
			    uint8_t *out, uint32_t outlen, int access)
{
	return _kcapi_cipher_crypt(handle, in, inlen, out, outlen, access,
				   ALG_OP_SIGN);
}

DSO_PUBLIC
int32_t kcapi_akcipher_verify(struct kcapi_handle *handle,
			      const uint8_t *in, uint32_t inlen,
			      uint8_t *out, uint32_t outlen, int access)
{
	return _kcapi_cipher_crypt(handle, in, inlen, out, outlen, access,
				   ALG_OP_VERIFY);
}

DSO_PUBLIC
int32_t kcapi_akcipher_stream_init_enc(struct kcapi_handle *handle,
				       struct iovec *iov, uint32_t iovlen)
{
	return _kcapi_common_send_meta(handle, iov, iovlen, ALG_OP_ENCRYPT,
				       MSG_MORE);
}

DSO_PUBLIC
int32_t kcapi_akcipher_stream_init_dec(struct kcapi_handle *handle,
				       struct iovec *iov, uint32_t iovlen)
{
	return _kcapi_common_send_meta(handle, iov, iovlen, ALG_OP_DECRYPT,
				       MSG_MORE);
}

DSO_PUBLIC
int32_t kcapi_akcipher_stream_init_sgn(struct kcapi_handle *handle,
				       struct iovec *iov, uint32_t iovlen)
{
	return _kcapi_common_send_meta(handle, iov, iovlen, ALG_OP_SIGN,
				       MSG_MORE);
}

DSO_PUBLIC
int32_t kcapi_akcipher_stream_init_vfy(struct kcapi_handle *handle,
				       struct iovec *iov, uint32_t iovlen)
{
	return _kcapi_common_send_meta(handle, iov, iovlen, ALG_OP_VERIFY,
				       MSG_MORE);
}

DSO_PUBLIC
int32_t kcapi_akcipher_stream_update(struct kcapi_handle *handle,
				     struct iovec *iov, uint32_t iovlen)
{
	/* TODO: vmsplice only works with ALG_MAX_PAGES - 1 -- no clue why */
	if (handle->processed_sg < ALG_MAX_PAGES)
		return _kcapi_common_vmsplice_iov(handle, iov, iovlen,
						  SPLICE_F_MORE);
	else
		return _kcapi_common_send_data(handle, iov, iovlen, MSG_MORE);
}

DSO_PUBLIC
int32_t kcapi_akcipher_stream_update_last(struct kcapi_handle *handle,
				          struct iovec *iov, uint32_t iovlen)
{
	/* TODO: vmsplice only works with ALG_MAX_PAGES - 1 -- no clue why */
	if (handle->processed_sg < ALG_MAX_PAGES)
		return _kcapi_common_vmsplice_iov(handle, iov, iovlen, 0);
	else
		return _kcapi_common_send_data(handle, iov, iovlen, 0);
}

DSO_PUBLIC
int32_t kcapi_akcipher_stream_op(struct kcapi_handle *handle,
			         struct iovec *iov, uint32_t iovlen)
{
	if (!iov || !iovlen) {
		kcapi_dolog(LOG_ERR,
			    "Asymmetric operation: No buffer for output data provided");
		return -EINVAL;
	}
	return _kcapi_common_recv_data(handle, iov, iovlen);
}

/*
 * Fallback function if AIO is not present, but caller requested AIO operation.
 */
static int32_t
_kcapi_akcipher_encrypt_aio_fallback(struct kcapi_handle *handle,
				     struct iovec *iniov, struct iovec *outiov,
				     uint32_t iovlen)
{
	int32_t ret = kcapi_akcipher_stream_init_enc(handle, NULL, 0);

	if (ret < 0)
		return ret;

	ret = kcapi_akcipher_stream_update_last(handle, iniov, iovlen);
	if (ret < 0)
		return ret;


	return kcapi_cipher_stream_op(handle, outiov, iovlen);
}

DSO_PUBLIC
int32_t kcapi_akcipher_encrypt_aio(struct kcapi_handle *handle,
				   struct iovec *iniov, struct iovec *outiov,
				   uint32_t iovlen, int access)
{
	int32_t ret;

	if (handle->flags.aiosupp) {
		ret = _kcapi_cipher_crypt_aio(handle, iniov, outiov, iovlen,
					      access, ALG_OP_ENCRYPT);

		if (ret != -EOPNOTSUPP)
			return ret;
	}

	/* The kernel's AIO interface is broken. */
	return _kcapi_akcipher_encrypt_aio_fallback(handle, iniov, outiov,
						    iovlen);
}

/*
 * Fallback function if AIO is not present, but caller requested AIO operation.
 */
static int32_t
_kcapi_akcipher_decrypt_aio_fallback(struct kcapi_handle *handle,
				     struct iovec *iniov, struct iovec *outiov,
				     uint32_t iovlen)
{
	int32_t ret = kcapi_akcipher_stream_init_dec(handle, NULL, 0);

	if (ret < 0)
		return ret;

	ret = kcapi_akcipher_stream_update_last(handle, iniov, iovlen);
	if (ret < 0)
		return ret;


	return kcapi_akcipher_stream_op(handle, outiov, iovlen);
}

DSO_PUBLIC
int32_t kcapi_akcipher_decrypt_aio(struct kcapi_handle *handle,
				   struct iovec *iniov, struct iovec *outiov,
				   uint32_t iovlen, int access)
{
	int32_t ret;

	if (handle->flags.aiosupp) {
		ret = _kcapi_cipher_crypt_aio(handle, iniov, outiov, iovlen,
					      access, ALG_OP_DECRYPT);

		if (ret != -EOPNOTSUPP)
			return ret;
	}

	/* The kernel's AIO interface is broken. */
	return _kcapi_akcipher_decrypt_aio_fallback(handle, iniov, outiov,
						    iovlen);
}

/*
 * Fallback function if AIO is not present, but caller requested AIO operation.
 */
static int32_t
_kcapi_akcipher_sign_aio_fallback(struct kcapi_handle *handle,
				  struct iovec *iniov, struct iovec *outiov,
				  uint32_t iovlen)
{
	int32_t ret = kcapi_akcipher_stream_init_sgn(handle, NULL, 0);

	if (ret < 0)
		return ret;

	ret = kcapi_akcipher_stream_update_last(handle, iniov, iovlen);
	if (ret < 0)
		return ret;


	return kcapi_akcipher_stream_op(handle, outiov, iovlen);
}

DSO_PUBLIC
int32_t kcapi_akcipher_sign_aio(struct kcapi_handle *handle,
				struct iovec *iniov, struct iovec *outiov,
				uint32_t iovlen, int access)
{
	int32_t ret;

	if (handle->flags.aiosupp) {
		ret = _kcapi_cipher_crypt_aio(handle, iniov, outiov, iovlen,
					      access, ALG_OP_SIGN);

		if (ret != -EOPNOTSUPP)
			return ret;
	}

	/* The kernel's AIO interface is broken. */
	return _kcapi_akcipher_sign_aio_fallback(handle, iniov, outiov, iovlen);
}

/*
 * Fallback function if AIO is not present, but caller requested AIO operation.
 */
static int32_t
_kcapi_akcipher_verify_aio_fallback(struct kcapi_handle *handle,
				    struct iovec *iniov, struct iovec *outiov,
				    uint32_t iovlen)
{
	int32_t ret = kcapi_akcipher_stream_init_vfy(handle, NULL, 0);

	if (ret < 0)
		return ret;

	ret = kcapi_akcipher_stream_update_last(handle, iniov, iovlen);
	if (ret < 0)
		return ret;

	return kcapi_aead_stream_op(handle, outiov, iovlen);

	return kcapi_akcipher_stream_op(handle, outiov, iovlen);
}

DSO_PUBLIC
int32_t kcapi_akcipher_verify_aio(struct kcapi_handle *handle,
				  struct iovec *iniov, struct iovec *outiov,
				  uint32_t iovlen, int access)
{
	int32_t ret;

	if (handle->flags.aiosupp) {
		ret = _kcapi_cipher_crypt_aio(handle, iniov, outiov, iovlen,
					      access, ALG_OP_VERIFY);

		if (ret != -EOPNOTSUPP)
			return ret;
	}

	/* The kernel's AIO interface is broken. */
	return _kcapi_akcipher_verify_aio_fallback(handle, iniov, outiov,
						   iovlen);
}
