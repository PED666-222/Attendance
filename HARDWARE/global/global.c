#include "allhead.h"

/* 异或加密 *//*key值、原始密码、加密密文、长度*/
void xor_encryption(uint8_t key,char *src,char *dest,uint32_t len)
{
	char *psrc=src;
	char *pdest=dest;

	while(len--)
	{
		*pdest = *psrc ^ key;
		psrc++;
		pdest++;
	}
}

/* 异或解密 *//*key值、加密密码、原始密文、长度*/
void xor_decryption(uint8_t key,char *src,char *dest,uint32_t len)
{

	char *psrc=src;
	char *pdest=dest;

	while(len--)
	{
		*pdest = *psrc ^ key;
		psrc++;
		pdest++;
	}

}
