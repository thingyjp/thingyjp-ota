#pragma once

#include <glib.h>
#include <nettle/rsa.h>
#include "manifest.h"

#define CRYPTO_KEYNAME_RSA_PUB  "rsa.pub"
#define CRYPTO_KEYNAME_RSA_PRIV "rsa.priv"

struct crypto_keys {
	struct rsa_public_key pubkey;
	struct rsa_private_key privatekey;
};

struct manifest_signature* crypto_sign(enum manifest_signaturetype sigtype,
		struct crypto_keys* keys, guint8* data, gsize len);
gboolean crypto_verify(struct manifest_signature* signature,
		struct crypto_keys* keys, guint8* data, gsize len);
gboolean crypto_keygen(struct crypto_keys* keys);
void crypto_writekeys(struct crypto_keys* keys, const gchar* rsapubkeypath,
		const gchar* rsaprivkeypath);
struct crypto_keys* crypto_readkeys(const gchar* rsapubkeypath,
		const gchar* rsaprivkeypath);
void crypto_keys_free(struct crypto_keys* keys);
