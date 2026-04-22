#include <limits.h>
#include <string.h>
#include "lauxlib.h"
#include "lmem.h"
#include "psa/crypto.h"
#include "module.h"
#include "platform.h"

#define HASH_METATABLE "crypto.hasher"

// algo_info_t describes a hashing algorithm and output size
typedef struct {
    const char* name;
    const size_t size;
    const psa_algorithm_t algo;
} algo_info_t;

// hash_context_t contains information about an ongoing hash operation
typedef struct {
    union {
      psa_hash_operation_t hash_op;
      psa_mac_operation_t hmac_op;
    };
    psa_key_id_t key_id;
    const algo_info_t* ainfo;
    bool hmac_mode;
} hash_context_t;

// the constant algorithms array below contains a table of functions and other
// information about each enabled hashing algorithm
static const algo_info_t algorithms[] = {
    { "MD5",       16, PSA_ALG_MD5    },
    { "RIPEMD160", 20, PSA_ALG_RIPEMD160 },
    { "SHA1",      20, PSA_ALG_SHA_1   },
    { "SHA224",    32, PSA_ALG_SHA_224 },
    { "SHA256",    32, PSA_ALG_SHA_256 },
    { "SHA384",    64, PSA_ALG_SHA_384 },
    { "SHA512",    64, PSA_ALG_SHA_512 },
};


//NUM_ALGORITHMS contains the actual number of enabled algorithms
const int NUM_ALGORITHMS = sizeof(algorithms) / sizeof(algo_info_t);

// crypto_new_hash (LUA: hasher = crypto.new_hash(algo)) allocates
// a hashing context for the requested algorithm
static int crypto_new_hash_or_hmac(lua_State* L, bool is_hmac) {
    const algo_info_t *ainfo = NULL;
    const char *algo = luaL_checkstring(L, 1);
    const unsigned char *key = NULL;
    size_t key_len = 0;
    if (is_hmac)
        key = (const unsigned char *)luaL_checklstring(L, 2, &key_len);

    for (int i = 0; i < NUM_ALGORITHMS; i++) {
        if (strcasecmp(algo, algorithms[i].name) == 0) {
            ainfo = &algorithms[i];
            break;
        }
    }

    if (ainfo == NULL) {
        return luaL_error(L, "Unsupported algorithm: %s", algo);
    }

    // Instantiate a hasher object as a Lua userdata object
    // it will contain a pointer to a hash_context_t structure in which
    // we will store the mbedtls context information and also
    // what hashing algorithm this context is for.
    hash_context_t* phctx = (hash_context_t*)lua_newuserdata(L, sizeof(hash_context_t));
    luaL_getmetatable(L, HASH_METATABLE);
    lua_setmetatable(L, -2);

    phctx->ainfo = ainfo;
    phctx->hmac_mode = is_hmac;

    psa_status_t res = PSA_SUCCESS;
    if (is_hmac)
    {
      phctx->hmac_op = psa_mac_operation_init();

      psa_key_attributes_t attrs = PSA_KEY_ATTRIBUTES_INIT;
      psa_set_key_algorithm(&attrs, ainfo->algo);
      psa_set_key_usage_flags(&attrs, PSA_KEY_USAGE_SIGN_MESSAGE);
      res = psa_import_key(&attrs, key, key_len, &phctx->key_id);
      psa_reset_key_attributes(&attrs);
      if (res != PSA_SUCCESS)
        return luaL_error(L, "hmac key import failed: %d", res);

      res = psa_mac_sign_setup(&phctx->hmac_op, phctx->key_id, ainfo->algo);
      if (res != PSA_SUCCESS)
        return luaL_error(L, "hmac setup failed: %d", res);
    }
    else
    {
      phctx->hash_op = psa_hash_operation_init();
      res = psa_hash_setup(&phctx->hash_op, ainfo->algo);
      if (res != PSA_SUCCESS)
        return luaL_error(L, "hash setup failed: %d", res);
    }

    return 1;  // one object returned, the hasher userdata object.
}

