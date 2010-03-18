/*
   +----------------------------------------------------------------------+
   | Copyright (c) 2002-2009 J Smith <dark.panda@gmail.com>               |
   | Crypto++ sources (not included) copyright (c) 1995-2009 Wei Dai      |
   +----------------------------------------------------------------------+
   | Check out COPYING for the exact license.                             |
   +----------------------------------------------------------------------+

$Id: $
*/

// hash algorithms:

#include "jadler32.h"
#include "jcrc32.h"
#include "jhaval.h"
#include "jmd2.h"
#include "jmd4.h"
#include "jmd5.h"
#include "jpanamahash.h"
#include "jripemd160.h"
#include "jsha.h"
#include "jtiger.h"
#include "jwhirlpool.h"

#include "jexception.h"

#include "cryptopp_ruby_api.h"

extern void hash_mark(JHash *c);
extern void hash_free(JHash *c);

// forward declarations

static bool hash_enabled(HashEnum hash);
static void digest_options(VALUE self, VALUE options);
static JHash* digest_factory(long algorithm);
static VALUE wrap_digest_in_ruby(JHash* hash);
static string digest_digest(VALUE self, bool hex);
static string digest_plaintext(VALUE self, bool hex);
static string digest_plaintext_eq(VALUE self, VALUE plaintext, bool hex);
static string digest_calculate(VALUE self, bool hex);
static string digest_digest_eq(VALUE self, VALUE digest, bool hex);
static string module_digest(int argc, VALUE *argv, VALUE self, bool hex);
static string module_digest_io(int argc, VALUE *argv, VALUE self, bool hex);
static string digest_digest_io(VALUE self, VALUE io, bool hex);
static void digest_hmac_options(VALUE self, VALUE options);
static string digest_hmac_key_eq(VALUE self, VALUE key, bool hex);
static string digest_hmac_key(VALUE self, bool hex);
static string module_hmac_digest(int argc, VALUE *argv, VALUE self, bool hex);

/* See if a hash algorithm is enabled. */
static bool hash_enabled(HashEnum hash)
{
	switch (hash) {
#		define CHECKSUM_ALGORITHM(klass, r, c) \
			case r ##_CHECKSUM:
#		include "checksums.def"
#		undef CHECKSUM_ALGORITHM

#		define HASH_ALGORITHM(klass, r, c) \
			case r ##_HASH:
#		include "hashes.def"
#		undef HASH_ALGORITHM

#		define HMAC_ALGORITHM(klass, r, c) \
			case r ##_HMAC:
#		include "hmacs.def"
#		undef HMAC_ALGORITHM
			return true;
	}
	return false;
}


/* Figure out options for a digest. */
static void digest_options(VALUE self, VALUE options)
{
	Check_Type(options, T_HASH);

	{
		VALUE plaintext = rb_hash_aref(options, ID2SYM(rb_intern("plaintext")));
		VALUE plaintext_hex = rb_hash_aref(options, ID2SYM(rb_intern("plaintext_hex")));
		if (!NIL_P(plaintext) && !NIL_P(plaintext_hex)) {
			rb_raise(rb_eCryptoPP_Error, "can't set both plaintext and plaintext_hex in options");
		}
		else if (!NIL_P(plaintext)) {
			digest_plaintext_eq(self, plaintext, false);
		}
		else if (!NIL_P(plaintext_hex)) {
			digest_plaintext_eq(self, plaintext_hex, true);
		}
	}

	{
		VALUE digest = rb_hash_aref(options, ID2SYM(rb_intern("digest")));
		VALUE digest_hex = rb_hash_aref(options, ID2SYM(rb_intern("digest_hex")));
		if (!NIL_P(digest) && !NIL_P(digest_hex)) {
			rb_raise(rb_eCryptoPP_Error, "can't set both digest and digest_hex in options");
		}
		else if (!NIL_P(digest)) {
			digest_digest_eq(self, digest, false);
		}
		else if (!NIL_P(digest_hex)) {
			digest_digest_eq(self, digest_hex, true);
		}
	}
}


