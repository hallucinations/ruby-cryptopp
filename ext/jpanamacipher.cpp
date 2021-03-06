
/*
 * Copyright (c) 2002-2014 J Smith <dark.panda@gmail.com>
 * Crypto++ copyright (c) 1995-2013 Wei Dai
 * See MIT-LICENSE for the extact license
 */

#include "jpanamacipher.h"

#if ENABLED_PANAMA_LITTLE_ENDIAN_CIPHER
SymmetricCipher* JPanamaCipherLE::getEncryptionObject()
{
  return new PanamaCipher<LittleEndian>::Encryption((byte*) itsKey.data(), itsKeylength, (byte*) itsIV.data());
}

SymmetricCipher* JPanamaCipherLE::getDecryptionObject()
{
  return new PanamaCipher<LittleEndian>::Decryption((byte*) itsKey.data(), itsKeylength, (byte*) itsIV.data());
}
#endif

#if ENABLED_PANAMA_BIG_ENDIAN_CIPHER
SymmetricCipher* JPanamaCipherBE::getEncryptionObject()
{
  return new PanamaCipher<BigEndian>::Encryption((byte*) itsKey.data(), itsKeylength, (byte*) itsIV.data());
}

SymmetricCipher* JPanamaCipherBE::getDecryptionObject()
{
  return new PanamaCipher<BigEndian>::Decryption((byte*) itsKey.data(), itsKeylength, (byte*) itsIV.data());
}
#endif
