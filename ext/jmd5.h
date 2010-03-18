/*
   +----------------------------------------------------------------------+
   | Copyright (c) 2002-2009 J Smith <dark.panda@gmail.com>               |
   | Crypto++ sources (not included) copyright (c) 1995-2009 Wei Dai      |
   +----------------------------------------------------------------------+
   | Check out COPYING for the exact license.                             |
   +----------------------------------------------------------------------+

$Id: $
*/

#ifndef __JMD5_H__
#define __JMD5_H__

#include "jconfig.h"

#if ENABLED_MD5_HASH || ENABLED_MD5_HMAC

#ifndef CRYPTOPP_ENABLE_NAMESPACE_WEAK
#define CRYPTOPP_ENABLE_NAMESPACE_WEAK 1
#endif

#if ENABLED_MD5_HASH
#include "jhash_t.h"
#endif

#if ENABLED_MD5_HMAC
#include "jhmac_t.h"
#endif

// Crypto++ headers...

#include "md5.h"

using namespace CryptoPP;

#if ENABLED_MD5_HASH
class JMD5 : public JHash_Template<Weak::MD5, MD5_HASH>
{
	public:
		JMD5(string plaintext = "") : JHash_Template<Weak::MD5, MD5_HASH>(plaintext) { }

		static string getHashName() { return "MD5"; }
};
#endif

#if ENABLED_MD5_HMAC
class JMD5_HMAC : public JHMAC_Template<Weak::MD5, MD5_HMAC>
{
	public:
		JMD5_HMAC(string plaintext = "") : JHMAC_Template<Weak::MD5, MD5_HMAC>(plaintext) { }

		static string getHashName() { return "MD5-HMAC"; }
};
#endif

#undef CRYPTOPP_ENABLE_NAMESPACE_WEAK

#endif
#endif