/* Creates a new Digest object. */
static JHash* digest_factory(long algorithm)
{
	if (!hash_enabled((enum HashEnum) algorithm)) {
		throw JException("the requested algorithm has been disabled");
	}
	else {
		try {
			switch (algorithm) {
#				define CHECKSUM_ALGORITHM(klass, r, c) \
					case r ## _CHECKSUM: \
						return static_cast<c*>(new c);
#				include "checksums.def"
#				undef CHECKSUM_ALGORITHM

#				define HASH_ALGORITHM(klass, r, c) \
					case r ## _HASH: \
						return static_cast<c*>(new c);
#				include "hashes.def"
#				undef HASH_ALGORITHM

#				define HMAC_ALGORITHM(klass, r, c) \
					case r ## _HMAC: \
						return static_cast<c*>(new c);
#				include "hmacs.def"
#				undef HMAC_ALGORITHM
			}
		}
		catch (Exception& e) {
			throw JException("Crypto++ exception: " + e.GetWhat());
		}
	}
	return NULL;
}

/* Wraps a Digest/HMAC object into a Ruby object. May throw a JException if no
 * suitable algorithm is found. */
static VALUE wrap_digest_in_ruby(JHash* hash)
{
	const type_info& info = typeid(*hash);
#	define CHECKSUM_ALGORITHM(klass, r, c) \
		if (info == typeid(c)) { \
			return Data_Wrap_Struct(rb_cCryptoPP_Digest_## r, hash_mark, hash_free, hash); \
		} \
		else
#	include "checksums.def"
#	undef CHECKSUM_ALGORITHM

#	define HASH_ALGORITHM(klass, r, c) \
		if (info == typeid(c)) { \
			return Data_Wrap_Struct(rb_cCryptoPP_Digest_## r, hash_mark, hash_free, hash); \
		} \
		else
#	include "hashes.def"
#	undef HASH_ALGORITHM

#	define HMAC_ALGORITHM(klass, r, c) \
		if (info == typeid(c)) { \
			return Data_Wrap_Struct(rb_cCryptoPP_Digest_HMAC_## r, hash_mark, hash_free, hash); \
		} \
		else
#	include "hmacs.def"
#	undef HMAC_ALGORITHM
	{
		throw JException("the requested algorithm has been disabled");
	}
}

/**
 *	call-seq:
 *		digest_factory(constant) => Digest
 *		digest_factory(constant, plaintext) => Digest
 *		digest_factory(constant, options) => Digest
 *
 * Creates a new Digest object. Use a <tt>*_DIGEST</tt> constant to choose an
 * algorithm.
 *
 * See the Digest class for available options.
 */
VALUE rb_module_digest_factory(int argc, VALUE *argv, VALUE self)
{
	JHash* hash = NULL;
	VALUE algorithm, options, retval;

	rb_scan_args(argc, argv, "11", &algorithm, &options);
	{
		long a = NUM2LONG(algorithm);
		if (!IS_NON_HMAC(a)) {
			rb_raise(rb_eCryptoPP_Error, "invalid digest algorithm");
		}
		else {
			try {
				hash = digest_factory(a);
				retval = wrap_digest_in_ruby(hash);
			}
			catch (Exception& e) {
				if (hash != NULL) {
					delete hash;
				}
				rb_raise(rb_eCryptoPP_Error, e.GetWhat().c_str());
			}
			if (argc == 2) {
				if (TYPE(options) == T_STRING) {
					rb_digest_plaintext_eq(retval, options);
					hash->hash();
				}
				else {
					digest_options(retval, options);
				}
			}
			return retval;
		}
	}
}

#define CHECKSUM_ALGORITHM(klass, r, n) \
VALUE rb_digest_ ## r ##_new(int argc, VALUE *argv, VALUE self) \
{ \
	VALUE options, retval; \
	JHash* hash = NULL; \
	try { \
		hash = digest_factory(r ## _CHECKSUM); \
		retval = wrap_digest_in_ruby(hash); \
	} \
	catch (Exception& e) { \
		if (hash != NULL) { \
			delete hash; \
		} \
		rb_raise(rb_eCryptoPP_Error, e.GetWhat().c_str()); \
	} \
	rb_scan_args(argc, argv, "01", &options); \
	if (!NIL_P(options)) { \
		if (TYPE(options) == T_STRING) { \
			rb_digest_plaintext_eq(retval, options); \
			hash->hash(); \
		} \
		else { \
			digest_options(retval, options); \
		} \
	} \
	return retval; \
}
#include "checksums.def"
#undef CHECKSUM_ALGORITHM

#define HASH_ALGORITHM(klass, r, n) \
VALUE rb_digest_ ## r ##_new(int argc, VALUE *argv, VALUE self) \
{ \
	VALUE options, retval; \
	JHash* hash = NULL; \
	try { \
		hash = digest_factory(r ## _HASH); \
		retval = wrap_digest_in_ruby(hash); \
	} \
	catch (Exception& e) { \
		if (hash != NULL) { \
			delete hash; \
		} \
		rb_raise(rb_eCryptoPP_Error, e.GetWhat().c_str()); \
	} \
	rb_scan_args(argc, argv, "01", &options); \
	if (!NIL_P(options)) { \
		if (TYPE(options) == T_STRING) { \
			rb_digest_plaintext_eq(retval, options); \
			hash->hash(); \
		} \
		else { \
			digest_options(retval, options); \
		} \
	} \
	return retval; \
}
#include "hashes.def"
#undef HASH_ALGORITHM


/**
 * call-seq:
 *		update(plaintext) => String
 *
 * Updates the plaintext on a Digest and returns the new digested text.
 */
VALUE rb_digest_update(VALUE self, VALUE plaintext)
{
	JHash *hash = NULL;
	Check_Type(plaintext, T_STRING);
	Data_Get_Struct(self, JHash, hash);
	hash->updatePlaintext(string(StringValuePtr(plaintext), RSTRING(plaintext)->len));
	hash->hash();
	return rb_tainted_str_new(hash->getHashtext().data(), hash->getHashtext().length());
}


/* Returns the digested text. */
static string digest_digest(VALUE self, bool hex)
{
	JHash *hash = NULL;
	Data_Get_Struct(self, JHash, hash);
	return hash->getHashtext(hex);
}

/**
 * Returns the digested text in binary.
 */
VALUE rb_digest_digest(VALUE self)
{
	string retval = digest_digest(self, false);
	return rb_tainted_str_new(retval.data(), retval.length());
}

/**
 * Returns the digested text in hex.
 */
VALUE rb_digest_digest_hex(VALUE self)
{
	string retval = digest_digest(self, true);
	return rb_tainted_str_new(retval.data(), retval.length());
}


/* Gets the plaintext from a hash. */
static string digest_plaintext(VALUE self, bool hex)
{
	JHash *hash = NULL;
	Data_Get_Struct(self, JHash, hash);
	return hash->getPlaintext(hex);;
}

/**
 * Returns the plaintext used to generate the digest in binary.
 */
VALUE rb_digest_plaintext(VALUE self)
{
	string retval = digest_plaintext(self, false);
	return rb_tainted_str_new(retval.data(), retval.length());
}

/**
 * Returns the plaintext used to generate the digest in hex.
 */
VALUE rb_digest_plaintext_hex(VALUE self)
{
	string retval = digest_plaintext(self, true);
	return rb_tainted_str_new(retval.data(), retval.length());
}


/* Sets the plaintext on a digest. */
static string digest_plaintext_eq(VALUE self, VALUE plaintext, bool hex)
{
	JHash *hash = NULL;
	Check_Type(plaintext, T_STRING);
	Data_Get_Struct(self, JHash, hash);
	hash->setPlaintext(string(StringValuePtr(plaintext), RSTRING(plaintext)->len), hex);
	return hash->getPlaintext(hex);
}

/**
 * call-seq:
 *		plaintext=(plaintext)
 *
 * Sets the plaintext on a Digest in binary.
 */
VALUE rb_digest_plaintext_eq(VALUE self, VALUE plaintext)
{
	digest_plaintext_eq(self, plaintext, false);
	return plaintext;
}

/**
 * call-seq:
 *		plaintext=(plaintext)
 *
 * Sets the plaintext on a Digest in hex.
 */
VALUE rb_digest_plaintext_hex_eq(VALUE self, VALUE plaintext)
{
	digest_plaintext_eq(self, plaintext, true);
	return plaintext;
}


/* Calculates the digest. */
static string digest_calculate(VALUE self, bool hex)
{
	JHash *hash = NULL;
	Data_Get_Struct(self, JHash, hash);
	hash->hash();
	return hash->getHashtext(hex);
}

/**
 * Calculates the digest and returns the result in binary.
 */
VALUE rb_digest_calculate(VALUE self)
{
	string retval = digest_calculate(self, false);
	return rb_tainted_str_new(retval.data(), retval.length());
}

/**
 * Calculates the digest and returns the result in hex.
 */
VALUE rb_digest_calculate_hex(VALUE self)
{
	string retval = digest_calculate(self, true);
	return rb_tainted_str_new(retval.data(), retval.length());
}


/* Sets the hashtext on a digest. */
static string digest_digest_eq(VALUE self, VALUE digest, bool hex)
{
	JHash *hash = NULL;
	Check_Type(digest, T_STRING);
	Data_Get_Struct(self, JHash, hash);
	hash->setHashtext(string(StringValuePtr(digest), RSTRING(digest)->len), hex);
	return hash->getHashtext(hex);
}

/**
 * Sets the digest text on a Digest in binary.
 */
VALUE rb_digest_digest_eq(VALUE self, VALUE digest)
{
	digest_digest_eq(self, digest, false);
	return digest;
}

/**
 * Sets the digest text on a Digest in hex.
 */
VALUE rb_digest_digest_hex_eq(VALUE self, VALUE digest)
{
	digest_digest_eq(self, digest, true);
	return digest;
}


/**
 * Inspect method.
 */
VALUE rb_digest_inspect(VALUE self)
{
	JHash* hash = NULL;
	string retval;
	string cname = rb_obj_classname(self);
	Data_Get_Struct(self, JHash, hash);
	retval = "#<" + cname + ": " + hash->getHashtext(true) + ">";
	return rb_str_new(retval.c_str(), retval.length());
}


/**
 * Compares a Digest directly to a String. We'll attempt to detect whether or
 * not the String is in binary or hex based on the number of characters in 
 * it -- if it's exactly double the expected number of bytes, then we'll
 * assume we've got a hex String.
 */
VALUE rb_digest_equals(VALUE self, VALUE compare)
{
	JHash *hash = NULL;
	VALUE str1, str2;
	Check_Type(compare, T_STRING);
	Data_Get_Struct(self, JHash, hash);
	if (RSTRING(compare)->len == ((long) hash->getDigestSize() / 2)) {
		str1 = rb_str_new(hash->getHashtext(false).data(), hash->getHashtext(false).length());
		str2 = compare;
	}
	else if (RSTRING(compare)->len == ((long) hash->getDigestSize())) {
		str1 = rb_str_new(hash->getHashtext(true).data(), hash->getHashtext(true).length());
		str2 = rb_funcall(compare, rb_intern("downcase"), 0);
	}
	else {
		rb_raise(rb_eCryptoPP_Error, "expected %d bytes, got %d", hash->getDigestSize() / 2, RSTRING(compare)->len);
	}

	if (rb_str_cmp(str1, str2) == 0) {
		return Qtrue;
	}
	else {
		return Qfalse;
	}
}


/* Singleton method for digesting good stuff. */
static string module_digest(int argc, VALUE *argv, VALUE self, bool hex)
{
	JHash* hash = NULL;
	VALUE algorithm, plaintext, key;
	if (argc < 2) {
		rb_raise(rb_eArgError, "wrong number of arguments (%d for 2)", argc);
	}

	if (IS_HMAC(NUM2LONG(argv[0]))) {
		rb_scan_args(argc, argv, "21", &algorithm, &plaintext, &key);
		Check_Type(plaintext, T_STRING);
		Check_Type(key, T_STRING);
	}
	else {
		rb_scan_args(argc, argv, "2", &algorithm, &plaintext);
		Check_Type(plaintext, T_STRING);
	}

	try {
		string retval;
		hash = digest_factory(NUM2LONG(algorithm));
		hash->setPlaintext(string(StringValuePtr(plaintext), RSTRING(plaintext)->len));
		if (IS_HMAC(NUM2LONG(algorithm))) {
			((JHMAC*) hash)->setKey(string(StringValuePtr(key), RSTRING(key)->len));
		}
		hash->hash();
		retval = hash->getHashtext(hex);

		delete hash;
		return retval;
	}
	catch (Exception& e) {
		if (hash != NULL) {
			delete hash;
		}
		rb_raise(rb_eCryptoPP_Error, e.GetWhat().c_str());
	}
}

/**
 * call-seq:
 *		digest(constant, plaintext) => String
 *
 * Digest the plaintext and returns the result in binary. Use the
 * <tt>*_DIGEST</tt> constants to select an algorithm.
 */
VALUE rb_module_digest(int argc, VALUE *argv, VALUE self)
{
	string retval = module_digest(argc, argv, self, false);
	return rb_tainted_str_new(retval.data(), retval.length());
}

/**
 * call-seq:
 *		digest_hex(constant, plaintext) => String
 *
 * Digest the plaintext and returns the result in hex. Use the
 * <tt>*_DIGEST</tt> constants to select an algorithm.
 */
VALUE rb_module_digest_hex(int argc, VALUE *argv, VALUE self)
{
	string retval = module_digest(argc, argv, self, true);
	return rb_tainted_str_new(retval.data(), retval.length());
}


/* Digests an appropriate Ruby IO object. */
static string module_digest_io(int argc, VALUE *argv, VALUE self, bool hex)
{
	JHash* hash = NULL;
	VALUE algorithm, io;

	rb_scan_args(argc, argv, "2", &algorithm, &io);
	try {
		string retval;
		hash = digest_factory(NUM2LONG(algorithm));
		retval = hash->hashRubyIO(&io, hex);

		delete hash;
		return retval;
	}
	catch (Exception& e) {
		if (hash != NULL) {
			delete hash;
		}
		rb_raise(rb_eCryptoPP_Error, e.GetWhat().c_str());
	}
}

/**
 * call-seq:
 *		digest_io(io) => String
 *
 * Digests a Ruby IO object and spits out the result in binary. You can use
 * any sort of Ruby object as long as it implements <tt>eof?</tt>,
 * <tt>read</tt>, <tt>write</tt> and <tt>flush</tt>.
 *
 * Example:
 *
 *	cipher.digest_io(File.open("http://example.com/"))
 */
VALUE rb_module_digest_io(int argc, VALUE *argv, VALUE self)
{
	string retval = module_digest_io(argc, argv, self, false);
	return rb_tainted_str_new(retval.data(), retval.length());
}

/**
 * call-seq:
 *		digest_io_hex(io) => String
 *
 * Digests a Ruby IO object and spits out the result in hex. You can use
 * any sort of Ruby object as long as it implements <tt>eof?</tt>,
 * <tt>read</tt>, <tt>write</tt> and <tt>flush</tt>.
 *
 * Example:
 *
 *	cipher.digest_io_hex(File.open("http://example.com/"))
 */
VALUE rb_module_digest_io_hex(int argc, VALUE *argv, VALUE self)
{
	string retval = module_digest_io(argc, argv, self, true);
	return rb_tainted_str_new(retval.data(), retval.length());
}


/**
 * Is a Digest/HMAC algorithm available?
 */
VALUE rb_module_digest_enabled(VALUE self, VALUE d)
{
	if (hash_enabled((enum HashEnum) NUM2INT(d))) {
		return Qtrue;
	}
	else {
		return Qfalse;
	}
}


/* Returns the name of a hash algorithm. */
VALUE rb_module_digest_name(VALUE self, VALUE h)
{
	switch ((enum HashEnum) NUM2INT(h)) {
		default:
			rb_raise(rb_eCryptoPP_Error, "could not find a valid digest type");
		break;

#		define CHECKSUM_ALGORITHM(klass, r, c) \
			case r ## _CHECKSUM: \
				return rb_tainted_str_new2(c::getHashName().c_str());
#		include "checksums.def"
#		undef CHECKSUM_ALGORITHM

#		define HASH_ALGORITHM(klass, r, c) \
			case r ## _HASH: \
				return rb_tainted_str_new2(c::getHashName().c_str());
#		include "hashes.def"
#		undef HASH_ALGORITHM

# 		define HMAC_ALGORITHM(klass, r, c) \
			case r ## _HMAC: \
				return rb_tainted_str_new2(c::getHashName().c_str());
#		include "hmacs.def"
#		undef HMAC_ALGORITHM
	}
}


/**
 * Returns the name of the algorithm being used.
 */
VALUE rb_digest_algorithm_name(VALUE self)
{
	JHash *hash = NULL;
	Data_Get_Struct(self, JHash, hash);
	return rb_module_digest_name(self, INT2NUM(hash->getHashType()));
}


/**
 * Clears a Digest's plaintext and hashtext.
 */
VALUE rb_digest_clear(VALUE self)
{
	JHash *hash = NULL;
	Data_Get_Struct(self, JHash, hash);
	hash->clear();
	return Qnil;
}


/**
 * Validates if the digest text is a valid digest for plaintext.
 */
VALUE rb_digest_validate(VALUE self)
{
	JHash *hash = NULL;
	Data_Get_Struct(self, JHash, hash);
	if (hash->validate()) {
		return Qtrue;
	}
	else {
		return Qfalse;
	}
}


/* Instance version of <tt>CryptoPP#digest_io</tt>. */
static string digest_digest_io(VALUE self, VALUE io, bool hex)
{
	try {
		JHash *hash;
		Data_Get_Struct(self, JHash, hash);
		return hash->hashRubyIO(&io, hex);
	}
	catch (Exception& e) {
		rb_raise(rb_eCryptoPP_Error, e.GetWhat().c_str());
	}
}

/**
 * Instance version of <tt>CryptoPP#digest_io</tt>.
 */
VALUE rb_digest_digest_io(VALUE self, VALUE io)
{
	string retval = digest_digest_io(self, io, false);
	return rb_tainted_str_new(retval.data(), retval.length());
}

/**
 * Instance version of <tt>CryptoPP#digest_io_hex</tt>.
 */
VALUE rb_digest_digest_io_hex(VALUE self, VALUE io)
{
	string retval = digest_digest_io(self, io, true);
	return rb_tainted_str_new(retval.data(), retval.length());
}


/**
 * Returns an Array of available Digest algorithms.
 */
VALUE rb_module_digest_list(VALUE self)
{
	VALUE ary;
	ary = rb_ary_new();

#	define CHECKSUM_ALGORITHM(klass, r, c) \
		rb_ary_push(ary, INT2NUM(r ##_CHECKSUM));
#	include "checksums.def"
#	undef CHECKSUM_ALGORITHM

#	define HASH_ALGORITHM(klass, r, c) \
		rb_ary_push(ary, INT2NUM(r ##_HASH));
#	include "hashes.def"
#	undef HASH_ALGORITHM

	return ary;
}


/* Figure out options for a HMAC. */
static void digest_hmac_options(VALUE self, VALUE options)
{
	digest_options(self, options);

	{
		VALUE key = rb_hash_aref(options, ID2SYM(rb_intern("key")));
		VALUE key_hex = rb_hash_aref(options, ID2SYM(rb_intern("key_hex")));
		if (!NIL_P(key) && !NIL_P(key_hex)) {
			rb_raise(rb_eCryptoPP_Error, "can't set both key and key_hex in options");
		}
		else if (!NIL_P(key)) {
			digest_hmac_key_eq(self, key, false);
		}
		else if (!NIL_P(key_hex)) {
			digest_hmac_key_eq(self, key_hex, true);
		}
	}

	{
		VALUE key_length = rb_hash_aref(options, ID2SYM(rb_intern("key_length")));
		if (!NIL_P(key_length)) {
			rb_digest_hmac_key_length_eq(self, key_length);
		}
	}
}


/**
 * Creates a new HMAC object.
 */
VALUE rb_module_hmac_factory(int argc, VALUE *argv, VALUE self)
{
	if (argc < 1 || argc > 3) {
		rb_raise(rb_eArgError, "wrong number of arguments (%d for 1)", argc);
	}
	else {
		JHash* hash = NULL;
		VALUE algorithm = argv[0];
		VALUE retval;

		long a = NUM2LONG(algorithm);
		if (!IS_HMAC(a)) {
			rb_raise(rb_eCryptoPP_Error, "invalid HMAC algorithm");
		}
		else {
			try {
				hash = digest_factory(a);
				retval = wrap_digest_in_ruby(hash);
			}
			catch (Exception& e) {
				if (hash != NULL) {
					delete hash;
				}
				rb_raise(rb_eCryptoPP_Error, e.GetWhat().c_str());
			}
			if (argc >= 2) {
				if (TYPE(argv[1]) == T_STRING) {
					digest_plaintext_eq(retval, argv[1], false);
					if (argc == 3) {
						Check_Type(argv[2], T_STRING);
						digest_hmac_key_eq(retval, argv[2], false);
					}
					hash->hash();
				}
				else if (argc > 2) {
					rb_raise(rb_eArgError, "wrong argument types (expected a String or a Hash");
				}
				else {
					digest_hmac_options(retval, argv[1]);
				}
			}
			return retval;
		}
	}
}

#define HMAC_ALGORITHM(klass, r, n) \
VALUE rb_digest_hmac_ ## r ##_new(int argc, VALUE *argv, VALUE self) \
{ \
	if (argc > 2) { \
		rb_raise(rb_eArgError, "wrong number of arguments (%d for 2)", argc); \
	} \
	else { \
		VALUE retval; \
		JHash* hash = NULL; \
		try { \
			hash = digest_factory(r ## _HMAC); \
			retval = wrap_digest_in_ruby(hash); \
		} \
		catch (Exception& e) { \
			if (hash != NULL) { \
				delete hash; \
			} \
			rb_raise(rb_eCryptoPP_Error, e.GetWhat().c_str()); \
		} \
		if (argc >= 1) { \
			if (TYPE(argv[0]) == T_STRING) { \
				digest_plaintext_eq(retval, argv[0], false); \
				if (argc == 2) { \
					Check_Type(argv[1], T_STRING); \
					digest_hmac_key_eq(retval, argv[1], false); \
				} \
				hash->hash(); \
			} \
			else if (argc > 1) { \
				rb_raise(rb_eArgError, "wrong argument types (expected a String or a Hash"); \
			} \
			else { \
				digest_hmac_options(retval, argv[0]); \
			} \
		} \
		return retval; \
	} \
}
#include "hmacs.def"
#undef HMAC_ALGORITHM


/* Set the key. The true length of the key might not be what you expect,
 * as different algorithms behave differently */
static string digest_hmac_key_eq(VALUE self, VALUE key, bool hex)
{
	JHash *hash = NULL;
	Check_Type(key, T_STRING);
	Data_Get_Struct(self, JHash, hash);
	((JHMAC*) hash)->setKey(string(StringValuePtr(key), RSTRING(key)->len), hex);
	return ((JHMAC*) hash)->getKey(hex);
}

/**
 * Sets the key on a HMAC in binary.
 */
VALUE rb_digest_hmac_key_eq(VALUE self, VALUE key)
{
	digest_hmac_key_eq(self, key, false);
	return key;
}

/**
 * Sets the key on a HMAC in hex.
 */
VALUE rb_digest_hmac_key_hex_eq(VALUE self, VALUE key)
{
	digest_hmac_key_eq(self, key, true);
	return key;
}


/* Get the key. */
static string digest_hmac_key(VALUE self, bool hex)
{
	JHash *hash = NULL;
	Data_Get_Struct(self, JHash, hash);
	return ((JHMAC*) hash)->getKey(hex);
}

/**
 * Returns the key from the HMAC in binary.
 */
VALUE rb_digest_hmac_key(VALUE self)
{
	string retval = digest_hmac_key(self, false);
	return rb_tainted_str_new(retval.data(), retval.length());
}

/**
 * Returns the key from the HMAC in hex.
 */
VALUE rb_digest_hmac_key_hex(VALUE self)
{
	string retval = digest_hmac_key(self, false);
	return rb_tainted_str_new(retval.data(), retval.length());
}


/**
 * Sets the key length. Some HMACs require rather specific key lengths,
 * and if the key length you attempt to set is invalid, an exception will
 * be thrown. The key length being set is set in terms of bytes in binary, not
 * hex characters.
 */
VALUE rb_digest_hmac_key_length_eq(VALUE self, VALUE l)
{
	JHash *hash = NULL;
	unsigned int length = NUM2UINT(l);
	Data_Get_Struct(self, JHash, hash);
	((JHMAC*) hash)->setKeylength(length);
	if (((JHMAC*) hash)->getKeylength() != length) {
		rb_raise(rb_eCryptoPP_Error, "tried to set a key length of %d but %d was used", length, ((JHMAC*) hash)->getKeylength());
	}
	else {
		return l;
	}
}


/**
 * call-seq:
 *		key_length=(length) => Integer
 *
 * Sets the key length. Some HMACs require rather specific key lengths,
 * and if the key length you attempt to set is invalid, an exception will
 * be thrown. The key length being set is set in terms of bytes in binary, not
 * hex characters.
 */
VALUE rb_digest_hmac_key_length(VALUE self)
{
	JHash *hash = NULL;
	Data_Get_Struct(self, JHash, hash);
	return rb_fix_new(((JHMAC*) hash)->getKeylength());
}


/* Digest the plaintext. */
static string module_hmac_digest(int argc, VALUE *argv, VALUE self, bool hex)
{
	JHash *hash;
	VALUE algorithm, plaintext, key;

	rb_scan_args(argc, argv, "12", &algorithm, &plaintext, &key);
	Check_Type(plaintext, T_STRING);
	{
		string retval;
		hash = digest_factory(NUM2LONG(algorithm));
		hash->setPlaintext(string(StringValuePtr(plaintext), RSTRING(plaintext)->len));
		if (argc == 3) {
			Check_Type(plaintext, T_STRING);
			((JHMAC*) hash)->setKey(string(StringValuePtr(key), RSTRING(key)->len));
		}
		hash->hash();
		retval = hash->getHashtext(hex);

		delete hash;
		return retval;
	}
}

/**
 * call-seq:
 *		digest(constant, plaintext) => String
 *		digest(constant, plaintext, key) => String
 *
 * Singleton method for digesting with a HMAC. The plaintext and key values
 * are in binary and the return value is in binary.
 */
VALUE rb_module_hmac_digest(int argc, VALUE *argv, VALUE self)
{
	string retval = module_hmac_digest(argc, argv, self, false);
	return rb_tainted_str_new(retval.data(), retval.length());
}

/**
 * call-seq:
 *		digest_hex(constant, plaintext) => String
 *		digest_hex(constant, plaintext, key) => String
 *
 * Singleton method for digesting with a HMAC. The plaintext and key values
 * are in binary and the return value is in hex.
 */
VALUE rb_module_hmac_digest_hex(int argc, VALUE *argv, VALUE self)
{
	string retval = module_hmac_digest(argc, argv, self, true);
	return rb_tainted_str_new(retval.data(), retval.length());
}


/**
 * Returns an Array of available HMAC algorithms.
 */
VALUE rb_module_hmac_list(VALUE self)
{
	VALUE ary;
	ary = rb_ary_new();

#	define HMAC_ALGORITHM(klass, r, c) \
		rb_ary_push(ary, INT2NUM(r ##_HMAC));
#	include "hmacs.def"
#	undef HMAC_ALGORITHM

	return ary;
}