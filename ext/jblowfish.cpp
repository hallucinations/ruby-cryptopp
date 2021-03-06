
/*
 * Copyright (c) 2002-2014 J Smith <dark.panda@gmail.com>
 * Crypto++ copyright (c) 1995-2013 Wei Dai
 * See MIT-LICENSE for the extact license
 */

#include "jblowfish.h"

#if ENABLED_BLOWFISH_CIPHER

BlockCipher* JBlowfish::getEncryptionObject()
{
  return new BlowfishEncryption((byte*) itsKey.data(), itsKeylength);
}

BlockCipher* JBlowfish::getDecryptionObject()
{
  return new BlowfishDecryption((byte*) itsKey.data(), itsKeylength);
}

#endif