static int crypto_new_hash(lua_State* L) {
  return crypto_new_hash_or_hmac(L, false);
}

static int crypto_new_hmac(lua_State* L)
{
  return crypto_new_hash_or_hmac(L, true);
}


// crypto_hash_update (LUA: hasher:update(data)) submits data
// to be hashed.
static int crypto_hash_update(lua_State* L) {
    // retrieve the hashing context:
    hash_context_t* phctx = (hash_context_t*)luaL_checkudata(L, 1, HASH_METATABLE);

    size_t size;  // size of the input string
    // retrieve the input string:
    const unsigned char* input = (const unsigned char*)luaL_checklstring(L, 2, &size);

    psa_status_t res = PSA_SUCCESS;
    // call the update hashing function:
    if (phctx->hmac_mode)
      res = psa_mac_update(&phctx->hmac_op, input, size);
    else
      res = psa_hash_update(&phctx->hash_op, input, size);

    if (res != PSA_SUCCESS)
      return luaL_error(L, "Error updating hash: %d", res);

    return 0;  // no return value
}

// crypto_hash_finalize (LUA: hasher:finalize()) returns the hash result
// as a binary string.
static int crypto_hash_finalize(lua_State* L) {
    // retrieve the hashing context:
    hash_context_t* phctx = (hash_context_t*)luaL_checkudata(L, 1, HASH_METATABLE);

    size_t size = phctx->ainfo->size;
    // reserve some space to retrieve the output hash, according to the current algorithm
    unsigned char output[size];

    psa_status_t res = PSA_SUCCESS;
    // call the hash finish function to retrieve the result
    if (phctx->hmac_mode)
      res = psa_mac_sign_finish(&phctx->hmac_op, output, size, &size);
    else
      res = psa_hash_finish(&phctx->hash_op, output, size, &size);
    if (res != PSA_SUCCESS)
      return luaL_error(L, "Error finalizing hash: %d", res);
    if (size != phctx->ainfo->size)
      return luaL_error(L, "Mismatched hash size; got %d expected %d", size, phctx->ainfo->size);

    // pack the output into a lua string
    lua_pushlstring(L, (const char*)output, size);

    return 1;  // 1 result returned, the hash.
}

// crypto_hash_gc is called automatically by LUA when the hasher object is
// dereferenced, in order to free resources associated with the hashing process.
static int crypto_hash_gc(lua_State* L) {
    // retrieve the hashing context:
    hash_context_t* phctx = (hash_context_t*)luaL_checkudata(L, 1, HASH_METATABLE);

    if (phctx->hmac_mode)
    {
      psa_mac_abort(&phctx->hmac_op);
      psa_destroy_key(phctx->key_id);
    }
    else {
      psa_hash_abort(&phctx->hash_op);
    }

    return 0;
}

// The following table defines methods of the hasher object
LROT_BEGIN(crypto_hasher, NULL, LROT_MASK_GC_INDEX)
    LROT_FUNCENTRY(__gc,     crypto_hash_gc)
    LROT_TABENTRY(__index,   crypto_hasher)
    LROT_FUNCENTRY(update,   crypto_hash_update)
    LROT_FUNCENTRY(finalize, crypto_hash_finalize)
LROT_END(crypto_hasher, NULL, LROT_MASK_GC_INDEX)

// This table defines the functions of the crypto module:
LROT_BEGIN(crypto, NULL, 0)
    LROT_FUNCENTRY(new_hash, crypto_new_hash)
    LROT_FUNCENTRY(new_hmac, crypto_new_hmac)
LROT_END(crypto, NULL, 0)

// luaopen_crypto is the crypto module initialization function
int luaopen_crypto(lua_State* L) {
    luaL_rometatable(L, HASH_METATABLE, LROT_TABLEREF(crypto_hasher));  // create metatable for crypto.hash

    if (psa_crypto_init() != PSA_SUCCESS)
      return luaL_error(L, "mbedtls init failed");

    return 0;
}

// define the crypto NodeMCU module
NODEMCU_MODULE(CRYPTO, "crypto", crypto, luaopen_crypto);
