/*************************************************************************
       > File Name: SM4.h
       > Author:NEWPLAN
       > E-mail:newplan001@163.com
       > Created Time: Thu Apr 13 23:55:50 2017
************************************************************************/
#ifndef XYSSL_SM4_H
#define XYSSL_SM4_H

#include <string>

/**
 * \brief          SM4 context structure
 */
typedef struct
{
    int mode;                   /*!<  encrypt/decrypt   */
    unsigned long sk[32];       /*!<  SM4 subkeys       */
} sm4_context;


#ifdef __cplusplus
extern "C" {
#endif

/**
 * \brief          SM4 key schedule (128-bit, encryption)
 *
 * \param ctx      SM4 context to be initialized
 * \param key      16-byte secret key
 */
void sm4_setkey_enc( sm4_context *ctx, unsigned char key[16] );

/**
 * \brief          SM4 key schedule (128-bit, decryption)
 *
 * \param ctx      SM4 context to be initialized
 * \param key      16-byte secret key
 */
void sm4_setkey_dec( sm4_context *ctx, unsigned char key[16] );

/**
 * \brief          SM4-ECB block encryption/decryption
 * \param ctx      SM4 context
 * \param mode     SM4_ENCRYPT or SM4_DECRYPT
 * \param length   length of the input data
 * \param input    input block
 * \param output   output block
 */
void sm4_crypt_ecb( sm4_context *ctx,
                    int length,
                    unsigned char *input,
                    unsigned char *output);

/**
 * \brief          SM4-CBC buffer encryption/decryption
 * \param ctx      SM4 context
 * \param mode     SM4_ENCRYPT or SM4_DECRYPT
 * \param length   length of the input data
 * \param iv       initialization vector (updated after use)
 * \param input    buffer holding the input data
 * \param output   buffer holding the output data
 */
void sm4_crypt_cbc( sm4_context *ctx,
                    int length,
                    unsigned char iv[16],
                    unsigned char *input,
                    unsigned char *output );

int SM4DecryptCBC(const std::string &encryptInput,
                  const char key[16],
                  const char iv[16],
                  std::string &decryptOutput);
int SM4EncryptCBC(const std::string &sourceInput,
                  const char key[16],
                  const char iv[16],
                  std::string &encryptOutput);

#ifdef __cplusplus
}
#endif

#endif /* sm4.h */
