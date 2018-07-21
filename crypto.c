#include <nettle/yarrow.h>
#include <nettle/buffer.h>
#include "crypto.h"
#include "utils.h"

#define DEFAULT_KEYSIZE 2048

static gboolean crypto_genrsakey(struct rsa_public_key* pubkey,
		struct rsa_private_key* privatekey, struct yarrow256_ctx* yarrowctx) {
	rsa_public_key_init(pubkey);
	rsa_private_key_init(privatekey);

	int ret = rsa_generate_keypair(pubkey, privatekey, yarrowctx,
			(nettle_random_func *) yarrow256_random,
			NULL, NULL, DEFAULT_KEYSIZE, 30);
	if (!ret) {
		g_message("rsa keygen failed; %d", ret);
		return FALSE;
	}
	return TRUE;
}

static gboolean crypto_inityarrow(struct yarrow256_ctx* yarrowctx) {
	GIOChannel* devrandom = g_io_channel_new_file("/dev/random", "r", NULL);
	g_io_channel_set_encoding(devrandom, NULL, NULL);
	g_io_channel_set_buffered(devrandom, FALSE);
	gchar seed[128];
	gsize total = 0;
	gsize want = 20;
	while (total < want) {
		gsize read;
		g_io_channel_read_chars(devrandom, seed + total, sizeof(seed) - total,
				&read, NULL);
		total += read;
	}

	yarrow256_init(yarrowctx, 0, NULL);
	yarrow256_seed(yarrowctx, total, (uint8_t*) seed);

	if (!yarrow256_is_seeded(yarrowctx)) {
		g_message("failed to seed yarrow");
		return FALSE;
	}
	return TRUE;
}

struct manifest_signature* crypto_sign(enum manifest_signaturetype sigtype,
		struct crypto_keys* keys, guint8* data, gsize len) {

	struct yarrow256_ctx yarrowctx;
	struct manifest_signature* s = NULL;

	if (!crypto_inityarrow(&yarrowctx)) {
		goto err_yarrowinit;
	}

	mpz_t sig;
	mpz_init(sig);

	switch (sigtype) {
	case OTA_SIGTYPE_RSASHA256: {
		struct sha256_ctx sha256hash;
		sha256_init(&sha256hash);
		sha256_update(&sha256hash, len, (unsigned char*) data);
		rsa_sha256_sign_tr(&keys->pubkey, &keys->privatekey, &yarrowctx,
				(nettle_random_func *) yarrow256_random, &sha256hash, sig);
	}
		break;
	case OTA_SIGTYPE_RSASHA512: {
		struct sha512_ctx sha512hash;
		sha512_init(&sha512hash);
		sha512_update(&sha512hash, len, (unsigned char*) data);
		rsa_sha512_sign_tr(&keys->pubkey, &keys->privatekey, &yarrowctx,
				(nettle_random_func *) yarrow256_random, &sha512hash, sig);
	}
		break;
	default:
		g_message("unhandled signature type");
		goto err_sigtype;
	}

	char* sighex = mpz_get_str(NULL, 16, sig);
	//g_message("sig: %s", sighex);

	s = g_malloc0(sizeof(*s));
	s->type = sigtype;
	s->data = sighex;

	err_sigtype: //
	err_yarrowinit: //
	return s;
}

gboolean crypto_verify(struct manifest_signature* signature,
		struct crypto_keys* keys, guint8* data, gsize len) {
	return FALSE;
}

gboolean crypto_keygen(struct crypto_keys* keys) {
	struct yarrow256_ctx yarrowctx;
	if (!crypto_inityarrow(&yarrowctx))
		return FALSE;
	return crypto_genrsakey(&keys->pubkey, &keys->privatekey, &yarrowctx);
}

void crypto_writekeys(struct crypto_keys* keys, const gchar* rsapubkeypath,
		const gchar* rsaprivkeypath) {
	struct nettle_buffer pub_buffer;
	struct nettle_buffer priv_buffer;
	nettle_buffer_init(&pub_buffer);
	nettle_buffer_init(&priv_buffer);

	rsa_keypair_to_sexp(&pub_buffer, NULL, &keys->pubkey, NULL);
	rsa_keypair_to_sexp(&priv_buffer, NULL, &keys->pubkey, &keys->privatekey);

	g_message("pubkey:");
	teenyhttp_hexdump(pub_buffer.contents, pub_buffer.size);
	g_message("privkey:");
	teenyhttp_hexdump(priv_buffer.contents, priv_buffer.size);

	g_file_set_contents(rsapubkeypath, (gchar*) pub_buffer.contents,
			pub_buffer.size,
			NULL);
	g_file_set_contents(rsaprivkeypath, (gchar*) priv_buffer.contents,
			priv_buffer.size,
			NULL);

	nettle_buffer_clear(&priv_buffer);
	nettle_buffer_clear(&pub_buffer);
}

struct crypto_keys* crypto_readkeys(const gchar* rsapubkeypath,
		const gchar* rsaprivkeypath) {

	gchar* rawpubkey;
	gsize rawpubkeysz;
	if (!g_file_get_contents(rsapubkeypath, &rawpubkey, &rawpubkeysz, NULL)) {
		g_message("failed to read rsa public key");
	}

	gchar* rawprivkey;
	gsize rawprivkeysz;
	if (!g_file_get_contents(rsaprivkeypath, &rawprivkey, &rawprivkeysz,
	NULL)) {
		g_message("failed to read rsa private key");
	}

	struct crypto_keys* keys = g_malloc0(sizeof(*keys));
	rsa_public_key_init(&keys->pubkey);
	rsa_private_key_init(&keys->privatekey);

	if (!rsa_keypair_from_sexp(&keys->pubkey,
	NULL, 0, rawpubkeysz, (uint8_t*) rawpubkey)) {
		g_message("failed to load rsa public key");
	}
	if (!rsa_keypair_from_sexp(&keys->pubkey, &keys->privatekey, 0,
			rawprivkeysz, (uint8_t*) rawprivkey)) {
		g_message("failed to load rsa private key");
	}

	return keys;
}
